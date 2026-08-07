// AntennaCharTab.h — アンテナ特性タブ (openfdtd-family.jsx AntennaCharTab 相当)。
// OpenFDTD ドキュメント §2.13「アンテナ特性」の評価指標を選ぶチェックリスト:
//   入力特性 (Z / VSWR / Γ) ・放射特性 (パターン / ゲイン / 効率) ・偏波 ・アレイ。
// The mock is a static prototype: every toggle lives in local state here, so
// nothing is written back to Project (no matching Opts field exists).
#pragma once
#include <QScrollArea>
#include <QVector>

class QCheckBox;

class QLabel;

namespace ofd {

class Project;
class SectionBox;

class AntennaCharTab : public QScrollArea {
    Q_OBJECT
public:
    explicit AntennaCharTab(Project *project, QWidget *parent = nullptr);

private:
    // 直近の計算結果 (給電点表 + 遠方界パターン) を CSV へ書き出す
    void exportCsv();
    QLabel *m_exportNote = nullptr;

    // 1セクション = チェックボックスの縦並び (モックの <Row><Check/></Row> 群)
    SectionBox *checkSection(QWidget *parent, const char *titleKey,
                             const char *const *keys, const bool *checked, int n,
                             QVector<QCheckBox *> *out);

    Project *m_p;

    QVector<QCheckBox *> m_input;       // 入力特性
    QVector<QCheckBox *> m_radiation;   // 放射特性
    QVector<QCheckBox *> m_polar;       // 偏波特性
    QVector<QCheckBox *> m_array;       // アレイ特性
};

} // namespace ofd
