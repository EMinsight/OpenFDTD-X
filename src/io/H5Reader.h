// H5Reader.h — カーネルが出力した HDF5 結果ファイルの読み取り。
//
// ofd / obpm は作業ディレクトリへ time_series_data.h5 を書く:
//   obpm: /field/Ixz (Nz×Nx), /field/Efinal_r|_i, /field/n_out_r|_i (Ny×Nx),
//         /field/frames (nframes×Ny×Nx), /metadata/*
//   ofd:  /data%06d/E|H (1×NFreq2×NN×6, 空間へは未展開), /metadata/*
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
};

} // namespace ofd
