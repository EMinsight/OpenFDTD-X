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
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;
class QProcess;
class QPushButton;
class QStackedWidget;
class QTableWidget;

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
    void runCrossValidation();      // チェックしたソルバを順に実行する
    void onCrossFinished(int exitCode);

private:
    void apply();
    void setMethod(int id);
    // 層構造テーブル = OpticalOpts::rcwaLayerList (光学タブで編集) のビュー
    void rebuildLayerTable();
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
    QTableWidget *m_layerTable = nullptr;   // 層構造 (RCWA 層スタックのビュー)

    // クロスバリデーション (チェックしたソルバを順に実行する)
    QVector<QCheckBox *> m_crossChecks;     // FDTD / RCWA / BPM / FMM の順
    QPushButton  *m_crossRun = nullptr;
    QLabel       *m_crossStatus = nullptr;
    QTableWidget *m_crossTable = nullptr;
    QProcess     *m_crossProc = nullptr;
    QVector<int>  m_crossQueue;             // 残りのソルバ (m_method と同じ番号)
    int           m_crossCurrent = -1;
    QString       m_crossDir;
    void startNextCrossRun();
    // 全ソルバ実行のあと、回折効率を出したものどうしを突き合わせる
    void compareCrossResults();
    void addCrossRow(const QString &solver, const QString &state,
                     const QString &out, const QString &note);
};

} // namespace ofd
