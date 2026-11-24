# FMLUME テスト計画（SPEC §17 との対応）

実行: `ctest --test-dir build -C Release --output-on-failure`（4グループ: `fmlume_dsp` / `fmlume_unit` / `fmlume_integration` / `fmlume_stress`）。
個別実行: `build/tests/Release/FMLUMETests <グループ名 または テスト名の一部>`。
ハーネスは `tests/TestFramework.h`（自前、DECISIONS D-003）。`tests/TestMain.cpp` が `operator new` をフックし、`AllocationScope` でオーディオ処理中の割当てを数える。

## 17.1 DSPユニット

| ID | テスト（ファイル） | 判定内容 |
|---|---|---|
| DSP-01 | `DSP-01 A4 sine fundamental...` (EngineBasicsTests) | 44.1/48/96 kHz でゼロ交差測定 440 Hz ±0.5、L=R |
| DSP-02 | `DSP-02 two operators with index 0...` (FmEngineTests) | index 0 の2オペと純キャリアの差 < 1e-6 |
| DSP-03 | `DSP-03 raising the modulation index...` | 2倍音成分が index とともに単調増加、接続違いで波形が変わる |
| DSP-04 | `DSP-04 cyclic graphs are rejected...` | 循環・自己/重複エッジ・キャリアなし・NaN・範囲外を拒否。拒否後もエンジン出力は直前パッチとビット一致 |
| DSP-05 | `DSP-05 maximum self feedback...` | 全opフィードバック1.5＋index 16、1x/2x/4x で有限かつ |x| ≤ 1 |
| DSP-06 | `DSP-06 ADSR timing...` | Sustainレベル、Release −30dB@R/2・−60dB@R、Attack時間、CC64の保持と解放 |
| DSP-07 | `DSP-07 note off / velocity 0 / all notes off...` | 取り残しボイス 0 |
| DSP-08 | `DSP-08 polyphony 32 with unison 8...` ＋ `Voice stealing is click free...` | SubVoice予算内、スチール時の最大サンプル段差が波形の自然な傾き程度、最新ノート優先 |
| DSP-09 | `DSP-09 macros: neutral equals the base patch...` ＋ `Macro delta is zero at neutral...` (ParamsTests) | 50%で基準音と一致（<1e-6）、極値で範囲内・有限、存在しないターゲットは警告付きで除去 |
| DSP-10 | `DSP-10 pitch and envelope timing are sample-rate independent` ＋ FX-01内のディレイ時間 | 44.1〜192 kHz でピッチ±0.5Hz、Release傾き一致、エコー間隔一致 |
| DSP-11 | `DSP-11 zero-length blocks...` ＋ `DSP-11 rejected patches and hostile host values...` | 0フレーム・空MIDI・可変ブロック・NaNゲイン・異常ホスト値・異常MIDIで非有限値なし |
| DSP-12 | `DSP-12 quality 1x/2x/4x...` | 3品質でピッチ一致、相互相関のラグ = 報告レイテンシ差、補正後の差 < 0.02 |
| — | `Output does not depend on the host block size` | ブロック 32/77/512/2048 で差 < 1e-5 |
| — | `Patch generations...` | 押さえた音は旧パッチ・新ノートは新パッチ、インプレース編集は滑らか、世代上限でフェード |
| — | `Mono / legato / portamento and pitch bend` / `Unison is stereo-symmetric...` / `Filter, waveforms and modulation matrix...` | ピッチ遷移、ステレオ対称・決定論・ゲイン正規化、LP/HP減衰、帯域制限波形のエイリアス < −50 dB相当、LFO/ModWheel変調 |
| — | `Every algorithm template and category template...` | 18テンプレート＋9カテゴリが有効・発音・全マクロ可聴 |

## 17.2 機能統合

| ID | テスト | 判定内容 |
|---|---|---|
| EXP-01 | `EXP-01 explorer is deterministic...` | 同seedで8候補のパッチ完全一致、9カテゴリ×3強度で全候補が安全・相互に異なる、Lock 32通りを遵守、方向でスコアが動く |
| EXP-02 | `EXP-02 preview / cancel / use...` | 生成・Previewでモデル不変、Cancel後の出力が元とビット一致、Use→Undoで復帰、Regenerate/From This/履歴20 |
| PRS-01 | `PRS-01 JSON round trip is lossless` | 再シリアライズ文字列が一致、float がビット一致、metadata保持、不正値のExport拒否 |
| PRS-02 | `PRS-02 saved state restores an identical render` | 状態→新規エンジン/コントローラー→同MIDI・同トランスポートで maxAbs < 1e-5（全カテゴリ＋Arp/Gate/4x） |
| PRS-03 | `PRS-03 broken, unknown-version and extreme presets...` | 28種のメモリ内破損＋`tests/Fixtures/broken` 21ファイルを拒否、schema 0 と schema 1（2音目つき）をマイグレーション、未知Modターゲットは警告のみ |
| FX-01 | `FX-01 reverb and delay tails...` | ノートオフ後のテール、ディレイ間隔と減衰、バイパス時 `process` 呼び出し0回 |
| FX-02 | `FX-02 reordering, toggling and bypassing...` ＋ PRS-01/02 | 並べ替え・個別オフ・全バイパスでクリックなし、Drive有無でオンセット位置一定、FX状態の保存復元 |
| PERF-01 | `PERF-01 arpeggiator...` / `Chord mode...` / `Trance gate...` | 16分のステップ時刻（±1サンプル）、Gate長、モード、スウィング、ジャンプ時の再同期、ノート残りなし、共有音の参照カウント、ゲートがFXテールを切らない |
| — | `Controller: edits validate, coalesce, undo/redo...` | Simple/Advancedが同一モデル、不正編集の拒否、ドラッグ1段Undo、Undo上限 |
| — | `PresetManager: save, rescan, search...` | 保存/上書き/検索/タグ/お気に入り/Import/Export、壊れたファイルのスキップ |
| — | `Factory bank: 108 presets...` | 9カテゴリ×12、ID一意、タグ・説明、全マクロ割当て、発音レベル、FX全オフでも発音 |
| HOST-01 | **手動**（下記） | |

## 17.3 性能・安定性

| 項目 | 方法 |
|---|---|
| 性能目標（基準ケース < 20%） | `fmlume-bench`。実測値は `docs/IMPLEMENTATION_STATUS.md` |
| 5分ストレス | `fmlume_stress`: ランダムMIDI、マクロ/モーフ/Unison/Quality の極端なオートメーション、約150回のプリセット交換、Explorer、Undo、トランスポートのPlay/Stop/Jump/Loop、可変・0フレームブロック。割当て0・有限・|x|≤1・最後に発音0 |
| RT安全 | `Real-time safety: process() never allocates...` |
| ホスト適合 | `auval -v aumu Fmld Chbw`（開発用。製品版は `Fmlu`）、`pluginval --strictness-level 10`（VST3 / AU） |
| Intel | `arch -x86_64 build/tests/Release/FMLUMETests`（Rosetta 上で x86_64 スライスを実行） |

## 手動テスト（自動化していないもの）

### HOST-01 ホスト確認（Cubase を最優先、次に Logic Pro / REAPER / Ableton Live）

1. プラグインスキャンで VST3（Cubase/REAPER/Live）と AU（Logic/REAPER/Live）が Instrument として認識される。
2. MIDIトラックで発音。ベロシティ、ピッチベンド、モジュレーションホイール、サステインペダルが効く。
3. `Macro 1`〜`Macro 8`、`FX Warmth` などをオートメーション記録→再生。
4. 音色を編集（マクロ・Advanced・FX順序）→プロジェクト保存→DAW再起動→再読込で同じ音。ユーザープリセットファイルを削除しても復元されること。
5. オフライン書き出し（Export Audio Mixdown / Bounce）がリアルタイム再生と一致。Quality を変えたときの遅延補正。
6. テンポ同期: Arp / Trance Gate / Delay sync / LFO sync がテンポ変更・ループ・ロケートに追従。
7. バッファ 32〜2048、サンプルレート 44.1〜192 kHz を切り替えて音切れ・ノイズがないこと。
8. プラグインのバイパス/有効化、エディタの開閉・リサイズ（960×620〜1920×1200）、複数インスタンス。

### §17.4 リスニングチェック

`fmlume-render` で各カテゴリを書き出して試聴する（例: `fmlume-render --preset "Resources/Presets/E-Piano-01-Classic-Tines.fmlume" --midi phrase.mid --out ep.wav`）。

- E.Piano: ベロシティで硬さが変わる / Chorus・Reverb で透明感
- Bell: 非整数ratioの金属感、長いReleaseで暴走しない
- Bass: 短いAttackと歪みで低域を保ったまま歯切れが良い
- Pad: Unison / Filter でアナログ風の広がり
- Explorer: 8候補に意味のある違い
