# FMLUME 実装状況

最終更新: 2026-11-24 ／ 対象: Phase 0〜7（Phase 8「配布の準備」は未着手・配布方針未決定）

## 1. ビルド・検証結果（実測）

環境: Apple M1 Max / macOS 26 (Darwin 25.6) / Xcode 27.2 / CMake 4.4.5 / JUCE 9.0.6（`external/JUCE`、タグ固定）

| 項目 | コマンド | 結果 |
|---|---|---|
| 構成 | `cmake -S . -B build -G Xcode` | 成功 |
| ビルド | `cmake --build build --config Release --parallel` | **BUILD SUCCEEDED**。`FMLUME.component`（AU）、`FMLUME.vst3`、`FMLUME.app`（Standalone）を生成、いずれも Universal 2（arm64 + x86_64）、アドホック署名 |
| 自動テスト | `ctest --test-dir build -C Release` | **4/4 グループ通過**（59テストケース、失敗0。2026-11-24 の見直し D-060 後に再実行） |
| Standalone | `open build/FMLUME_artefacts/Release/Standalone/FMLUME.app` | 起動しエディタが表示されること、正常終了することを確認（クラッシュなし）。**発音・MIDI入力・Audio設定ダイアログは人手で未確認** |
| x86_64（Intel 向け） | `cmake -S . -B build-x86 -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DFMLUME_BUILD_PLUGIN=OFF -DCMAKE_OSX_ARCHITECTURES=x86_64` → `ctest --test-dir build-x86` | 54ケース全通過（2026-11-24、Morph 削除後。Rosetta 2 上。Intel実機は未検証） |
| AU 検証 | `auval -v aumu Fmld Chbw`（開発用。製品版は `Fmlu`） | **AU VALIDATION SUCCEEDED** |
| VST3 検証 | `pluginval --strictness-level 10 --validate FMLUME.vst3`（GUIテスト含む） | **SUCCESS** |
| AU を pluginval | `pluginval --strictness-level 10 --validate FMLUME.component` | SUCCESS（警告1件: "Current program is -1"。AUラッパー由来で実害なし） |
| サニタイザー | `-fsanitize=address,undefined` / `-fsanitize=thread` で Core とテストをビルド（`build-asan` / `build-tsan`） | ASan / UBSan: dsp・unit・integration 全通過、検出 0。TSan: integration（裏側での Variations 生成を含む）全通過、検出 0（2026-11-24、D-060） |
| JSON Schema | Draft 2020-12 バリデーターで照合 | ファクトリー152＋フィクスチャ2を受理、schemaVersion 2 のスキーマで再照合（2026-11-24）: ファクトリー152＋新形式フィクスチャ2を受理、破損フィクスチャ21のうち15を構造で拒否（残り6はローダー専用規則で拒否）。旧形式（schemaVersion 1）のフィクスチャはスキーマでは拒否され、ローダーがマイグレーションして読む |

### 性能（`fmlume-bench`、48 kHz / 256 frames / 単一スレッド / Release / M1 Max、実時間1コア比）

| ケース | 平均 | 最悪ブロック |
|---|---|---|
| **基準（SPEC §17.3）**: Standard 2x、16ノート×Unison 1、5オペE.Piano、EQ+Chorus+Delay+Reverb | **7.1 %** | 12.1 % |
| 同 Eco 1x | 3.8 % | 6.0 % |
| 同 High 4x | 14.0 % | 21.0 % |
| Standard 2x、16ノート、6オペ全使用 | 9.5 % | 13.8 % |
| Standard 2x、32ノート、6オペ | 11.6 % | 17.2 % |
| Standard 2x、8ノート×8 Unison（64 SubVoice）、6オペ | 19.0 % | 27.1 % |
| 最悪: 8×8 Unison、6オペ、全9 FX、2x | 19.5 % | 25.2 % |
| 最悪: 同 4x | 40.0 % | 52.8 % |

2026-11-24 に Morph を削除した後の計測。削除前の最悪ケース（Dual Morph、4x）は平均 56.1 % / 最悪 86.1 % だった。

基準ケースは目標「20%未満」を満たす（7.1%）。数値はエンジン単体の計測で、ホストやGUIのオーバーヘッドを含まない。機種・負荷状況で変わる設計目標値であり性能保証ではない。

## 2. 実装済み機能

### Phase 0 — 基盤
- Git、JUCE 9.0.6 submodule、CMake（`FMLUMECore` / `FMLUME` AU+VST3 / `FMLUMETests` / ツール群）、`CMakePresets.json`、GitHub Actions ワークフロー定義（`.github/workflows/build.yml`）。
- オフライン MIDI→WAV レンダラー `fmlume-render`（SMF読込、プリセット/テンプレート/マクロ/モーフ指定、検証モード）。

### Phase 1 — FMエンジン
- 6オペレーター位相変調、DAG検証とトポロジカルソート、エッジ最大30、セルフフィードバック（前サンプル参照）、指数ADSR、Ratio/Fixed、detune、key tracking、key sync/フリーラン位相、velocity→level/index。
- アルゴリズムテンプレート18種。ポリフォニー1〜32、ボイススチール（5msフェード）、モノ/レガート/ポルタメント、Pitch Bend、CC1、CC64、Channel Aftertouch、All Notes/Sound Off。
- 品質 Eco/Standard/High（1x/2x/4x）、線形位相デシメーション、正確なレイテンシ報告。

### Phase 2 — Simple / Advanced とマクロ
- Canonical Model、`PatchCompiler`、パッチ世代管理（インプレース更新／新世代）、ロックフリーのパッチ受け渡し。
- 8マクロ（正規化ドメイン合成、最大8マッピング、linear/exp/sCurve、bipolar/unipolar、min/max）、カテゴリテンプレート9種。
- UI: Simple（Macros / Character / FX / Trance Gate / Arpeggiator / Variations）と Advanced（Operators / Filter & Mod / FX / Rhythm / Performance。Algorithm は Operators に統合、D-057）。両モードは同一モデルを編集。プリセットブラウザはヘッダーのプリセット名から開く独立ページ。2026-11-23 に POLYBRISK とシリーズ共通の外観へ全面改訂（D-028）。

### Phase 3 — 音色拡張と基本FX
- 帯域制限Wavetable（triangle/saw/pulse）、TPT SVF（LP12/LP24/HP12/BP12＋Drive）、Unison（1〜8、detune、spread）、LFO×2（5波形、テンポ同期）、グローバルModEnv、Mod Matrix 8スロット。
- EQ / Drive（2x OS）/ Chorus / Delay / Reverb、FXクイックコントロール4種。

### Phase 4 — Preset / State
- `.fmlume` JSON（schemaVersion 2）、厳格な検証、schema 0 / 1 のマイグレーション（1 は Morph 用の 2 音目を捨てて読む）、`schemas/fmlume-preset.schema.json`、`docs/PRESET_SCHEMA.md`。
- DAW状態にCanonical Model全体＋ホスト値を保存。プリセットブラウザ（カテゴリ、タグ、フリーワード、お気に入り、Prev/Next、編集マーク、Save/上書き確認、Import/Export）。

### Phase 5 — Sound Explorer
- seed固定の8候補、方向8種、強度3段、Lock（graph / ratios / envelopes / FX / 個別マクロ）、安全性検証、重複抑制、ラウドネス補正、フォールバック、Preview/Cancel/Use/Explore From This/Regenerate/履歴20。

### Phase 6 — 演奏機能・全FX
- Morph（A/B スロット、Compatible / Dual）は実装後、2026-11-24 に削除した（D-055）。
- Arpeggiator、Chord Mode、Trance Gate（16ステップ、パターン/Randomise/Clear/Invert）。
- Phaser / Flanger / Compressor / Bitcrusher、FXの自由な並べ替え・個別バイパス・全バイパス。

### Phase 7 — 安定性・最適化・プリセット
- ファクトリープリセット**152音色**（11カテゴリ。Arp 30、Bell と Poly は13、他は各12。`fmlume-presetgen` で生成・検証）。
- 出力メーター（ヘッダーの OUT スライダー下）、実効最大ノート数表示（Performance タブ）、Dual×4x の負荷制御。CPU / Voice の数値表示は UI から削除（D-028。エンジン側の計測値は残存）。
- 5分ストレステスト、RT割当て検査、44.1〜192 kHz・ブロック32〜2048・0フレームの検証、x86_64 スライスのテスト実行。
- ツールチップとステータス行による操作ヘルプ、リサイズ（960×620〜1920×1200）。

## 3. ホスト公開パラメーター（不変）

`master.gainDb` `master.pan` `master.polyphony` `master.unison` `master.detune` `master.spread` `master.quality` `macro.1`〜`macro.8` `fx.bypassAll` `fx.quick.warmth` `fx.quick.width` `fx.quick.space` `fx.quick.echo` `arp.enabled` `tranceGate.enabled`（計22。`morph.amount` と `morph.enabled` は D-055 で廃止）。プラグインID: manufacturer `Chbw` / plugin `Fmlu` / bundle `info.cheebow.fmlume`（**暫定**、DECISIONS D-001）。

## 4. 未検証・未実施（重要）

| 項目 | 状態 |
|---|---|
| **実DAWでの確認（HOST-01）** | **Cubase は確認済み**（VST3、開発用ビルド、利用者による確認）。2026-11-23: 読み込み・演奏・保存は問題なし。2026-11-24（Morph 削除後）: オートメーションの記録と再生は問題なし、スペースキーや Cmd+S などのキー操作がホストに届くことも問題なし、オフライン書き出し（オーディオミックスダウン）も利用者が聴いて「大丈夫そう」と確認。Morph 削除前に保存したプロジェクトの読み込みは、該当するプロジェクトが無いため実機では確認できない（旧形式の状態を読む経路は自動テストで確認済み）。Logic Pro / REAPER / Ableton Live は**未実施**（手順は `docs/TEST_PLAN.md`）。自動検証は auval と pluginval |
| **耳による音質確認（§17.4）** | **未実施**（準備済み）。`python3 scripts/audition.py` が全音色の数値スクリーニング（`build/audition/report.md`）と試聴ページ（`build/audition/index.html`。1音色ずつ再生し、キー操作で評価を付けて結果をテキストにまとめる）を作る。2026-11-23 の実行で DC オフセットを発見し修正（D-051）。利用者の1回目の試聴で「他と似すぎ」等と指摘された 22 音色を作り直した（D-052）。2回目の試聴で残った指摘 4 件も修正し、利用者が「問題なし」と確認（D-053）。マクロの数値上の問題（リミッター・効きが聞こえない・音量が跳ねる）は生成時の自動ガード等で解消（D-054）。現在の指摘は 29 音色（主に A 特性で見たカテゴリ内の音量差 19 件と左右の偏り 4 件。壊れているものはない）。152音色とFXはFM設計の知識に基づいて作成し、自動検査（発音・レベル・全マクロの可聴性・非有限値なし）のみ通過。音楽的な良し悪し、マクロの効き方の自然さ、Explorer候補の「意味のある違い」は試聴と調整が必要 |
| Intel Mac 実機 | 未検証（配布用の Universal ビルドと、x86_64 でビルドしたテストの Rosetta 上での実行のみ） |
| GitHub Actions | `main` への push ごとに実行され成功（Morph 削除後の `53fe9af` を含む 3 回）。`actions/checkout` を v7 に更新した変更は push 後に確認する |
| 実機GUI操作 | 利用者が実際に操作して「大丈夫そう」と確認（2026-11-24、Morph 削除後のビルド）: エンベロープのドラッグ編集、アルゴリズム図のドラッグ接続、ゲートのステップ描画、ブラウザーの Delete / Import / Export。自動では `fmlume-uisnap`（全ページ・最小サイズの描画、キー操作の検査）と pluginval の GUI テストで確認。キーボードの Tab 移動と HiDPI 以外の表示は未確認 |
| コード署名・公証・インストーラー | `scripts/package.sh` で**署名なし**の製品版インストーラーを作成できることを確認（2026-11-24、Morph 削除後。`FMLUME-0.1.0-macOS.pkg`、VST3 / AU / Standalone とも Universal、最低 macOS 12.0、識別子 `aumu Fmlu Chbw`。製品版 VST3 は pluginval 厳しさ 10 に arm64 と x86_64（Rosetta）の両方で合格）。署名・公証・インストールの実行・別の Mac での確認は未実施 |

## 5. 仕様との差異・既知の制約

- **Poly Aftertouch / MPE**、**Per-Voice Filter**、**MIDI出力**、Shimmer Reverb 等は仕様どおり将来項目で未実装。UIには置いていない。
- Mod ソース（LFO/ModEnv/Velocity/Keytrack）はグローバル（D-004）。
- FXチェーンの再構成はクロスフェードではなく約10msのディップ（D-012）。
- ボイス終了判定はキャリアのみ（D-008）。パッチ受け渡しはFIFOではなく最新優先メールボックス（D-005）。
- Quality 切替時、発音中の音は短くフェードする。
- プリセット読込はマスター音量/パン/Quality/全FXバイパスを変更しない（D-010）。
- 操作マニュアルは未作成（ツールチップとステータス行のみ）。

## 6. 次のタスク候補

1. **HOST-01 の残り**（人手、任意）: Logic Pro（AU）と REAPER。Cubase は確認済み。
2. 操作マニュアルの作成。
3. ロゴの文字のアウトライン化（現在はシステムフォント依存）。
4. Phase 8（`docs/RELEASE_CHECKLIST.md`）: バージョン番号とプラグイン ID の最終確認、署名・公証、別の Mac でのインストール確認、リポジトリの公開、GitHub Releases。配布方針は AGPL-3.0 で決定済み（D-049）。
5. 任意: 数値スクリーニングに残る 29 音色の指摘（主にカテゴリ内の音量差）、Variations の方向づけの調整。
