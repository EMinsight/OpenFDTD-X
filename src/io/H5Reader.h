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

class H5Reader {
public:
    // ビルドに HDF5 読み取りが含まれるか (USE_HDF5)
    static bool available();

    // ファイル内の全データセットを列挙する
    static bool listDatasets(const QString &path,
                             QVector<H5DatasetInfo> &out,
                             QString *err = nullptr);

    // 2 次元データセットを double で読む (rows = dims[0], cols = dims[1])
    static bool read2D(const QString &path, const QString &dset,
                       QVector<double> &out, int &rows, int &cols,
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

    // ofd/orcwa レイアウトの空間再構成: 最終 (最大反復番号の)
    // /data%06d/E {1,NFreq2,NN,6} と /metadata の格子定数から z 中央断面の
    // |E| を作る (rows = Ny+1, cols = Nx+1, 行 0 = +y 側)。ノード番号は
    // n = Ni*i + Nj*j + Nk*k + N0 (カーネル sol/solve.c と同じ)。
    // 成分順 {Ex_r, Ey_r, Ez_r, Ex_i, Ey_i, Ez_i}。groupName には選ばれた
    // グループ名 ("data001000" 等) を返す
    static bool readOfdMidSlice(const QString &path, QVector<double> &cells,
                                int &rows, int &cols,
                                QString *groupName = nullptr,
                                QString *err = nullptr);
};

} // namespace ofd
