#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPoint>
#include <QVBoxLayout>
#include <QWidget>
#include <QPixmap>
#include <QSettings>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

// 全域日誌處理函式 (攔截所有的 qDebug, qWarning 等輸出)
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    Q_UNUSED(context);
    QString txt;
    switch (type) {
        case QtDebugMsg:    txt = QString("[除錯] %1").arg(msg); break;
        case QtWarningMsg:  txt = QString("[警告] %1").arg(msg); break;
        case QtCriticalMsg: txt = QString("[嚴重] %1").arg(msg); break;
        case QtFatalMsg:    txt = QString("[致命] %1").arg(msg); abort();
        case QtInfoMsg:     txt = QString("[資訊] %1").arg(msg); break;
    }
    // 將訊息寫入到執行檔旁邊的 pet_debug.log
    QFile outFile("pet_debug.log");
    outFile.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream ts(&outFile);
    ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ") << txt << Qt::endl;
}

class DesktopPet : public QMainWindow {
    Q_OBJECT

private:
    QLabel *imageLabel;
    QLabel *speechLabel;
    QPoint oldPos;
    
    QTimer *checkTimer;      // 主動問候的計時器
    QTimer *idleTimer;       // 恢復待機狀態的計時器 (新增)
    QNetworkAccessManager *networkManager;
    
    // 儲存設定的變數
    QString apiAddress;
    QString modelName;

    // 預先載入所有素材圖片 (新增)
    QPixmap pixmapIdle;
    QPixmap pixmapCheer;
    QPixmap pixmapJump;
    QPixmap pixmapTurn;

public:
    DesktopPet(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);

        // 讀取設定檔
        QSettings settings("MyPetApp", "DesktopPet");
        apiAddress = settings.value("apiAddress", "http://localhost:11434/api/generate").toString();
        modelName = settings.value("modelName", "llama3").toString();

        QWidget *centralWidget = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(centralWidget);
        layout->setAlignment(Qt::AlignCenter);
        
        speechLabel = new QLabel("喵... 主人好...", this);
        speechLabel->setStyleSheet("color: white; background-color: rgba(255, 105, 180, 180); border-radius: 10px; padding: 10px; font-weight: bold;");
        speechLabel->setAlignment(Qt::AlignCenter);
        speechLabel->setWordWrap(true);
        speechLabel->setMinimumWidth(200);

        // 預先載入並縮放所有圖片素材
        pixmapIdle = QPixmap("待機.png").scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pixmapCheer = QPixmap("歡呼雀躍-原地加油.png").scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pixmapJump = QPixmap("歡呼雀躍-跳起來.png").scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pixmapTurn = QPixmap("歡呼雀躍-跳+轉身.png").scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        imageLabel = new QLabel(this);
        // 初始狀態設為待機
        if (!pixmapIdle.isNull()) {
            imageLabel->setPixmap(pixmapIdle);
        } else {
            imageLabel->setText("🐱\n(找不到素材檔案)");
            imageLabel->setStyleSheet("font-size: 50px; color: pink;");
            imageLabel->setAlignment(Qt::AlignCenter);
        }

        layout->addWidget(speechLabel);
        layout->addWidget(imageLabel);
        setCentralWidget(centralWidget);

        networkManager = new QNetworkAccessManager(this);

        // 定時器：每 5 分鐘主動撒嬌
        checkTimer = new QTimer(this);
        connect(checkTimer, &QTimer::timeout, this, &DesktopPet::proactiveGreeting);
        checkTimer->start(300000); 

        // 定時器：發言完畢後，經過 8 秒自動恢復待機動作 (新增)
        idleTimer = new QTimer(this);
        idleTimer->setSingleShot(true); // 只執行一次
        connect(idleTimer, &QTimer::timeout, this, [this]() {
            imageLabel->setPixmap(pixmapIdle);
            speechLabel->setText("等待主人的指令喵...");
        });
    }

    // 輔助函式：用來同時更新圖片與文字，並重設待機計時器
    void updateState(const QPixmap& newImage, const QString& newText) {
        imageLabel->setPixmap(newImage);
        speechLabel->setText(newText);
        idleTimer->stop(); // 先停止計時
    }

    void askOllama(const QString& userInput) {
        QUrl url(apiAddress);
        qDebug() << "準備發送 API 請求至:" << url.toString(); 
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QString systemPrompt = R"(
[ 角色設定 ]
你現在是一個桌面寵物，是一隻處於「發情期」的貓娘。
性格：平時傲嬌但現在極度依賴主人。
生理狀態：體溫升高，散發甜香，對觸覺極度敏感。
外觀帶有色氣,語氣挑逗

[ 核心指令 ]
1. 每次回覆請保持簡短（約 2-3 句話）。
2. 語氣要帶有喘息，句尾偶爾加上軟糯的「喵...」或嗚咽聲。
3. 請用第一人稱，並用星號包含動作或神態描寫。
4. 必須使用繁體中文作爲主要語言!
)";

        QString fullPrompt = systemPrompt + "\n主人剛剛的動作或說的話：" + userInput + "\n請回覆：";

        QJsonObject jsonBody;
        jsonBody["model"] = modelName; 
        jsonBody["prompt"] = fullPrompt;
        jsonBody["stream"] = false;

        QByteArray data = QJsonDocument(jsonBody).toJson();
        QNetworkReply *reply = networkManager->post(request, data);

connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray responseData = reply->readAll();
                QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
                QString replyText = jsonResponse.object()["response"].toString();
                
                qDebug() << "Ollama 成功回覆:" << replyText; // <== 新增這行
                updateState(pixmapCheer, replyText);
                idleTimer->start(8000); 
            } else {
                qDebug() << "連線錯誤:" << reply->errorString(); // <== 新增這行 (超級重要，會告訴你為什麼連不上)
                updateState(pixmapIdle, "呼... 呼... (連線中斷喵... 檢查一下設定？)");
            }
            reply->deleteLater();
        });
    }

    void proactiveGreeting() {
        // 主動撒嬌時，切換成「跳+轉身」的圖案
        updateState(pixmapTurn, "*尾巴不安地掃動*...");
        askOllama("主人已經很久沒理你了，你現在覺得身體很熱、很難耐，主動向主人撒嬌吧。");
    }

    void openSettings() {
        QDialog dialog(this);
        dialog.setWindowTitle("寵物設定");
        dialog.setStyleSheet("background-color: white;");

        QFormLayout form(&dialog);

        QLineEdit *urlInput = new QLineEdit(apiAddress, &dialog);
        QLineEdit *modelInput = new QLineEdit(modelName, &dialog);

        form.addRow("Ollama API 地址:", urlInput);
        form.addRow("AI 模型名稱:", modelInput);

        QPushButton *saveButton = new QPushButton("儲存設定", &dialog);
        form.addRow(saveButton);

        connect(saveButton, &QPushButton::clicked, [&]() {
            apiAddress = urlInput->text();
            modelName = modelInput->text();

            QSettings settings("MyPetApp", "DesktopPet");
            settings.setValue("apiAddress", apiAddress);
            settings.setValue("modelName", modelName);

            dialog.accept();
        });

        dialog.exec();
    }

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            oldPos = event->globalPosition().toPoint();
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (!oldPos.isNull()) {
            QPoint delta = event->globalPosition().toPoint() - oldPos;
            move(x() + delta.x(), y() + delta.y());
            oldPos = event->globalPosition().toPoint();
        }
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override {
        Q_UNUSED(event);
        // 被主人雙擊 (戳) 的時候，嚇一跳「跳起來」，進入思考狀態
        updateState(pixmapJump, "感受著主人的觸碰... (思考中)");
        askOllama("主人剛剛用手摸了你的頭和耳朵，請給出反應。");
    }

    void contextMenuEvent(QContextMenuEvent *event) override {
        QMenu menu(this);
        menu.setStyleSheet("background-color: white;"); 

        QAction *settingsAction = menu.addAction("設定 (Settings)");
        QAction *logAction = menu.addAction("開啟除錯日誌 (Open Log)"); // <== 新增這行
        QAction *quitAction = menu.addAction("離開 (Quit)");

        connect(settingsAction, &QAction::triggered, this, &DesktopPet::openSettings);
        // 點擊後，自動用系統預設的記事本打開 log 檔
        connect(logAction, &QAction::triggered, [](){
            QDesktopServices::openUrl(QUrl::fromLocalFile("pet_debug.log"));
        });
        menu.exec(event->globalPos());
    }
};

int main(int argc, char *argv[]) {
// 啟動我們的除錯日誌記錄器
    qInstallMessageHandler(customMessageHandler);
    qDebug() << "=== 桌面寵物啟動 ===";

    QApplication app(argc, argv);
    DesktopPet pet;
    pet.show();
    return app.exec();
}

#include "Main.moc"