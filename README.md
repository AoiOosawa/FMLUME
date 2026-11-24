# FMLUME（エフエムリューム）

**The Power of FM. Without the Complexity.**

macOS 用 AU（AUv2）/ VST3 / Standalone の6オペレーター FM/PM シンセサイザー。Simple モードの8マクロと Sound Explorer で、オペレーターやアルゴリズムを知らなくても音作りができ、Advanced モードでは自由接続FM・Mod Matrix・9種のFXを直接編集できます。

- 使い方: [`docs/USER_GUIDE.md`](docs/USER_GUIDE.md)（公開用の HTML は `site/guide.html`。`python3 scripts/build_guide.py` で作る）
- 互換性の方針: [`docs/COMPATIBILITY.md`](docs/COMPATIBILITY.md)
- 仕様: [`SPEC.md`](SPEC.md)
- 実装状況と実測結果: [`docs/IMPLEMENTATION_STATUS.md`](docs/IMPLEMENTATION_STATUS.md)
- 構造: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) / 設計判断: [`docs/DECISIONS.md`](docs/DECISIONS.md)
- プリセット形式: [`docs/PRESET_SCHEMA.md`](docs/PRESET_SCHEMA.md) / テスト計画: [`docs/TEST_PLAN.md`](docs/TEST_PLAN.md)

## インストール

[最新のリリース](https://github.com/cheebow/FMLUME/releases/latest) からインストーラー（`.pkg`）をダウンロードして開き、画面の案内に沿って進めてください。VST3、Audio Unit、スタンドアロンアプリの中から、入れたいものを選べます。

- macOS 12 以降
- Apple Silicon。Intel Mac でも動くように作っていますが、実機ではまだ試せていません
- Apple の公証済みです

音と画面は [紹介ページ](https://cheebow.github.io/FMLUME/)、使い方とアンインストールの手順は [ユーザーガイド](docs/USER_GUIDE.md) にあります。

## 必要環境

- macOS 12 以降。開発用のビルドは Apple Silicon のみ、配布用（`scripts/package.sh`）は Universal（Apple Silicon + Intel。Intel の実機では未確認）
- Xcode + Command Line Tools、CMake 3.25 以降
- JUCE **9.0.6**（`external/JUCE`、Git submodule でタグ固定）

## ビルドとテスト

```bash
git submodule update --init --recursive

# プラグイン（AU + VST3 + Standalone）+ テスト + ツール
cmake -S . -B build -G Xcode
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

通常のビルドは**開発用プラグイン「FMLUME Dev」**になります（製品版「FMLUME」とは別の識別子・別の名前。DAW には別々のプラグインとして並び、取り違えたり上書きしたりしません）。ビルド後、`~/Library/Audio/Plug-Ins/Components/FMLUME Dev.component` と `~/Library/Audio/Plug-Ins/VST3/FMLUME Dev.vst3` へコピーされます（無効化: `-DFMLUME_COPY_PLUGIN=OFF`）。

製品版としてビルドするとき:

```bash
cmake -S . -B build-release -G Xcode -DFMLUME_DEV_BUILD=OFF
cmake --build build-release --config Release --parallel
```

| | 開発用（既定） | 製品版（`-DFMLUME_DEV_BUILD=OFF`） |
|---|---|---|
| 名前 | FMLUME Dev | FMLUME |
| AU | `aumu Fmld Chbw` | `aumu Fmlu Chbw` |
| Bundle ID | `info.cheebow.fmlume.dev` | `info.cheebow.fmlume` |
| ユーザープリセット | `~/Library/Application Support/FMLUME/Presets`（製品版と共通） | `~/Library/Application Support/FMLUME/Presets` |
| 画面 | ロゴの横に DEV の印 | 印なし |

開発用で保存した DAW プロジェクトは製品版では開けません（識別子が違うため）。開発用はテスト用と割り切ってください。

スタンドアロン版は `build/FMLUME_artefacts/Release/Standalone/FMLUME Dev.app` に生成されます（DAW不要。Audio/MIDIデバイスは左上の Options から設定。画面下の鍵盤でも発音できます）。

```bash
open "build/FMLUME_artefacts/Release/Standalone/FMLUME Dev.app"
```

DSPだけを素早く回す場合（JUCE不要・数秒でビルド）:

```bash
cmake --preset core && cmake --build build-core -j
ctest --test-dir build-core --output-on-failure
```

検証:

```bash
auval -v aumu Fmld Chbw   # 開発用。製品版は Fmlu
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --validate "$HOME/Library/Audio/Plug-Ins/VST3/FMLUME Dev.vst3"
```

## ツール

| コマンド | 用途 |
|---|---|
| `fmlume-render` | オフライン MIDI→WAV レンダラー。`--preset file.fmlume --midi song.mid --out out.wav`、`--template Bell --note 60 --note 67 --macro 1=0.9`、`--validate file.fmlume` など（引数なしでヘルプ） |
| `fmlume-presetgen Resources/Presets` | ファクトリープリセット152音色の再生成と検証（全マクロの可聴性・ラウドネス調整） |
| `fmlume-bench` | エンジンのCPU負荷計測（SPEC §17.3 の基準ケースを含む） |
| `python3 scripts/audition.py` | ファクトリー音色のスクリーニング（音量・音色・DC・マクロの効き・重複）と、1音色ずつ再生して評価を付けられる試聴ページ（`build/audition/index.html`）を作る（numpy / scipy が必要） |
| `fmlume-uisnap <dir>` | エディタ画面をPNGに書き出す開発用ツール（`-DFMLUME_BUILD_UI_SNAPSHOT=ON`）。`--icon in.svg out.png [size] [canvas]` でブランディングSVGをPNG化。`--check-edited <dir>` でプリセット名の `*`（編集済み表示）を実プロセッサー＋エディタで検査 |

## 使い方（概要）

画面の骨格（配色・ノブ・スイッチ・ヘッダー・ブラウザ）は同シリーズの POLYBRISK と共通です。FMLUME の基調色はシアン→バイオレットです。

1. **プリセットを選ぶ** — ヘッダーのプリセット名をクリックするとブラウザが開きます（検索、カテゴリ、Favorites、タグ、Init / Save As / Delete / Import / Export）。`<` `>` で前後に移動。
2. **MACROS / CHARACTER** — `Brightness / Attack / Release / Character` とカテゴリ別の4マクロ。50%が元の音。
3. **FX / TRANCE GATE / ARPEGGIATOR** — `Warmth / Width / Space / Echo`、ゲートのパターン（POLYBRISK と同じパターン集）、アルペジオのモードとレート。
4. **VARIATIONS** — `Generate` で8つのバリエーションを作り、タイル（A〜H）をクリックして試聴、気に入ったら `Keep this sound`（元の音と置き換わります）。`More like this` でさらに派生、`Original` で元の音に戻ります。
5. **ADVANCED** — Operators（上段が結線図。ドラッグで接続、箱をクリックしてオペレーターを選び、下段で波形・エンベロープ・レベルを編集。`Expand` で結線図を全面に広げ、18 種のテンプレートを並べる）/ Filter & Mod（Mod Envelope もグラフをドラッグして編集）/ FX / Rhythm / Performance。

ロゴとアイコンは `assets/branding/`（`python3 tools/branding/make_assets.py` で SVG を再生成、`fmlume-uisnap --icon` で PNG 化）。

## サイト

製品紹介ページは `site/index.html`（静的 HTML。画像は `site/assets/screens/`、音声デモは `site/assets/audio/`）。ブラウザーで直接開けます。

## 配布用インストーラー

```bash
scripts/package.sh          # build-dist/package/FMLUME-<version>-macOS.pkg
scripts/validate.sh         # auval と pluginval（直前にビルドしたプラグインを検査）
```

`scripts/package.sh` は製品版の名前と識別子で Universal ビルドを行い、VST3 / Audio Unit / Standalone を選んで入れられる `.pkg` を作ります。環境変数 `SIGN_APP` / `SIGN_PKG` / `NOTARY_PROFILE` を指定すると Developer ID での署名と Apple の公証まで行います。指定しなければ署名なしのインストーラーになり、作った Mac 以外では開けません。配布前の確認項目は [`docs/RELEASE_CHECKLIST.md`](docs/RELEASE_CHECKLIST.md)。

## ライセンス

FMLUME は [GNU Affero General Public License v3.0](LICENSE) で公開しています。使っているソフトウェアは [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) にまとめてあります。

JUCE 9 は AGPLv3 と商用ライセンスのデュアルライセンスです（`external/JUCE/LICENSE.md`）。FMLUME は AGPLv3 の条件で JUCE を使っており、その条件（対応するソースコードの提供）はこのリポジトリの公開で満たします。ソースを公開せずに配布・販売する場合は、JUCE の商用ライセンスが必要になり得ます（https://juce.com/legal/juce-9-licence/）。
