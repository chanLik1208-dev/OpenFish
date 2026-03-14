# 圖片版權由Satsuki擁有!
禁止未經授權使用圖片進行商業活動
此項目由AI編寫!
使用此項目就是接受你自己是**貓娘控**這個**事實**
https://drive.google.com/drive/folders/1JcUlzJN6uToIkgZ4xF7xURpsONqtL-Iu?usp=sharing

## Mac會遇到的問題解決方法:

**不是你的檔案真的壞掉或編譯失敗**，而是 macOS 特有的安全機制（Gatekeeper）在作祟。

因為這個 `.app` 檔案是從 GitHub Actions（雲端）編譯好，然後透過 Safari 下載下來的，而且我們沒有花錢買 Apple Developer 憑證幫這個程式「數位簽章」。macOS 為了保護你的電腦，會自動幫它貼上一張「隔離標籤 (Quarantine)」，並直接跟你說它壞了，逼你把它丟進垃圾桶。

**🛠️ 解決辦法非常簡單，我們只需要用指令把那張「隔離標籤」撕掉就好了：**


### 第一步：打開 Mac 的「終端機」(Terminal)

按下鍵盤的 `Command (⌘) + 空白鍵` 打開 Spotlight，搜尋「終端機」或「Terminal」並打開它。

### 第二步：輸入解除隔離的指令

在終端機裡面輸入以下指令（**注意最後面要留一個半形空白**，不要按 Enter 喔！）：

```bash
xattr -cr 

```

### 第三步：把貓娘拖拉進去

接著，把桌面上（或下載資料夾裡）那個顯示損壞的 `DesktopPet.app`，直接**用滑鼠拖曳到終端機視窗裡面放開**。
終端機會自動幫你補上檔案路徑，看起來會像這樣：
`xattr -cr /Users/你的名字/Downloads/DesktopPet.app`

這時候，**按下 Enter 鍵**。
*(如果沒有跳出任何錯誤訊息，直接跑出新的一行，就代表執行成功了！)*

---

### 🚀 再次啟動！

現在，你可以關閉終端機，再次去雙擊那個 `DesktopPet.app`。
這次 macOS 就不會再說它壞掉了！貓娘應該就能順利在你的 Mac 桌面上跳出來囉！
