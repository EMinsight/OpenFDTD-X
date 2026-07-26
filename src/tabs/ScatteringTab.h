// ScatteringTab.h — 散乱特性タブ (openfdtd-family.jsx ScatteringTab 相当)。
// OpenFDTD ドキュメント §2.15「散乱」: 平面波入射に対する散乱体の解析。
//   入射波 (θ, φ, 偏波, 角度スイープ) ・RCS ・近傍/遠方界変換 (NTFF) ・その他散乱量。
// Static prototype: values/toggles are local state (no matching Project field).
#pragma once
#include <QScrollArea>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLineEdit;

namespace ofd {

class Project;
class SectionBox;

class ScatteringTab : public QScrollArea {
    Q_OBJECT
public:
    explicit ScatteringTab(Project *project, QWidget *parent = nullptr);

private:
    SectionBox *checkSection(QWidget *parent, const char *titleKey,
                             const char *const *keys, const bool *checked, int n,
                             QVector<QCheckBox *> *out);

    Project *m_p;

    // 入射波 / Incident wave
    QLineEdit *m_theta, *m_phi;
    QComboBox *m_pol;                       // V(TE) / H(TM) / 円偏波
    QCheckBox *m_sweep;                     // 入射角スイープ (バイスタティック)
    QLineEdit *m_sweepFrom, *m_sweepTo, *m_sweepPts;

    // RCS
    QCheckBox *m_rcsMono, *m_rcsBi, *m_rcsMatrix;
    QComboBox *m_rcsUnit;                   // m² / dBsm / σ/λ²

    // NTFF
    QCheckBox *m_ntffExtract, *m_ntffWide;
    QComboBox *m_ntffSurface;               // 直方体閉曲面 / 球面

    // その他散乱量
    QVector<QCheckBox *> m_misc;
};

} // namespace ofd
