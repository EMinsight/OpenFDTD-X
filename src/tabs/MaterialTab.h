// MaterialTab.h — materials & lumped elements (物性値・集中定数タブ).
// Maps 1:1 to the "material =" and "load =" lines (+ rfeed on GeneralTab).
#pragma once
#include <QScrollArea>

class QLabel;
class QTableWidget;

namespace ofd {

class Project;
class SectionBox;

class MaterialTab : public QScrollArea {
    Q_OBJECT
public:
    explicit MaterialTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    void applyMaterials();
    void applyLoads();
    // ドメイン別の列 (電磁 εr/σ/μr/σm ↔ 音響 ρ/c/α/Z) と見出し
    bool isAcousticDomain() const;
    bool isOpticalDomain() const;
    void updateColumns();

    Project      *m_p;
    bool          m_updating = false;
    QTableWidget *m_mats;
    QTableWidget *m_loads;
    SectionBox   *m_matSection;
    SectionBox   *m_lumpedSection; // 集中定数素子 (.ofd の load — EM のみ表示)
    QLabel       *m_dispHint;    // 分散モデルの案内 (光ドメインのみ)
    QLabel       *m_libStatus;   // ライブラリ読込の状態表示
    QWidget      *m_libRowRi;    // RefractiveIndex.info 行 (光のみ表示)
    QWidget      *m_libRowAstm;  // ASTM 音響材料行 (音響/水中のみ表示)
};

} // namespace ofd
