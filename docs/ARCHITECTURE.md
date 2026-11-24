# FMLUME アーキテクチャ

最終更新: 2026-11-23（Phase 0〜7 実装時点）

## ターゲット構成

```text
FMLUMECore (static lib, JUCE非依存・標準C++20のみ)
 ├─ Source/Core         Model / Params / PatchCompiler / Templates / PatchController / Json / MidiFile / WavFile
 ├─ Source/DSP          FM/SynthEngine, Filter/Svf, Modulation/Lfo, FX/*, Sequencer/*, Util/*
 ├─ Source/Exploration  SoundExplorer
 └─ Source/Presets      PresetSerializer / PresetManager
        ▲                    ▲                   ▲                 ▲
 FMLUME (juce_add_plugin)   FMLUMETests     fmlume-render     fmlume-presetgen / fmlume-bench
 AU + VST3                  (CTest)         (MIDI→WAV)
 Source/Plugin, Source/UI
```

Core をホストやGUIなしでテストできるよう、JUCE に依存させていない（DECISIONS D-002）。プラグインは Core をリンクし、JUCE は `juce_audio_utils` 以下のみ使用する。

## データモデル（`Source/Core/Model.h`）

| 型 | 役割 |
|---|---|
| `PatchData` | 音声処理に必要な全データ。**trivially copyable（ヒープなし）**。`EngineSide a, b`（音源＋マクロ8＋Matrix8）、`PerformanceState`、`FxState`、`OutputState`、プリセット保存用ホスト値 |
| `PresetMeta` | 名前・タグ・マクロ表示名など文字列。オーディオスレッドへは渡らない |
| `Preset` | `PresetMeta + PatchData` = Canonical Model。`.fmlume` とDAW状態の内容そのもの |
| `HostParams` | ホスト公開値（APVTSが真実）のスナップショット。毎ブロック `process()` に渡す |
| `CompiledPatch` | 検証済み `PatchData` ＋ トポロジカル順・入力エッジ表・構造ハッシュ |

**パラメーターコード表**（`Source/Core/Params.h`）: パッチパラメーターに整数コード（0〜212）を割り当て、範囲・スケール（lin/log）・フラグを1か所で定義。マクロ、Mod Matrix、Explorer、JSONのターゲットID、UIのノブがすべてこの表を共有する。

```text
0..83     opN × 14フィールド          120..128  filter / lfo rate / modEnv
84..119   edge(from,to) index         129..131  pitch / master.level / master.pan（仮想: Mod用オフセット）
132..139  matrix depth                140..211  fx(type, param)        212  performance.portamento
```

## スレッドとデータの流れ

```text
メッセージスレッド                                          オーディオスレッド
────────────────                                           ─────────────────
UI ──edit()──▶ PatchController (Canonical Model, Undo)
                 │ validatePatch → 不正なら拒否（音は変わらない）
                 ▼
              SynthEngine::setPatch()
                 │ compilePatch（DAG検証・トポロジカルソート）
                 ▼
              メールボックス（4スロット, atomic {slot, generation}） ──▶ process() 冒頭で取得・PODコピー
                                                                         │ 構造同一 & 非強制 → 現行スロットをインプレース更新
                                                                         └ それ以外 → 次のリングスロットへ（新世代）
APVTS（ホスト公開24パラメーター） ──atomic読み出し──────────▶ HostParams（毎ブロック）
UI鍵盤 / 試聴 ──juce::AbstractFifo（ロックフリー）────────▶ MidiEvent
状態保存/復元 ── modelMutex（非RT同士のみ）── suspendProcessing で一括反映
```

- オーディオスレッドは割当て・mutex・I/O・JSON・ログを行わない。テスト `Real-time safety` と `Five-minute stress` が `operator new` フックで検証している。
- `modelMutex` はメッセージスレッドと状態保存スレッドの間だけで使い、オーディオスレッドは触れない。
- 真実の所在: ホスト公開値 = APVTS、それ以外 = Canonical Model（`PatchController`）。プリセット読込時は `applyHostValues` でAPVTSへ書き戻す（対象は DECISIONS D-010）。

## オーディオ処理（`SynthEngine::process`）

```text
MIDI + UIノート ─▶ Performer（Chord → Arp → サンプルオフセット付きスケジューラ）
                       │
                 イベント境界 と 32サンプルのコントロールティック でチャンク分割
                       │
   コントロールティック（絶対サンプル位置に整列 → 出力はブロックサイズ非依存）
     ├ ホスト値スムージング（マクロ等 15ms）
     ├ LFO×2 / ModEnv 更新
     ├ 有効パラメーター = 基準値(正規化) + Σマクロデルタ + Σ Matrixデルタ → 物理単位 → clamp
     └ ランプ目標（level / carrier / feedback / edge index / 正規化）, フィルター, FX パラメーター
                       │
   ボイス（最大32ノート × Unison 8, 予算64 SubVoice）
     └ オペレーターをDAG順に、チャンク単位でバッファ計算（内部レート 1x/2x/4x）
          mod = Σ index·out[from] + feedback·(前2サンプル平均)
          out = env·level·wave(phase + mod)   ※ sine=4096点テーブル, 他=帯域制限Wavetable 11段
                       │
              ハーフバンドFIRデシメーション ─▶ DCカット(5 Hz) ─▶ グローバルフィルター(TPT SVF + Drive)
                       │   プリセットの level / pan
                 Trance Gate（ホストppq同期, FXの前）
                       │
                 FXチェーン（9種常駐・順序配列のみ変更, 固定レイテンシ7）
                       │
                 Master Gain / Pan → ソフトリミッター → 出力, メーター
```

### パッチ世代

- 10スロットのリング。新世代は「新ノートのみ新パッチ」、旧ノートは旧スロットを参照したままリリースまで鳴る。旧スロットのランプは凍結。
- リングが一周して使用中のスロットに当たると、そのボイスを10msでフェードし、空くまで適用を次ブロックへ延期。

### ボイス管理

- 予算超過時は「最古のRelease中 → 最古の発音中」の順にスチール。スチールされたボイスは5msフェードし、新ノートはそのボイスに予約されてフェード完了時に発音。
- モノ: last-note priority のノートスタック、Legato、Portamento。Sustain（CC64）、Pitch Bend（±1〜24）、Mod Wheel、Channel Aftertouch。

### レイテンシ

| Quality | エンジン | FXチェーン | 報告値 |
|---|---|---|---|
| Eco (1x) | 0 | 7 | 7 |
| Standard (2x) | 11 | 7 | 18 |
| High (4x) | 13 | 7 | 20 |

## 非リアルタイム側の主要クラス

| クラス | 役割 |
|---|---|
| `PatchController` | 編集の唯一の入口。編集→検証→Undo積み→エンジン送信。Explorer（生成/試聴/採用/履歴20）、Undo/Redo（64段、ノブドラッグは1段に集約） |
| `SoundExplorer` | PCG32による決定論的な8候補生成。Lock、方向バイアス、安全性チェック（オフラインレンダーで無音・過大・非有限を排除）、重複抑制、ラウドネス補正、フォールバック |
| `PresetSerializer` | `.fmlume` JSON の読み書き、厳格な検証、schema 0 / 1 → 2 マイグレーション、プロジェクト状態 |
| `PresetManager` | ファクトリー（埋め込み）＋ユーザーフォルダの走査、検索、タグ、お気に入り、保存/Import/Export |
| `FmlumeAudioProcessor` | APVTS、MIDI/トランスポート変換、状態保存、レイテンシ報告、CPUメーター |
| `FmlumeAudioProcessorEditor` | Header / SimplePage / AdvancedPage（5タブ）/ 画面鍵盤。30Hzタイマーでモデルのバージョン番号を監視して再描画 |

## UI 構成（`Source/UI`）

```text
Header        ロゴ, プリセット名/カテゴリ/編集マーク, Prev/Next, Fav, Save, File, Undo/Redo, Simple/Advanced, CPU/Voices, 出力メーター
SimplePage    BrowserPanel(Choose) | MacroPanel(Shape) / ExplorerPanel(Explore) + QuickFxPanel(Finish)
AdvancedPage  Operators(GraphView + 選択オペレーター, Expand で結線図を全面) | Modulation(LFO, ModEnv, Matrix, Macro Editor) | FX | Performance
```

- `ModelKnob` はパラメーターコードに、`HostKnob` / `HostToggle` はAPVTSにバインド。`BoundSlider / BoundCombo / BoundToggle` は任意フィールドに getter/setter でバインド。
- Simple と Advanced は同じ `PatchController` を編集するので常に同じ状態を表示する。
- 未実装機能のボタンは置かない。
