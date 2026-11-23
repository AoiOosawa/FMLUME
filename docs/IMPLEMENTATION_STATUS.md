# FMLUME 実装状況

最終更新: 2026-11-23 ／ 対象: Phase 0〜7（Phase 8「配布の準備」は未着手・配布方針未決定）

## 1. ビルド・検証結果（実測）

環境: Apple M1 Max / macOS 26 (Darwin 25.6) / Xcode 27.2 / CMake 4.4.5 / JUCE 9.0.6（`external/JUCE`、タグ固定）

| 項目 | コマンド | 結果 |
|---|---|---|
| 構成 | `cmake -S . -B build -G Xcode` | 成功 |
| ビルド | `cmake --build build --config Release --parallel` | **BUILD SUCCEEDED**。`FMLUME.component`（AU）、`FMLUME.vst3`、`FMLUME.app`（Standalone）を生成、いずれも Universal 2（arm64 + x86_64）、アドホック署名 |
| 自動テスト | `ctest --test-dir build -C Release` | **4/4 グループ通過**（57テストケース、失敗0、約53秒。2026-11-23 の最終変更後に再実行） |
| Standalone | `open build/FMLUME_artefacts/Release/Standalone/FMLUME.app` | 起動しエディタが表示されること、正常終了することを確認（クラッシュなし）。**発音・MIDI入力・Audio設定ダイアログは人手で未確認** |
| x86_64 スライス | `arch -x86_64 build/tests/Release/FMLUMETests` | 41ケース全通過（Rosetta 2 上。Intel実機は未検証） |
| AU 検証 | `auval -v aumu Fmld Chbw`（開発用。製品版は `Fmlu`） | **AU VALIDATION SUCCEEDED** |
| VST3 検証 | `pluginval --strictness-level 10 --validate FMLUME.vst3`（GUIテスト含む） | **SUCCESS** |
| AU を pluginval | `pluginval --strictness-level 10 --validate FMLUME.component` | SUCCESS（警告1件: "Current program is -1"。AUラッパー由来で実害なし） |
| JSON Schema | Draft 2020-12 バリデーターで照合 | ファクトリー152＋フィクスチャ2を受理、破損フィクスチャ22のうち16を構造で拒否（残り6はローダー専用規則で拒否） |

### 性能（`fmlume-bench`、48 kHz / 256 frames / 単一スレッド / Release / M1 Max、実時間1コア比）

| ケース | 平均 | 最悪ブロック |
|---|---|---|
| **基準（SPEC §17.3）**: Standard 2x、16ノート×Unison 1、5オペE.Piano、EQ+Chorus+Delay+Reverb、Morph off | **7.1 %** | 14.5 % |
| 同 Eco 1x | 3.8 % | 5.2 % |
| 同 High 4x | 13.9 % | 19.6 % |
| Standard 2x、16ノート、6オペ全使用 | 9.4 % | 12.7 % |
| Standard 2x、32ノート、6オペ | 11.6 % | 17.5 % |
| Standard 2x、8ノート×8 Unison（64 SubVoice）、6オペ | 18.9 % | 25.8 % |
| 最悪: Dual Morph、8×8 Unison、全9 FX、2x | 39.5 % | 53.9 % |
| 最悪: 同 4x（予算をエンジンあたり32に制限、D-019） | 56.1 % | 86.1 % |

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
- UI: Simple（Macros / Character / FX / Trance Gate / Arpeggiator / Variations / Morph）と Advanced（Operators / Algorithm / Filter & Mod / FX / Rhythm / Performance）。両モードは同一モデルを編集。プリセットブラウザはヘッダーのプリセット名から開く独立ページ。2026-11-23 に POLYBRISK とシリーズ共通の外観へ全面改訂（D-028）。

### Phase 3 — 音色拡張と基本FX
- 帯域制限Wavetable（triangle/saw/pulse）、TPT SVF（LP12/LP24/HP12/BP12＋Drive）、Unison（1〜8、detune、spread）、LFO×2（5波形、テンポ同期）、グローバルModEnv、Mod Matrix 8スロット。
- EQ / Drive（2x OS）/ Chorus / Delay / Reverb、FXクイックコントロール4種。

### Phase 4 — Preset / State
- `.fmlume` JSON（schemaVersion 1）、厳格な検証、schema 0 マイグレーション、`schemas/fmlume-preset.schema.json`、`docs/PRESET_SCHEMA.md`。
- DAW状態にCanonical Model全体＋ホスト値を保存。プリセットブラウザ（カテゴリ、タグ、フリーワード、お気に入り、Prev/Next、編集マーク、Save/上書き確認、Import/Export）。

### Phase 5 — Sound Explorer
- seed固定の8候補、方向8種、強度3段、Lock（graph / ratios / envelopes / FX / 個別マクロ）、安全性検証、重複抑制、ラウドネス補正、フォールバック、Preview/Cancel/Use/Explore From This/Regenerate/履歴20。

### Phase 6 — Morph・演奏機能・全FX
- A/Bスロット、Compatible/Dual 自動判定、等パワークロスフェード、`macrosB`/`matrixB`、Morphプリセット（A+B+m）。
- Arpeggiator、Chord Mode、Trance Gate（16ステップ、パターン/Randomise/Clear/Invert）。
- Phaser / Flanger / Compressor / Bitcrusher、FXの自由な並べ替え・個別バイパス・全バイパス。

### Phase 7 — 安定性・最適化・プリセット
- ファクトリープリセット**152音色**（11カテゴリ。Arp 30、Bell と Poly は13、他は各12。`fmlume-presetgen` で生成・検証）。
- 出力メーター（ヘッダーの OUT スライダー下）、実効最大ノート数表示（Performance タブ）、Dual×4x の負荷制御。CPU / Voice の数値表示は UI から削除（D-028。エンジン側の計測値は残存）。
- 5分ストレステスト、RT割当て検査、44.1〜192 kHz・ブロック32〜2048・0フレームの検証、x86_64 スライスのテスト実行。
- ツールチップとステータス行による操作ヘルプ、リサイズ（960×620〜1920×1200）。

## 3. ホスト公開パラメーター（不変）

`master.gainDb` `master.pan` `master.polyphony` `master.unison` `master.detune` `master.spread` `master.quality` `macro.1`〜`macro.8` `morph.amount` `morph.enabled` `fx.bypassAll` `fx.quick.warmth` `fx.quick.width` `fx.quick.space` `fx.quick.echo` `arp.enabled` `tranceGate.enabled`（計24）。プラグインID: manufacturer `Chbw` / plugin `Fmlu` / bundle `info.cheebow.fmlume`（**暫定**、DECISIONS D-001）。

## 4. 未検証・未実施（重要）

| 項目 | 状態 |
|---|---|
| **実DAWでの確認（HOST-01）** | **一部実施**。Cubase（VST3、開発用ビルド）: 読み込み・演奏・保存は利用者が確認し問題なし（2026-11-23）。このとき「マクロ等を動かしてもプリセット名に `*` が付かない」不具合が見つかり修正済み（原因は全形式共通のコード。`fmlume-uisnap --check-edited` で実プロセッサー＋エディタ上の表示を確認済み、Cubase での再確認は未実施）。Cubase のオートメーション記録再生・オフライン書き出し、および Logic Pro / REAPER / Ableton Live は**未実施**で、人手での確認が必要（手順は `docs/TEST_PLAN.md`）。自動検証は auval と pluginval のみ |
| **耳による音質確認（§17.4）** | **未実施**。152音色とFXはFM設計の知識に基づいて作成し、自動検査（発音・レベル・全マクロの可聴性・非有限値なし）のみ通過。音楽的な良し悪し、マクロの効き方の自然さ、Explorer候補の「意味のある違い」は試聴と調整が必要 |
| Intel Mac 実機 | 未検証（配布用の Universal ビルドと、x86_64 でビルドしたテストの Rosetta 上での実行のみ） |
| GitHub Actions | ワークフロー定義のみ。リモート未設定のため未実行 |
| 実機GUI操作 | 2026-11-23 のUI全面改訂後、エディタはオフスクリーン描画（`fmlume-uisnap`、全ページ・最小サイズ）と pluginval のGUIテストで確認。エンベロープのドラッグ編集、ゲートのステップ描画、アルゴリズム図のドラッグ接続、ブラウザの Delete / Import / Export は**人手で未確認**。マウス操作・キーボードTab移動・HiDPI以外の表示・ファイルダイアログは人手で未確認 |
| コード署名・公証・インストーラー | Phase 8。現在はアドホック署名で、他のMacへそのまま配布はできない |

## 5. 仕様との差異・既知の制約

- **Full Morph**（A/B別FXのクロスフェード）、**Poly Aftertouch / MPE**、**Per-Voice Filter**、**MIDI出力**、Shimmer Reverb 等は仕様どおり将来項目で未実装。UIには置いていない（Full Morph のみ「coming later」と文言表示）。
- Mod ソース（LFO/ModEnv/Velocity/Keytrack）はグローバル（D-004）。
- FXチェーンの再構成はクロスフェードではなく約10msのディップ（D-012）。
- ボイス終了判定はキャリアのみ（D-008）。パッチ受け渡しはFIFOではなく最新優先メールボックス（D-005）。
- Capture Morph は専用ボタンではなく、`Save` で A+B+amount を1つのMorphプリセットとして保存する形。
- マクロ編集UIで `min/max`（クランプ窓）は編集できない（モデルとファイルは対応、既定 0/1）。
- ユーザープリセットの削除は `File > Show user preset folder` からFinderで行う（UIに削除ボタンなし）。
- Quality 切替・Morph種別の変更時、発音中の音は短くフェードする。
- プリセット読込はマスター音量/パン/Quality/全FXバイパスを変更しない（D-010）。
- 操作マニュアルは未作成（ツールチップとステータス行のみ）。

## 6. 次のタスク候補

1. **HOST-01 の残り**: Cubase でオートメーション記録再生とオフライン書き出しを確認。次に Logic Pro（AU）と REAPER。
2. **試聴パス**: `fmlume-render` で152音色を書き出して聴き、`tools/PresetGen/Factory.cpp` の値とカテゴリテンプレートのマクロ量を調整→ `fmlume-presetgen` で再生成。
3. Explorer の方向別ヒューリスティックとカテゴリ別制約の調整（試聴ベース）。
4. プラグインID・会社名・Bundle ID の確定（公開前に1回だけ、D-001）。
5. Phase 8: 配布方針（AGPLv3 か JUCE 商用ライセンスか）の決定、署名・公証、インストーラー、マニュアル、商標調査。
