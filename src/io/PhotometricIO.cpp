// PhotometricIO.cpp — IES LM-63 配光ファイルの読み書き (詳細は .h)
#include "PhotometricIO.h"
#include "../optics/IlluminationTrace.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

#include <cmath>

namespace ofd {

namespace {

constexpr double kPi = 3.14159265358979323846;

QString num(double v)
{
    // IES は自由形式。桁は 6 桁有効で足り、指数表記も許される
    return QString::number(v, 'g', 6);
}

// 空白区切りの数値を必要な個数だけ読む。足りなければ false。
bool readNumbers(QTextStream &in, int count, QVector<double> *out, QString *err)
{
    out->clear();
    out->reserve(count);
    while (out->size() < count) {
        if (in.atEnd()) {
            if (err) *err = QStringLiteral("IES: 数値が足りません (期待 %1 個, 実際 %2 個)")
                                .arg(count).arg(out->size());
            return false;
        }
        const QString line = in.readLine();
        const QStringList tok = line.split(QRegularExpression("[\\s,]+"),
                                           Qt::SkipEmptyParts);
        for (const QString &t : tok) {
            bool ok = false;
            const double v = t.toDouble(&ok);
            if (!ok) {
                if (err) *err = QStringLiteral("IES: 数値として読めません: %1").arg(t);
                return false;
            }
            out->push_back(v);
            if (out->size() > count) {
                if (err) *err = QStringLiteral("IES: 数値が多すぎます");
                return false;
            }
        }
    }
    return true;
}

} // namespace

bool PhotometricIO::writeIes(const QString &path, const PhotometricData &d,
                             QString *err)
{
    if (d.isEmpty()) {
        if (err) *err = QStringLiteral("IES: 配光データが空です");
        return false;
    }
    if (d.candela.size() != d.horizAngles_deg.size()) {
        if (err) *err = QStringLiteral("IES: 水平角の数と配光の面数が合いません");
        return false;
    }
    for (const QVector<double> &row : d.candela)
        if (row.size() != d.vertAngles_deg.size()) {
            if (err) *err = QStringLiteral("IES: 鉛直角の数と光度の数が合いません");
            return false;
        }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = f.errorString();
        return false;
    }
    QTextStream out(&f);

    out << "IESNA:LM-63-2002\n";
    auto kw = [&out](const char *key, const QString &v) {
        if (!v.isEmpty()) out << "[" << key << "] " << v << "\n";
    };
    kw("TEST", d.test);
    kw("TESTLAB", d.testLab);
    kw("ISSUEDATE", d.issueDate);
    kw("MANUFAC", d.manufacturer);
    kw("LUMCAT", d.lumCat);
    kw("LUMINAIRE", d.luminaire);
    kw("LAMP", d.lamp);
    for (const QString &m : d.more)
        if (!m.isEmpty()) out << "[MORE] " << m << "\n";
    out << "TILT=NONE\n";

    // ランプ数 / ランプ光束 / 倍率 / 鉛直角数 / 水平角数 / 測光型 / 単位 / 幅 / 長さ / 高さ
    out << d.lamps << " " << num(d.lumensPerLamp) << " 1 "
        << d.vertAngles_deg.size() << " " << d.horizAngles_deg.size()
        << " 1 2 " << num(d.width) << " " << num(d.length) << " "
        << num(d.height) << "\n";
    // バラスト係数 / 将来用 / 入力電力
    out << num(d.ballastFactor) << " 1 " << num(d.inputWatts) << "\n";

    auto writeRow = [&out](const QVector<double> &v) {
        for (int i = 0; i < v.size(); ++i) {
            out << num(v[i]);
            // 1 行が長くなりすぎないよう 12 個ごとに折る (自由形式なので任意)
            out << (((i + 1) % 12 == 0 || i + 1 == v.size()) ? "\n" : " ");
        }
    };
    writeRow(d.vertAngles_deg);
    writeRow(d.horizAngles_deg);
    for (const QVector<double> &row : d.candela) writeRow(row);

    out.flush();
    f.close();
    return true;
}

bool PhotometricIO::readIes(const QString &path, PhotometricData *d, QString *err)
{
    if (!d) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = f.errorString();
        return false;
    }
    QTextStream in(&f);
    PhotometricData r;

    // ── ヘッダ: TILT= が出るまで ────────────────────────────────────────────
    bool sawTilt = false;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.startsWith(QStringLiteral("TILT="), Qt::CaseInsensitive)) {
            const QString mode = line.mid(5).trimmed();
            if (mode.compare(QStringLiteral("NONE"), Qt::CaseInsensitive) != 0) {
                // TILT=INCLUDE / <filename> は傾斜補正表を伴う。黙って無視すると
                // 光度が実際と食い違うので受け取らない (絶対規則 5)
                if (err) *err = QStringLiteral("IES: TILT=%1 は未対応です "
                                               "(TILT=NONE のみ)").arg(mode);
                return false;
            }
            sawTilt = true;
            break;
        }
        if (!line.startsWith('[')) continue;
        const int close = line.indexOf(']');
        if (close < 0) continue;
        const QString key = line.mid(1, close - 1).toUpper();
        const QString val = line.mid(close + 1).trimmed();
        if (key == QStringLiteral("TEST")) r.test = val;
        else if (key == QStringLiteral("TESTLAB")) r.testLab = val;
        else if (key == QStringLiteral("ISSUEDATE")) r.issueDate = val;
        else if (key == QStringLiteral("MANUFAC")) r.manufacturer = val;
        else if (key == QStringLiteral("LUMCAT")) r.lumCat = val;
        else if (key == QStringLiteral("LUMINAIRE")) r.luminaire = val;
        else if (key == QStringLiteral("LAMP")) r.lamp = val;
        else if (key == QStringLiteral("MORE")) r.more.push_back(val);
    }
    if (!sawTilt) {
        if (err) *err = QStringLiteral("IES: TILT= 行がありません");
        return false;
    }

    // ── 10 個の諸元 + 3 個の電気諸元 ───────────────────────────────────────
    QVector<double> a;
    if (!readNumbers(in, 10, &a, err)) return false;
    r.lamps = static_cast<int>(a[0]);
    r.lumensPerLamp = a[1];
    const double mult = a[2];
    const int nv = static_cast<int>(a[3]);
    const int nh = static_cast<int>(a[4]);
    const int photoType = static_cast<int>(a[5]);
    r.width = a[7]; r.length = a[8]; r.height = a[9];
    if (nv <= 0 || nh <= 0) {
        if (err) *err = QStringLiteral("IES: 角度の数が 0 以下です");
        return false;
    }
    if (photoType != 1) {
        // Type B / A は座標系そのものが違う。読み替えずに断る
        if (err) *err = QStringLiteral("IES: 測光型 %1 は未対応です (Type C のみ)")
                            .arg(photoType);
        return false;
    }
    QVector<double> b;
    if (!readNumbers(in, 3, &b, err)) return false;
    r.ballastFactor = b[0];
    r.inputWatts = b[2];

    if (!readNumbers(in, nv, &r.vertAngles_deg, err)) return false;
    if (!readNumbers(in, nh, &r.horizAngles_deg, err)) return false;
    r.candela.resize(nh);
    for (int h = 0; h < nh; ++h) {
        QVector<double> row;
        if (!readNumbers(in, nv, &row, err)) return false;
        // 倍率を適用して実光度にする (ファイル中の値は倍率適用前)
        for (double &v : row) v *= mult;
        r.candela[h] = row;
    }

    *d = r;
    return true;
}

double PhotometricIO::integratedFlux(const PhotometricData &d)
{
    if (d.isEmpty()) return 0.0;
    const int nv = d.vertAngles_deg.size();
    const int nh = d.horizAngles_deg.size();

    // 鉛直方向の境界 = 隣り合う角度の中点 (端は 0° / 180°)
    QVector<double> edge(nv + 1);
    edge[0] = 0.0;
    edge[nv] = 180.0;
    for (int k = 1; k < nv; ++k)
        edge[k] = 0.5 * (d.vertAngles_deg[k - 1] + d.vertAngles_deg[k]);

    double flux = 0.0;
    for (int h = 0; h < nh; ++h) {
        // 水平方向の割り当て: C 平面 1 枚なら全周、複数なら等分
        const double frac = 1.0 / nh;
        for (int k = 0; k < nv; ++k) {
            const double a0 = edge[k] * kPi / 180.0;
            const double a1 = edge[k + 1] * kPi / 180.0;
            const double omega = 2.0 * kPi * (std::cos(a0) - std::cos(a1));
            flux += d.candela[h][k] * omega * frac;
        }
    }
    return flux;
}

PhotometricData PhotometricIO::fromTrace(const illum::Result &r,
                                         double lampLumens)
{
    PhotometricData d;
    if (!r.valid || r.intensity_cd.empty()) return d;

    const int n = static_cast<int>(r.intensity_cd.size());
    const double dth = 180.0 / n;

    d.lamps = 1;
    d.lumensPerLamp = lampLumens;
    d.horizAngles_deg = { 0.0 };          // 軸対称 (C 平面 1 枚 = 全周)
    d.vertAngles_deg.reserve(n);
    QVector<double> row;
    row.reserve(n);
    for (int k = 0; k < n; ++k) {
        d.vertAngles_deg.push_back(dth * (k + 0.5));   // ビン中心
        row.push_back(r.intensity_cd[static_cast<size_t>(k)]);
    }
    d.candela = { row };

    // 値の出所をファイルに残す (受け取った側が読み方を誤らないように)
    d.more.push_back(QStringLiteral(
        "Computed by OpenFDTD-X non-sequential Monte Carlo ray trace "
        "(%1 rays), not a measurement.").arg(r.rays));
    d.more.push_back(QStringLiteral(
        "Vertical angles are bin centres; each value is the mean intensity "
        "over its %1 deg bin.").arg(QString::number(dth, 'g', 4)));
    d.more.push_back(QStringLiteral(
        "Source flux %1 lm, flux leaving the system %2 lm "
        "(light output ratio %3).")
        .arg(QString::number(r.fluxIn_lm, 'g', 6),
             QString::number(r.fluxOut_lm, 'g', 6),
             QString::number(r.efficiency, 'g', 4)));
    return d;
}

} // namespace ofd
