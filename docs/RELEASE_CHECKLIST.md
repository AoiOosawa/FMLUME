# リリース前チェックリスト

配布用のインストーラーを出す前に確認する項目。POLYBRISK と同じ流れ（`docs/DECISIONS.md` D-049, D-050）。

## 1. コードとテスト
- [ ] `cmake --build build --config Release` が通る（開発用「FMLUME Dev」）
- [ ] `ctest --test-dir build -C Release` が全件合格
- [ ] `scripts/validate.sh` が合格（auval、pluginval 厳格度 10、VST3 と AU）
- [ ] `fmlume-presetgen Resources/Presets --check` が問題 0 件（全マクロの可聴性、音量）
- [ ] `fmlume-uisnap --check-keys` が問題 0 件（ホストのキー操作を奪わない）
- [ ] Intel 向けコード: テスト一式を x86_64 でビルドし、Rosetta 上で全件合格
- [ ] `python3 scripts/build_guide.py --check`（使い方ページが `docs/USER_GUIDE.md` と一致）

## 2. 実機確認（自動化できないもの）
- [ ] DAW での認識・発音・オートメーション・プロジェクトの保存と再読込・書き出し（`docs/TEST_PLAN.md` の HOST-01。Cubase / Logic Pro ほか）
- [ ] DAW 上でスペースキーや Cmd+S などのキー操作がホストに届く
- [ ] 画面の実操作（エンベロープのドラッグ、FX の並べ替え、ゲートのステップ、ブラウザーの Delete / Import / Export）
- [ ] ファクトリー音色の試聴（音量の飛び出し、ノイズ、マクロの効き方）

## 3. 識別子とバージョン（配布後は変更しない）
- [ ] 名前 `FMLUME`、Bundle ID `info.cheebow.fmlume`、AU `aumu Fmlu Chbw`
- [ ] `CMakeLists.txt` の `project(FMLUME VERSION x.y.z)` が出したいバージョン
- [ ] プリセット形式（`schemaVersion`）とホストパラメーター ID を、前回の配布から壊していない（`docs/COMPATIBILITY.md` §5）
- [ ] 音の指紋（`tests/Golden/sound_fingerprints.json`）を記録し直した場合は、理由が DECISIONS にある

## 4. インストーラー
- [ ] `SIGN_APP` / `SIGN_PKG` / `NOTARY_PROFILE` を指定して `scripts/package.sh` を実行（署名と公証。Developer ID を使い Apple へ送信するので、実行は本人の判断で）
- [ ] 3 つの部品が Universal（arm64 + x86_64）、最低 macOS 12.0、`DEV` の印なし
- [ ] `spctl --assess --type install -vv <pkg>` が「Notarized Developer ID」
- [ ] インストール先: `/Library/Audio/Plug-Ins/VST3`、`/Library/Audio/Plug-Ins/Components`、`/Applications`
- [ ] インストールした製品版で `auval -v aumu Fmlu Chbw` と pluginval が合格
- [ ] 開発環境のない別の Mac でインストールして起動

## 5. 公開
- [x] リポジトリの公開（AGPL-3.0 の「対応するソースコードの提供」を満たす。2026-11-24）
- [ ] GitHub Releases にインストーラーと SHA-256 を掲載
- [ ] 製品ページ（`site/`）と README にダウンロードの案内
- [x] GitHub Pages で `site/`（製品紹介と使い方 `guide.html`）を公開（`.github/workflows/pages.yml`。2026-11-24）
- [ ] ロゴの文字をアウトライン化（現在はシステムフォント依存）
