// H5Reader.h — カーネルが出力した HDF5 結果ファイルの読み取り。
//
// ofd / obpm は作業ディレクトリへ time_series_data.h5 を書く:
//   obpm: /field/Ixz (Nz×Nx), /field/Efinal_r|_i, /field/n_out_r|_i (Ny×Nx),
//         /field/frames (nframes×Ny×Nx), /metadata/*
//   ofd/orcwa: /data%06d/E|H (1×NFreq2×NN×6) + /metadata/{Nx,Ny,Nz,
//         Ni,Nj,Nk,N0,NN,Xn,Yn,Zn,Freq2,...} — ノード番号は
//         n = Ni*i + Nj*j + Nk*k + N0 で空間へ展開できる (sol/solve.c)
// GUI はこれを読んで 2D 断面表示 (CenterPane) と H5 アニメタブに反映する。
// -DUSE_HDF5=ON のときのみ実働 (無効ビルドでは available() = false)。
#pragma once
#include <QString>
#include <QVector>

namespace ofd {

// データセット 1 個の情報 (列挙結果)
struct H5DatasetInfo {
    QString path;               // 例 "/field/Ixz"
    QVector<qlonglong> dims;    // 例 {200, 128}
    QString typeName;           // "float32" / "float64" / "int…" / "other"
};

// ofd 系の伝搬時系列 (断面のフレーム列) の情報
struct H5OfdSeriesInfo {
    int  frames = 0;
    int  rows = 0, cols = 0;      // 既定 (XY 断面) の Ny+1, Nx+1
    int  nx1 = 0, ny1 = 0, nz1 = 0;   // 各軸のノード数 (断面位置の範囲)
    bool instantaneous = false;   // true = /timeseries (瞬時値スナップショット)
                                  // false = /data%06d (DFT 蓄積の経過)
};

class H5Reader {
public:
    // ビルドに HDF5 読み取りが含まれるか (USE_HDF5)
    static bool available();

    // path が実際に HDF5 ファイルか (署名を見るだけ。開かない)。
    // 「拡張子が .h5 でも中身が違う」ケースを、HDF5 ライブラリを呼ぶ前に
    // はじくために使う。ライブラリに開かせると失敗のたびに 7 行の
    // HDF5-DIAG スタックが stderr に出て、しかも呼び出し側は同じファイルへ
    // 複数のデータセットを試すので同じ出力が何度も並ぶ。
    // HDF5 無効ビルドでは常に false。
    static bool isHdf5(const QString &path);

    // ファイル内の全データセットを列挙する
    static bool listDatasets(const QString &path,
                             QVector<H5DatasetInfo> &out,
                             QString *err = nullptr);

    // 2 次元データセットを double で読む (rows = dims[0], cols = dims[1])
    static bool read2D(const QString &path, const QString &dset,
                       QVector<double> &out, int &rows, int &cols,
                       QString *err = nullptr);

    // 2 次元データセットの一部だけを読む (ハイパースラブ)。
    // 全球水深グリッド (GEBCO/ETOPO は 43200×86400 = 数 GB) を丸ごと展開
    // しないために要る。範囲はデータセット内へクランプされ、実際に読めた
    // 大きさが rows/cols に返る。
    static bool read2DWindow(const QString &path, const QString &dset,
                             qlonglong row0, qlonglong col0,
                             int &rows, int &cols, QVector<double> &out,
                             QString *err = nullptr);

    // 1 次元データセットの一部だけを読む (座標軸 lat/lon 用)。
    static bool read1DWindow(const QString &path, const QString &dset,
                             qlonglong i0, int &count, QVector<double> &out,
                             QString *err = nullptr);

    // 3 次元データセット (frames×rows×cols) の 1 フレームを読む
    static bool readFrame(const QString &path, const QString &dset, int frame,
                          QVector<double> &out, int &rows, int &cols,
                          QString *err = nullptr);

    // 任意次元 (スカラー〜4D) のデータセットを double でフラット読みする。
    // スカラーは dims 空 + 要素 1。整数型は HDF5 の型変換で double になる
    static bool readAll(const QString &path, const QString &dset,
                        QVector<double> &out, QVector<qlonglong> &dims,
                        QString *err = nullptr);

    // ofd/orcwa レイアウトの空間再構成 — z 中央断面の |E| を作る
    // (rows = Ny+1, cols = Nx+1, 行 0 = +y 側)。2 レイアウトに対応:
    //  (a) 新 (OpenFDTD sol/outputHdf5.c): /freqdomain/E
    //      {NFreq2, Nx+1, Ny+1, Nz+1, 3, 2} — 既に空間形状。k 断面だけを
    //      ハイパースラブで読む。groupName = "freqdomain"
    //  (b) 旧 (orcwa 等 sol/solve.c): 最終 (最大反復番号の) /data%06d/E
    //      {1,NFreq2,NN,6} + /metadata の格子定数。ノード番号は
    //      n = Ni*i + Nj*j + Nk*k + N0、成分順 {Ex_r,Ey_r,Ez_r,Ex_i,Ey_i,Ez_i}。
    //      groupName = "data%06d"
    static bool readOfdMidSlice(const QString &path, QVector<double> &cells,
                                int &rows, int &cols,
                                QString *groupName = nullptr,
                                QString *err = nullptr);

    // ofd 系の伝搬時系列。comp は "E" か "H"。2 レイアウト対応:
    //  (a) 新: /timeseries/<comp> {Nt, Nx+1, Ny+1, Nz+1, 3} — 瞬時値
    //      スナップショット (hdf5 キーの interval / solver nout ごと)。
    //      ラベルは /timeseries/time (E) / time_H (H) の時刻 [s]
    //  (b) 旧: /data%06d/<comp> {1,NFreq2,NN,6} のグループ列 — DFT 蓄積の
    //      経過。ラベルはグループ名
    // 断面は axis (0=X→YZ 面, 1=Y→XZ 面, 2=Z→XY 面) と index (軸方向の
    // ノード位置。-1 = 中央。範囲外はクランプ) で選ぶ。振幅 (成分の RSS)
    // を行列で返す (行 0 は上側 = 第 2 軸の +側)
    static bool ofdSeriesInfo(const QString &path, const QString &comp,
                              H5OfdSeriesInfo &out, QString *err = nullptr);
    // 断面の面内 2 軸 (axis = 固定軸)。**readOfdSeriesFrame が返す行列の
    // 列 = uAxis、行 = vAxis (行 0 = vAxis の + 側)** という規約そのもの。
    // 3D シーンへ断面を置く側 (H5ViewerTab) も同じ対応を使う必要があるので、
    // 定義をここ 1 箇所に置く (2 箇所に書くと片方だけ直して図が転置する)。
    static void seriesSliceAxes(int axis, int *uAxis, int *vAxis)
    {
        if (axis < 0 || axis > 2) axis = 2;
        if (uAxis) *uAxis = (axis == 0) ? 1 : 0;
        if (vAxis) *vAxis = (axis == 2) ? 1 : 2;
    }

    // 伝搬時系列の各フレームの時刻 [s] を読む (/timeseries/time、H 成分は
    // time_H)。旧 /data%06d 形式には時刻データセットが無いので false を
    // 返す — その場合、時間範囲での絞り込みはできない (推測しない)。
    static bool readOfdSeriesTimes(const QString &path, const QString &comp,
                                   QVector<double> &out, QString *err = nullptr);

    static bool readOfdSeriesFrame(const QString &path, const QString &comp,
                                   int frame, int axis, int index,
                                   QVector<double> &cells,
                                   int &rows, int &cols,
                                   QString *label = nullptr,
                                   QString *err = nullptr);

    // ofd 系 HDF5 の節点座標 [m]。新 (/geometry/Xn,Yn,Zn) と
    // 旧 (/metadata/Xn,Yn,Zn) の両方に対応する (軸ごとに新 → 旧の順で探す)。
    // 断面を 3D 空間の正しい位置・寸法へ置くために使う。
    // 取得できない軸は空ベクタで返す (呼び出し側が「座標不明」を判断できる
    // ようにする)。1 軸も取れなければ false。
    static bool ofdGridCoords(const QString &path,
                              QVector<double> &xs, QVector<double> &ys,
                              QVector<double> &zs, QString *err = nullptr);
};

} // namespace ofd
