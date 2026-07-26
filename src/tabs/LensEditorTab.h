// LensEditorTab.h — レンズデータエディタ (optics-tabs.jsx LensEditorTab 相当)。
//   - Lens Data Editor: Zemax OpticStudio 風の面テーブル (初期値 Cooke triplet)。
//     ガラス欄は GlassCatalog から自動補完、行の挿入/削除、STO 行ハイライト。
//   - システム諸元 (入射瞳径 / 視野 / 波長サンプル / 座標系)
//   - Merit Function (FoM) オペランド表 + 最適化ボタン
//   - 解析プロット起動ボタン (Spot / Ray Fan / MTF …)
//   - 面テーブルから子午面 2D 光線追跡するレイアウトプレビュー
// 光ドメイン選択時のみ表示される。面データはローカル状態 (モック忠実)。
#pragma once
#include <QScrollArea>
#include <QString>
#include <QVector>

class QComboBox;
class QLineEdit;
class QTableWidget;

namespace ofd {

class Project;

// 1面ぶんの行データ (mock の rows と同フィールド。数値も文字列で保持し
// "Infinity" / "-" をそのまま表現する)
struct LensSurface {
    bool    enabled = true;
    QString type;      // OBJ / STD / STO / ASP / BIN / EVN / HOL / FRE / IMG
    QString R;         // 曲率半径 [mm] ("Infinity" 可)
    QString thick;     // 厚さ [mm]
    QString glass;     // ガラス名 / AIR / -
    QString semiD;     // 半径 [mm]
    QString conic;     // コーニック定数
    QString comment;
};

// 面テーブルから描くレンズ断面 + 子午光線追跡プレビュー (QPainter)
class LensLayoutView : public QWidget {
    Q_OBJECT
public:
    explicit LensLayoutView(QWidget *parent = nullptr);
    void setSystem(const QVector<LensSurface> &rows, double epd, double fieldDeg);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    struct Surf {
        double z = 0, R = 0, semiD = 7, conic = 0;
        double n2 = 1.0;         // 面の後ろ側の屈折率
        bool   plane = false, stop = false, image = false;
    };
    QVector<Surf> m_surfs;
    double m_epd = 12.0, m_field = 20.0;
};

class LensEditorTab : public QScrollArea {
    Q_OBJECT
public:
    explicit LensEditorTab(Project *project, QWidget *parent = nullptr);

private slots:
    void retrace();                 // 面テーブル + 諸元 → プレビュー再追跡

private:
    void rebuildTable();            // m_rows → QTableWidget (行挿入/削除後)
    void syncRowFromTable(int row); // QTableWidget → m_rows (セル編集後)
    void applyStopHighlight();      // STO 行の背景ハイライト

    Project      *m_p;
    bool          m_updating = false;
    QVector<LensSurface> m_rows;

    QTableWidget *m_table;
    QLineEdit    *m_epd, *m_field;
    QComboBox    *m_coord;
    LensLayoutView *m_layout;
};

} // namespace ofd
