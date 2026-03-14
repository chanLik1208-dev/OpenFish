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

// 全域日誌處理函式
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
    
    QTimer *checkTimer;      
    QTimer *idleTimer;       
    QNetworkAccessManager *networkManager;
    
    QString apiAddress;
    QString modelName;

    QPixmap pixmapIdle;
    QPixmap pixmapCheer;
    QPixmap pixmapJump;
    QPixmap pixmapTurn;

public:
    DesktopPet(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);

        QSettings settings("MyPetApp", "DesktopPet");
        apiAddress = settings.value("apiAddress", "http://localhost:11434/api/generate").toString();
        modelName = settings.value("modelName", "llama3").toString();

        QWidget *centralWidget = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(centralWidget);
        layout->setAlignment(Qt::AlignCenter);
        
        speechLabel = new QLabel(this);
        
        // 🔥【排版優化 1】：指定易讀字體、放大字體(14px)、加寬內邊距(padding)
        speechLabel->setStyleSheet(
            "QLabel {"
            "color: white; "
            "background-color: rgba(255, 105, 180, 210); " // 稍微調高不透明度，讓字更清晰
            "border-radius: 12px; "
            "padding: 12px 16px; " // 上下 12px，左右 16px
            "font-family: 'Microsoft JhengHei', 'PingFang TC', sans-serif; "
            "font-size: 14px; "
            "font-weight: bold;"
            "}"
        );
        speechLabel->setAlignment(Qt::AlignCenter);
        speechLabel->setWordWrap(true);
        speechLabel->setMinimumWidth(200);
        speechLabel->setMaximumWidth(350); // 🔥 防止文字太長時氣泡變得無限寬

        pixmapIdle = QPixmap("Idle.png").scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pixmapCheer = QPixmap("Idle-Happy.png").scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pixmapJump = QPixmap("Happy-Jump.png").scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pixmapTurn = QPixmap("Happy-Jump-Back.png").scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        imageLabel = new QLabel(this);
        imageLabel->setFixedSize(200, 200); 
        
        // 🔥【復原優化】：讓貓娘的腳永遠貼在框框底部，切換圖片時不會浮空
        imageLabel->setAlignment(Qt::AlignBottom | Qt::AlignHCenter); 

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

        checkTimer = new QTimer(this);
        connect(checkTimer, &QTimer::timeout, this, &DesktopPet::proactiveGreeting);
        checkTimer->start(300000); 

        idleTimer = new QTimer(this);
        idleTimer->setSingleShot(true); 
        connect(idleTimer, &QTimer::timeout, this, [this]() {
            imageLabel->setPixmap(pixmapIdle);
            updateStateText("等待主人的指令喵..."); // 改用專屬函式更新文字
        });
        
        // 初始狀態文字
        updateStateText("喵... 主人好...");
    }

    // 🔥【排版優化 2】：專門處理文字，強制轉成 HTML 並加上 150% 的行距！
    void updateStateText(const QString& newText) {
        // 先將普通的換行符號轉成 HTML 的 <br>
        QString safeText = newText.toHtmlEscaped().replace("\n", "<br>");
        // 用 div 包起來，強制設定行距
        QString formattedText = QString("<div style='line-height: 150%;'>%1</div>").arg(safeText);
        speechLabel->setText(formattedText);
    }

    void updateState(const QPixmap& newImage, const QString& newText) {
        imageLabel->setPixmap(newImage);
        updateStateText(newText);
        idleTimer->stop(); 
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
1. 每次回覆請保持簡短（約 2-3 句話,包括形容詞!!）| 這很重要!!
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
                
                qDebug() << "Ollama 成功回覆:" << replyText; 
                updateState(pixmapCheer, replyText);
                
                // 🔥【復原優化】：根據字數動態決定要停留多少秒
                int displayTimeMs = 3000 + (replyText.length() * 250);
                if (displayTimeMs < 5000) displayTimeMs = 5000;
                if (displayTimeMs > 30000) displayTimeMs = 30000;
                idleTimer->start(displayTimeMs); 
            } else {
                qDebug() << "連線錯誤:" << reply->errorString(); 
                updateState(pixmapIdle, "呼... 呼... (連線中斷喵... 檢查一下設定？)");
                idleTimer->start(8000);
            }
            reply->deleteLater();
        });
    }

    void proactiveGreeting() {
        updateState(pixmapTurn, "*尾巴不安地掃動*...");
        askOllama("主人已經很久沒理你了，你現在覺得身體很熱、很難耐，主動向主人撒嬌吧。");
    }

    void openSettings() {
        QDialog dialog(nullptr); 
        dialog.setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint);
        
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
        updateState(pixmapJump, "感受著主人的觸碰... (思考中)");
        askOllama("主人剛剛用手摸了你的頭和耳朵，請給出反應。");
    }

    void contextMenuEvent(QContextMenuEvent *event) override {
        QMenu menu(this);
        menu.setStyleSheet("background-color: white;"); 

        QAction *settingsAction = menu.addAction("設定 (Settings)");
        QAction *logAction = menu.addAction("開啟除錯日誌 (Open Log)"); 
        QAction *quitAction = menu.addAction("離開 (Quit)");

        connect(settingsAction, &QAction::triggered, this, &DesktopPet::openSettings);
        connect(logAction, &QAction::triggered, [](){
            QDesktopServices::openUrl(QUrl::fromLocalFile("pet_debug.log"));
        });
        connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

        menu.exec(event->globalPos());
    }
};

int main(int argc, char *argv[]) {
    qInstallMessageHandler(customMessageHandler);
    qDebug() << "=== 桌面寵物啟動 ===";

    QApplication app(argc, argv);
    DesktopPet pet;
    pet.show();
    return app.exec();
}

#include "Main.moc"