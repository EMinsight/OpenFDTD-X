// BathymetryIO.h — 緯度経度で与えた測点から伝搬経路に沿った海底地形断面を
// 作る (ローカルに配置した水深データセットから)。
//
// OceanEnvironmentTab の「地形断面」と BellhopIO の .bty はここが出所になる。
// 対応形式 (いずれも外部ライブラリ無しで読む):
//
//   .asc / .grd  Esri ASCII Grid — GEBCO / ETOPO の「ユーザー指定領域」
//                ダウンロードで選べるテキスト形式。ヘッダ (ncols nrows
//                xllcorner yllcorner cellsize NODATA_value) + 行列。
//   .xyz / .csv  lon lat 標高 の 3 列テキスト (等間隔格子であること)。
//   .nc          netCDF-4 (= HDF5) — GEBCO_2024.nc / ETOPO_*.nc。
//                座標 lat/lon (または y/x)、値 elevation (または z/Band1)。
//                **全球グリッドは数 GB あるので必要な窓だけを
//                H5Reader::read2DWindow で読む** (丸ごと展開しない)。
//                USE_HDF5=ON のビルドでのみ対応 (OFF では理由を返す)。
//   .bty         BELLHOP の地形ファイルそのもの (距離[km] 水深[m])。
//                緯度経度を持たないので断面としてそのまま取り込む。
//
// 符号の規約: 内部では **水深 [m] を正** で扱う。標高系のデータ
// (GEBCO/ETOPO は海面下が負) は読み込み時に符号を反転する。陸域
// (水深 <= 0) は欠測として扱い、断面からは落とす。
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

#include "../core/Project.h"   // BathyPoint

namespace ofd {

// 測地座標 (度)
struct GeoPoint {
    double lat_deg = 0.0;
    double lon_deg = 0.0;
};

// 球面 (半径 6371 km) 上で from から方位 bearing へ dist_km 進んだ点。
// 方位は真北 0°・時計回り。
GeoPoint geoDestination(const GeoPoint &from, double bearing_deg,
                        double dist_km);

// 2 点間の大圏距離 [km]
double geoDistanceKm(const GeoPoint &a, const GeoPoint &b);

// 緯度経度の等間隔ラスタ (読み込んだ窓のみを保持する)
struct BathyGrid {
    int    ncols = 0, nrows = 0;
    double lon0_deg = 0.0, lat0_deg = 0.0;   // [0][0] セルの中心
    double dLon_deg = 0.0, dLat_deg = 0.0;   // 正なら東/北へ増える
    QVector<float> depth_m;   // nrows*ncols、水深 [m] 正。欠測は NaN
    QString source;           // 出所 (ファイル名) — 断面の記録に使う

    bool isValid() const
    {
        return ncols >= 2 && nrows >= 2
               && depth_m.size() == qsizetype(nrows) * ncols;
    }
    // 双線形補間。範囲外・欠測を含むと NaN
    double sampleDepth(double lat_deg, double lon_deg) const;
};

class BathymetryIO {
public:
    // 対応形式か (拡張子で判定)
    static bool isSupported(const QString &path);

    // path から緯度経度の窓 [latMin,latMax]×[lonMin,lonMax] を読む。
    // 窓は余裕を持って読むこと (経路がグリッド境界をまたぐため)。
    static bool readGrid(const QString &path, double latMin, double latMax,
                         double lonMin, double lonMax, BathyGrid &out,
                         QString *err = nullptr);

    // BELLHOP .bty をそのまま断面として読む (緯度経度なし)
    static bool readBty(const QString &path, QVector<BathyPoint> &out,
                        QString *err = nullptr);

    // データセットフォルダ以下から読める水深ファイルを列挙する
    // (サイズの大きい順 — 全球グリッドを優先したいため)
    static QStringList findGrids(const QString &dir);

    // 測点から方位 bearing へ rangeMax_km まで n 点サンプリングして断面にする。
    // 欠測 (陸域・範囲外) の点は落とすので、返る点数は n 以下。
    static QVector<BathyPoint> sampleTrack(const BathyGrid &g,
                                           const GeoPoint &site,
                                           double bearing_deg,
                                           double rangeMax_km, int n);

    // データが無いときの合成断面 (従来の表示式と同じ)。
    // **実データではない**ことを呼び出し側が必ず表示すること。
    static QVector<BathyPoint> syntheticTrack(double depth_m,
                                              double rangeMax_km, int n);
};

} // namespace ofd
