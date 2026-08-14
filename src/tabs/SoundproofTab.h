// SoundproofTab.h — 防音・遮音設計タブ (soundproof.jsx 相当)。
// 8つの解析シナリオをカードで切替える:
//   間仕切壁 (Airborne)  — 壁構成テーブル → 質量則/コインシデンスの R(f)
//                          + Rw/STC/C/Ctr/DnT,w (ISO 717-1 / ASTM E413)
//   外壁・窓 (Facade)    — 複合 R と室内 Lp2 = Lp1 − R + 10log10(S/A)
//   床衝撃音 (Impact)    — 素床 Ln,w − ΔLw のみ計算 (EN 12354-2 の予測は未実装、
//                          IIC / JIS 等級はスペクトルが要るため未計算)
//   側路伝搬 (Flanking)  — Dd/Ff/Df/Fd 経路合成 R'w (経路別 R は入力値)
//   ダクト・配管音       — ASHRAE の減衰式 → 帯域別 Lp → dB(A) / NC
//   設備機器囲い         — 挿入損失 IL = R_eff − 10log10(S/A_in) (ISO 11546)
//   室内残響対策         — Sabine RT60 (core/RoomAcoustics)
//   会話プライバシー     — STI (IEC 60268-16 の MTF 法)
// 物理計算はすべて Qt 非依存の src/acoustics/core/SoundInsulation にあり、
// selftest から規格の不変量と直接突き合わせている。このタブは入力の収集と
// 表示だけを行う。設定はローカル state のみで Project へは永続化しない。
// 音響ドメイン選択時のみ表示。
#pragma once
#include <QScrollArea>
#include <QVector>

#include "../io/BandSpectrumCsv.h"

class QButtonGroup;
class QStackedWidget;

class QLineEdit;
class QLabel;

namespace ofd {

class Project;

class SoundproofTab : public QScrollArea {
    Q_OBJECT
public:
    explicit SoundproofTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    // .dxf から仕切壁面積 S を読む (io/DxfOutline)。単位と輪郭は利用者が選ぶ
    void importDxfArea(QLineEdit *areaEdit, QLabel *status);
    // 表示中のシナリオの帯域スペクトルを CSV で書き出す。**画面に出ている
    // 曲線そのもの**を書く (再計算しないので図と必ず一致する)
    void exportSpectrumCsv();

    QWidget *buildPartitionPage();
    QWidget *buildFacadePage();
    QWidget *buildFloorPage();
    QWidget *buildFlankingPage();
    QWidget *buildDuctPage();
    QWidget *buildMachinePage();
    QWidget *buildReverbPage();
    QWidget *buildSpeechPage();

    Project        *m_p;
    bool            m_updating = false;
    int             m_scenario = 0;   // mock: useState("partition") のローカル state
    QButtonGroup   *m_scenarioGroup;
    QStackedWidget *m_stack;
    // シナリオ毎の最新の帯域スペクトル (添字は m_stack のページ番号)。
    // 曲線を描くたびに更新し、書出はここから読む。曲線が引けない入力では
    // isValid() が false になり、書出は「まだ計算されていない」と言う
    QVector<io::BandSpectrum> m_spectra;
};

} // namespace ofd
