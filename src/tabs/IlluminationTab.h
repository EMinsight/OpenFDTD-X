// IlluminationTab.h — 照明光学・測色タブ (optical-applications.jsx IlluminationTab 相当)。
// 非結像光学系の配光設計と測色評価 (LightTools / Photopia 相当):
//   - 用途 (LED照明 / 車載ランプ / バックライト / 太陽光集光)
//   - 光源   : モデル (ランバート/レイデータ/LEDチップ) + レイデータ + スペクトル + 光束
//   - 光学系 : リフレクタ・TIRレンズ・拡散板・導光板・蛍光体散乱 + 表面特性
//   - 測光・測色: 全光束〜UGR までの指標判定表と配光ファイル書出
// 表示専用 (.ofd に対応フィールドが無いため状態はすべてローカル)。
#pragma once
#include <QScrollArea>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QTableWidget;

namespace ofd {

class Project;

class IlluminationTab : public QScrollArea {
    Q_OBJECT
public:
    explicit IlluminationTab(Project *project, QWidget *parent = nullptr);

private:
    Project      *m_p;

    // 上段
    QButtonGroup *m_app;            // 用途

    // 光源
    QButtonGroup *m_srcModel;       // ランバート面 / レイデータ / LEDチップ
    QLineEdit    *m_rayFile;
    QComboBox    *m_spectrum;
    QLineEdit    *m_flux;
    QLineEdit    *m_rays;

    // 光学系
    QCheckBox    *m_reflector;
    QCheckBox    *m_tirLens;
    QCheckBox    *m_diffuser;
    QCheckBox    *m_lightGuide;
    QCheckBox    *m_phosphor;
    QButtonGroup *m_surface;        // 鏡面 / 拡散 / BSDF実測 / ABGモデル

    // 測光・測色
    QTableWidget *m_photoTable;
};

} // namespace ofd
