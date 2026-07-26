// PerFaceBCTab.h — 境界面詳細タブ (ansys-tabs.jsx PerFaceBCTab 相当)。
// Ansys Lumerical FDTD と同様、6面 (X±/Y±/Z±) それぞれに独立した境界条件を
// コンボボックスで割り当てる表 + PML パラメータ設定。
// 層数は GeneralOpts::pmlL、多項式次数は GeneralOpts::pmlM に永続化。
#pragma once
#include <QScrollArea>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QTableWidget;

namespace ofd {

class Project;

class PerFaceBCTab : public QScrollArea {
    Q_OBJECT
public:
    explicit PerFaceBCTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    void apply();

    Project      *m_p;
    bool          m_updating = false;

    // 境界面別設定 / Per-face boundary conditions
    QTableWidget *m_faces;
    QComboBox    *m_faceBC[6];    // X- X+ Y- Y+ Z- Z+
    QCheckBox    *m_useSym, *m_usePeriodic;

    // PML設定 / PML parameters
    QComboBox    *m_profile;
    QSpinBox     *m_layers;
    QLineEdit    *m_alphaMax, *m_kappaMax, *m_sigmaMax, *m_polyOrder;
    QCheckBox    *m_dissipative, *m_evanescent;
};

} // namespace ofd
