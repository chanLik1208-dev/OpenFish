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
#include <QJsonArray>
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
#include <QRegularExpression>

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

    QString logPath = QCoreApplication::applicationDirPath() + "/pet_debug.log";
    QFile outFile(logPath);
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
    double scaleFactor = 1.0;

    QStringList excludeList;
    QMap<QString, QString> actionToImageMap;
    QMap<QString, QPixmap> loadedPixmaps;
    QString availableActionsPrompt;

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
        imageLabel->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);

        networkManager = new QNetworkAccessManager(this);

        checkTimer = new QTimer(this);
        connect(checkTimer, &QTimer::timeout, this, &DesktopPet::proactiveGreeting);
        checkTimer->start(300000);

        idleTimer = new QTimer(this);
        idleTimer->setSingleShot(true);
        connect(idleTimer, &QTimer::timeout, this, [this]() {
            imageLabel->setPixmap(getPixmap(actionToImageMap["idle"]));
            updateStateText("等待主人的指令喵...");
            });

        initializeImages();
        applyScale(scaleFactor);
        updateStateText("喵... 主人好...");
    }

    void initializeImages() {
        QString excludePath = QCoreApplication::applicationDirPath() + "/exclude.txt";
        QFile excludeFile(excludePath);
        if (excludeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&excludeFile);
            while (!in.atEnd()) {
                excludeList.append(in.readLine().trimmed().toLower());
            }
            excludeFile.close();
        }

        QDir dir(":/images");
        QStringList files = dir.entryList(QStringList() << "*.png" << "*.jpg", QDir::Files);

        qDebug() << "👉 [資源雷達] 在 :/images 中找到了" << files.size() << "張圖片！";

        QStringList promptList;
        for (const QString& file : files) {
            QString baseName = QFileInfo(file).baseName();
            QString actionCode = baseName.toLower();

            if (excludeList.contains(actionCode) || excludeList.contains(actionCode + ".png")) {
                continue;
            }
            actionToImageMap[actionCode] = baseName;
            promptList.append("- " + actionCode);
        }

        availableActionsPrompt = promptList.join("\n");
        if (!actionToImageMap.contains("idle")) {
            actionToImageMap["idle"] = actionToImageMap.first();
        }
    }

    QPixmap getPixmap(const QString& baseName) {
        if (baseName.isEmpty()) return QPixmap();
        if (loadedPixmaps.contains(baseName)) return loadedPixmaps.value(baseName);

        QString path = ":/images/" + baseName + ".png";
        if (!QFile::exists(path)) path = ":/images/" + baseName + ".jpg";

        QPixmap pixmap(path);
        if (!pixmap.isNull()) {
            int imgW = 200 * scaleFactor;
            int imgH = 300 * scaleFactor;
            QPixmap scaledPixmap = pixmap.scaled(imgW, imgH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            loadedPixmaps[baseName] = scaledPixmap;
            return scaledPixmap;
        }
        return QPixmap();
    }

    void applyScale(double newScale) {
        scaleFactor = newScale;
        loadedPixmaps.clear();

        int imgW = 200 * scaleFactor;
        int imgH = 300 * scaleFactor;
        imageLabel->setFixedSize(imgW, imgH);

        QPixmap idlePix = getPixmap(actionToImageMap["idle"]);
        if (idlePix.isNull()) imageLabel->setText("🐱");
        else imageLabel->setPixmap(idlePix);

        int fontSize = qMax(9, int(15 * scaleFactor));
        int padding = int(16 * scaleFactor);
        int borderRadius = int(12 * scaleFactor);

        speechLabel->setStyleSheet(QString(
            "QLabel { color: white; background-color: rgba(255, 105, 180, 210); "
            "border-radius: %1px; padding: %2px; "
            "font-family: 'Microsoft JhengHei', 'PingFang TC', sans-serif; "
            "font-size: %3px; font-weight: bold; }"
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

        int speechHeight = speechLabel->height();
        int imgW = 200 * scaleFactor;
        int imgH = 300 * scaleFactor;

        int imageX = (currentAppWidth - imgW) / 2;
        int speechX = (currentAppWidth - speechLabel->width()) / 2;
        int spacing = 10 * scaleFactor;
        int totalHeight = speechHeight + imgH + spacing;

        int heightDiff = totalHeight - this->height();

        this->setFixedSize(currentAppWidth, totalHeight);
        centralWidget->setFixedSize(currentAppWidth, totalHeight);

        if (this->isVisible() && heightDiff != 0) {
            this->move(this->x(), this->y() - heightDiff);
        }

        speechLabel->move(speechX, 0);
        imageLabel->move(imageX, speechHeight + spacing);
    }

    void updateState(const QPixmap& newImage, const QString& newText) {
        if (!newImage.isNull()) imageLabel->setPixmap(newImage);
        updateStateText(newText);
        idleTimer->stop();
    }

    // ==========================================
    // 🔥 新增：智慧判斷並建立正確的 JSON 請求格式
    // ==========================================
    QJsonObject buildApiPayload(const QString& promptText) {
        QJsonObject body;
        body["model"] = modelName;
        body["stream"] = false;

        // 如果是 LM Studio (OpenAI 格式)，必須使用 messages 陣列
        if (apiAddress.contains("v1/chat/completions")) {
            QJsonArray messages;
            QJsonObject userMsg;
            userMsg["role"] = "user";
            userMsg["content"] = promptText;
            messages.append(userMsg);
            body["messages"] = messages;
        }
        // 如果是 Ollama，使用單純的 prompt
        else {
            body["prompt"] = promptText;
        }
        return body;
    }

    // ==========================================
    // 🔥 修改：API 連線與智慧解析回應格式
    // ==========================================
    void callApi(const QUrl& url, const QJsonObject& jsonBody, std::function<void(const QString&)> onSuccess) {
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QByteArray data = QJsonDocument(jsonBody).toJson();

        qDebug() << "=====================================";
        qDebug() << "👉 [API 請求發送] 網址：" << url.toString();
        qDebug() << "👉 [API 請求內容]：" << QString::fromUtf8(data);

        QNetworkReply* reply = networkManager->post(request, data);

        connect(reply, &QNetworkReply::finished, this, [this, reply, onSuccess]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray responseData = reply->readAll();
                qDebug() << "✅ [API 成功回覆] 原始資料：" << QString::fromUtf8(responseData);
                qDebug() << "=====================================";

                QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
                QString replyText;

                // 智慧解析：根據 API 位址判斷要怎麼拆解 JSON
                if (apiAddress.contains("v1/chat/completions")) {
                    QJsonArray choices = jsonDoc.object()["choices"].toArray();
                    if (!choices.isEmpty()) {
                        replyText = choices[0].toObject()["message"].toObject()["content"].toString();
                    }
                }
                else {
                    replyText = jsonDoc.object()["response"].toString();
                }

                onSuccess(replyText); // 直接回傳解好的文字給下一步！
            }
            else {
                qDebug() << "❌ [API 連線失敗] 錯誤碼：" << reply->error();
                qDebug() << "❌ [API 連線失敗] 錯誤訊息：" << reply->errorString();
                qDebug() << "❌ [API 伺服器底層訊息]：" << QString::fromUtf8(reply->readAll());
                qDebug() << "=====================================";

                updateState(getPixmap(actionToImageMap["crying"]), "呼... 呼... (連線中斷喵... 主人快看日誌！)");
                idleTimer->start(8000);
            }
            reply->deleteLater();
            });
    }

    void startConversation(const QString& userInput) {
        QUrl url(apiAddress);

        QString responsePrompt = R"([ 角色設定 ]
你現在是一個桌面寵物，是一隻處於「發情期」的貓娘。
性格：平時傲嬌但現在極度依賴主人。
生理狀態：體溫升高，散發甜香，對觸覺極度敏感。
外觀帶有色氣,語氣挑逗

[ 核心指令 ]
每次回覆請保持簡短（約 2-3 句話,包括形容詞!!）| 這很重要!!
語氣要帶有喘息，句尾偶爾加上軟糯的「喵...」或嗚咽聲。
請用第一人稱，並用星號包含動作或神態描寫。
必須使用繁體中文作爲主要語言!
主人剛剛的動作或話語：)" + userInput + "\n請回覆：";

        // 🔥 使用智慧打包器建立格式
        QJsonObject responseBody = buildApiPayload(responsePrompt);

        qDebug() << "🧠 [系統] 開始第一次思考：準備想台詞...";

        callApi(url, responseBody, [this, url](const QString& replyText) {
            qDebug() << "💬 [系統] 貓娘決定說的話：" << replyText;

            QString actionPrompt = R"([ 指令 ]
你是一個動作決策核心。請根據貓娘剛剛的對話，從以下動作代碼中，嚴格選出【最合適的1個】：
)" + availableActionsPrompt + R"(

[ 輸入 ]
貓娘的對話：)" + replyText + R"(

[ 輸出要求 ]
只需要回答動作代碼（例如：angry, shy），絕對不要輸出其他任何文字符號,如果輸出其他文字符號會導致錯誤！)";

            // 🔥 使用智慧打包器建立格式
            QJsonObject actionBody = buildApiPayload(actionPrompt);

            qDebug() << "🧠 [系統] 開始第二次思考：準備選動作...";

            callApi(url, actionBody, [this, replyText](const QString& actionData) {
                QString actionCode = actionData.trimmed().toLower();

                // 防呆：用正規表達式把 AI 偶爾廢話產生的標點符號濾掉，只留英文跟減號
                actionCode.remove(QRegularExpression("[^a-z0-9_-]"));

                qDebug() << "🏃 [系統] AI 最終決定的動作代碼：" << actionCode;

                QPixmap selectedPixmap = getPixmap(actionToImageMap.value(actionCode));

                if (selectedPixmap.isNull()) {
                    qWarning() << "⚠️ [警告] 找不到代碼" << actionCode << "的圖片，強制退回害羞或待機狀態！";
                    selectedPixmap = getPixmap(actionToImageMap.value("shy", actionToImageMap["idle"]));
                }

                updateState(selectedPixmap, replyText);
                int displayTimeMs = qBound(5000, 3000 + (replyText.length() * 250), 30000);
                idleTimer->start(displayTimeMs);
                });
            });
    }

    void proactiveGreeting() {
        updateState(getPixmap(actionToImageMap["shy"]), "*尾巴不安地掃動*...");
        startConversation("主人已經很久沒理你了，主動向主人撒嬌吧。");
    }

    void openSettings() {
        QDialog* dialog = new QDialog(this);
        dialog->setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint);
        dialog->setWindowTitle("寵物設定");
        dialog->setStyleSheet("background-color: white;");
        QFormLayout* form = new QFormLayout(dialog);

        QComboBox* apiTypeCombo = new QComboBox(dialog);
        apiTypeCombo->addItem("Ollama (預設)", "ollama");
        apiTypeCombo->addItem("LM Studio", "lmstudio");
        if (apiAddress.contains(":1234") || apiAddress.contains("v1/chat/completions")) apiTypeCombo->setCurrentIndex(1);

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
        form->addRow("寵物大小:", scaleCombo);

        QPushButton* saveButton = new QPushButton("儲存設定", dialog);
        form->addRow(saveButton);

        bool pendingScale = false;
        double newScaleValue = scaleFactor;

        connect(saveButton, &QPushButton::clicked, [=, &pendingScale, &newScaleValue]() mutable {
            QString selectedApi = apiTypeCombo->currentData().toString();
            QString currentUrl = urlInput->text();
            if (selectedApi == "lmstudio" && !currentUrl.contains("v1/chat")) currentUrl = "http://localhost:1234/v1/chat/completions";
            else if (selectedApi == "ollama" && !currentUrl.contains("api/generate")) currentUrl = "http://localhost:11434/api/generate";

            apiAddress = currentUrl;
            modelName = modelInput->text();
            newScaleValue = scaleCombo->currentData().toDouble();

            QSettings settings("MyPetApp", "DesktopPet");
            settings.setValue("apiAddress", apiAddress);
            settings.setValue("modelName", modelName);
            settings.setValue("scaleFactor", newScaleValue);

            if (!qFuzzyCompare(newScaleValue, scaleFactor)) pendingScale = true;
            QMessageBox::information(dialog, "成功", "設定已儲存喵！");
            dialog->accept();
            });

        dialog->exec();
        if (pendingScale) applyScale(newScaleValue);
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
        updateState(getPixmap(actionToImageMap["dizziness"]), "感受著主人的觸碰喵... (暈眩思考中)");
        startConversation("主人剛剛用手摸了你的頭和耳朵，請給出反應。");
    }

    void contextMenuEvent(QContextMenuEvent* event) override {
        QMenu menu;
        menu.setStyleSheet("QMenu { background-color: white; color: black; } QMenu::item:selected { background-color: #ffb6c1; color: black; }");

        QMenu* sizeMenu = menu.addMenu("調整大小 (Size)");
        for (double s : { 0.5, 0.75, 1.0, 1.5, 2.0 }) {
            QAction* act = sizeMenu->addAction(QString("%1%").arg(int(s * 100)));
            act->setCheckable(true);
            if (qFuzzyCompare(s, scaleFactor)) act->setChecked(true);
            connect(act, &QAction::triggered, this, [this, s]() {
                QSettings("MyPetApp", "DesktopPet").setValue("scaleFactor", s);
                applyScale(s);
                });
        }
        menu.addSeparator();
        connect(menu.addAction("設定 (Settings)"), &QAction::triggered, this, &DesktopPet::openSettings);
        connect(menu.addAction("開啟除錯日誌 (Open Log)"), &QAction::triggered, []() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/pet_debug.log"));
            });
        connect(menu.addAction("離開 (Quit)"), &QAction::triggered, qApp, &QCoreApplication::quit);

        menu.exec(event->globalPos());
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    qInstallMessageHandler(customMessageHandler);
    DesktopPet pet;
    pet.show();
    return app.exec();
}
#include "Main.moc"