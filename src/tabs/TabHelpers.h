// TabHelpers.h — タブ間で共有する小さな GUI ヘルパー群.
//
// RirAnalysisTab / VocalAnalysisTab / AuralizationTab で同じコードが
// コピーされていたものをここへ 1 箇所に集約する (.claude/rules/gui.md:
// 「タブ間で共有できるヘルパーは既存タブからコピーせず共有ヘッダへ抽出」)。
#pragma once
#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>
#include <vector>

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

} // namespace tabhelp
} // namespace ofd
