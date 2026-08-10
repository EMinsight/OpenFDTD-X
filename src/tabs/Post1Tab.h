// Post1Tab.h — ポスト処理制御(1): 周波数特性プロット類.
// Maps 1:1 to plotiter/plotfeed/plotpoint/plotsmith/plotzin/plotyin/
// plotref/plotspara/plotcoupling/matchingloss/freqdiv.
//
// mock (tabs.jsx Post1Tab) との対応:
//   時間特性(2D)   — 収束状況 (plotiter) / 給電点波形・スペクトル (plotfeed) /
//                    観測点波形・スペクトル (plotpoint)
//   周波数特性(2D) — スミスチャート + 入力インピーダンス…結合係数 +
//                    自動スケール + 周波数目盛分割
#pragma once
#include <QScrollArea>
#include <QVector>

class QCheckBox;
class QLabel;
class QSpinBox;
class QLineEdit;

namespace ofd {

class Project;
class SectionBox;
struct FreqPlot;

class Post1Tab : public QScrollArea {
    Q_OBJECT
public:
    explicit Post1Tab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();
    // チェックしても図が出ない項目を名指しで出す (core/PostPrereq)
    void updatePrereq();

private:
    struct FreqRow {
        QCheckBox *enabled;
        QCheckBox *userScale;
        QLineEdit *min, *max;
        QSpinBox  *div;
        FreqPlot  *target;
    };
    void addFreqRow(QWidget *parent, class SectionBox *s,
                    const QString &label, FreqPlot *target);
    void apply();
    // 自動スケール (mock: pp_auto_scale) を各行の「スケール指定」から復元する
    void syncAutoScale();
    // ドメイン別の出し分け (周波数特性(2D)は EM のみ / 給電点→音源ラベル切替)
    void updateDomainVisibility();

    Project   *m_p;
    bool       m_updating = false;
    QCheckBox *m_iter, *m_feed, *m_point, *m_smith, *m_matching;
    QSpinBox  *m_freqdiv;
    QVector<FreqRow> m_rows;
    // mock の「自動スケール」— 各行の userScale をまとめて反転するマスター。
    // モデルを持つのは各行なので、ここはウィジェット操作のみ。
    QCheckBox *m_autoScale = nullptr;
    // 周波数特性(2D) セクション — スミスチャート/Zin/Yin/反射/Sパラ/結合/整合損。
    // 音響・水中・光カーネルはこれらを出力しないので EM 以外では丸ごと隠す。
    SectionBox *m_freqSection = nullptr;
    // 前提条件の警告 (空なら非表示)
    QLabel     *m_prereq = nullptr;
};

} // namespace ofd
