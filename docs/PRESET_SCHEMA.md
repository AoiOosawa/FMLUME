# FMLUME プリセット形式（`.fmlume` / schemaVersion 2）

- UTF-8 JSON。構造スキーマ: [`schemas/fmlume-preset.schema.json`](../schemas/fmlume-preset.schema.json)（JSON Schema Draft 2020-12）。
- 読込実装: `Source/Presets/PresetSerializer.cpp`。**値はclampせず、範囲外・型違い・NaN/Inf・欠損は拒否**する。書出しも不正なパッチは拒否。
- 検証コマンド: `fmlume-render --validate file.fmlume`

## トップレベル（すべて必須。★は任意）

| キー | 型 | 内容 |
|---|---|---|
| `schemaVersion` | int | `2`。`0` と `1` は読込時にマイグレーション（下記「旧形式」）、`>2` は拒否 |
| `pluginVersion` | string | 書き出したプラグインのバージョン |
| `patchId` | string | 安定ID。ファクトリーは `factory.<category>.<nn>.<name>`、ユーザーはUUID |
| `displayName` `author` `description` | string | |
| `category` | enum | `Arp` `E.Piano` `Bell` `Bass` `Lead` `Pad` `Poly` `Pluck` `Brass` `Keys` `SFX` |
| `tags` | string[] | 最大64 |
| ★`createdAt` ★`modifiedAt` | string | ISO 8601 (UTC) |
| ★`favorite` | bool | 互換のため保持。実際のお気に入りはユーザー設定（DECISIONS D-018） |
| `engine` | EnginePatch | 音源 |
| `macros` | Macro[8] | マクロ |
| `matrix` | ModSlot[8] | Mod Matrix |
| `performance` | object | 下記 |
| `fx` | object | 下記 |
| `output` | object | `levelDb`(-24..12): プリセット固有の音量トリム |
| `metadata` | object | 任意の追加情報。未知のキーはそのまま保持して再保存 |

未知のトップレベルキーは無視する（同じ schemaVersion 内の前方互換）。

## EnginePatch

| キー | 内容 |
|---|---|
| `operators` | Operator **ちょうど6個** |
| `edges` | `{from:0..5, to:0..5, index:0..16 [rad]}` 最大30。自己エッジ・重複・**循環は拒否** |
| `filter` | `type`(`off`\|`LP12`\|`LP24`\|`HP12`\|`BP12`) `cutoff`(20..20000 Hz) `resonance`(0..0.95) `drive`(0..1) |
| `lfo` | LFO **ちょうど2個**: `shape`(`sine`\|`triangle`\|`saw`\|`square`\|`randomSmooth`) `sync`(bool) `rateHz`(0.02..20) `division` `phase`(0..1) `retrig`(bool) |
| `modEnv` | ADSR |
| `pitchBendRange` | 1..24 半音 |

### Operator（全フィールド必須）

| フィールド | 範囲 | 既定 |
|---|---|---|
| `enabled` | bool | false（Op1のみtrue） |
| `waveform` | `sine` `triangle` `saw` `pulse` | sine |
| `pitchMode` | `ratio` `fixed` | ratio |
| `ratio` | 0.125..32 | 1 |
| `fixedHz` | 0.1..16000 | 440 |
| `detuneCents` | -100..100 | 0 |
| `level` | 0..1 | 1 |
| `velocityToLevel` | 0..1 | 0.5 |
| `velocityToIndex` | 0..1 | 0.3 |
| `keyTracking` | 0..1 | 1 |
| `env` | ADSR: `a` 0.001..20 s, `d` 0.001..30 s, `s` 0..1, `r` 0.001..40 s | 0.01 / 0.5 / 0.6 / 0.3 |
| `phaseOffset` | 0..1（×2π） | 0 |
| `keySync` | bool | true |
| `feedback` | 0..1.5 rad | 0 |
| `carrierGain` | 0..1 | 0（Op1のみ1） |

有効かつ `carrierGain > 0` かつ `level > 0` のオペレーターが最低1つ必要。

### テンポ分割（`division` / `rate`）

`1/1 1/2 1/4 1/8 1/16 1/32 1/64` に、付点 `.`、3連 `T` を付けた21種（例 `1/8.`、`1/16T`）。Arp と Trance Gate の `rate` は `1/4`〜`1/32` の12種のみ。

## Macro / ModSlot

```text
Macro   { id: "macro.N", name, defaultValue: 0..1,
          mappings: [{ targetParamId, amount: -1..1, curve: linear|exp|sCurve,
                       polarity: bipolar|unipolar, min?: 0..1 (既定0), max?: 0..1 (既定1) }]  // 最大8 }
ModSlot { source, target: パラメーターID または "", depth: -1..1, curve, enabled }
```

- マクロ値50%は常にニュートラル（デルタ0）。`amount` は正規化ドメインでのフルスロー時の変化量。`min/max` は結果の正規化クランプ窓。
- `source`: `none lfo1 lfo2 modEnv velocity keytrack modWheel aftertouch macro1..macro8`
- 存在しないターゲット（接続されていないエッジ等）を指す ModSlot / マッピングは**警告付きで無効化**（エラーにはしない）。ModSlot が未接続エッジを新規生成することはない。

### ターゲットとなるパラメーターID（公開後不変）

| ID | 範囲 | スケール | Macro | Mod |
|---|---|---|---|---|
| `opN.ratio` (N=1..6) | 0.125..32 | log | ○ | |
| `opN.fixedHz` | 0.1..16000 | log | ○ | |
| `opN.detune` | -100..100 ct | lin | ○ | ○ |
| `opN.level` | 0..1 | lin | ○ | ○ |
| `opN.velocityToLevel` `opN.velocityToIndex` `opN.keyTracking` `opN.phaseOffset` | 0..1 | lin | ○ | |
| `opN.attack` `opN.decay` `opN.release` | ADSR範囲 | log | ○ | |
| `opN.sustain` | 0..1 | lin | ○ | |
| `opN.feedback` | 0..1.5 | lin | ○ | |
| `opN.carrierGain` | 0..1 | lin | ○ | |
| `edge.J.I.index`（J→I、1始まり。**既存エッジのみ**） | 0..16 rad | lin | ○ | ○ |
| `filter.cutoff` | 20..20000 Hz | log | ○ | ○ |
| `filter.resonance` | 0..0.95 | lin | ○ | ○ |
| `filter.drive` | 0..1 | lin | ○ | |
| `lfo1.rate` `lfo2.rate` | 0.02..20 Hz | log | ○ | |
| `modEnv.attack/decay/sustain/release` | ADSR範囲 | | ○ | |
| `matrix.N.depth` (N=1..8) | -1..1 | lin | ○ | |
| `pitch` | ±12 半音 | lin | ○ | ○ |
| `master.level` | 0..2（1=等倍） | lin | ○ | ○ |
| `master.pan` | -1..1 | lin | ○ | ○ |
| `fx.<type>.<param>`（下表、離散値を除く） | | | ○ | wet のみ○ |
| `performance.portamento` | 0..2 s | lin | ○ | |

離散パラメーター（波形、グラフ、`fx.chorus.voices`、`fx.delay.sync`、`fx.delay.division`）はターゲットにできない。

## performance

```text
polyphony: 1..32
unison: { count: 1..8, detuneCents: 0..50, spread: 0..100 }
mono, legato: bool      portamento: 0..2 s
arp:        { enabled, mode: up|down|upDown|random|asPlayed, rate, gate: 0.05..1, octaves: 1..4, swing: 0..0.6, resetOnStop }
chord:      { type: off|major|minor|sus2|sus4|dim|aug|7|m7|maj7, voicing: root|1st|2nd, octave: -2..2, strumMs: 0..100 }
tranceGate: { enabled, steps: number[16] (0..1), rate, swing: 0..0.6, smoothMs: 0..30, mix: 0..1,
              stereo?: bool, stepsRight?: number[16] (0..1), width?: 0..1 }   // 省略可（D-030）
```

`polyphony`、`unison.*`、`arp.enabled`、`tranceGate.enabled`、各マクロの `defaultValue`、`fx.quick[].value` はホストパラメーターの保存値で、プリセット読込時にホストへ適用される。

## fx

```text
order:      FxType[9]   // 全9種の順列
bypassAll:  bool        // プロジェクト状態用。プリセット読込では適用しない
processors: [{ type, enabled, params: { ... } }] × 9（各タイプ1回ずつ）
quick:      [{ id: fx.quick.warmth|width|space|echo, value: 0..1, mappings: [...] }] × 4  // FXパラメーターのみ対象
```

| type | params（範囲） |
|---|---|
| `eq` | `lowGain` ±18 dB, `lowFreq` 40..600 Hz, `midGain` ±18 dB, `midFreq` 200..8000 Hz, `midQ` 0.3..8, `highGain` ±18 dB, `highFreq` 1500..16000 Hz |
| `drive` | `gain` 0..1 (0..36 dB), `tone` 0..1, `wet` 0..1, `trim` -24..12 dB |
| `chorus` | `rate` 0.05..8 Hz, `depth` 0..1, `width` 0..1, `wet` 0..1, `voices` 1..4 |
| `phaser` | `rate` 0.02..10 Hz, `depth` 0..1, `feedback` 0..0.9, `wet` 0..1 |
| `flanger` | 同上 |
| `compressor` | `threshold` -60..0 dB, `ratio` 1..20, `attack` 0.1..200 ms, `release` 5..2000 ms, `makeup` 0..24 dB, `wet` 0..1 |
| `bitcrusher` | `bits` 4..16, `downsample` 1..32, `wet` 0..1 |
| `delay` | `time` 1..2000 ms, `sync` 0/1, `division` 0..20（テンポ分割の番号）, `feedback` 0..0.9, `stereo` 0..1, `wet` 0..1 |
| `reverb` | `size` 0..1, `decay` 0.2..20 s, `preDelay` 0..200 ms, `damping` 0..1, `wet` 0..1 |

## スキーマで表現できずローダーが検査する規則

1. オペレーターグラフはDAG（循環・自己エッジ・重複エッジ禁止）。
2. 可聴キャリアが最低1つ。
3. `fx.processors` は9種を各1回。`fx.order` は順列。
4. マクロのターゲットは連続パラメーターのみ、`min <= max`。
5. 数値は有限（JSONパーサーが `NaN` / `Infinity` / 範囲外指数を拒否）。
6. オブジェクトのキー重複、64段を超えるネスト、4MBを超えるファイルは拒否。

## プロジェクト状態（DAW保存）

`getStateInformation` は JUCE の ValueTree バイナリ `FMLUME_STATE { version: 1, json: "..." }`。`json` は `{ "fmlumeState": 1, "preset": <上記プリセット全体>, "host": { gainDb, pan, polyphony, unison, detune, spread, quality, macros[8], fxBypassAll, quick[4], arpEnabled, gateEnabled } }`。プリセットIDだけでなくCanonical Model全体を含むので、元の `.fmlume` が削除されても復元できる。

## テスト用フィクスチャ

- `tests/Fixtures/valid/` — 正しいファイル（読めること）。`legacy_schema1_second_sound.fmlume` は 2 音目を持つ旧形式（schemaVersion 1）で、1 音目だけが読み込まれることを確かめる
- `tests/Fixtures/broken/` — 意図的に壊した21ファイル（すべて拒否されること）: 切り詰め、非JSON、将来バージョン、バージョン欠損、循環グラフ、自己エッジ、重複エッジ、キャリアなし、範囲外ratio、負のindex、オペレーター5個、未知の波形、不正マクロターゲット、マッピング9個、FX順序の重複、過大feedback、型違い、ゲート15ステップ、engineA=null、NaNリテラル、空ファイル

### Trance Gate のステレオ拡張（D-030）

- `steps` は左チャンネル（モノラル時は左右共通）、`stepsRight` は右チャンネルのパターン。`stereo` が `false` のとき `stepsRight` は無視される。
- `width` は左右の分離（1 = 完全に別、0 = 左右とも「どちらかが開いていれば開く」）。
- 3つのキーは**省略可能**。省略時は `stereo = false`、`stepsRight = steps`、`width = 1`。書き出し時は、情報を持つ場合（`stereo` が真、`stepsRight` が `steps` と異なる、`width` が 1 以外）だけ3つまとめて出力する。
- `tranceGate.rate` は `1/64` も指定できる（アルペジエーターの `rate` は従来どおり `1/32T` まで）。

### アルペジエーターのオクターブ順（D-037）

- `performance.arp.octaveMode`: `range`（既定。ノート順が全音域を通る）/ `up` / `down` / `upDown`（ノート順を 1 オクターブ内で演奏し、形ごとオクターブを移動する）。省略時は `range`。

## 旧形式（schemaVersion 0 / 1）の読込（D-055）

- **schemaVersion 1** は Morph 用の 2 音目を持っていた（`engineA` / `engineB` / `macrosB` / `matrixB` / `morph` / `slotNames`）。Morph は最初のリリース前に削除した。読込時に `engineA` を `engine` として読み、2 音目に関するキーはすべて捨てる。2 音目が実際に入っていた場合は警告を 1 つ返す。
- **schemaVersion 0** は開発初期の形式（`engine` が 1 つ、`output` なし）。`output.levelDb = 0` を補って読む。
- 書き出しは常に schemaVersion 2。旧形式で書き戻すことはない。
- DAW プロジェクトの状態に残っている `host.morphAmount` / `host.morphEnabled` は無視する。
