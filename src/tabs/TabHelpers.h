// TabHelpers.h — タブ間で共有する小さな GUI ヘルパー群.
//
// RirAnalysisTab / VocalAnalysisTab / AuralizationTab で同じコードが
// コピーされていたものをここへ 1 箇所に集約する (.claude/rules/gui.md:
// 「タブ間で共有できるヘルパーは既存タブからコピーせず共有ヘッダへ抽出」)。
#pragma once
#include <QColor>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <vector>

class QAbstractButton;
class QLabel;
class QTableWidgetItem;
class QWidget;

namespace ofd {
namespace tabhelp {

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
void markNotImplemented(QAbstractButton *b);

// モック由来のサンプル値 (固定の表・グラフ・バッジ) の直下に置く注記ラベル:
// 「⚠ サンプル表示 — 実行結果ではありません (機能未実装)」
QLabel *sampleNote(QWidget *parent);

// どこにも反映されない設定フォームの節に置く注記ラベル:
// 「この設定は現在計算へ反映されません (未実装)」
QLabel *unwiredNote(QWidget *parent);

} // namespace tabhelp
} // namespace ofd
