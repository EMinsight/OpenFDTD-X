// SoundproofTab.h — 防音・遮音設計タブ (soundproof.jsx 相当)。
// 8つの解析シナリオをカードで切替える:
//   間仕切壁 (Airborne)  — 壁構成テーブル + 質量則/コインシデンスの R(f) カーブ
//                          + Rw/STC/C/Ctr/DnT,w シングルナンバー評価
//   外壁・窓 (Facade)    — 交通騒音スペクトル C_tr と室内 SPL 予測
//   床衝撃音 (Impact)    — 床構成 + タッピングマシン → Ln,w / IIC / JIS 等級
//   側路伝搬 (Flanking)  — Dd/Ff/Df/Fd 経路合成 R'w と改善案
//   ダクト・配管音 / 設備機器囲い (挿入損失 IL) / 室内残響対策 / 会話プライバシー
// 音響ドメイン選択時のみ表示。モックは静的プロトタイプのため、
// 設定はローカル state (メンバ既定値) のみで Project へは永続化しない。
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
