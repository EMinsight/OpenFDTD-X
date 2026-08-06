// BathymetryIO.cpp
#include "BathymetryIO.h"
#include "H5Reader.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace ofd;

namespace {

const double kEarthR_km = 6371.0;
const double kDeg = M_PI / 180.0;

void setErr(QString *err, const QString &msg) { if (err) *err = msg; }

double nan_() { return std::numeric_limits<double>::quiet_NaN(); }

// 標高 (海面下が負) → 水深 (正)。陸域・欠測は NaN
float elevToDepth(double elev, double nodata)
{
    if (!std::isfinite(elev)) return float(nan_());
    if (nodata != 0.0 && std::abs(elev - nodata) < 1e-6) return float(nan_());
    const double d = -elev;
    return (d > 0.0) ? float(d) : float(nan_());
}

} // namespace

GeoPoint ofd::geoDestination(const GeoPoint &from, double bearing_deg,
                             double dist_km)
{
    const double d = dist_km / kEarthR_km;
    const double th = bearing_deg * kDeg;
    const double la1 = from.lat_deg * kDeg, lo1 = from.lon_deg * kDeg;
    const double la2 = std::asin(std::sin(la1) * std::cos(d)
                                 + std::cos(la1) * std::sin(d) * std::cos(th));
    const double lo2 = lo1 + std::atan2(std::sin(th) * std::sin(d) * std::cos(la1),
                                        std::cos(d) - std::sin(la1) * std::sin(la2));
    GeoPoint p;
    p.lat_deg = la2 / kDeg;
    // 経度を [-180, 180] へ正規化
    double lon = std::fmod(lo2 / kDeg + 540.0, 360.0) - 180.0;
    p.lon_deg = lon;
    return p;
}

double ofd::geoDistanceKm(const GeoPoint &a, const GeoPoint &b)
{
    const double la1 = a.lat_deg * kDeg, la2 = b.lat_deg * kDeg;
    const double dla = la2 - la1;
    const double dlo = (b.lon_deg - a.lon_deg) * kDeg;
    const double h = std::sin(dla / 2) * std::sin(dla / 2)
                   + std::cos(la1) * std::cos(la2)
                         * std::sin(dlo / 2) * std::sin(dlo / 2);
    return 2.0 * kEarthR_km * std::asin(std::min(1.0, std::sqrt(h)));
}

double BathyGrid::sampleDepth(double lat_deg, double lon_deg) const
{
    if (!isValid() || dLon_deg == 0.0 || dLat_deg == 0.0) return nan_();
    const double fx = (lon_deg - lon0_deg) / dLon_deg;
    const double fy = (lat_deg - lat0_deg) / dLat_deg;
    if (fx < 0.0 || fy < 0.0 || fx > ncols - 1.0 || fy > nrows - 1.0)
        return nan_();
    const int i0 = int(std::floor(fx)), j0 = int(std::floor(fy));
    const int i1 = std::min(i0 + 1, ncols - 1), j1 = std::min(j0 + 1, nrows - 1);
    const double tx = fx - i0, ty = fy - j0;
    const double z00 = depth_m[qsizetype(j0) * ncols + i0];
    const double z10 = depth_m[qsizetype(j0) * ncols + i1];
    const double z01 = depth_m[qsizetype(j1) * ncols + i0];
    const double z11 = depth_m[qsizetype(j1) * ncols + i1];
    // 1 点でも欠測 (陸・NoData) なら補間しない — 海岸線を勝手に埋めないため
    if (!std::isfinite(z00) || !std::isfinite(z10) || !std::isfinite(z01)
        || !std::isfinite(z11))
        return nan_();
    return (z00 * (1 - tx) + z10 * tx) * (1 - ty)
         + (z01 * (1 - tx) + z11 * tx) * ty;
}

bool BathymetryIO::isSupported(const QString &path)
{
    const QString s = QFileInfo(path).suffix().toLower();
    return s == QLatin1String("asc") || s == QLatin1String("grd")
        || s == QLatin1String("xyz") || s == QLatin1String("csv")
        || s == QLatin1String("nc")  || s == QLatin1String("bty");
}

// ── Esri ASCII Grid ─────────────────────────────────────────────────────────
static bool readEsriAscii(const QString &path, double latMin, double latMax,
                          double lonMin, double lonMax, BathyGrid &out,
                          QString *errOut)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setErr(errOut, QStringLiteral("cannot open %1").arg(path));
        return false;
    }
    QTextStream in(&f);
    int ncols = 0, nrows = 0;
    double xll = 0, yll = 0, cell = 0, nodata = -9999;
    bool corner = true, haveX = false, haveY = false;
    // ヘッダ (6 行が定番だが、順序も行数も実装差があるのでキーで拾う)
    while (!in.atEnd()) {
        const qint64 pos = in.pos();
        const QString line = in.readLine();
        const QStringList t =
            line.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (t.size() < 2) continue;
        const QString k = t[0].toLower();
        bool ok = false;
        const double v = t[1].toDouble(&ok);
        if (!ok) continue;
        if      (k == QLatin1String("ncols")) ncols = int(v);
        else if (k == QLatin1String("nrows")) nrows = int(v);
        else if (k == QLatin1String("xllcorner")) { xll = v; corner = true;  haveX = true; }
        else if (k == QLatin1String("xllcenter")) { xll = v; corner = false; haveX = true; }
        else if (k == QLatin1String("yllcorner")) { yll = v; corner = true;  haveY = true; }
        else if (k == QLatin1String("yllcenter")) { yll = v; corner = false; haveY = true; }
        else if (k == QLatin1String("cellsize")) cell = v;
        else if (k.startsWith(QLatin1String("nodata"))) nodata = v;
        else { in.seek(pos); break; }   // ヘッダ終わり = 数値行の始まり
    }
    if (ncols < 2 || nrows < 2 || cell <= 0 || !haveX || !haveY) {
        setErr(errOut, QStringLiteral("%1: not an Esri ASCII grid header")
                           .arg(QFileInfo(path).fileName()));
        return false;
    }
    // セル中心の左下
    const double x0 = corner ? xll + cell / 2 : xll;
    const double y0 = corner ? yll + cell / 2 : yll;

    // 読み出す列・行の範囲 (1 セル分の余裕を付けて双線形補間を成立させる)
    const int c0 = std::max(0, int(std::floor((lonMin - x0) / cell)) - 1);
    const int c1 = std::min(ncols - 1, int(std::ceil((lonMax - x0) / cell)) + 1);
    // ASCII grid は北の行から並ぶ → 行 r の緯度 = y0 + (nrows-1-r)*cell
    const int rTop = std::max(0, int(std::floor((y0 + (nrows - 1) * cell - latMax) / cell)) - 1);
    const int rBot = std::min(nrows - 1, int(std::ceil((y0 + (nrows - 1) * cell - latMin) / cell)) + 1);
    if (c1 < c0 || rBot < rTop) {
        setErr(errOut, QStringLiteral("%1: the site is outside the grid")
                           .arg(QFileInfo(path).fileName()));
        return false;
    }

    out = BathyGrid();
    out.ncols = c1 - c0 + 1;
    out.nrows = rBot - rTop + 1;
    out.dLon_deg = cell;
    out.dLat_deg = cell;
    out.lon0_deg = x0 + c0 * cell;
    // 行 rBot (南) を出力の 0 行目にする (北向きに増える配列にする)
    out.lat0_deg = y0 + (nrows - 1 - rBot) * cell;
    out.depth_m.fill(float(nan_()), qsizetype(out.nrows) * out.ncols);
    out.source = QFileInfo(path).fileName();

    // 本体はホワイトスペース区切りで nrows*ncols 個並ぶ (改行位置は不定)
    int idx = 0;
    QString tok;
    const qsizetype total = qsizetype(nrows) * ncols;
    QChar ch;
    while (idx < total && !in.atEnd()) {
        in >> tok;
        if (tok.isEmpty()) break;
        const int r = int(idx / ncols), c = int(idx % ncols);
        if (r >= rTop && r <= rBot && c >= c0 && c <= c1) {
            const int orow = rBot - r;            // 南が 0 行目
            const int ocol = c - c0;
            out.depth_m[qsizetype(orow) * out.ncols + ocol] =
                elevToDepth(tok.toDouble(), nodata);
        }
        ++idx;
        (void)ch;
    }
    if (idx < total) {
        setErr(errOut, QStringLiteral("%1: truncated (%2 of %3 values)")
                           .arg(QFileInfo(path).fileName())
                           .arg(idx).arg(total));
        return false;
    }
    return true;
}

// ── XYZ / CSV (lon lat 標高) ────────────────────────────────────────────────
static bool readXyz(const QString &path, double latMin, double latMax,
                    double lonMin, double lonMax, BathyGrid &out,
                    QString *errOut)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setErr(errOut, QStringLiteral("cannot open %1").arg(path));
        return false;
    }
    // 窓の中の点だけ拾って等間隔格子を組み立てる
    struct P { double lon, lat, z; };
    QVector<P> pts;
    QTextStream in(&f);
    static const QRegularExpression sep(QStringLiteral("[,;\\s]+"));
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        const QStringList t = line.split(sep, Qt::SkipEmptyParts);
        if (t.size() < 3) continue;
        bool a = false, b = false, c = false;
        const double lon = t[0].toDouble(&a), lat = t[1].toDouble(&b),
                     z = t[2].toDouble(&c);
        if (!a || !b || !c) continue;   // ヘッダ行など
        if (lat < latMin || lat > latMax || lon < lonMin || lon > lonMax) continue;
        pts.push_back({ lon, lat, z });
    }
    if (pts.size() < 4) {
        setErr(errOut, QStringLiteral("%1: no points in the requested area")
                           .arg(QFileInfo(path).fileName()));
        return false;
    }
    QVector<double> lons, lats;
    for (const P &p : pts) { lons.push_back(p.lon); lats.push_back(p.lat); }
    std::sort(lons.begin(), lons.end());
    std::sort(lats.begin(), lats.end());
    lons.erase(std::unique(lons.begin(), lons.end()), lons.end());
    lats.erase(std::unique(lats.begin(), lats.end()), lats.end());
    if (lons.size() < 2 || lats.size() < 2) {
        setErr(errOut, QStringLiteral("%1: not a grid (needs >= 2x2 cells)")
                           .arg(QFileInfo(path).fileName()));
        return false;
    }
    out = BathyGrid();
    out.ncols = int(lons.size());
    out.nrows = int(lats.size());
    out.lon0_deg = lons.first();
    out.lat0_deg = lats.first();
    out.dLon_deg = (lons.last() - lons.first()) / (lons.size() - 1);
    out.dLat_deg = (lats.last() - lats.first()) / (lats.size() - 1);
    out.depth_m.fill(float(nan_()), qsizetype(out.nrows) * out.ncols);
    out.source = QFileInfo(path).fileName();
    for (const P &p : pts) {
        const int i = int(std::lround((p.lon - out.lon0_deg) / out.dLon_deg));
        const int j = int(std::lround((p.lat - out.lat0_deg) / out.dLat_deg));
        if (i < 0 || j < 0 || i >= out.ncols || j >= out.nrows) continue;
        out.depth_m[qsizetype(j) * out.ncols + i] = elevToDepth(p.z, -9999);
    }
    return true;
}

// ── netCDF-4 (= HDF5): GEBCO / ETOPO ────────────────────────────────────────
static bool readNetcdf4(const QString &path, double latMin, double latMax,
                        double lonMin, double lonMax, BathyGrid &out,
                        QString *errOut)
{
    if (!H5Reader::available()) {
        setErr(errOut, QStringLiteral(
                   "%1: netCDF-4 needs a HDF5-enabled build (USE_HDF5=ON)")
                   .arg(QFileInfo(path).fileName()));
        return false;
    }
    QVector<H5DatasetInfo> dsets;
    if (!H5Reader::listDatasets(path, dsets, errOut)) return false;

    // 座標軸と値の名前は配布元で違う (GEBCO: lat/lon/elevation,
    // ETOPO: lat/lon/z、GDAL 変換: y/x/Band1)。1 次元/2 次元の形で当てる。
    QString latDs, lonDs, zDs;
    qlonglong nLat = 0, nLon = 0;
    for (const H5DatasetInfo &d : dsets) {
        const QString n = d.path.section(QLatin1Char('/'), -1).toLower();
        if (d.dims.size() == 1) {
            if (latDs.isEmpty() && (n == QLatin1String("lat")
                                    || n == QLatin1String("latitude")
                                    || n == QLatin1String("y"))) {
                latDs = d.path; nLat = d.dims[0];
            }
            if (lonDs.isEmpty() && (n == QLatin1String("lon")
                                    || n == QLatin1String("longitude")
                                    || n == QLatin1String("x"))) {
                lonDs = d.path; nLon = d.dims[0];
            }
        } else if (d.dims.size() == 2 && zDs.isEmpty()
                   && (n == QLatin1String("elevation") || n == QLatin1String("z")
                       || n == QLatin1String("band1")
                       || n == QLatin1String("depth"))) {
            zDs = d.path;
        }
    }
    if (latDs.isEmpty() || lonDs.isEmpty() || zDs.isEmpty()) {
        setErr(errOut, QStringLiteral(
                   "%1: no lat/lon/elevation variables (not a GEBCO/ETOPO grid)")
                   .arg(QFileInfo(path).fileName()));
        return false;
    }
    // 座標軸は 1 次元なので全部読んでよい (全球 15 秒でも lat 43200 /
    // lon 86400 個 = 数百 KB。数 GB あるのは elevation だけ)。
    QVector<double> lat, lon;
    QVector<qlonglong> d1;
    if (!H5Reader::readAll(path, latDs, lat, d1, errOut)) return false;
    if (!H5Reader::readAll(path, lonDs, lon, d1, errOut)) return false;
    if (lat.size() < 2 || lon.size() < 2 || lat.size() != nLat
        || lon.size() != nLon) {
        setErr(errOut, QStringLiteral("%1: malformed lat/lon axes")
                           .arg(QFileInfo(path).fileName()));
        return false;
    }
    const double dLat = (lat.last() - lat.first()) / (lat.size() - 1);
    const double dLon = (lon.last() - lon.first()) / (lon.size() - 1);
    if (dLat == 0.0 || dLon == 0.0) {
        setErr(errOut, QStringLiteral("%1: degenerate lat/lon axes")
                           .arg(QFileInfo(path).fileName()));
        return false;
    }
    // 要求範囲 → 軸インデックス (軸が降順でも成り立つように両端を取る)
    auto indexOf = [](const QVector<double> &ax, double v) {
        const double step = (ax.last() - ax.first()) / (ax.size() - 1);
        return int(std::lround((v - ax.first()) / step));
    };
    int r0 = indexOf(lat, latMin), r1 = indexOf(lat, latMax);
    int c0 = indexOf(lon, lonMin), c1 = indexOf(lon, lonMax);
    if (r0 > r1) std::swap(r0, r1);
    if (c0 > c1) std::swap(c0, c1);
    r0 = std::max(0, r0 - 1); r1 = std::min(int(lat.size()) - 1, r1 + 1);
    c0 = std::max(0, c0 - 1); c1 = std::min(int(lon.size()) - 1, c1 + 1);
    if (r1 <= r0 || c1 <= c0) {
        setErr(errOut, QStringLiteral("%1: the site is outside the grid")
                           .arg(QFileInfo(path).fileName()));
        return false;
    }

    int rows = r1 - r0 + 1, cols = c1 - c0 + 1;
    QVector<double> z;
    if (!H5Reader::read2DWindow(path, zDs, r0, c0, rows, cols, z, errOut))
        return false;

    out = BathyGrid();
    out.ncols = cols;
    out.nrows = rows;
    out.dLon_deg = std::abs(dLon);
    out.dLat_deg = std::abs(dLat);
    out.lon0_deg = std::min(lon[c0], lon[c1]);
    out.lat0_deg = std::min(lat[r0], lat[r1]);
    out.depth_m.fill(float(nan_()), qsizetype(rows) * cols);
    out.source = QFileInfo(path).fileName();
    // 軸が降順なら向きを揃える (内部は常に東/北へ増える配列)
    for (int j = 0; j < rows; ++j) {
        const int jj = (dLat > 0) ? j : rows - 1 - j;
        for (int i = 0; i < cols; ++i) {
            const int ii = (dLon > 0) ? i : cols - 1 - i;
            out.depth_m[qsizetype(j) * cols + i] =
                elevToDepth(z[qsizetype(jj) * cols + ii], 0.0);
        }
    }
    return true;
}

bool BathymetryIO::readBty(const QString &path, QVector<BathyPoint> &out,
                           QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setErr(err, QStringLiteral("cannot open %1").arg(path));
        return false;
    }
    out.clear();
    QTextStream in(&f);
    static const QRegularExpression sep(QStringLiteral("[,;\\s]+"));
    int lineNo = 0;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        ++lineNo;
        if (line.isEmpty() || line.startsWith(QLatin1Char('!'))) continue;
        if (line.startsWith(QLatin1Char('\''))) continue;   // 補間種別 'L'
        const QStringList t = line.split(sep, Qt::SkipEmptyParts);
        if (t.size() < 2) continue;   // 点数だけの行
        bool a = false, b = false;
        const double r = t[0].toDouble(&a), d = t[1].toDouble(&b);
        if (a && b && d > 0.0) out.push_back({ r, d });
    }
    if (out.size() < 2) {
        setErr(err, QStringLiteral("%1: fewer than 2 bathymetry points")
                        .arg(QFileInfo(path).fileName()));
        return false;
    }
    return true;
}

bool BathymetryIO::readGrid(const QString &path, double latMin, double latMax,
                            double lonMin, double lonMax, BathyGrid &out,
                            QString *err)
{
    const QString s = QFileInfo(path).suffix().toLower();
    if (s == QLatin1String("asc") || s == QLatin1String("grd"))
        return readEsriAscii(path, latMin, latMax, lonMin, lonMax, out, err);
    if (s == QLatin1String("xyz") || s == QLatin1String("csv"))
        return readXyz(path, latMin, latMax, lonMin, lonMax, out, err);
    if (s == QLatin1String("nc"))
        return readNetcdf4(path, latMin, latMax, lonMin, lonMax, out, err);
    setErr(err, QStringLiteral("%1: unsupported bathymetry format")
                    .arg(QFileInfo(path).fileName()));
    return false;
}

QStringList BathymetryIO::findGrids(const QString &dir)
{
    QVector<QPair<qint64, QString>> found;
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (!isSupported(it.filePath())) continue;
        if (QFileInfo(it.filePath()).suffix().toLower() == QLatin1String("bty"))
            continue;   // 断面そのもの — グリッドではない
        found.push_back({ it.fileInfo().size(), it.filePath() });
    }
    std::sort(found.begin(), found.end(),
              [](const QPair<qint64, QString> &a,
                 const QPair<qint64, QString> &b) { return a.first > b.first; });
    QStringList out;
    for (const auto &p : found) out << p.second;
    return out;
}

QVector<BathyPoint> BathymetryIO::sampleTrack(const BathyGrid &g,
                                              const GeoPoint &site,
                                              double bearing_deg,
                                              double rangeMax_km, int n)
{
    QVector<BathyPoint> out;
    if (!g.isValid() || n < 2 || rangeMax_km <= 0.0) return out;
    for (int i = 0; i < n; ++i) {
        const double r = rangeMax_km * i / double(n - 1);
        const GeoPoint p = geoDestination(site, bearing_deg, r);
        const double d = g.sampleDepth(p.lat_deg, p.lon_deg);
        if (std::isfinite(d) && d > 0.0) out.push_back({ r, d });
    }
    return out;
}

QVector<BathyPoint> BathymetryIO::syntheticTrack(double depth_m,
                                                 double rangeMax_km, int n)
{
    QVector<BathyPoint> out;
    if (!(depth_m > 0.0) || n < 2 || rangeMax_km <= 0.0) return out;
    // OeBathyView の表示式と同じ (画面と .bty を食い違わせないため)
    for (int i = 0; i < n; ++i) {
        const double x = i / double(n - 1);
        const double d = depth_m * (0.75
                       + 0.25 * std::sin(x * 5.1) * std::sin(x * 2.3 + 1.0)
                       + 0.12 * x);
        out.push_back({ rangeMax_km * x, d });
    }
    return out;
}
