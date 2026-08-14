// TabHelpers.h — タブ間で共有する小さな GUI ヘルパー群.
//
// RirAnalysisTab / VocalAnalysisTab / AuralizationTab で同じコードが
// コピーされていたものをここへ 1 箇所に集約する (.claude/rules/gui.md:
// 「タブ間で共有できるヘルパーは既存タブからコピーせず共有ヘッダへ抽出」)。
#pragma once
#include "../audio/AudioEditEngine.h"   // SourcePrep
#include "../acoustics/qt/QtAcousticAdapter.h"
#include "../core/Project.h"            // AcousticOpts

#include <QColor>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <vector>

class QAbstractButton;
class QComboBox;
class QLabel;
class QTableWidgetItem;
class QWidget;

namespace ofd {
namespace tabhelp {

// 音源モデリングタブの入力信号設定 (.ofdx acoustic.source_wav) →
// AudioEditEngine の前処理パラメータ。音源モデリングタブ (プレビュー) と
// 可聴化タブ (レンダリング) が同じ変換を使うために共有する。
audioedit::SourcePrep sourcePrep(const AcousticOpts &a);

// ドライ音源へ前処理を掛けてから畳み込む。可聴化の 3 経路 (単発 / 一括 /
// 音響タブ) が同じ変換を通るように 1 箇所へ集約する。prep が既定
// (isIdentity) なら読み込んだバッファをそのまま畳み込むので、従来の
// QtAcousticAdapter::convolveFiles と結果はビット一致する。
// outPrepped には前処理を適用したかを入れる (UI が必ず明示するため)。
acoustics::AcousticResult<acoustics::ConvolutionInfo>
convolveWithPrep(const QString &dryPath, const QString &rirPath,
                 const QString &outputPath, int gainMode,
                 const audioedit::SourcePrep &prep,
                 bool *outPrepped = nullptr,
                 std::vector<double> *outDry = nullptr,
                 std::vector<double> *outWet = nullptr,
                 double *outSampleRate = nullptr,
                 QtAcousticAdapter::RirResampleNote *outResample = nullptr);

// 品質トークン ("valid" / "warning" / それ以外=invalid) → バッジ文字列
QString qualityBadge(const QString &token);

// 品質トークン → バッジ色 (緑 / 黄土 / 赤)
QColor qualityColor(const QString &token);

// 読み取り専用 (編集不可) のテーブルセルを作る
QTableWidgetItem *roItem(const QString &text);

// 波形表示の時間軸単位。envelopeSeries の X 値のスケールを決める
// (RIR は ms、可聴化 A/B 波形は s を使う)。
enum class TimeUnit { Seconds, Milliseconds };

// 長い系列を maxBins 区間の min/max 包絡線 2 系列に間引く (波形表示用)。
// X は unit で指定した時間軸単位、Y は振幅そのまま。
void envelopeSeries(const std::vector<double> &x, double fs, int maxBins,
                    TimeUnit unit, QVector<QPointF> &top,
                    QVector<QPointF> &bottom);

// 保存先をユーザーに選ばせて UTF-8 テキストを書き出す (CSV / JSON 出力)。
// キャンセル時は何もしない。書き込み失敗は QMessageBox で通知する。
void saveTextFile(QWidget *parent, const QString &caption,
                  const QString &suggested, const QString &filter,
                  const QString &content);

// ── 可聴化の RIR サンプルレート注記 ────────────────────────────────────────
// 畳み込み結果に必ず添える注記を作る (可聴化タブの単発/一括、音響解析タブの
// 3 箇所が同じ文言を出すため共有する)。返る各行に行頭記号は付かない。
//   1) RIR を変換した場合: 変換前後の fs を明示する (黙って変換しない)
//   2) RIR の帯域 (fs/2) が可聴帯域に届かない場合: ウェット音にそれ以上の
//      高域が無いことを警告する (出力 fs が高いと「高域まである音」に
//      見えてしまうため — CLAUDE.md 絶対規則 5)
// rirFsHz は RIR ファイル本来の fs、outFsHz は出力 (= ドライ) の fs。
// validBandHz は RIR の**物理的に有効な帯域上限** [Hz] (FDTD ソルバーの
// metadata.json の source.fmax_hz)。0 = 不明で、その場合だけ fs/2
// (ナイキスト) を上限とみなす。FDTD の有効帯域は格子分解能で決まり
// fmax = c/(10·dx) ≈ fs/17.5 なので、ナイキストで代用すると帯域を
// 桁で過大に表示してしまう — 分かるときは必ず渡すこと。
QStringList rirSampleRateNotes(double rirFsHz, double outFsHz,
                               double validBandHz = 0.0);

// 上の 2) の判定しきい値 [Hz]。RIR のナイキストがこれ未満なら警告する。
double rirBandWarnThresholdHz();

// ── 仮対応 (モック) の明示 (CLAUDE.md 絶対規則 5) ───────────────────────────
// 未実装機能のボタン/チェックを「押せるのに何も起きない」状態にしない。
// 無効化して「未実装」ツールチップを付ける。
//
// **理由を必ず添えること。** 「未実装」の一言だけでは、利用者は「対応予定が
// 無いのか」「自分の操作が足りないのか」を区別できない — 主語付き注記
// (`unwiredNote`) と同じ問題を抱える。why には**何が足りなくてできないのか**を
// 書く。よく使う分類は下の `notimpl::` に用意してある (個別の事情があるときは
// タブ固有の I18n キーを渡す)。
// selftest の `notimpl-reason` が、理由なしの呼び出しを検出する。
// 対象はボタンに限らない (無効化とツールチップは QWidget の機能なので、
// 何も動かさないコンボボックスや入力欄にもそのまま使える)。
void markNotImplemented(QWidget *w, const QString &why);

// 使い回せる「できない理由」。I18n キーなので `I18n::tr()` を通して渡す。
namespace notimpl {
constexpr const char *kFormat   = "th_ni_format";    // 外部ファイル書式の仕様
constexpr const char *kParser   = "th_ni_parser";    // 読み手 (パーサ)
constexpr const char *kKernel   = "th_ni_kernel";    // カーネル側の対応
constexpr const char *kData     = "th_ni_data";      // 同梱していないデータ
constexpr const char *kEngine   = "th_ni_engine";    // 計算エンジンそのもの
constexpr const char *kAudio    = "th_ni_audio";     // 音声入出力 (方針で持たない)
constexpr const char *kExternal = "th_ni_external";  // 外部アプリの起動
constexpr const char *kControl  = "th_ni_control";   // 実行中の中断・再開
constexpr const char *kReport   = "th_ni_report";    // 報告書の様式
constexpr const char *kPlot     = "th_ni_plot";      // 作図の実装
constexpr const char *kModel    = "th_ni_model";     // 物理モデルそのもの
} // namespace notimpl

// コンボボックスの一部の項目だけを選べなくする (残りは通常どおり選べる)。
// 「その選択肢が何故無いのか」を **項目のツールチップ**として残すため、
// 項目を消さずに無効化する — 消すと利用者は「対応予定が無い」のか
// 「見落としている」のか分からない。
// 例: 出力チャネルのうちバイノーラル (HRTF 未同梱) だけを落とす。
void disableComboItems(QComboBox *box, const QVector<int> &indices,
                       const QString &why);

// モック由来のサンプル値 (固定の表・グラフ・バッジ) の直下に置く注記ラベル:
// 「⚠ サンプル表示 — 実行結果ではありません (機能未実装)」
QLabel *sampleNote(QWidget *parent);

// どこにも反映されない設定フォームの節に置く注記ラベル:
// 「この設定は現在計算へ反映されません (未実装)」
//
// **主語のある方を使うこと。** 引数なしの版は「この設定」としか言わないので、
// 節の中に反映される入力と反映されない入力が混在していると、利用者は節ごと
// 死んでいると受け取る (実際にそう報告された)。what に「何が」反映されない
// のかを、wired に「代わりに何が」反映されるのかを渡す。
//   unwiredNote(s, I18n::tr("xxx_unwired_what"), I18n::tr("xxx_unwired_ok"))
//     → 「▸ <what> は現在計算へ反映されません (未実装)。<wired> は反映されます。」
QLabel *unwiredNote(QWidget *parent);
QLabel *unwiredNote(QWidget *parent, const QString &what,
                    const QString &wired = QString());

// ── ポスト作図の前提条件 (core/PostPrereq) ──────────────────────────────────
// チェックが入っていても、カーネル側の前提 (給電点 / 観測点 / frequency1・2 /
// 対象行) を満たさなければ `ofd_post` は図を 1 枚も出さない。**チェックを
// 受け付けて黙っているのが一番わかりにくい**ので、出ない項目とその理由を
// タブの先頭に出す。空文字列 = 全部出る。
//   group 0 = ポスト(1) の項目、group 1 = ポスト(2) の項目
QString postPrereqWarning(const Project &p, int group);

} // namespace tabhelp
} // namespace ofd
