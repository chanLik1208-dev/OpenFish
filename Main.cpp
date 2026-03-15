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
#include <QDir>
#include <QMap>

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

    // 獲取程式路徑，並確保檔案名稱正確
    QString logPath = QCoreApplication::applicationDirPath() + "/pet_debug.log";
    QFile outFile(logPath);

    // 使用 Append (附加) 模式，如果檔案不存在會自動建立
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&outFile);
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

    // 🔥 儲存目前的縮放比例
    double scaleFactor = 1.0;

    // 🔥 動態圖片系統所需的變數
    QStringList excludeList;
    QMap<QString, QString> actionToImageMap; // 動作代碼 -> 圖片檔名 (不含附檔名)
    QMap<QString, QPixmap> loadedPixmaps;    // 圖片檔名 -> 已縮放的快取圖片

public:
    DesktopPet(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);

        QSettings settings("MyPetApp", "DesktopPet");
        apiAddress = settings.value("apiAddress", "http://localhost:11434/api/generate").toString();
        modelName = settings.value("modelName", "llama3").toString();
        scaleFactor = settings.value("scaleFactor", 1.0).toDouble();

        centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        speechLabel = new QLabel(centralWidget);
        speechLabel->setAlignment(Qt::AlignCenter);
        speechLabel->setWordWrap(true);

        imageLabel = new QLabel(centralWidget);
        imageLabel->setAlignment(Qt::AlignBottom | Qt::AlignHCenter); // 確保腳掌貼地

        networkManager = new QNetworkAccessManager(this);

        checkTimer = new QTimer(this);
        connect(checkTimer, &QTimer::timeout, this, &DesktopPet::proactiveGreeting);
        checkTimer->start(300000);

        idleTimer = new QTimer(this);
        idleTimer->setSingleShot(true);
        connect(idleTimer, &QTimer::timeout, this, [this]() {
            // 返回待機狀態
            imageLabel->setPixmap(getPixmap(actionToImageMap["idlemea"]));
            updateStateText("等待主人的指令喵...");
            });

        // 🔥 初始化圖片對應與排除列表
        initializeImages();

        // 🔥 套用縮放並載入初始畫面
        applyScale(scaleFactor);
        updateStateText("喵... 主人好...");
    }

    // ==========================================
    // 🔥 動態載入圖片系統
    // ==========================================
    void initializeImages() {
        // 1. 讀取排除列表 (exclude.txt 放於 exe 旁邊)
        QString excludePath = QCoreApplication::applicationDirPath() + "/exclude.txt";
        QFile excludeFile(excludePath);
        if (excludeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&excludeFile);
            while (!in.atEnd()) {
                excludeList.append(in.readLine().trimmed());
            }
            excludeFile.close();
        }

        // 2. 設定動作與檔名的對應 (不用寫 .png)
        actionToImageMap["cheermea"] = "Idle-Happy";
        actionToImageMap["jumpmea"] = "Happy-Jump";
        actionToImageMap["turnmea"] = "Happy-Jump-Back";
        actionToImageMap["idlemea"] = "Idle";
    }

    QPixmap getPixmap(const QString& imageName) {
        // 如果在排除清單中，回傳空圖
        if (excludeList.contains(imageName + ".png") || excludeList.contains(imageName + ".jpg")) {
            qWarning() << "圖片在排除列表中喵，不予載入喵:" << imageName;
            return QPixmap();
        }

        // 檢查快取 (換比例時快取會被清空，重新縮放)
        if (loadedPixmaps.contains(imageName)) {
            return loadedPixmaps.value(imageName);
        }

        // 讀取 exe 旁邊的 images 資料夾
        QString imagePath = QCoreApplication::applicationDirPath() + "/images/" + imageName + ".png";

        // 支援 JPG 容錯 (如果您放的是 JPG)
        if (!QFile::exists(imagePath)) {
            imagePath = QCoreApplication::applicationDirPath() + "/images/" + imageName + ".jpg";
        }

        QPixmap pixmap(imagePath);
        if (!pixmap.isNull()) {
            int imgW = 200 * scaleFactor;
            int imgH = 300 * scaleFactor;
            QPixmap scaledPixmap = pixmap.scaled(imgW, imgH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            loadedPixmaps[imageName] = scaledPixmap; // 存入快取
            return scaledPixmap;
        }
        else {
            qWarning() << "找不到圖片素材喵:" << imagePath;
            return QPixmap();
        }
    }

    // ==========================================
    // 🔥 縮放與排版系統 (保留您最完美的邏輯)
    // ==========================================
    void applyScale(double newScale) {
        scaleFactor = newScale;

        // 🔥 清空快取，這樣下次 getPixmap 就會以新尺寸重新縮放圖片！
        loadedPixmaps.clear();

        int imgW = 200 * scaleFactor;
        int imgH = 300 * scaleFactor;
        imageLabel->setFixedSize(imgW, imgH);

        // 載入待機圖片
        QPixmap idlePix = getPixmap(actionToImageMap["idlemea"]);
        if (idlePix.isNull()) {
            imageLabel->setText("🐱\n(找不到素材檔案)");
            imageLabel->setStyleSheet(QString("font-size: %1px; color: pink;").arg(40 * scaleFactor));
        }
        else {
            imageLabel->setPixmap(idlePix);
        }

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

        updateStateText(speechLabel->text());
    }

    void updateStateText(const QString& newText) {
        speechLabel->setText(newText);
        speechLabel->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        speechLabel->adjustSize();

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

        int spacing = 10 * scaleFactor;
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
        if (!newImage.isNull()) {
            imageLabel->setPixmap(newImage);
        }
        updateStateText(newText);
        idleTimer->stop();
    }

    // ==========================================
    // 🔥 雙重 API 思考系統
    // ==========================================
    void callApi(const QUrl& url, const QJsonObject& jsonBody, std::function<void(const QByteArray&)> onSuccess) {
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QByteArray data = QJsonDocument(jsonBody).toJson();
        QNetworkReply* reply = networkManager->post(request, data);

        connect(reply, &QNetworkReply::finished, this, [this, reply, onSuccess]() {
            if (reply->error() == QNetworkReply::NoError) {
                onSuccess(reply->readAll());
            }
            else {
                qWarning() << "連線錯誤喵:" << reply->errorString();
                updateState(getPixmap(actionToImageMap["idlemea"]), "呼... 呼... (連線中斷喵... 檢查一下設定？)");
            }
            reply->deleteLater();
            });
    }

    void startConversation(const QString& userInput) {
        QUrl url(apiAddress);

        // 1. 第一次思考：回應文本
        QString responseSystemPrompt = R"(
[ 角色設定 ]
你現在是一個桌面寵物，是一隻處於「發情期」的貓娘。
性格：平時傲嬌但現在極度依賴主人。生理狀態：體溫升高，散發甜香，對觸覺極度敏感。
[ 核心指令 ]
1. 每次回覆請保持簡短（約 2-3 句話,包括形容詞!!）。
2. 語氣要帶有喘息，句尾加上軟糯的「喵...」。
3. 必須使用繁體中文作爲主要語言!
)";
        QString fullResponsePrompt = responseSystemPrompt + "\n主人說：" + userInput + "\n請回覆：";

        QJsonObject responseBody;
        responseBody["model"] = modelName;
        responseBody["prompt"] = fullResponsePrompt;
        responseBody["stream"] = false;

        callApi(url, responseBody, [this, url, userInput](const QByteArray& responseData) {
            QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
            QString replyText = jsonResponse.object()["response"].toString();

            // 2. 第二次思考：決定動作
            QString actionSystemPrompt = R"(
[ 指令 ]
你是一個桌面寵物的動作決策核心。請根據貓娘的回應，從以下動作中選出一個最合適的代碼：
- cheermea (表示開心、加油)
- jumpmea (表示驚訝、興奮)
- turnmea (表示傲嬌、撒嬌、害羞)

[ 輸入 ]
貓娘的回應：%1

[ 輸出格式 ]
只需要回答動作代碼，不需要任何其他字！
)";
            QJsonObject actionBody;
            actionBody["model"] = modelName;
            actionBody["prompt"] = actionSystemPrompt.arg(replyText);
            actionBody["stream"] = false;

            callApi(url, actionBody, [this, replyText](const QByteArray& actionData) {
                QJsonDocument jsonAction = QJsonDocument::fromJson(actionData);
                QString actionCode = jsonAction.object()["response"].toString().trimmed();

                // 3. 獲取對應圖片並更新
                QString imageName = actionToImageMap.value(actionCode);
                QPixmap selectedPixmap;

                if (!imageName.isEmpty()) {
                    selectedPixmap = getPixmap(imageName);
                }

                // 防呆：如果圖片找不到或代碼亂答，用撒嬌動作
                if (selectedPixmap.isNull()) {
                    selectedPixmap = getPixmap(actionToImageMap["turnmea"]);
                }

                updateState(selectedPixmap, replyText);

                int displayTimeMs = 3000 + (replyText.length() * 250);
                idleTimer->start(qBound(5000, displayTimeMs, 30000));
                });
            });
    }

    void proactiveGreeting() {
        updateState(getPixmap(actionToImageMap["turnmea"]), "*尾巴不安地掃動*...");
        startConversation("主人已經很久沒理你了，你現在覺得身體很熱、很難耐，主動向主人撒嬌吧。");
    }

    void openSettings() {
        QDialog* dialog = new QDialog(this);
        dialog->setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint);
        dialog->setWindowTitle("寵物設定");
        dialog->setStyleSheet("background-color: white;");

        QFormLayout* form = new QFormLayout(dialog);

        // API 快速切換選單
        QComboBox* apiTypeCombo = new QComboBox(dialog);
        apiTypeCombo->addItem("Ollama (預設)", "ollama");
        apiTypeCombo->addItem("LM Studio", "lmstudio");
        if (apiAddress.contains(":1234")) apiTypeCombo->setCurrentIndex(1);

        QLineEdit* urlInput = new QLineEdit(apiAddress, dialog);
        QLineEdit* modelInput = new QLineEdit(modelName, dialog);

        QComboBox* scaleCombo = new QComboBox(dialog);
        QList<double> scales = { 0.5, 0.75, 1.0, 1.5, 2.0 };
        for (double s : scales) scaleCombo->addItem(QString("%1%").arg(int(s * 100)), s);
        int idx = scaleCombo->findData(scaleFactor);
        if (idx >= 0) scaleCombo->setCurrentIndex(idx);

        form->addRow("API 類型:", apiTypeCombo);
        form->addRow("API 地址:", urlInput);
        form->addRow("AI 模型名稱:", modelInput);
        form->addRow("寵物顯示大小:", scaleCombo);

        QPushButton* saveButton = new QPushButton("儲存設定", dialog);
        form->addRow(saveButton);

        bool pendingScale = false;
        double newScaleValue = scaleFactor;

        connect(saveButton, &QPushButton::clicked, [=, &pendingScale, &newScaleValue]() mutable {
            QString selectedApi = apiTypeCombo->currentData().toString();
            QString currentUrl = urlInput->text();

            // 自動幫忙改網址
            if (selectedApi == "lmstudio" && !currentUrl.contains(":1234")) {
                currentUrl = "http://localhost:1234/v1/chat/completions";
            }
            else if (selectedApi == "ollama" && currentUrl.contains(":1234")) {
                currentUrl = "http://localhost:11434/api/generate";
            }

            apiAddress = currentUrl;
            modelName = modelInput->text();
            newScaleValue = scaleCombo->currentData().toDouble();

            QSettings settings("MyPetApp", "DesktopPet");
            settings.setValue("apiAddress", apiAddress);
            settings.setValue("modelName", modelName);
            settings.setValue("scaleFactor", newScaleValue);

            if (!qFuzzyCompare(newScaleValue, scaleFactor)) {
                pendingScale = true;
            }

            QMessageBox::information(dialog, "成功", "設定已成功儲存喵！");
            dialog->accept();
            });

        dialog->exec();

        if (pendingScale) {
            applyScale(newScaleValue);
        }
        dialog->deleteLater();
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) oldPos = event->globalPosition().toPoint();
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if ((event->buttons() & Qt::LeftButton) && !oldPos.isNull()) {
            QPoint delta = event->globalPosition().toPoint() - oldPos;
            move(x() + delta.x(), y() + delta.y());
            oldPos = event->globalPosition().toPoint();
        }
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) oldPos = QPoint();
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        Q_UNUSED(event);
        // 先顯示思考狀態
        updateState(getPixmap(actionToImageMap["jumpmea"]), "感受著主人的觸碰喵... (思考中)");
        startConversation("主人剛剛用手摸了你的頭和耳朵，請給出反應。");
    }

    void contextMenuEvent(QContextMenuEvent* event) override {
        QMenu menu;
        menu.setStyleSheet(
            "QMenu { background-color: white; color: black; border: 1px solid gray; }"
            "QMenu::item:selected { background-color: #ffb6c1; color: black; }"
        );

        QMenu* sizeMenu = menu.addMenu("調整大小 (Size)");
        QList<double> scales = { 0.5, 0.75, 1.0, 1.5, 2.0 };
        for (double s : scales) {
            QAction* act = sizeMenu->addAction(QString("%1%").arg(int(s * 100)));
            act->setCheckable(true);
            if (qFuzzyCompare(s, scaleFactor)) act->setChecked(true);

            connect(act, &QAction::triggered, this, [this, s]() {
                QSettings settings("MyPetApp", "DesktopPet");
                settings.setValue("scaleFactor", s);
                applyScale(s);
                });
        }
        menu.addSeparator();

        QAction* settingsAction = menu.addAction("設定 (Settings)");
        QAction* logAction = menu.addAction("開啟除錯日誌 (Open Log)");
        QAction* quitAction = menu.addAction("離開 (Quit)");

        connect(settingsAction, &QAction::triggered, this, &DesktopPet::openSettings);
        connect(logAction, &QAction::triggered, []() {
            QString logFilePath = QCoreApplication::applicationDirPath() + "/pet_debug.log";
            QDesktopServices::openUrl(QUrl::fromLocalFile(logFilePath));
            });
        connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

        menu.exec(event->globalPos());
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    qInstallMessageHandler(customMessageHandler);
    qDebug() << "=== 桌面寵物啟動 ===";
    DesktopPet pet;
    pet.show();
    return app.exec();
}
#include "Main.moc"