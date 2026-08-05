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

class QButtonGroup;
class QStackedWidget;

namespace ofd {

class Project;

class SoundproofTab : public QScrollArea {
    Q_OBJECT
public:
    explicit SoundproofTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();

private:
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
};

} // namespace ofd
