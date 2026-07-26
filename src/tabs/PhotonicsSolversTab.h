// PhotonicsSolversTab.h — 光学ソルバ選択 (photonics-solvers.jsx 相当)。
// FDTD / RCWA / BPM / FMM の4ソルバをカードで切替え、ソルバ別設定を表示:
//   FDTD — 広帯域フルウェーブ / RCWA — 周期構造の層別固有値分解
//   BPM  — パラキシャル導波路   / FMM  — S行列カスケード (一般化RCWA)
// + クロスバリデーション + ハイブリッド解析表。
// 選択ソルバと共有パラメータ (RCWA次数/周期, BPM方式/入射, FMM Li則) は
// OpticalOpts (.ofdx) に永続化。光ドメイン選択時のみ表示される。
#pragma once
#include <QFrame>
#include <QScrollArea>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;
class QStackedWidget;

namespace ofd {

class Project;

// ソルバ選択カード (mock のクリック選択カードUI)
class SolverCard : public QFrame {
    Q_OBJECT
public:
    SolverCard(int id, const QString &name, const QString &full,
               const QString &bodyHtml, QWidget *parent = nullptr);
    void setSelected(bool on);
signals:
    void picked(int id);
protected:
    void mousePressEvent(QMouseEvent *) override;
private:
    int     m_id;
    QLabel *m_badge;
};

class PhotonicsSolversTab : public QScrollArea {
    Q_OBJECT
public:
    explicit PhotonicsSolversTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    void apply();
    void setMethod(int id);
    QWidget *buildFdtdPage();
    QWidget *buildRcwaPage();
    QWidget *buildBpmPage();
    QWidget *buildFmmPage();

    Project *m_p;
    bool     m_updating = false;
    int      m_method = 0;          // 0=FDTD 1=RCWA 2=BPM 3=FMM
    SolverCard     *m_cards[4];
    QStackedWidget *m_stack;

    // FDTD
    QSlider   *m_meshAcc;
    QLabel    *m_meshAccVal;
    QLineEdit *m_simTime, *m_shutLevel;
    QCheckBox *m_shutOn, *m_subpixel;
    QComboBox *m_confMesh;
    // RCWA
    QLineEdit *m_px, *m_py;
    QSpinBox  *m_nx, *m_ny;
    QLabel    *m_harmLabel;
    QComboBox *m_trunc;
    QSpinBox  *m_slices;
    QLineEdit *m_incTheta, *m_incPhi, *m_incPsi;
    QLineEdit *m_lamMin, *m_lamMax;
    QSpinBox  *m_lamPts;
    // BPM
    QComboBox *m_bpmAlgo, *m_bpmDir, *m_bpmBc, *m_bpmInput;
    QLineEdit *m_bpmDz, *m_bpmLen, *m_bpmNref;
    // FMM
    QSpinBox  *m_fmmM, *m_fmmN;
    QLabel    *m_fmmTotal;
    QCheckBox *m_fmmLi;
};

} // namespace ofd
