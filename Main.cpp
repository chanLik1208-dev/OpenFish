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
#include <QComboBox>
#include <QFileInfo>
//
void customMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    Q_UNUSED(context);
    QString txt;
    switch (type) {
        case QtDebugMsg:    txt = QString("[除錯] %1").arg(msg); break;
        case QtWarningMsg:  txt = QString("[警告] %1").arg(msg); break;
        case QtCriticalMsg: txt = QString("[嚴重] %1").arg(msg); break;
        case QtFatalMsg:    txt = QString("[致命] %1").arg(msg); abort();
        case QtInfoMsg:     txt = QString("[資訊] %1").arg(msg); break;
    }

    // 🔥 獲取程式路徑，並確保檔案名稱正確
    QString logPath = QCoreApplication::applicationDirPath() + "/pet_debug.log";
    QFile outFile(logPath);
    
    // 使用 Append (附加) 模式，如果檔案不存在會自動建立
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&outFile);
        // 設定編碼為 UTF-8，防止中文亂碼
        ts.setEncoding(QStringConverter::Utf8); 
        ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ") << txt << Qt::endl;
        ts.flush();
        outFile.close();
    }
}

class DesktopPet : public QMainWindow {
    Q_OBJECT

private:
    QWidget* centralWidget;
    QLabel* imageLabel;
    QLabel* speechLabel;
    QPoint oldPos;

    QTimer* checkTimer;
    QTimer* idleTimer;
    QNetworkAccessManager* networkManager;

    QString apiAddress;
    QString modelName;

    QPixmap pixmapIdle;
    QPixmap pixmapCheer;
    QPixmap pixmapJump;
    QPixmap pixmapTurn;

    // 🔥 新增：儲存目前的縮放比例 (1.0 = 100%)
    double scaleFactor = 1.0;

public:
    DesktopPet(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);

        QSettings settings("MyPetApp", "DesktopPet");
        apiAddress = settings.value("apiAddress", "http://localhost:11434/api/generate").toString();
        modelName = settings.value("modelName", "llama3").toString();
        scaleFactor = settings.value("scaleFactor", 1.0).toDouble(); // 讀取縮放設定

        centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        speechLabel = new QLabel(centralWidget);
        speechLabel->setAlignment(Qt::AlignCenter);
        speechLabel->setWordWrap(true);

        imageLabel = new QLabel(centralWidget);
        imageLabel->setAlignment(Qt::AlignCenter);

        networkManager = new QNetworkAccessManager(this);

        checkTimer = new QTimer(this);
        connect(checkTimer, &QTimer::timeout, this, &DesktopPet::proactiveGreeting);
        checkTimer->start(300000);

        idleTimer = new QTimer(this);
        idleTimer->setSingleShot(true);
        connect(idleTimer, &QTimer::timeout, this, [this]() {
            imageLabel->setPixmap(pixmapIdle);
            updateStateText("等待主人的指令喵...");
            });

        // 🔥 初始化：套用目前的縮放比例並顯示
        applyScale(scaleFactor);
        updateStateText("喵... 主人好...");
    }

    // 🔥 新增：動態調整所有元件大小的核心函式
    void applyScale(double newScale) {
        scaleFactor = newScale;

        // 1. 計算縮放後的圖片尺寸 (原圖基準：寬 200, 高 300)
        int imgW = 200 * scaleFactor;
        int imgH = 300 * scaleFactor;

        // 2. 重新讀取並縮放圖片
        pixmapIdle = QPixmap(":/images/Idle.png").scaled(imgW, imgH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pixmapCheer = QPixmap(":/images/Idle-Happy.png").scaled(imgW, imgH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pixmapJump = QPixmap(":/images/Happy-Jump.png").scaled(imgW, imgH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pixmapTurn = QPixmap(":/images/Happy-Jump-Back.png").scaled(imgW, imgH, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        imageLabel->setFixedSize(imgW, imgH);

        if (pixmapIdle.isNull()) {
            imageLabel->setText("🐱\n(找不到素材檔案)");
            imageLabel->setStyleSheet(QString("font-size: %1px; color: pink;").arg(50 * scaleFactor));
        }
        else {
            imageLabel->setPixmap(pixmapIdle);
        }

        // 3. 計算並套用氣泡框的文字與 padding 縮放 (確保字體不會小於 9px)
        int fontSize = qMax(9, int(15 * scaleFactor));
        int padding = int(16 * scaleFactor);
        int borderRadius = int(12 * scaleFactor);

        speechLabel->setStyleSheet(QString(
            "QLabel {"
            "color: white; "
            "background-color: rgba(255, 105, 180, 210); "
            "border-radius: %1px; "
            "padding: %2px; "
            "font-family: 'Microsoft JhengHei', 'PingFang TC', sans-serif; "
            "font-size: %3px; "
            "font-weight: bold;"
            "}"
        ).arg(borderRadius).arg(padding).arg(fontSize));

        // 4. 觸發畫面排版更新
        updateStateText(speechLabel->text());
    }

    void updateStateText(const QString& newText) {
        speechLabel->setText(newText);

        speechLabel->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        speechLabel->adjustSize();

        // 🔥 動態計算視窗最大寬度 (基準：350)
        int currentAppWidth = 350 * scaleFactor;

        if (speechLabel->width() > currentAppWidth) {
            speechLabel->setFixedWidth(currentAppWidth);
            speechLabel->adjustSize();
        }

        int speechWidth = speechLabel->width();
        int speechHeight = speechLabel->height();

        int imgW = 200 * scaleFactor;
        int imgH = 300 * scaleFactor;

        int imageX = (currentAppWidth - imgW) / 2;
        int speechX = (currentAppWidth - speechWidth) / 2;

        int spacing = 10 * scaleFactor; // 圖片與氣泡的間距也要縮放
        int totalHeight = speechHeight + imgH + spacing;

        int oldHeight = this->height();
        int heightDiff = totalHeight - oldHeight;

        this->setFixedSize(currentAppWidth, totalHeight);
        centralWidget->setFixedSize(currentAppWidth, totalHeight);

        if (this->isVisible() && heightDiff != 0) {
            this->move(this->x(), this->y() - heightDiff);
        }

        speechLabel->move(speechX, 0);
        imageLabel->move(imageX, speechHeight + spacing);
    }

    void updateState(const QPixmap& newImage, const QString& newText) {
        imageLabel->setPixmap(newImage);
        updateStateText(newText);
        idleTimer->stop();
    }

    void askOllama(const QString& userInput) {
        QUrl url(apiAddress);
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
        QNetworkReply* reply = networkManager->post(request, data);

        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray responseData = reply->readAll();
                QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
                QString replyText = jsonResponse.object()["response"].toString();

                updateState(pixmapCheer, replyText);

                int displayTimeMs = 3000 + (replyText.length() * 250);
                if (displayTimeMs < 5000) displayTimeMs = 5000;
                if (displayTimeMs > 30000) displayTimeMs = 30000;
                idleTimer->start(displayTimeMs);
            }
            else {
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
        // 改用指標並指定 parent，避免記憶體洩漏與懸空參照
        QDialog* dialog = new QDialog(this);
        dialog->setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint);
        dialog->setWindowTitle("寵物設定");
        dialog->setStyleSheet("background-color: white;");

        QFormLayout* form = new QFormLayout(dialog);

        QLineEdit* urlInput = new QLineEdit(apiAddress, dialog);
        QLineEdit* modelInput = new QLineEdit(modelName, dialog);

        QComboBox* scaleCombo = new QComboBox(dialog);
        scaleCombo->addItem("50%", 0.5);
        scaleCombo->addItem("75%", 0.75);
        scaleCombo->addItem("100%", 1.0);
        scaleCombo->addItem("150%", 1.5);
        scaleCombo->addItem("200%", 2.0);

        int idx = scaleCombo->findData(scaleFactor);
        if (idx >= 0) scaleCombo->setCurrentIndex(idx);

        form->addRow("Ollama API 地址:", urlInput);
        form->addRow("AI 模型名稱:", modelInput);
        form->addRow("寵物顯示大小:", scaleCombo);

        QPushButton* saveButton = new QPushButton("儲存設定", dialog);
        form->addRow(saveButton);

        // 用來記錄「視窗關閉後」是否需要進行縮放
        bool pendingScale = false;
        double newScaleValue = scaleFactor;

        // 🔥 使用 [=] 捕獲指標，確保安全
        connect(saveButton, &QPushButton::clicked, [=, &pendingScale, &newScaleValue]() {
            apiAddress = urlInput->text();
            modelName = modelInput->text();
            newScaleValue = scaleCombo->currentData().toDouble();

            QSettings settings("MyPetApp", "DesktopPet");
            settings.setValue("apiAddress", apiAddress);
            settings.setValue("modelName", modelName);
            settings.setValue("scaleFactor", newScaleValue);

            if (!qFuzzyCompare(newScaleValue, scaleFactor)) {
                pendingScale = true; // 標記需要縮放，但不立刻執行
            }

            dialog->accept(); // 關閉視窗
            });

        dialog->exec(); // 程式會停在這裡，直到設定視窗關閉

        // 🔥 核心修復：等設定視窗「完全關閉」後，再來改變主視窗大小，徹底消滅 Crash！
        if (pendingScale) {
            applyScale(newScaleValue);
        }

        dialog->deleteLater();
    }
protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            oldPos = event->globalPosition().toPoint();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if ((event->buttons() & Qt::LeftButton) && !oldPos.isNull()) {
            QPoint delta = event->globalPosition().toPoint() - oldPos;
            move(x() + delta.x(), y() + delta.y());
            oldPos = event->globalPosition().toPoint();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            oldPos = QPoint();
        }
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        Q_UNUSED(event);
        updateState(pixmapJump, "感受著主人的觸碰... (思考中)");
        askOllama("主人剛剛用手摸了你的頭和耳朵，請給出反應。");
    }

    void contextMenuEvent(QContextMenuEvent* event) override {
        QMenu menu;
        menu.setStyleSheet(
            "QMenu { background-color: white; color: black; border: 1px solid gray; }"
            "QMenu::item:selected { background-color: #ffb6c1; color: black; }"
        );

        // 🔥 新增：在右鍵選單中加入「調整大小」子選單，方便快速切換
        QMenu* sizeMenu = menu.addMenu("調整大小 (Size)");
        QList<double> scales = { 0.5, 0.75, 1.0, 1.5, 2.0 };
        for (double s : scales) {
            QAction* act = sizeMenu->addAction(QString("%1%").arg(int(s * 100)));
            act->setCheckable(true);
            if (qFuzzyCompare(s, scaleFactor)) act->setChecked(true); // 勾選目前的大小

            connect(act, &QAction::triggered, this, [this, s]() {
                QSettings settings("MyPetApp", "DesktopPet");
                settings.setValue("scaleFactor", s);
                applyScale(s); // 立刻套用新大小
                });
        }
        menu.addSeparator(); // 加一條分隔線會比較好看

        QAction* settingsAction = menu.addAction("設定 (Settings)");
        QAction* logAction = menu.addAction("開啟除錯日誌 (Open Log)");
        QAction* quitAction = menu.addAction("離開 (Quit)");

        connect(settingsAction, &QAction::triggered, this, &DesktopPet::openSettings);
        connect(logAction, &QAction::triggered, []() {
            // 🔥 使用絕對路徑
            QString logFilePath = QCoreApplication::applicationDirPath() + "/pet_debug.log";
            QFile file(logFilePath);

            if (!file.exists()) {
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&file);
                    out << "[資訊] 日誌檔案沒有找到。" << Qt::endl;
                    file.close();
                }
            }
            QDesktopServices::openUrl(QUrl::fromLocalFile(logFilePath));
            });
        connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

        menu.exec(event->globalPos());
    }
};

int main(int argc, char* argv[]) {
    // 🔥 必須先初始化 app，才能註冊 MessageHandler
    QApplication app(argc, argv);
    qInstallMessageHandler(customMessageHandler);

    DesktopPet pet;
    pet.show();
    return app.exec();
}
#include "Main.moc"