// MonitorsTab.h — モニタータブ (ansys-tabs.jsx MonitorsTab 相当)。
// Ansys Lumerical 風のモニター管理:
//   - モニター一覧表 (ドメイン別の既定行 / 名前は編集可)
//   - モニタータイプ追加グリッド (ドメインでフィルタされた 2 列ボタン)
//   - モニター設定 (レコーダ / apodization / サンプリング周波数)
// ドメイン切替で一覧・グリッド・設定行を再構築。
// サンプリング周波数のみ AcousticOpts::sampleRate に永続化。
#pragma once
#include <QScrollArea>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QTableWidget;

namespace ofd {

class Project;
class SectionBox;

class MonitorsTab : public QScrollArea {
    Q_OBJECT
public:
    explicit MonitorsTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    void apply();
    void rebuildDomain();   // ドメイン → 一覧/タイプ追加/設定行を再構築

    Project      *m_p;
    bool          m_updating = false;

    QTableWidget *m_list;
    SectionBox   *m_addSection;
    QWidget      *m_addHost;      // タイプ追加グリッドの入れ物 (再構築対象)

    SectionBox   *m_settings;
    QCheckBox    *m_syncAuto;
    QCheckBox    *m_recPhase, *m_recAmp, *m_recDft;
    QWidget      *m_apodRow;      // EM/光のみ表示
    QComboBox    *m_apod;
    QWidget      *m_srateRow;     // 音響/水中のみ表示
    QLineEdit    *m_srate;
};

} // namespace ofd
