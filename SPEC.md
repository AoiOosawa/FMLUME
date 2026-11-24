# FMLUME — 開発仕様書（SPEC.md）

> **製品名:** FMLUME（エフエムリューム）  
> **キャッチコピー:** The Power of FM. Without the Complexity.  
> **文書バージョン:** 1.1 / 2026-11-23（改訂内容は末尾の「21. 改訂履歴」と `docs/DECISIONS.md` を参照）  
> **実装対象:** macOS AU（AUv2）/ VST3 ソフトウェア音源、および Standalone アプリ  
> **実装言語:** C++20 / JUCE 9.0.6 / CMake  
> **読者:** Claude Code および音声DSP・UI開発者  
> **状態:** ユーザーが合意した製品方針に基づく実装基準仕様。未指定の詳細は本書の決定値を初期値とする。

---

## 0. Claude Code への実装指示（最初に読む）

1. 本ファイル全体と `CLAUDE.md` を読んでから作業する。
2. **段階的に実装する。** `Phase 0 → 1 → … → 8` の順で、各フェーズの受入条件を満たしてから進む。各フェーズはビルド可能な状態で完了させる。
3. `README.md`、`docs/ARCHITECTURE.md`、`docs/IMPLEMENTATION_STATUS.md` を実装開始時に生成し、実装済み/未実装/制約と実行コマンドを更新する。未実装機能を「実装済み」と表示しない。
4. 仕様中の **MUST（必須）** は省略しない。**SHOULD（推奨）** は障害時に代案を記録してよい。将来案は初期版の完了条件に含めない。
5. DSPコードとUIコードを分離し、DSPはホストやGUIなしでテスト可能にする。DSPは原則 `float`、位相蓄積など精度が必要な部分に `double` を用いる。
6. オーディオスレッドではメモリ確保、mutex/待機、ログ出力、ファイルI/O、JSON処理、スレッド生成を行わない。グラフ/プリセット/FX変更は非リアルタイム側で準備した不変スナップショットを安全な境界で適用する。
7. `JUCE/LICENSE.md` を**全文確認**し、AGPLv3または適切なJUCE商用ライセンスの適用条件を利用者に明示する。配布前の商標・第三者ライブラリ・プリセットの権利確認は別途必要。
8. 実装上の重大な未確定事項は `docs/DECISIONS.md` に日付、案、採用案、理由、互換性影響を記録する。製品の基本コンセプトは変更しない。
9. コード生成だけで終えず、**ビルド・自動テスト・実機での起動と基本演奏**を受入条件として扱う。環境上テストできない項目は明記する。

## 1. 製品コンセプトと成功条件

### 1.1 コンセプト

FMLUME は、FM/PM合成の音色表現力を維持しながら、オペレーターやアルゴリズムを知らないユーザーが直感的に音を作れるシンセ。**プリセットの軽微な改変しかできない製品ではなく、Simpleモードだけで初期音色から新規音色を作れる製品**とする。

主要フロー：**Choose（カテゴリ/初期音色）→ Shape（マクロ）→ Explore（派生候補）→ Finish（エフェクト）**。

### 1.2 対象ユーザー・音色

- 初心者: 音色カテゴリ、4つの共通マクロだけで成立する操作。
- 中級者: カテゴリ別4マクロ、Explorer、FX編集、プリセット保存。
- 上級者: 6オペレーターのルーティング、エンベロープ、Mod Matrix、各FX詳細。
- カバーする音色: Arp / E.Piano / Bell / Bass / Lead / Pad / Poly / Pluck / Brass / Keys / SFX（v1.1 で Arp＝アルペジエーターが動く状態で読み込まれる音色、Poly＝ポリシンセ系を追加）。アナログ風の質感もFM本体、波形、フィルター、ユニゾン、FXで実現する。サンプル音源は使用しない。

### 1.3 リリース定義

| 段階 | 範囲 | 完了条件 |
|---|---|---|
| **MVP 0.1** | FM発音、ADSR、単純な接続テンプレート、MIDI、Simple 4マクロ、最小UI、AU/VST3 | ホストで音が鳴り、保存/再読込後も同一音になる |
| **Alpha 0.5** | 自由接続FM、波形/フィルター、8マクロ、Mod Matrix、基本FX、プリセット | Advancedからゼロ作成可能、各種テスト通過 |
| **Beta 0.9** | Explorer、Arp/Chord/Trance Gate、自由FX順 | Cubaseを含むDAWでプロジェクト再読込、オフラインバウンスが成立 |
| **v1.0** | 品質・最適化、全カテゴリの工場プリセット、UI調整、検証、ドキュメント | §17の受入条件を満たす |

**初期リリースでは不要:** Windows/Linux、iOS/AUv3、AAX、CLAP、サンプラー、音声生成AI、クラウド、ログイン、オンライン課金、DX7 SysEx完全互換、Sound Morph（§9、v1.2 で削除）、マルチティンバー。

## 2. 対応環境とビルド

| 項目 | 仕様 |
|---|---|
| OS | macOS 12以上（v1.1）。配布用は Universal 2（Apple Silicon + Intel）、開発用ビルドは Apple Silicon のみ |
| 開発言語 | C++20 |
| JUCE | **9.0.6** を Git tag で固定。更新時は明示的に変更・回帰テスト |
| ビルド | CMake 3.25+ / Xcode + Command Line Tools |
| プラグイン | AUv2 MusicDevice（インストゥルメント）、VST3 instrument |
| 出力バス | Stereo 2ch、初期状態では外部音声入力なし |
| サンプリング周波数 | 44.1 / 48 / 88.2 / 96 / 192 kHz をサポート |
| バッファ | 32〜2048 frames およびホストからの可変ブロック長、0フレームも安全処理 |
| UI | JUCE Componentによるネイティブ描画。再スケール可能 |
| DAWでの最初の検証 | Cubase。その他 Ableton Live、Logic Pro、REAPER などでスポットチェック |
| フレームワーク取得 | Git submodule `external/JUCE`、固定タグをリポジトリで記録 |
| 単体テスト | CTest + JUCE UnitTest または Catch2（追加依存はライセンス確認） |

実行例（JUCE submoduleが設定済みの想定）:

```bash
cmake -S . -B build -G Xcode -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

CMakeのJUCE API（`juce_add_plugin`）を用いて `FORMATS AU VST3`。製品ID、Bundle ID、manufacturer code、plugin code は初期実装時に一意な固定値を決めて `docs/DECISIONS.md` に記録。公開後は変更しない。AU/VST3のプラグインIDとホスト公開パラメーターIDは不変条件。

## 3. 全体アーキテクチャ

```text
MIDI + Host Transport + Host Automation
          │
   Event / Parameter Dispatcher
          │
   Arp / Chord Scheduler
          │
   Voice Allocator (notes / sustain / steal)
          │
   ┌──────┴─────────────────────────────────┐
   │   6-op FM/PM engine                   │
   └──────┬─────────────────────────────────┘
          │
     Unison / Detune / Stereo Mix
          │
   Per-voice / Global Filter
          │
    Trance Gate (pre-FX)
          │
   Reorderable FX Chain (non-allocating)
          │
      Output Gain / Meter / Limiter
          │
       Stereo Output
```

- `EngineState`: 非リアルタイム側の編集可能な完全な音色データ（演奏機能/FX含む）。
- `CompiledPatch`: 音声処理用の不変データ。接続を検証してトポロジカル順を格納し、正規化/係数を算出済み。
- `RuntimeVoice`: 発音中の位相・EG・フィルタ状態等。固定容量プールから利用。
- `PresetManager`: JSON検証、読込・保存、ファクトリー・ユーザープリセット検索。
- `AudioProcessor`: ホストとオーディオエンジンを仲介。UIからDSP内部構造へ直接アクセスしない。
- `UI`: コマンド経由で編集。ホスト公開パラメーターはJUCE `AudioProcessorValueTreeState`（APVTS）を通す。

### 3.1 スレッド・スナップショット規則

- パッチ編集、Explorer候補作成、JSON処理、接続グラフの検証・コンパイルはメッセージ/ワーカースレッドのみ。
- 完成したパッチを**固定長のSPSCコマンドキュー**でオーディオスレッドに通知する。キューにはヒープを共有する裸ポインタではなく、事前確保したパッチスロットIDと世代番号を渡す。破棄処理も非RT側で行う。
- パッチ適用はブロック境界。既存発音中のノートは旧パッチを使ってリリースまで鳴らすか、10〜30msクロスフェードする。原則は**新しいノートに新パッチ、既存ノートに旧パッチ**、ただし旧パッチの存続上限は8世代。上限超過時は旧音を10msでフェードアウト。
- ホスト自動化のマクロ/ゲイン等はサンプル近傍で反映可能なスムージング状態を持つ。完全サンプル精度が必要なMIDI note-on/off等はMIDIメッセージのサンプルオフセット順に処理。
- ホスト状態読込 `setStateInformation` はUIとの競合を避ける。読み込んだ完全状態を検証後、一回のトランザクションとして反映し、中途半端な状態を音声処理に見せない。

## 4. 6オペレーター FM/PM 音源

### 4.1 合成方式

サンプルごとに、各オペレーター `i ∈ [0,5]` の出力を計算する。内部の命名は `phaseModulation` を採用（古典的なDX系FMと同様の実用的なPM方式）。

```text
phase_i[n+1] = wrap(phase_i[n] + 2π * frequency_i[n] / internalSampleRate)
mod_i[n] = Σ_{j→i} (index[j][i] * output_j[n]) + feedback_i * previousOutput_i[n-1]
output_i[n] = envelope_i[n] * level_i * waveform_i(phase_i[n] + mod_i[n])
voice[n] = (Σ_i carrierGain_i * output_i[n]) * normalization
```

- `phase` はdouble radian内部値、`output` は正規化した `[-1,1]` 前後のfloat。
- `index[j][i]` は**ラジアン単位の位相変調量**。各エッジ範囲 `[0,16] rad`、スムージング必須。
- `feedback_i` は自己フィードバック専用で `[0,1.5] rad`。**必ず前サンプル値**を参照。`previousOutput` をオペレーターごとに保持し、ナイキスト付近の発振増幅に対してソフトクリップ・振幅上限で防御。
- `carrierGain_i` は `[0,1]`、最低1個は `>0` にする。各オペレーターはキャリアとモジュレーターを兼ねてもよい。
- **演算グラフはDAG（有向非巡回グラフ）**。異なるオペレーター間の循環接続は禁止、自己フィードバックのみ可。UIは循環する接続を作れず、JSON読込ではエラーとして拒否。
- DAGをトポロジカルソートして変調元→先の順で計算する。エッジ数最大30（有向・自己以外の6×5）。重複エッジ禁止。
- 0出力や無効なキャリア/オペレーター指定は読み込めない。異常値NaN/Infはプリセット読込時に拒否し、DSP内でも非有限値をゼロ復帰。
- 同じ初期状態・同じイベント列・同じサンプルレートで、オフラインレンダー結果は決定論的。

### 4.2 オペレーターのパラメーター

| フィールド | 範囲/既定 | 備考 |
|---|---|---|
| `enabled` | bool / true | オフならその出力は0 |
| `waveform` | `sine`, `triangle`, `saw`, `pulse` / `sine` | 音色生成波形。非サイン波は帯域制限Wavetable等を使う |
| `pitchMode` | `ratio`, `fixed` / `ratio` | 比率指定/固定Hz |
| `ratio` | 0.125〜32 / 1.0 | 連続値。整数比へのスナップもUIで提供 |
| `fixedHz` | 0.1〜16000 / 440 | キーに非追従時の固定周波数 |
| `detuneCents` | -100〜100 / 0 | 変調とキャリア両方へ適用 |
| `level` | 0〜1 / 1 | オペレーター振幅 |
| `velocityToLevel` | 0〜1 / 0.5 | Velocity変化で振幅調整 |
| `velocityToIndex` | 0〜1 / 0.3 | そのオペレーターから出るエッジindexの強さに寄与 |
| `keyTracking` | 0〜1 / 1 | ratio/fixedのピッチ追従。fixedでは初期0 |
| `env` | A,D,S,R | §4.3 |
| `phaseOffset` | 0〜1 / 0 | 0〜2π位相。Key Syncモードでのみ新ノート時に反映 |
| `keySync` | on/off / on | offでは音色エンジン共通のフリーラン位相を参照 |
| `feedback` | 0〜1.5 / 0 | 自己フィードバック |
| `carrierGain` | 0〜1 / 0 | 最低1つ有効が必要 |

**周波数:** `noteHz = 440 × 2^((midiNote-69+pitchBendSemitone)/12)`。`ratio`では `noteHz × ratio × 2^(detuneCents/1200)`。`fixed`では `fixedHz × 2^(detuneCents/1200)` にkeyTrackingによるノート追従分を乗じる。固定値・比率の切替はクリック時に即時ジャンプさせず20msで滑らかに遷移。

### 4.3 Envelope

- 各オペレーターに **ADSR**（Attack 0.001〜20s, Decay 0.001〜30s, Sustain 0〜1, Release 0.001〜40s）。
- 線形時間係数ではなく、指数カーブの可変係数を使用。初期版はカーブ固定で十分。Envelopeの時間はテンポ非同期。
- note-off時は必ず現在値からRelease、Sustain Pedal CC64中はnote-offを保留。
- Voice終了は全キャリアと有効モジュレーターのEnvelopeが十分小さくなった時、またはR上限超過時。未使用モジュレーターをVoice継続判定に含めない。
- 初期音色: Operator 1 = Sine/Ratio1/Carrier1/ADSR(A=0.01,D=0.5,S=0.6,R=0.3)、2〜6はoff、エッジなし。

### 4.4 アルゴリズムテンプレートとAdvanced接続UI

- 16個以上（実装は18個）の初期接続テンプレートを提供。Advanced > Operators の結線図（`Templates...` または `Expand` でサムネイル一覧）と DX 風の図（キャリア下段・OUT バー・モジュレーター上段）で表示し、現在の音がどのテンプレートに当たるかを示す（聞こえないオペレーターと番号の違いは無視して判定）。例: 1→2（2がキャリア）、1→2→3（3がキャリア）、独立2キャリア、パラレル3キャリア、1→(2,3)、(1,2)→3、モジュレーター共有、自己フィードバックを含む構成。
- ユーザーはAdvancedでノード（6オペレーター）と矢印をドラッグして作成、接続量/キャリアゲイン/自己フィードバックを個別編集できる。
- 切断・再接続はクリックでプレビューが反映されるが、コンパイルしたグラフの安全な交換は§3.1に従う。作成途中の不正接続は音声へ反映しない。
- DX7パッチ完全互換、従来の32アルゴリズムの完全再現はv1では保証しない。

### 4.5 波形とエイリアシング

- FM/PMエンジン内部は**デフォルト2倍オーバーサンプリング**、品質設定 `Eco=1x / Standard=2x / High=4x`。1xも機能するが高変調でエイリアス増大を許容。
- `saw/pulse` は多段Bandlimited Wavetableまたは同等の手法を採用、生成テーブルは初期化時に作成。`triangle`も帯域制限した実装。
- FM側の追加高調波は依然エイリアス源なので、2x/4xで必ず聴感・スペクトル検証を行う。「完全な無エイリアス」は要求しない。
- 内部SR変換による遅延は正確にホストへ報告、ステレオ両chは同一遅延。品質設定変更は非RT側で新しいDSP状態を準備し、安全な切替にする。ブロックごとの再確保は禁止。

## 5. Voice / Unison / Filter

### 5.1 PolyphonyとVoice管理

- `polyphony`: 1〜32ノート、デフォルト16ノート。
- `unisonCount`: 1〜8、デフォルト1。1ノートにつきその数の `SubVoice` を使用する。
- **コスト上限**: 同時稼働FM `SubVoice` は最大64。音数×Unisonが予算超過した場合、最古のRelease中→最古の発音中の順でVoice Stealする。最終出力は5ms以内にクロスフェードしてクリック抑制。
- 32×8の組合せを無制限保証しない。UIに実効最大同時ノート数を表示。
- MIDI Note 0〜127、Velocity 1〜127、NoteOn velocity0はNoteOff、all notes off / all sound off対応。
- Pitch Bend初期±2 semitones、設定±1〜24 semitones。CC1 Mod Wheel、CC64 Sustain、Channel Aftertouch対応。Poly AT/MPEは将来。
- モノモード: last-note priority、Legato on/off、Portamento 0〜2s。PolyはPortamento初期off。

### 5.2 Unison / ステレオ

- `detuneCents`: 0〜50、`stereoSpread`: 0〜100%。
- Unisonの左右配置は対称、各SubVoiceへ均等パワーのゲイン正規化。再トリガー時のデチューン・位相は固定seedで決定論的。
- mono互換を考慮し、コーラス/ステレオ位相差を強制しない。

### 5.3 フィルター

- 1段の**音色共通フィルター**（各SubVoice混合後、パート単位）を初期実装。モノ/ポリ両方で有効。
- タイプ `off`, `LP12`, `LP24`, `HP12`, `BP12`。Cutoff 20Hz〜Nyquist未満、Resonance 0〜0.95、Drive 0〜1。
- 音量/音色を破綻させない安定型TPT SVF相当のアルゴリズム。フィルター/Driveは後述Mod Matrixの対象。
- **フィルターは発音全体（グローバル）に作用**するので、Filter Envは「各ノート独立」とはしない。初期版はグローバルMod Envのみ。将来のPer-Voice Filterは拡張項目として管理。

## 6. Simple / Advanced モードと8マクロ

### 6.1 Simpleの見せ方

- Simpleは**常時4つの共通ノブ** `Brightness / Attack / Release / Character`（MACROS）を表示。
- カテゴリー別の4ノブ（Macro5〜8）は隣の CHARACTER パネルに常時表示する（v1.1。`More` による展開は廃止）。合計8つ。
- ユーザーは各マクロの**表示名・割り当て先・深さ・範囲（min/max）**を変更可能。編集パネルはAdvancedで開く。共通4ノブもプリセットごとに再割当て可能だが、工場プリセットでは意味の一貫性を保つ。
- ホストのオートメーション名は**固定 `Macro 1` … `Macro 8`**。UIラベルはプリセットに従って変える。DAWのパラメーターIDは `macro.1` … `macro.8` で固定し、名称をプリセットごとに変更してホスト互換性を損なわない。
- 0〜100%表示、ノブ初期値50%。値50%で編集前のベース値（Neutral）。プリセット内で個別初期値を保持してよい。

### 6.2 マクロ・マッピング仕様

- マクロはそれぞれ最大8マッピングを持つ。`targetParamId`, `min`, `max`, `amount`, `curve` (`linear`,`exp`,`sCurve`), `polarity` (`bipolar`,`unipolar`) を記録。
- **計算順序:** `baseParameter` → **各Macroの正規化デルタを加算** → MIDI/Mod Matrixのデルタ → 物理単位へ変換 → 安全範囲にclamp → 平滑化（原則5〜30ms）。
- 原則として物理単位（秒/Hz/倍率）ではなく**正規化ドメイン**で合成。`frequency` / `attack`などはlogスケールへ変換してから復元。
- ターゲットに複数マクロが割り当てられた場合は正規化デルタを合算してclamp。プリセット内に循環した「マクロ→マクロ」は設定できない。
- Discreteパラメーター（waveform、graph、FX type）を連続マクロで直接操作することは禁止。音が跳ぶ設定はExplorerの別候補として扱う。
- グラフ切替やエンジン再構築を要する操作はマクロターゲットにしない。
- 1マクロが複数のFM edge index、carrier level、Filter cutoff、Envelope、FX wetを動かせる。

### 6.3 カテゴリ別既定ノブ

| Category | Macro 5 | Macro 6 | Macro 7 | Macro 8 |
|---|---|---|---|---|
| Arp（v1.1。Pluck と同じ） | Snap | Body | Damping | Space |
| E.Piano | Tine | Body | Bell | Tremolo |
| Bell | Metallic | Shimmer | Decay | Motion |
| Bass | Growl | Punch | Weight | Drive |
| Lead | Bite | Glide | Edge | Width |
| Pad | Softness | Width | Movement | Air |
| Poly（v1.1） | Sweep | Detune | Sub | Motion |
| Pluck | Snap | Body | Damping | Space |
| Brass | Bite | Body | Attack Shape | Ensemble |
| Keys | Digital | Body | Ring | Width |
| SFX | Chaos | Pitch Sweep | Texture | Tail |

- `Brightness` の典型的動作: 有効mod index＋、Filter cutoff＋、EQ high shelf微増。ただし「Bright=音が必ず大きくなる」にならないよう出力レベル補正。
- `Attack`: Envelope A時間を相対的に増減。`Release`: R時間を相対的に増減。
- `Character`: テンプレート固有の特徴（FM比率は原則固定、index/feedback/補助wave blend等）を変える。
- 例: E.Pianoの `Tine` は短いAttackを維持したまま高次モジュレーターのindexと減衰を連動。Bellの `Metallic` は非整数ratioの組合せプリセット内でindexを変える。**ratioを連続的に大きく変えるマクロはデフォルトにしない**（音程感の変化を回避）。

### 6.4 Advanced

タブ: `Operators`（自由接続グラフと、6オペレーターの各ADSR/波形/比率）/ `Modulation`（LFO/Matrix）/ `FX`（チェーン）/ `Performance`（演奏機能）。Simple⇄Advancedを切替えても**同じパッチ状態**を編集する。Simple編集結果はAdvancedにも反映、Advanced編集後もSimpleマクロが有効であること。

## 7. モジュレーション

- LFO ×2: shape `sine/triangle/saw/square/randomSmooth`、速度 `0.02〜20Hz` と `1/1〜1/64`テンポ同期（付点/3連を含む）、phase、retrig。
- `ModEnv`: ADSR ×1（§4.3と同じ数値範囲）。
- Matrix 8スロット、各 `source`, `target`, `depth(-1..1)`, `curve`, `enabled`。
- ソース: LFO1, LFO2, ModEnv, Velocity, Keytrack, ModWheel, Aftertouch, Macro1〜8。
- ターゲット: `opN.level`, `opN.detune`, `edge.J.I.index`（**既存エッジのみ**）, `filter.cutoff`, `filter.resonance`, `master.pan`, `master.level`, `fx.<instance>.wet`、`pitch`。
- **Mod Matrixで未接続エッジを新規生成しない**。未定義ターゲットのスロットはプリセット読み込み時に無効化して警告を出す。
- ピッチ・急峻なindex変調など音響上必要な変調を除き、制御信号は最大32サンプルごとに計算し線形補間。Zipper noiseが確認できる場合は対象パラメーターだけオーディオレートへ昇格。
- ループ（Mod MatrixでMacroを変調する、LFOが自分のrateに帰還する等）は不可。

## 8. Sound Explorer

### 8.1 ユーザー体験

- UI 上の名称は **Variations**（v1.1）。`Generate` で**現在のパッチを親にした8候補**を生成し、タイル（A〜H）に名前と音色傾向を表示する。タイルをクリックすると試聴（テスト音を自動で鳴らす）、`Keep this sound` で採用、`Original` で元の音に戻る。
- 変化方向: `Surprise / Brighter / Darker / Warmer / More Metallic / Softer / Punchier / More Motion`。
- 強度: `Subtle / Medium / Wild`。**初期はMedium、Surprise**。
- `Lock` 対象: operator graph、pitch ratios、envelopes、FX、個別マクロ（最大8）を独立に固定できる。
- `New set`（Regenerate）: 親を維持して別8候補。`More like this`（Explore From This）: 選択候補を新しい親へ。`Previous set`（旧表記 `Back`）: 候補の履歴を最大20ステップ戻る。
- `Preview` は採用前の一時試聴。プリセット保存をしない限りユーザーパッチを上書きしない。

### 8.2 生成アルゴリズム v1（機械学習不要）

1. 親パッチをdeep copyし、カテゴリ/Explorer強度/方向/Lockから許可対象のリストを作る。
2. PRNGは `PCG32`など**seed固定で再現可能**な実装を使用。全候補に `seed` と親パッチchecksumを記録。
3. FM index、operator level、Envelope、Filter、Modulation、FX量等を正規化ドメインで変動。変化量分布: Subtle `σ=0.08`、Medium `σ=0.20`、Wild `σ=0.35`（0〜1空間の概略上限、対象に応じ縮小）。カテゴリごとの上限・下限と安全制約を設ける。
4. **グラフとratioは初期状態で固定**。`Wild` かつロックなしのみ、同カテゴリの検証済みアルゴリズムテンプレートへ差し替え可。無秩序な浮動小数ratio生成を行わない。
5. ルーティングDAG、最低1キャリア、極端な周波数、NaN/Inf、全無音、過大出力、ロック違反を検証。NGは再試行（最大50回）→足りない場合は親から安全な小変化で補完。
6. 候補間の重複抑制: 主要パラメーターの正規化距離が小さすぎる候補を除外。同じ音に聞こえる8枚を避ける。
7. 極端なLevel変化を出力正規化で補い、**音量だけで良い音に見える偏り**を減らす。

方向スコアの実装例（*ヒューリスティック*）: Brightnessを高周波op寄与、filter cutoff、mod indexの重み付き和として近似。Metallicは非整数ratioを含むmodulatorの寄与、PunchはAttack 5〜50ms間のエネルギー傾斜、MotionはLFO depth/Rate。実際の音色を必ず保証する分類器ではない旨をUIヘルプへ明記。

### 8.3 試聴・安全性

- 候補生成は音声スレッド外。候補の音声処理用パッチは事前コンパイル。
- `Preview` 操作中の演奏は候補音で出す（録音中でも選択候補が音になることは明示）。`Cancel`で編集前の音に戻す。切替は新規ノート適用と既存Voiceの扱いを§3.1で統一。
- Explorer固有のサンプルを外部へ送信しない。完全ローカル処理。

## 9. Sound Morphing（v1.2 で削除）

当初は A / B の 2 音色を `m∈[0,1]` で行き来する機能（構造が一致すればパラメーターを補間する Compatible Morph、それ以外は 2 音色を同時に鳴らしてクロスフェードする Dual Morph）を定義し、実装していた。最初のリリース前に削除した（`docs/DECISIONS.md` D-055）。

- 理由: ファクトリー 152 音色の組み合わせのうち、パラメーター補間（音色そのものが変わる Morph）になるのは約 6% で、残りは 2 音色のミックスだった。構造が違う音色どうしも補間できるようにするにはエンジン中核の作り直しが必要で、Dual Morph は CPU 負荷を 2 倍にし、ボイス管理とプリセット形式を複雑にしていた。音色を連続的に変える役割はマクロ 8 本、派生を探す役割は Sound Explorer が担う。
- 影響: ホストパラメーター `morph.amount` / `morph.enabled` を廃止（計 22）。プリセット形式は schemaVersion 2（単一の `engine`）。schemaVersion 1 のファイルと旧プロジェクト状態は、2 音目を捨てて読み込む（§12）。
- 章番号は他の章からの参照を保つために残す。

## 10. エフェクト

### 10.1 基本方針

- エフェクト込みでプリセットが完成する。ただしすべてOFFにしても魅力的なFM音色となる設計。
- FXは**1インスタンスずつ最大9種類**、同種類の重複はv1では不可。全種類を常時インスタンス化し、**処理順序配列のみを変更**して自由なチェーンを実現（音声スレッドでnewしない）。オフを含めた順序変更、個別Bypass、全Bypass。
- **原則post-synth / post-Trance Gateで、Master Gain前**。FXのReverb/Delay tailはノートoff後も維持。自動化はスムージング。

| FX | v1パラメーター | 初期実装時期 |
|---|---|---|
| EQ | LowShelf/MidPeaking/HighShelf gain ±18dB、周波数、Q | Phase 3 |
| Drive | gain、tone、wet、output trim、2x OS | Phase 3 |
| Chorus/Ensemble | rate、depth、width、mix、voices | Phase 3 |
| Delay | ms/tempo sync、feedback（0〜0.9）、stereo、wet | Phase 3 |
| Reverb | size、decay（0.2〜20s）、pre-delay、damping、wet | Phase 3 |
| Phaser | rate、depth、feedback、wet | Phase 6 |
| Flanger | rate、depth、feedback、wet | Phase 6 |
| Compressor | threshold、ratio、attack、release、makeup、wet | Phase 6 |
| Bitcrusher | bits（4〜16）、downsample、wet | Phase 6 |

- FXの初期順序: EQ → Drive → Chorus → Phaser → Flanger → Compressor → Bitcrusher → Delay → Reverb。
- UIには個別エフェクトのOn/Off、ドラッグ順序変更、各詳細ノブ、FX全BYPASSを設置。
- SimpleのFX操作: `Warmth`（Drive/EQ）、`Width`（Chorus）、`Space`（Reverb）、`Echo`（Delay）の4つを**FXパネルで提供**。これらは音色8マクロとは別のFX Quick Controlsで、プリセット内部に対応を保存する。
- フィードバックが不安定になる設定を防止し、ディレイ/リバーブの最大バッファは`prepareToPlay`または事前準備時に確保。
- FX順序を変えたときの残響が瞬間的に切れないよう、可能なら各FXの内部状態を維持。切替時の小さな音量段差は10msのクロスフェードで抑える。
- オーバーサンプリング使用FXは明示的に内部遅延を報告。全FXの遅延補正をテストする。
- Shimmer Reverb / Digital Character / Stereo Motion は**v1.1以降の追加候補**。

## 11. 演奏機能

### 11.1 Arpeggiator

- on/off、`Up/Down/UpDown/Random/AsPlayed`、Rate (`1/4,1/8,1/16,1/32`、dot/triplet)、Gate 5〜100%、Octave 1〜4、Swing 0〜60%。
- **Octave Order**（v1.1）: `range`（ノート順が全音域を通る。従来の動作）/ `up` / `down` / `upDown`（ノート順を1オクターブ内で演奏し、形ごとオクターブを移動する。例: Up-Down × up で C E G E → C' E' G' E'）。
- ホストBPM、拍位置と同期。MIDIノートを保持・並べ替え、音声スレッド内の固定容量イベントスケジューラで**サンプルオフセット付き**NoteOn/Offを生成。
- ホスト情報欠落時は120BPMで内部フリーラン。Transport Stopでシーケンサ状態を停止/リセットする設定（初期はReset）。

### 11.2 Chord Mode

- 入力1ノート→コードを構成。`Off/Major/Minor/Sus2/Sus4/Dim/Aug/7/m7/maj7` を初期実装。Voicing `Root/1st/2nd`（適用可能なコードのみ）、Octave -2〜+2、Strum 0〜100ms。
- NoteOffはその入力NoteOnが生成した全音に送る。鍵盤同時押し、Sustain、All Notes OffでもNoteの参照カウントを壊さない。
- MIDI出力（DAWへノート送信）はv1対象外。発音を内部処理する。

### 11.3 Trance Gate

- 16ステップ・各ステップLevel 0〜1（初期1）、Rate `1/4〜1/64`（v1.1 で 1/64 を追加）と拍子同期、Swing、Smooth 0〜30ms、Mix 0〜1。
- **ステレオ**（v1.1）: `stereo` が ON のとき左右を別パターン（`steps` = 左、`stepsRight` = 右）で刻む。`width` 0〜1 は左右の分離（下げると左右とも「どちらかが開いていれば開く」に寄る）。
- 適用位置は**FM + Unison + Filterの後、空間系エフェクト前**。よってGateを閉じてもDelay/Reverbの既存Tailは残る。
- UIには16ステップのグリッド（ステレオ時は L / R の2段）、プリセットパターン（POLYBRISK と共通の52種）、ランダマイズ、Clear、Invertを提供。
- Host Transport Stop/Jumpでステップ位置を再同期。曲の位置が取得できないときは内部クロック。

## 12. プリセット、保存、互換性

### 12.1 形式と場所

- 内部状態のCanonical Modelを単一のデータ構造にする。ファイルプリセットはUTF-8 JSON拡張子 `.fmlume`。**JSONを使うので解析/破損検出が容易**。
- 工場プリセット: アプリケーションbundle内の読み取り専用リソース。ユーザープリセット: `~/Library/Application Support/FMLUME/Presets/`。
- ファクトリーの `id` はUUIDまたは安定した名前空間IDで固定。ユーザーの保存先は独立して管理。
- `schemaVersion: 2`, `pluginVersion`, `patchId`, `displayName`, `author`, `category`, `tags`, `description`, `createdAt`, `modifiedAt`, `favorite`（個人のお気に入りはユーザー設定でもよい）、`engine`, `macros[8]`, `matrix[8]`, `performance`, `fx`, `output`, `metadata` を定義。
- `engine` は§4〜7のパラメーター、接続行列、波形、EG、フィルター等を**省略なし**で保存。
- schemaVersion 1（Morph 用の 2 音目 `engineB` / `macrosB` / `matrixB` / `morph` / `slotNames` を持つ形式）は読込時にマイグレーションする: `engineA` を `engine` として読み、2 音目に関するキーは捨てる。2 音目が入っていた場合は警告を返す。
- Export時はパラメーターをclampして書き出すのではなく、異常値を**拒否**。Import時はschema検証後、変更不可な旧形式をマイグレーションする。
- ホストのproject stateはJUCEのバイナリステート（ValueTreeか互換形式）として保存し、**ファイルと同一Canonical Model全体**＋ホスト公開値を含める。単に`presetId`だけを保存してはならない。外部プリセットが削除されてもDAWプロジェクトの音が復元できること。
- `schemaVersion`ごとにmigration関数を持ち、未知のfuture schemaは安全に拒否。余分な`metadata`は可能な範囲で保持。

### 12.2 JSONスキーマの規範的ルール

以下の型と必須項目を基に、`schemas/fmlume-preset.schema.json`（JSON Schema Draft 2020-12）を作成すること。

```text
Preset {
  schemaVersion: 1,
  pluginVersion: string,
  patchId: string,
  displayName: string,
  author: string,
  category: enum<Arp,E.Piano,Bell,Bass,Lead,Pad,Poly,Pluck,Brass,Keys,SFX>,
  tags: string[], description: string,
  engine: EnginePatch,
  macros: Macro[8], matrix: ModSlot[8],
  performance: PerformanceState, fx: FxState, output: OutputState,
  metadata: object
}
EnginePatch {
  operators: Operator[6],
  edges: {from:int[0..5],to:int[0..5],index:number[0..16]}[],
  filter: FilterState, lfo: LFOState[2], modEnv: ADSR,
  pitchBendRange: number[1..24]
}
Macro {
  id: "macro.1".."macro.8", name: string, defaultValue:number[0..1],
  mappings: {targetParamId:string, amount:number[-1..1],
             curve:"linear"|"exp"|"sCurve",
             polarity:"bipolar"|"unipolar"}[] // max 8
}
FxState { order: FxType[], bypassAll:bool, processors: FxProcessorState[] }
```

**実装時補足:** `Operator`の全フィールドは§4.2、`PerformanceState`は§5と§11、`FxProcessorState`は§10に準拠。`Macro`にある`min/max`（§6.2）は**ターゲット固有の正規化適用範囲**として任意に指定できる（省略時0/1）。スキーマ必須フィールドの完全リストを `docs/PRESET_SCHEMA.md` に追記し、正しい1ファイルと意図的に壊した10ファイル以上でテストする。

### 12.3 ファクトリープリセット

- 11カテゴリ（v1.1 で Arp と Poly を追加）、最低**各12音色**。実装は152音色（Arp 30、Bell 13、Poly 13、他は各12）。MVPでは各カテゴリ最低4でよい。
- 1音色に `tags` / 説明 / 狙い / 想定ベロシティ / 8マクロの有用な変化を付与。
- 作曲家が用途を探しやすい `Soft / Bright / Retro / Modern / Cinematic / Punchy / Metallic / Warm` などタグを提供。
- お気に入り、カテゴリ・タグ検索、フリーワード、前後プリセット、変更の有無（Dirtyマーク）、Save As/Overwrite/Export/Import。
- 既存製品のパッチ、権利者のサンプル、商標を使った誤認名称は無断コピーしない。

## 13. ホスト公開パラメーターと自動化

- APVTSをホスト公開パラメーターの単一管理経路に使う。**一度リリースしたIDは削除/再利用しない。**
- 最低限公開するID:

```text
master.gainDb       [-60, +12] dB   初期値 0 dB、0 dB がスライダー中央（v1.1。POLYBRISK と同じ）
master.pan          [-1, 1]
master.polyphony    [1, 32] (discrete)
master.unison       [1, 8]  (discrete)
master.detune       [0, 50] cents
master.spread       [0, 100] percent
master.quality      [Eco, Standard, High] (discrete, non-realtime rebuild)
macro.1 .. macro.8  [0, 1]
fx.bypassAll        [0, 1] (discrete)
fx.quick.warmth     [0, 1]
fx.quick.width      [0, 1]
fx.quick.space      [0, 1]
fx.quick.echo       [0, 1]
arp.enabled         [0, 1]
tranceGate.enabled  [0, 1]
```

- **v1ではAdvancedのすべての内部パラメーターをホスト自動化対象にはしない。** それらはCanonical Modelに保存されUIから編集できる。将来ホスト公開を追加する場合は旧IDを維持。
- 1つの機能にAPVTSの値とCanonical Modelの値が二重存在しないよう、どちらがtruthかを定義。**ホスト公開値はAPVTSがtruth**、その他はCanonical Modelがtruth。パッチ読込時は両者を整合的に一括反映する。
- パラメーターのホスト単位表示（dB、ms、%）を正確に設定。Automationが録音/再生されること。DAW上でプリセットを切り替えたときはHost Parameter gestureの意味が一貫する。

## 14. UI/UX 詳細

### 14.1 画面構成

**共通ヘッダー（v1.1、POLYBRISK と共通の構成）:** ロゴ、Prev/Next、Preset Name（クリックでプリセットブラウザーを開く）、Save、Simple/Advanced、OUT（出力ゲインと出力メーター）。Favorite はブラウザー内。Undo/Redo と CPU/Voice 表示は廃止。

**プリセットブラウザー（v1.1）:** 独立した全面ページ。検索、カテゴリ、Favorites、タグ、Init / Save As / Delete / Import / Export。

**画面下部:** 88鍵の画面内鍵盤と BEND / MOD ホイール。プラグインはホストのキー操作を奪わない（文字入力欄にフォーカスがあるときを除く）。

**Simple（v1.1）:** MACROS（共通4）＋CHARACTER（カテゴリ別4）→ FX Quick Controls / Trance Gate / Arpeggiator → Variations（Sound Explorer）。詳細機能はAdvancedに隠すが、Simpleで初期音色を選び直してもゼロから音を作れる。

**Advanced（v1.2）:** `Operators / Filter & Mod / FX / Rhythm / Performance` タブ。Operators は上段が編集できる結線図（ドラッグで接続、線をクリックで深さ、オペレーターのオン/オフ、`Templates...` で18種から選択）、下段が選択した1オペレーターの波形・エンベロープ・レベルのグラフ編集。`Expand` で結線図をタブ全体に広げ、テンプレート一覧と選択中オペレーターの Ratio / Level / Output / Feedback を並べる。Filter & Mod の Mod Envelope も同じグラフで編集できる。FX の順番はドラッグ＆ドロップで並べ替える。ルーティング図、操作ノブ、値直接入力、右クリック初期化、双方向マクロ反映。

### 14.2 画面品質

- 初期ウィンドウ **1200×760**（デバイス独立ピクセル相当）、最小960×620、最大1920×1200、ホスト内リサイズ対応。
- マウスドラッグ、ダブルクリック数値入力、Shiftで微調整、Option/Altでデフォルト復帰、キーボードTab操作、ツールチップ。
- フォーカス表示、極端に小さいフォントを避ける。色のみで状態を示さない。
- GUI更新時に音声スレッドを停止しない。メーター更新最大30Hz。
- UIは音色に応じたカテゴリの意味づけが伝わる。**DX7のような6オペレーター画面を初回起動の第一画面にしない。**

### 14.3 サウンド探索の状態遷移

```text
Current Patch → Explore Generate → 8 Candidate Previews
                   │                    ├─ Preview（試聴）
                   │                    ├─ Use（採用）→ Current Patch
                   │                    ├─ Explore From This → 8 new candidates
                   │                    └─ Cancel → Current Patchを維持
                   └─ Regenerate（元の親から再生成）
```

- Undo/Redo は v1.1 で廃止した。採用（Keep this sound）は元の音を置き換える。Explorer候補は保存しない限り永続化しない。

## 15. 推奨ディレクトリ構成

```text
FMLUME/
├── CMakeLists.txt
├── CMakePresets.json
├── .gitignore
├── README.md
├── SPEC.md
├── CLAUDE.md
├── external/JUCE/                 # git submodule, tag 9.0.6
├── cmake/
├── Source/
│   ├── Plugin/                    # PluginProcessor, PluginEditor, APVTS
│   ├── Core/                      # Model, Validation, PatchCompiler, State
│   ├── DSP/
│   │   ├── FM/                    # Operator, ConnectionGraph, FmVoice, SynthEngine
│   │   ├── Voice/                 # Allocator, Env, Unison, Portamento
│   │   ├── Modulation/            # LFO, ModMatrix, MacroEngine
│   │   ├── Filter/
│   │   ├── FX/                     # one class per effect, reorder chain
│   │   └── Sequencer/              # Arp, Chord, TranceGate
│   ├── Exploration/               # SoundExplorer, CandidateScorer, PCG
│   ├── Presets/                   # Persistence, Factory, Search, Migrations
│   └── UI/
│       ├── Common/
│       ├── Simple/
│       └── Advanced/
├── Resources/Presets/              # bundled .fmlume
├── schemas/fmlume-preset.schema.json
├── tests/
│   ├── Unit/
│   ├── DSP/
│   ├── Integration/
│   └── Fixtures/
├── tools/                          # offline renderer / preset validator
└── docs/
    ├── ARCHITECTURE.md
    ├── DECISIONS.md
    ├── IMPLEMENTATION_STATUS.md
    ├── PRESET_SCHEMA.md
    └── TEST_PLAN.md
```

**テスト可能性:** `FMLUMECore` CMake static libraryから`FMLUMEPlugin`と`FMLUMETests`が依存する形にする。プリセットのシリアライズ・FM音源・Explorerはプラグインホストを起動せずテスト可能とする。

## 16. 実装フェーズ・作業単位

### Phase 0 — 基盤とCI

- CMake+JUCEを固定タグで導入。AU/VST3 instrumentの最小エントリーを実装。
- `AudioProcessor`, `AudioProcessorEditor` と `FMLUMECore` の分離。
- 単純サイン波OscillatorによるMIDI発音、テスト用オフラインレンダラーを実装。
- GitHub Actions macOS runnerでコンパイル/CTestを実行（署名や公証は不要）。
- **DoD:** AU/VST3が生成され、ホストが認識、NoteOn/Offでサイン波、単体テスト可。

### Phase 1 — FMエンジン

- 6-op DAG検証/ソート、Operator、ADSR、FM/PM、self-feedback。
- 最大16テンプレート、MIDI、Polyphony、VoiceSteal、ピッチベンド、Sustain。
- 初期波形SineとStandard（2x）で完成。非サイン波を実装する前にFM正しさを検証。
- **DoD:** 接続の違いで音色が変化し、無音・異常値・循環を適切に扱う。標準周波数・EGがテスト通過。

### Phase 2 — Simple / Advancedとマクロ

- Canonical Model、コンパイラ、基本パッチ切替。Simple 4+4、Advanced Algorithm/Operator。
- MacroEngine、カテゴリテンプレート（v1.1 で11個）、8マクロ編集。（Undo/Redo は v1.1 で廃止）
- **DoD:** Simpleからゼロ音色を作れ、Advancedが同じ音色を編集。マクロ動作がプリセット変更後も破綻しない。

### Phase 3 — 音色拡張と基本FX

- 波形、Bandlimited Wavetable、品質切替、Filter、Unison、LFO/Mod Matrix。
- EQ/Drive/Chorus/Delay/Reverb、FXクイック操作と状態保存。
- **DoD:** FM音色・EQ・FXが十分な品質で発音、モジュレーションとFXのパラメーター変更でクリック抑制。

### Phase 4 — Preset/State

- 完全JSONスキーマ、`.fmlume` import/export、100% state save/restore、検索/お気に入り。
- 工場プリセット36以上（9カテゴリ×4）と適切なマクロ割当て。
- **DoD:** 保存前と再読込後のオフラインレンダーが誤差許容範囲で一致。ファイルを削除してもDAWプロジェクトから音色復元。

### Phase 5 — Sound Explorer

- Seed固定生成、方向/強度、ロック、8候補、試聴、採用、履歴。
- **DoD:** 同seedで同候補、ロック不変、候補は安全かつ各々異なり、Cancelで元の音色に復帰。

### Phase 6 — Performance & 全FX

- Arp/Chord/Trance Gate、Phaser/Flanger/Compressor/Bitcrusher、FX並べ替え。
- **DoD:** BPM同期、音色保存/復元、AU/VST3ホスト確認。

### Phase 7 — 安定性/最適化/プリセット拡充

- 108音色以上、パフォーマンス調整、Voice/CPUメーター、解像度/HiDPI、操作ヘルプ、フォールト耐性。
- 44.1/48/96/192k、バッファ32/64/128/512/2048、offline render、macOS Intel/ARMスポットテスト。
- **DoD:** §17のバグ基準・性能目標・プロジェクト互換性を満たす。

### Phase 8 — 配布の準備（配布方針決定後）

- ライセンス、バンドル/製品ID、コード署名/公証、インストーラー、ユーザーマニュアル、EULA/著作権表記の確認。
- オープンソース化/無料公開/有料化は**未決定**。商用ライセンス条件に従って配布方針を決定する。
- **DoD:** 自分のMac以外のクリーン環境でインストール、認識、音色呼出、アンインストールが可能。

## 17. 受入テスト・品質基準

### 17.1 DSPユニット

| ID | テスト内容 | 合格条件 |
|---|---|---|
| DSP-01 | 1オペ、A4、ratio1、Sine、FX off | 基音440Hz（FFT/ゼロ交差測定で±0.5Hz） |
| DSP-02 | 2オペ index0 | ターゲットの純粋なキャリア音と等価 |
| DSP-03 | 2オペ index増加 | サイドバンド/聴感上の倍音変化が発生 |
| DSP-04 | 異オペレーター間循環 | コンパイラでエラー、DSPへ未反映 |
| DSP-05 | self-feedback上限 | NaN/Infなし、出力暴走なし |
| DSP-06 | ADSR note-off/Sustain | Release/CC64挙動が仕様どおり |
| DSP-07 | NoteOn/Off/Velocity0/AllNotesOff | 取り残しVoiceなし |
| DSP-08 | 32音/polyphonyと8 unison | Voice予算超過時に安全にSteal |
| DSP-09 | Macro neutralと再割当て | 中立値は基準音、範囲外アクセスなし |
| DSP-10 | Sample Rate切替 | 周波数/Envelope時間/FXタイミングが保たれる |
| DSP-11 | 無入力・不正値耐性 | DSP出力にNaN/Infが出ない |
| DSP-12 | 2x/4x品質 | 位相/音程の整合、ホスト遅延報告が正しい |

### 17.2 機能統合

- EXP-01: seed固定で8候補のパラメーターが完全一致。変更対象がLockを破らない。
- EXP-02: Preview→Cancelで元のパッチに戻る。Preview→Useで現在のパッチが採用される。
- PRS-01: JSON round-tripはデータ完全一致（正規化した順序で比較）。
- PRS-02: プロジェクト保存→プラグイン再起動→復元→同じ入力MIDIをレンダーし、許容誤差 `maxAbs < 1e-5`（決定論的ルートの場合）。乱数系はseedを固定。
- PRS-03: 旧版・未知版・壊れたJSON・極端な値でクラッシュしない。
- FX-01: Reverb/Delayはnote-off後に自然なtail。Bypassで不要なDSPを計算しない。
- FX-02: FX順序変更、リロード、バイパス、Macro連動の保存復元。
- PERF-01: Arp/Chordでノートが取り残されない。Transport jump/Loopでステップ同期。
- HOST-01: CubaseでAU（対応ホスト）/VST3を認識し、MIDI、Automation、Preset State、Offline Exportを確認。Logic ProなどAUホストでも別途試験。

### 17.3 定量的な性能目標

- 参考機: Apple Silicon M1 Max級、48kHz/256 frames、Release build。
- **目標**: Standard 2x、16ノート×1 unison、EQ/Chorus/Delay/Reverb有効で**単一コア実時間処理予算の20%未満**（コア型や環境に依存するため、計測値・測定手順を報告）。この値は設計目標であり未計測の性能保証ではない。
- 最悪ケース（8ノート×8 unison、全FX、High 4x）ではVoice予算と負荷制御を優先。**音切れよりも安全に発音数を制限**する。
- 連続5分の自動MIDIストレステスト、極端なAutomation、Preset交換、Transport JumpでもRTスレッドの割当て/ロック/クラッシュ/非有限値なし。
- `processBlock` が `numSamples==0`、mono/stereoレイアウト要求、空MIDIを受けてもクラッシュしない。
- バイパス時もプロジェクト保存/復元およびホスト側のプラグイン有効/無効が安定。

### 17.4 手動リスニングの最低チェック

- DX系エレピ: Velocityで硬さが変わり、Chorus/Reverbで透明感が得られる。
- Bell: 非整数ratioで金属感を作れ、Release長くしても暴走しない。
- Bass: 短いAttackと歪みで低域を保ったまま歯切れよく鳴る。
- Pad: Unison/Filterでアナログ風の広がりが得られる。
- Explorer: 8候補に実際に意味のある違いを感じられる。

## 18. リスクと設計上の注意

1. **FMの非線形性**: マクロやパラメーター補間の結果が聴感上直感的にならない場合がある。カテゴリ別の安全な制御マップと試聴テストで改善。
2. **高負荷**: 自由接続+4x+8 unison+Dualは高コスト。Voice予算・品質設定・重いFX停止を明示的に管理。
3. **RT安全性**: JSON/グラフ/UI/音声処理を混在させない。メモリ寿命とスナップショット交代をテスト。
4. **ホスト互換性**: パラメーターID/プラグイン固有IDは後方互換が必須。AU/VST3の動作差を統合テスト。
5. **Explorer音楽的品質**: 一般ランダム生成では破綻しやすい。初期はカテゴリ別テンプレートと制約付き変動を徹底。
6. **ライセンス**: JUCE 9はAGPLv3/商用のデュアルライセンス。商用製品としてソース非公開配布を行う場合などの条件を配布前に確認。
8. **商標**: FMLUMEは**開発名称として確定**。実際の製品公開前に同一・類似商標、プラグイン市場での名称衝突を別途調査する。

## 19. 仕様上の決定事項一覧（後から勝手に変更しない）

- 製品名 **FMLUME（エフエムリューム）**。
- **FM/PM主体**。サンプル音源・PCMレイヤーはv1搭載しない。
- **6オペレーター／自由接続**。異オペレーター間はDAGで自己フィードバックのみ許可。
- **Simple/Advanced**、共通4マクロ＋カテゴリ別4マクロ（いずれも常時表示）、ユーザー編集可。
- **Sound Explorer**: 8候補・方向指定・Lock・履歴。
- **音色**: 11カテゴリ（Arp、Poly を含む）のオールラウンド。フィルター/波形/ユニゾンでアナログ風も対応。
- **演奏**: Arp、Chord、Unison、Trance Gate。
- **FX**: EQ、Drive、Chorus、Delay、Reverbを基本に9種類、並べ替え可能。
- **ホスト**: macOS AUv2/VST3、JUCE+CMake、段階的実装。
- **配布**: 未定。商用配布を妨げる依存/素材の無断混入は禁止。

## 20. 参考資料（開発時に確認）

- JUCE releases: https://github.com/juce-framework/JUCE/releases （9.0.6は2026-11-19リリース）
- JUCE CMake API: https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md
- JUCE 9.0.6 license: https://github.com/juce-framework/JUCE/blob/9.0.6/LICENSE.md
- JUCE 9 EULA: https://juce.com/legal/juce-9-licence/
- JUCE Oversampling API: https://docs.juce.com/master/classjuce_1_1dsp_1_1Oversampling.html

---

**実装完了の定義:** `SPEC.md`にあるv1.0のMUST項目が実際に動作し、テスト結果・未解決の制約を文書化した状態。UIモックだけ、音の出ないパッチエディタだけ、あるいは大量の未接続ダミーボタンだけを「完成」として扱わない。

---

## 21. 改訂履歴

### v1.2（2026-11-24）

| 項目 | 変更 | 記録 |
|---|---|---|
| Advanced | Algorithm タブを Operators タブに統合。結線図を常時表示し、`Expand` で拡大 | D-057 |
| Sound Morph | 機能を削除（§9）。ホストパラメーター `morph.amount` / `morph.enabled` を廃止、プリセット形式は schemaVersion 2 | D-055 |
| ファクトリー音色 | 152 音色。試聴に基づく作り直し、出力の DC カット、マクロの音量ガード | D-051〜D-054 |

### v1.1（2026-11-23）

実装とユーザーレビューを経て決まった変更。理由と互換性への影響は `docs/DECISIONS.md` の該当番号に記録している。リリース前のため、プリセット形式とホストパラメーターの範囲には互換措置なしの変更を含む（D-036）。

| 項目 | 変更 | 記録 |
|---|---|---|
| 形式 | Standalone アプリを追加 | D-026 |
| 外観 | POLYBRISK とシリーズ共通の配色・部品・ヘッダー構成。基調色はシアン→バイオレット。ロゴはサイン波 | D-028, D-029, D-039 |
| ブラウザー | 独立した全面ページに変更 | D-028 |
| Sound Explorer | UI 名称を Variations に変更し、生成→試聴→採用の3段階に整理 | D-028 |
| Morph | B をブラウザーから選ぶ方式。A/B の名前をプリセットに保存 | D-028, D-044 |
| Trance Gate | ステレオ化、Rate に 1/64、POLYBRISK と共通の52パターン | D-027, D-030 |
| Arpeggiator | Octave Order（オクターブ単位で形を移動） | D-037 |
| カテゴリ | Arp（ブラウザーでは先頭）と Poly を追加（計11） | D-035, D-036, D-045 |
| 出力ゲイン | -60〜+12 dB、初期値 0 dB、0 dB が中央 | D-038 |
| 鍵盤 | 88鍵、BEND / MOD ホイール | D-033 |
| Undo / Redo | 廃止 | D-041, D-042 |
| キー操作 | ホストのショートカットを奪わない | D-043 |
| FX | 並べ替えはドラッグ＆ドロップのみ | D-041 |
| マクロ | 範囲（min/max）を画面で編集可能 | D-044 |
| 識別子 | 会社名 CHEEBOW、メーカーコード Chbw、Bundle ID info.cheebow.fmlume。開発用ビルドは別プラグイン「FMLUME Dev」（Fmld） | D-047, D-048 |
| 配布 | GNU AGPL v3.0、最低 macOS 12、Universal の `.pkg` インストーラー、最初のバージョンは 0.1.0 | D-049, D-050 |
| テスト | Catch2 ではなく自前の最小ハーネス | D-003 ほか |
| セキュリティ | プリセット／状態の読み込みに資源上限と検査を追加 | D-040 |

