// TransmissionLineTab.h — 伝送線路特性タブ (openfdtd-family.jsx TransmissionLineTab 相当)。
// OpenFDTD ドキュメント §2.14「伝送線路」: マイクロストリップ・同軸・導波管の
//   特性インピーダンス Z₀ / 伝搬定数 γ = α + jβ / Sパラメータ / 不連続部の設定。
//
// 断面形状と材料から **準 TEM の閉形式** (`core/TransmissionLine`) で
// Z₀ / ε_eff / β / α / 群遅延 / S パラメータを実計算し、下の結果表に出す。
// γ と S パラメータのチェックはその表に出す行の取捨で、Touchstone のチェックを
// 入れると .s2p 書出ボタンが有効になる。
//
// フォームは Project::tline() (.ofdx "transmission_line") の View。
// 未実装のまま残っているのは不連続部 (ベンド・ステップ・クロストーク・
// アイダイアグラム) と、Z₀ 抽出手法の選択 (準 TEM では 3 定義が一致する)。
#pragma once
#include <QScrollArea>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;

namespace ofd {

class Project;
class SectionBox;
class MiniPlot;

class TransmissionLineTab : public QScrollArea {
    Q_OBJECT
public:
    explicit TransmissionLineTab(Project *project, QWidget *parent = nullptr);

    void apply();     // widgets → model
    void refresh();   // model → widgets (m_updating ガード付き)

private:
    void updateEye();   // アイダイアグラムの図と数値 (S21(f) を掛けて折り返す)
public:

private slots:
    void onEdited();  // apply() + 結果表の再計算

private:
    void recompute();          // 断面 → Z₀ / γ / S の結果表
    void exportTouchstone();   // 周波数掃引して .s2p を書く

    Project *m_p;
    bool     m_updating = false;

    // 断面
    QComboBox      *m_kind;
    QStackedWidget *m_geom;      // 種別ごとの寸法入力
    QDoubleSpinBox *m_w, *m_h;          // マイクロストリップ W / h
    QDoubleSpinBox *m_slW, *m_slB;      // ストリップライン W / b
    QDoubleSpinBox *m_a, *m_b;          // 同軸 a / b
    QDoubleSpinBox *m_d, *m_dia;        // 平行 2 線 D / d
    QDoubleSpinBox *m_cpwS, *m_slot;    // CPW S / スロット
    QDoubleSpinBox *m_epsr, *m_tanD, *m_sigma;
    QDoubleSpinBox *m_length, *m_freq, *m_z0Ref;

    // Z₀ / γ / S の表示選択
    QComboBox *m_z0Method;              // 準 TEM では 3 定義が一致 (無効化)
    QCheckBox *m_z0FreqDep, *m_z0ReIm;
    QVector<QCheckBox *> m_gamma;       // β / v_p / v_g / α / ε_eff
    QSpinBox  *m_ports;
    QVector<QCheckBox *> m_spara;       // S 振幅位相 / IL / RL / 群遅延 / .s2p
    QVector<QCheckBox *> m_disc;        // 不連続部 (未実装)
    // アイダイアグラム (core/EyeDiagram)
    QCheckBox      *m_eyeShow = nullptr;
    QDoubleSpinBox *m_eyeRate = nullptr;
    QSpinBox       *m_eyePrbs = nullptr;
    QDoubleSpinBox *m_eyeRise = nullptr;
    MiniPlot       *m_eyePlot = nullptr;
    QLabel         *m_eyeNote = nullptr;

    QPushButton  *m_s2pBtn;
    QTableWidget *m_table;
    QLabel       *m_note;
};

} // namespace ofd
