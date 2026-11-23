# FMLUME — Claude Code 作業指示

このリポジトリは **FMLUME（エフエムリューム）**、直感的に音作りできるmacOS AU/VST3 FMシンセの開発用です。

## 読む順序

1. `SPEC.md` — 全製品要件と受入条件。**必ず全文読む**。
2. `docs/IMPLEMENTATION_STATUS.md`（存在すれば） — 実装進捗・未解決事項。
3. `docs/ARCHITECTURE.md`（存在すれば） — 構造の最新決定。
4. 使用するJUCEバージョンの `external/JUCE/LICENSE.md` — **全文読む**。配布形態に応じ商用ライセンスが必要となり得る点を利用者へ示す。

## 実行方針

- 仕様に書いたフェーズ順を守り、**まずPhase 0を実装**。初回から巨大な完成版を一括生成しない。
- 各フェーズで必要最低限の実装を行い、`cmake`ビルドと`ctest`を実行してから次へ進む。
- DSP処理はホスト非依存の`FMLUMECore`でテスト可能にする。
- オーディオスレッドではメモリ割当て・mutex・I/O・JSON・ログは禁止。
- ホストパラメーターID、Preset schema、FM内部計算単位（indexはradian）、DAGルールを変更しない。
- 変更の際は`docs/DECISIONS.md`に理由と後方互換性の影響を書く。
- `docs/IMPLEMENTATION_STATUS.md`に実際のビルド結果、実装済み機能、次のタスク、未検証機能を更新する。
- 未実装ボタンは動くように見せない。将来機能は非表示か「Coming later」と明示する。
- 音質・動作の確認にはオフラインMIDI→WAVレンダラーと自動テストを優先して作る。

## 最初のタスク

1. プロジェクトディレクトリ・Git・`.gitignore`を構築。
2. CMakeでJUCE **9.0.6**（固定タグ）を取り込み、`FMLUMECore`、AU、VST3、テスト実行体のターゲットを構成。
3. `Source/Plugin/PluginProcessor`と`PluginEditor`で最小シンセを実装。
4. 440Hz基準サイン発音・MIDI Note On/Off・ステレオ出力を確認。
5. `docs/ARCHITECTURE.md`、`docs/IMPLEMENTATION_STATUS.md`、`docs/DECISIONS.md`、`README.md` を生成。
6. 実行結果をユーザーに報告してから次フェーズの実装を進める。

## 受入報告フォーマット

- 対象フェーズ
- 作成/変更ファイル
- 実装した機能
- ビルドコマンドと結果
- テストコマンドと結果
- 実機で確認できたこと/できなかったこと
- 残る問題
- 次フェーズの着手候補
