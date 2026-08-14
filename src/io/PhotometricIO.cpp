// PhotometricIO.cpp — IES LM-63 配光ファイルの読み書き (詳細は .h)
#include "PhotometricIO.h"
#include "../optics/IlluminationTrace.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
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

// EULUMDAT の文字欄。改行を含めると行番号がずれるので潰し、桁も詰める
QString ldtText(const QString &s, int maxChars)
{
    QString t = s;
    t.replace('\r', ' ');
    t.replace('\n', ' ');
    t = t.trimmed();
    return t.left(maxChars);
}

// 昇順・等間隔なら間隔を、そうでなければ 0 を返す (EULUMDAT の Dc / Dg)
double uniformStep(const QVector<double> &a)
{
    if (a.size() < 2) return 0.0;
    const double s = a[1] - a[0];
    if (s <= 0.0) return 0.0;
    for (int i = 1; i < a.size(); ++i)
        if (std::fabs((a[i] - a[i - 1]) - s) > 1e-9 * std::max(1.0, std::fabs(s)))
            return 0.0;
    return s;
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

bool PhotometricIO::writeLdt(const QString &path, const PhotometricData &d,
                             QString *err)
{
    if (d.isEmpty()) {
        if (err) *err = QStringLiteral("EULUMDAT: 配光データが空です");
        return false;
    }
    if (d.candela.size() != d.horizAngles_deg.size()) {
        if (err) *err = QStringLiteral("EULUMDAT: 水平角の数と配光の面数が合いません");
        return false;
    }
    for (const QVector<double> &row : d.candela)
        if (row.size() != d.vertAngles_deg.size()) {
            if (err) *err = QStringLiteral("EULUMDAT: 鉛直角の数と光度の数が合いません");
            return false;
        }

    const int nv = d.vertAngles_deg.size();
    const int nh = d.horizAngles_deg.size();

    // ── 基準光束 (cd/1000lm への正規化にこれが要る) ────────────────────────
    int    lampsOut = (d.lamps > 0) ? d.lamps : 1;
    double refFlux  = 0.0;
    if (d.lumensPerLamp > 0.0) {
        refFlux = lampsOut * d.lumensPerLamp;
    } else {
        // 絶対測光 (IES の負値) — 配光自身の光束を基準にする (LORL = 100%)
        refFlux  = integratedFlux(d);
        lampsOut = 1;
    }
    if (!(refFlux > 0.0)) {
        if (err) *err = QStringLiteral(
            "EULUMDAT: 基準光束が 0 です (cd/1000lm に正規化できません)");
        return false;
    }

    // ── 対称指定 ──────────────────────────────────────────────────────────
    // 書き出すのは Isym = 1 (軸対称) と 0 (対称性なし) だけ。0 は「並んでいる
    // C 平面が全周をおおう」ことが前提なので、そうでない配光は断る (絶対規則 5)
    int    isym = 1, ityp = 1, mc = 24;
    double dc = 15.0;
    QVector<double> cAngles;
    if (nh == 1) {
        // 軸対称 — C 平面は 24 枚あるものとして角度だけ並べ、配光は 1 枚書く
        for (int i = 0; i < mc; ++i) cAngles.push_back(dc * i);
    } else {
        const double step = uniformStep(d.horizAngles_deg);
        const double full = 360.0 / nh;
        if (std::fabs(d.horizAngles_deg.first()) > 1e-9 ||
            step <= 0.0 || std::fabs(step - full) > 1e-6 * full) {
            if (err) *err = QStringLiteral(
                "EULUMDAT: C 平面が 0° から等間隔で全周をおおっていません "
                "(%1 枚, 先頭 %2°)。鏡映の解釈を推測しないため書き出しません")
                .arg(nh).arg(d.horizAngles_deg.first());
            return false;
        }
        isym = 0;
        ityp = 3;              // 鉛直軸まわりの対称性を持たない点光源
        mc   = nh;
        dc   = full;
        cAngles = d.horizAngles_deg;
    }

    // 書く面数と手持ちの面数が食い違ったまま進むと配光を範囲外まで読む。
    // ここで必ず突き合わせる (対称指定を足すときの安全網)
    const int planes = (isym == 1) ? 1 : mc;
    if (planes > d.candela.size()) {
        if (err) *err = QStringLiteral(
            "EULUMDAT: 書こうとした面数 (%1) が配光の面数 (%2) を超えています")
            .arg(planes).arg(d.candela.size());
        return false;
    }

    const double totalFlux = integratedFlux(d);
    const double dff  = (totalFlux > 0.0)
                            ? 100.0 * partialFlux(d, 0.0, 90.0) / totalFlux : 0.0;
    const double lorl = 100.0 * totalFlux / refFlux;

    QFile f(path);
    // 仕様どおり CRLF で書くため Text 変換は使わない (二重変換の防止)
    if (!f.open(QIODevice::WriteOnly)) {
        if (err) *err = f.errorString();
        return false;
    }
    QTextStream out(&f);
    auto line = [&out](const QString &s) { out << s << "\r\n"; };
    auto lineD = [&line](double v) { line(num(v)); };

    // 1〜7
    line(ldtText(d.manufacturer.isEmpty() ? QStringLiteral("OpenFDTD-X")
                                          : d.manufacturer, 78));
    line(QString::number(ityp));
    line(QString::number(isym));
    line(QString::number(mc));
    lineD(dc);
    line(QString::number(nv));
    lineD(uniformStep(d.vertAngles_deg));
    // 8〜12 (8 = 測定報告番号。値の出所をここに残す — 注記欄が無い形式なので)
    const QString provenance = !d.test.isEmpty()
                                   ? d.test
                                   : (d.more.isEmpty() ? QString() : d.more.first());
    line(ldtText(provenance, 78));
    line(ldtText(d.luminaire, 78));
    line(ldtText(d.lumCat, 78));
    line(ldtText(QFileInfo(path).completeBaseName(), 8));
    line(ldtText(d.testLab.isEmpty() ? d.issueDate
                                     : d.issueDate + QStringLiteral(" / ") + d.testLab,
                 78));
    // 13〜21 器具寸法 / 発光面寸法 [mm]
    lineD(d.length * 1000.0);
    lineD(d.width  * 1000.0);
    lineD(d.height * 1000.0);
    lineD(d.length * 1000.0);
    lineD(d.width  * 1000.0);
    for (int i = 0; i < 4; ++i) lineD(0.0);     // C0 / C90 / C180 / C270
    // 22〜25
    lineD(dff);
    lineD(lorl);
    lineD(1.0);                                  // 光度の換算係数
    lineD(0.0);                                  // 測定時の傾き
    // 26 ランプ組 1 組 (6 行)
    line(QStringLiteral("1"));
    line(QString::number(lampsOut));
    line(ldtText(d.lamp.isEmpty() ? QStringLiteral("n/a") : d.lamp, 78));
    lineD(refFlux);
    line(QStringLiteral("n/a"));                 // 光色 / 色温度 (持っていない)
    line(QStringLiteral("n/a"));                 // 演色群 / 演色評価数 (同上)
    lineD(d.inputWatts);
    // 27 直射比 — LiTG 3.5 の利用率法は実装していないので 0 (.h 参照)
    for (int i = 0; i < 10; ++i) lineD(0.0);
    // 28 C 角 / 29 γ 角
    for (double c : cAngles) lineD(c);
    for (double g : d.vertAngles_deg) lineD(g);
    // 30 光度 [cd/1000lm] — 軸対称なら 1 面、そうでなければ全 Mc 面
    for (int h = 0; h < planes; ++h)
        for (int k = 0; k < nv; ++k)
            lineD(d.candela[h][k] / (refFlux / 1000.0));

    out.flush();
    f.close();
    return true;
}

bool PhotometricIO::readLdt(const QString &path, PhotometricData *d, QString *err)
{
    if (!d) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = f.errorString();
        return false;
    }
    QTextStream in(&f);
    QStringList L;
    while (!in.atEnd()) L.push_back(in.readLine().trimmed());

    int at = 0;
    bool bad = false;
    auto text = [&L, &at, &bad]() -> QString {
        if (at >= L.size()) { bad = true; return QString(); }
        return L[at++];
    };
    auto number = [&L, &at, &bad, err]() -> double {
        if (at >= L.size()) { bad = true; return 0.0; }
        const QString t = L[at++].trimmed();
        bool ok = false;
        const double v = t.toDouble(&ok);
        if (!ok) {
            bad = true;
            if (err) *err = QStringLiteral("EULUMDAT: %1 行目が数値として読めません: %2")
                                .arg(at).arg(t);
        }
        return v;
    };
    auto truncated = [err, &at]() {
        if (err && err->isEmpty())
            *err = QStringLiteral("EULUMDAT: 行が足りません (%1 行目で尽きました)")
                       .arg(at);
        return false;
    };
    if (err) err->clear();

    PhotometricData r;
    r.manufacturer = text();                     // 1
    /* ityp */ number();                         // 2 (読み替えには使わない)
    const int isym = static_cast<int>(number()); // 3
    const int mc   = static_cast<int>(number()); // 4
    /* dc */ number();                           // 5
    const int ng   = static_cast<int>(number()); // 6
    /* dg */ number();                           // 7
    if (bad) return truncated();
    if (isym != 0 && isym != 1) {
        if (err) *err = QStringLiteral(
            "EULUMDAT: 対称指定 Isym=%1 は未対応です (0 と 1 のみ)。"
            "鏡映の向きを推測すると配光が黙って裏返るため読みません").arg(isym);
        return false;
    }
    if (mc <= 0 || ng <= 0) {
        if (err) *err = QStringLiteral("EULUMDAT: 角度の数が 0 以下です (Mc=%1, Ng=%2)")
                            .arg(mc).arg(ng);
        return false;
    }

    r.test      = text();                        // 8
    r.luminaire = text();                        // 9
    r.lumCat    = text();                        // 10
    /* file name */ text();                      // 11
    r.issueDate = text();                        // 12
    r.length = number() / 1000.0;                // 13
    r.width  = number() / 1000.0;                // 14
    r.height = number() / 1000.0;                // 15
    for (int i = 0; i < 6; ++i) number();        // 16〜21 発光面
    /* dff */ number();                          // 22
    /* lorl */ number();                         // 23
    const double conv = number();                // 24
    /* tilt */ number();                         // 25
    const int sets = static_cast<int>(number()); // 26
    if (bad) return truncated();
    if (sets < 1) {
        if (err) *err = QStringLiteral(
            "EULUMDAT: ランプ組が %1 組です (基準光束が決まりません)").arg(sets);
        return false;
    }
    if (!(conv > 0.0)) {
        // 0 を掛けると配光が丸ごと消える。黙って全 0 を返さない (絶対規則 5)
        if (err) *err = QStringLiteral(
            "EULUMDAT: 光度の換算係数が %1 です (正の値が要ります)").arg(conv);
        return false;
    }
    double refFlux = 0.0;
    for (int s = 0; s < sets; ++s) {
        const int    n    = static_cast<int>(number());   // 26a
        const QString type = text();                      // 26b
        const double flux = number();                     // 26c
        text();                                           // 26d 光色
        text();                                           // 26e 演色
        const double watt = number();                     // 26f
        if (s == 0) {                                     // 基準は 1 組目
            r.lamps = (n > 0) ? n : 1;
            r.lamp = type;
            refFlux = flux;
            r.inputWatts = watt;
        }
    }
    if (bad) return truncated();
    if (!(refFlux > 0.0)) {
        if (err) *err = QStringLiteral(
            "EULUMDAT: ランプ光束が %1 lm です (cd/1000lm を実光度に戻せません)")
            .arg(refFlux);
        return false;
    }
    r.lumensPerLamp = refFlux / r.lamps;

    for (int i = 0; i < 10; ++i) number();       // 27 直射比 (使わないが在ること)
    QVector<double> cAngles;
    for (int i = 0; i < mc; ++i) cAngles.push_back(number());   // 28
    for (int i = 0; i < ng; ++i) r.vertAngles_deg.push_back(number()); // 29
    if (bad) return truncated();

    // 30 — Isym=1 は全 C 平面が同一なので 1 面だけ入っている。畳んで持つ
    const int planes = (isym == 1) ? 1 : mc;
    r.horizAngles_deg = (isym == 1) ? QVector<double>{ 0.0 } : cAngles;
    r.candela.resize(planes);
    const double scale = (refFlux / 1000.0) * conv;
    for (int h = 0; h < planes; ++h) {
        QVector<double> row;
        row.reserve(ng);
        for (int k = 0; k < ng; ++k) row.push_back(number() * scale);
        if (bad) return truncated();
        r.candela[h] = row;
    }

    *d = r;
    return true;
}

double PhotometricIO::integratedFlux(const PhotometricData &d)
{
    return partialFlux(d, 0.0, 180.0);
}

double PhotometricIO::partialFlux(const PhotometricData &d, double g0, double g1)
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
            // 求める範囲でビンを切る (またぐビンを丸ごと入れない)
            const double lo = std::max(edge[k], g0);
            const double hi = std::min(edge[k + 1], g1);
            if (hi <= lo) continue;
            const double a0 = lo * kPi / 180.0;
            const double a1 = hi * kPi / 180.0;
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
