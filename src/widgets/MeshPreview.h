// MeshPreview.h — メッシュ表示 (app.jsx MeshPreview 相当)。
//
// 実際の xmesh/ymesh/zmesh から選択平面のグリッド線を描く。5本ごとに
// 太線 (モックと同じ強調)。物体形状ユニットは点線の矩形で重ね描きする。
#pragma once
#include <QWidget>

namespace ofd {

class Project;

class MeshPreview : public QWidget {
    Q_OBJECT
public:
    // plane: 0 = XY, 1 = YZ, 2 = ZX
    explicit MeshPreview(Project *project, QWidget *parent = nullptr);

    void setPlane(int plane) { m_plane = plane; update(); }
    int  plane() const { return m_plane; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Project *m_p;
    int      m_plane = 0;
};

} // namespace ofd
