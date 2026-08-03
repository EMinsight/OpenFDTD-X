// ModeSolverTab.h — 〓 モードソルバ FDE (元 mock: pic-tools.jsx ModeSolverTab)。
//
// 導波路断面の実効屈折率と各種指標の設計検討画面 (光ドメイン)。
// neff は mock と同じ簡易近似式 (Si/SiO2 1550nm の傾向を再現する表示用の
// 近似) で幅・高さ・スラブ・偏波の入力に追従して更新する (材料・波長・
// 温度は近似式に入らず、未反映である旨を画面に明示)。実 FDE ソルバ
// (OpenBPM の虚軸伝搬法モードソルバ等) との連携は未実装 — 近似値である
// ことを画面に明示する (CLAUDE.md 絶対規則 5)。
#pragma once
#include <QScrollArea>

class QDoubleSpinBox;
class QComboBox;
class QLabel;
class QTableWidget;

namespace ofd {

class MiniPlot;
class Project;

class ModeSolverTab : public QScrollArea {
    Q_OBJECT
public:
    explicit ModeSolverTab(Project *project, QWidget *parent = nullptr);

    // 設計検討ツール (モデル非結合) のため apply/refresh は何もしない
    void apply() {}
    void refresh() {}

private:
    void recalc();   // 入力変更 → 近似 neff・表・判定の再計算

    Project *m_p;

    QComboBox      *m_shape = nullptr;      // ストリップ / リブ
    QDoubleSpinBox *m_width = nullptr, *m_height = nullptr, *m_slab = nullptr;
    QComboBox      *m_material = nullptr;
    QDoubleSpinBox *m_lambda = nullptr, *m_temp = nullptr;
    QComboBox      *m_pol = nullptr;        // TE / TM

    QTableWidget *m_modeTable = nullptr;
    QLabel       *m_singleModeBadge = nullptr;
    MiniPlot     *m_dispPlot = nullptr;
    QTableWidget *m_cornerTable = nullptr;
};

} // namespace ofd
