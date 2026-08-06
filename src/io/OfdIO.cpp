// OfdIO.cpp
#include "OfdIO.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

using namespace ofd;

// ── helpers ─────────────────────────────────────────────────────────────────
static QString num(double v)            { return QString::number(v, 'g', 10); }
static QString joinNums(const double *v, int n) {
    QString s;
    for (int i = 0; i < n; ++i) { s += ' '; s += num(v[i]); }
    return s;
}

// 音源リスト 1 行の .ofdx 表現 (室内音響 / 水中音響で共用)。
// 追加キーのみ — 既存キーの改名・削除・型変更は禁止。
static QJsonObject sourceRowToJson(const AcousticSourceRow &r) {
    return QJsonObject{
        {"enabled", r.enabled}, {"name", r.name}, {"kind", r.kind},
        {"pos_m", QJsonArray{ r.x_m, r.y_m, r.z_m }},
        {"aim", r.aim}, {"signal", r.signal}, {"level_db", r.level_dB} };
}

static AcousticSourceRow sourceRowFromJson(const QJsonObject &o) {
    AcousticSourceRow r;
    r.enabled = o.value("enabled").toBool(r.enabled);
    r.name    = o.value("name").toString(r.name);
    // 壊れたファイルの範囲外値で不正な種別を作らない (0..3 にクランプ)
    r.kind    = qBound(int(AcousticSourceRow::Omni),
                       o.value("kind").toInt(r.kind),
                       int(AcousticSourceRow::Directional));
    const QJsonArray pos = o.value("pos_m").toArray();
    if (pos.size() >= 3) {
        r.x_m = pos[0].toDouble(r.x_m);
        r.y_m = pos[1].toDouble(r.y_m);
        r.z_m = pos[2].toDouble(r.z_m);
    }
    r.aim      = o.value("aim").toString(r.aim);
    r.signal   = o.value("signal").toString(r.signal);
    r.level_dB = o.value("level_db").toDouble(r.level_dB);
    return r;
}

// Split off a trailing " # name" comment (GUI metadata the kernel tokenizer
// simply ignores as extra tokens). Returns the name, shortens the line.
static QString takeTrailingName(QString &line) {
    const int hash = line.indexOf('#');
    if (hash < 0) return {};
    const QString name = line.mid(hash + 1).trimmed();
    line = line.left(hash).trimmed();
    return name;
}

// ── Serializer ──────────────────────────────────────────────────────────────
QString OfdIO::serialize(const Project &p)
{
    QString text;
    QTextStream out(&text);

    // ── RCWA コア設定 (OpenRCWA: rcwa / rcwalayer) を書き出すか ──────────
    // 条件: 光ソルバーが RCWA、かつ層スタックが有効 (空でなく全層が有効)。
    // どちらかが崩れていれば RCWA 関連は一切出力しない = 従来出力とバイト
    // 一致 (後方互換)。不正な設定のままカーネルを走らせない意図も兼ねる。
    const OpticalOpts &oo = p.optical();
    // FMM (Fourier Modal Method) は RCWA と同一手法の別名で、カーネルも
    // 同じ OpenRCWA (orcwa) を使う — 調和次数だけ FMM 設定 (fmmHarmonics)
    // を反映する。
    const bool rcwaOut = (oo.solver == OpticalSolver::RCWA ||
                          oo.solver == OpticalSolver::FMM) &&
                         isValidRcwaStack(oo.rcwaLayerList);

    // ヘッダ: orcwa のパーサは "OpenRCWA" / "OpenTHFD" しか受け付けない
    // (OpenRCWA/sol/input_data.c)。RCWA 設定を出力するときだけ切り替える。
    out << (rcwaOut ? "OpenRCWA 4 2\n" : "OpenFDTD 4 2\n");
    const GeneralOpts &g = p.general();
    if (!g.title.isEmpty())
        out << "title = " << g.title << "\n";

    // mesh: x0 d1 x1 d2 x2 ...
    static const char *meshKey[3] = { "xmesh", "ymesh", "zmesh" };
    for (int a = 0; a < 3; ++a) {
        const MeshAxis &ax = p.mesh(a);
        if (ax.nodes.isEmpty()) continue;
        out << meshKey[a] << " = " << num(ax.nodes[0]);
        for (int i = 0; i < ax.divs.size(); ++i)
            out << " " << ax.divs[i] << " " << num(ax.nodes[i+1]);
        out << "\n";
    }

    // materials (ID 0/1 are built-in; user materials start at 2)
    for (const Material &m : p.materials()) {
        out << "material = " << m.type;
        if (m.type == 2)
            out << " " << num(m.einf) << " " << num(m.ae)
                << " " << num(m.be)   << " " << num(m.ce);
        else
            out << " " << num(m.epsr) << " " << num(m.esgm)
                << " " << num(m.amur) << " " << num(m.msgm);
        if (!m.name.isEmpty()) out << " # " << m.name;
        out << "\n";
    }

    // geometries
    for (const Geometry &ge : p.geometries()) {
        out << "geometry = " << ge.materialId << " " << ge.shape
            << joinNums(ge.g, Geometry::paramCount(ge.shape));
        if (!ge.name.isEmpty()) out << " # " << ge.name;
        out << "\n";
    }

    // sources
    for (const Feed &f : p.feeds())
        out << "feed = " << f.dir << " " << num(f.x) << " " << num(f.y)
            << " " << num(f.z) << " " << num(f.volt) << " " << num(f.delay)
            << " " << num(f.z0) << "\n";
    if (p.planewave().enabled)
        out << "planewave = " << num(p.planewave().theta) << " "
            << num(p.planewave().phi) << " " << p.planewave().pol << "\n";
    for (int i = 0; i < p.probes().size(); ++i) {
        const Probe &pr = p.probes()[i];
        out << "point = " << pr.dir << " " << num(pr.x) << " " << num(pr.y)
            << " " << num(pr.z);
        if (i == 0 && !pr.propagation.isEmpty())
            out << " " << pr.propagation;
        out << "\n";
    }
    for (const Load &l : p.loads())
        out << "load = " << l.dir << " " << num(l.x) << " " << num(l.y)
            << " " << num(l.z) << " " << l.kind << " " << num(l.value) << "\n";
    if (g.rfeed != 0)
        out << "rfeed = " << num(g.rfeed) << "\n";

    // boundary conditions
    if (g.abc == 1)
        out << "abc = 1 " << g.pmlL << " " << num(g.pmlM)
            << " " << num(g.pmlR0) << "\n";
    if (g.pbcX || g.pbcY || g.pbcZ)
        out << "pbc = " << int(g.pbcX) << " " << int(g.pbcY)
            << " " << int(g.pbcZ) << "\n";

    // frequencies / solver
    if (g.hasF1)
        out << "frequency1 = " << num(g.f1min) << " " << num(g.f1max)
            << " " << g.f1div << "\n";
    if (g.hasF2)
        out << "frequency2 = " << num(g.f2min) << " " << num(g.f2max)
            << " " << g.f2div << "\n";
    out << "solver = " << g.maxiter << " " << g.nout
        << " " << num(g.converg) << "\n";
    if (g.dt > 0) out << "timestep = "   << num(g.dt) << "\n";
    if (g.tw > 0) out << "pulsewidth = " << num(g.tw) << "\n";
    if (g.plot3dgeom) out << "plot3dgeom = " << g.plot3dgeom << "\n";

    // ── post-processing keys ────────────────────────────────────────────
    const PostOpts &po = p.post();
    auto freqPlot = [&out](const char *key, const FreqPlot &fp) {
        if (!fp.enabled) return;
        out << key << " = ";
        if (fp.userScale)
            out << "2 " << num(fp.min) << " " << num(fp.max) << " " << fp.div;
        else
            out << "1";
        out << "\n";
    };

    if (po.matchingloss) out << "matchingloss = 1\n";
    if (po.plotiter)     out << "plotiter = 1\n";
    if (po.plotfeed)     out << "plotfeed = 1\n";
    if (po.plotpoint)    out << "plotpoint = 1\n";
    if (po.plotsmith)    out << "plotsmith = 1\n";
    freqPlot("plotzin",      po.zin);
    freqPlot("plotyin",      po.yin);
    freqPlot("plotref",      po.ref);
    freqPlot("plotspara",    po.spara);
    freqPlot("plotcoupling", po.coupling);
    if (po.freqdiv != 10) out << "freqdiv = " << po.freqdiv << "\n";

    if (po.far0d) {
        out << "plotfar0d = " << num(po.far0dTheta) << " " << num(po.far0dPhi);
        if (po.far0dUserScale)
            out << " 2 " << num(po.far0dMin) << " " << num(po.far0dMax)
                << " " << po.far0dDiv;
        else
            out << " 1";
        out << "\n";
    }

    for (const Far1d &f : po.far1d) {
        out << "plotfar1d = " << f.dir << " " << f.div;
        if (f.dir == 'V' || f.dir == 'H') out << " " << num(f.angle);
        out << "\n";
    }
    if (!po.far1d.isEmpty()) {
        out << "far1dstyle = " << po.far1dStyle << "\n";
        out << "far1dcomponent = " << po.far1dComp[0] << " "
            << po.far1dComp[1] << " " << po.far1dComp[2] << "\n";
        out << "far1ddb = " << int(po.far1dDb) << "\n";
        if (po.far1dNorm) out << "far1dnorm = 1\n";
        if (po.far1dUserScale)
            out << "far1dscale = " << num(po.far1dMin) << " "
                << num(po.far1dMax) << " " << po.far1dDiv << "\n";
    }

    if (po.far2d) {
        out << "plotfar2d = " << po.far2dDivTheta << " " << po.far2dDivPhi << "\n";
        out << "far2dcomponent =";
        for (int i = 0; i < 7; ++i) out << " " << po.far2dComp[i];
        out << "\n";
        out << "far2ddb = " << int(po.far2dDb) << "\n";
        if (po.far2dUserScale)
            out << "far2dscale = " << num(po.far2dMin) << " "
                << num(po.far2dMax) << "\n";
        out << "far2dobj = " << num(po.far2dObj) << "\n";
    }

    for (const Near1d &n : po.near1d)
        out << "plotnear1d = " << n.cmp << " " << n.dir << " "
            << num(n.pos1) << " " << num(n.pos2) << "\n";
    if (!po.near1d.isEmpty()) {
        out << "near1ddb = " << int(po.near1dDb) << "\n";
        if (po.near1dUserScale)
            out << "near1dscale = " << num(po.near1dMin) << " "
                << num(po.near1dMax) << " " << po.near1dDiv << "\n";
        out << "near1dnoinc = " << int(po.near1dNoinc) << "\n";
    }

    for (const Near2d &n : po.near2d)
        out << "plotnear2d = " << n.cmp << " " << n.dir << " "
            << num(n.pos) << "\n";
    if (!po.near2d.isEmpty()) {
        out << "near2ddim = " << po.near2dDim[0] << " " << po.near2dDim[1] << "\n";
        if (po.near2dFrame) out << "near2dframe = 1\n";
        out << "near2ddb = " << int(po.near2dDb) << "\n";
        if (po.near2dUserScale)
            out << "near2dscale = " << num(po.near2dMin) << " "
                << num(po.near2dMax) << "\n";
        out << "near2dcontour = " << int(po.near2dContour) << "\n";
        out << "near2dobj = " << po.near2dObj << "\n";
        out << "near2dnoinc = " << int(po.near2dNoinc) << "\n";
        if (po.near2dZoom)
            out << "near2dzoom = " << num(po.near2dHzoom[0]) << " "
                << num(po.near2dHzoom[1]) << " " << num(po.near2dVzoom[0])
                << " " << num(po.near2dVzoom[1]) << "\n";
    }

    // ── RCWA コア設定 (OpenRCWA: rcwa / rcwalayer) ──────────────────────
    // orcwa は「調和次数 1 個 + 周期 1 個」しか受け取らない 1D 格子ソルバー
    // なので、GUI の Nx / Λx をそのまま写像する。Ny / Λy (y 方向) は
    // カーネル側に対応するキーが無いため無視される (.ofdx には保存される)。
    // 単位: GUI は nm、orcwa は m (物理長 — γ = k0·neff スケーリング前提)。
    if (rcwaOut) {
        const int harmonics = (oo.solver == OpticalSolver::FMM)
                            ? oo.fmmHarmonics : oo.rcwaNx;
        out << "rcwa = " << harmonics << " "
            << num(oo.rcwaPeriodX * 1e-9) << "\n";
        // 層は入射側 → 透過側の順にそのまま並べる (先頭/末尾は半無限層)。
        for (const RcwaLayer &l : oo.rcwaLayerList)
            out << "rcwalayer = " << num(l.eps1) << " " << num(l.eps2)
                << " " << num(l.fill) << " "
                << num(l.thickness_nm * 1e-9) << "\n";
    }

    // ── 非線形 (TPA) / ONN 活性化キー (OpenBPM: tpa+powersweep, OpenFDTD:
    // tpa)。無効時は一切出力しない (従来ファイルとバイト一致 = 後方互換)。
    if (oo.tpaEnabled)
        out << "tpa = " << oo.tpaMaterialId << " "
            << num(oo.tpaBeta_cmGW) << "\n";
    if (oo.powerSweepEnabled)
        out << "powersweep = " << num(oo.psPmin_W) << " " << num(oo.psPmax_W)
            << " " << oo.psPoints << " " << (oo.psLog ? "log" : "lin") << "\n";

    // keys the GUI doesn't model — preserved from the loaded file
    for (const QString &line : p.extraLines())
        out << line << "\n";

    out << "end\n";
    return text;
}

bool OfdIO::save(const QString &path, const Project &project, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = f.errorString();
        return false;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(false);    // 本家は BOM 無し
    out << serialize(project);
    return true;
}

// ── Loader ──────────────────────────────────────────────────────────────────
bool OfdIO::load(const QString &path, Project &project, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = f.errorString();
        return false;
    }
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    return parse(in.readAll(), project, err);
}

bool OfdIO::parse(const QString &text, Project &project, QString *err)
{
    static const QRegularExpression ws("\\s+");

    GeneralOpts &g = project.general();
    PostOpts    &po = project.post();
    po.plotiter = false;   // file decides what is on
    g.hasF1 = g.hasF2 = false;

    // モニター定義 / 解析グループは .ofd に持たない (.ofdx 側のデータ)。
    // サイドカーが無い .ofd を開いたとき、直前のプロジェクトで編集した一覧が
    // 残らないようドメインの既定へ戻す。サイドカーがあれば直後の
    // OfdxIO::load が上書きする (キーが無ければ同じ既定で埋め直す)。
    project.monitors() = defaultMonitors(project.activeDomain());
    project.analysisGroups() = defaultAnalysisGroups(project.activeDomain());

    bool header = false;
    const QStringList lines = text.split('\n');
    for (QString rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        if (line.startsWith("end")) break;

        if (!header) {
            const QStringList t = line.split(ws, Qt::SkipEmptyParts);
            // "OpenRCWA" は orcwa (RCWA コア) 入力のヘッダ — GUI が RCWA
            // 設定を書き出したファイルを読み戻せるように受け付ける。
            if (t.size() < 3 || (t[0] != "OpenFDTD" && t[0] != "OpenTHFD" &&
                                 t[0] != "OpenRCWA")) {
                if (err) *err = "not OpenFDTD/OpenTHFD data";
                return false;
            }
            header = true;
            continue;
        }

        const int eq = line.indexOf('=');
        if (eq < 0) continue;
        const QString key = line.left(eq).trimmed();
        QString val = line.mid(eq + 1).trimmed();
        const QString name = (key == "material" || key == "geometry")
                             ? takeTrailingName(val) : QString();
        const QStringList t = val.split(ws, Qt::SkipEmptyParts);
        auto d = [&t](int i) { return (i < t.size()) ? t[i].toDouble() : 0.0; };
        auto n = [&t](int i) { return (i < t.size()) ? t[i].toInt()    : 0;   };

        if (key == "title") {
            g.title = val;
        }
        else if (key == "xmesh" || key == "ymesh" || key == "zmesh") {
            const int a = (key[0] == 'x') ? 0 : (key[0] == 'y') ? 1 : 2;
            MeshAxis &ax = project.mesh(a);
            ax.nodes.clear(); ax.divs.clear();
            if (t.size() < 3 || t.size() % 2 == 0) {
                if (err) *err = "invalid " + key + " data";
                return false;
            }
            ax.nodes.push_back(d(0));
            for (int i = 1; i + 1 < t.size(); i += 2) {
                ax.divs.push_back(t[i].toInt());
                ax.nodes.push_back(t[i+1].toDouble());
            }
        }
        else if (key == "material") {
            Material m;
            if (t.size() >= 5 && (t[0] == "1" || t[0] == "2")) {
                m.type = n(0);
                if (m.type == 2) {
                    m.einf = d(1); m.ae = d(2); m.be = d(3); m.ce = d(4);
                } else {
                    m.epsr = d(1); m.esgm = d(2); m.amur = d(3); m.msgm = d(4);
                }
            } else if (t.size() >= 4) {
                // old format (version < 2.2): εr σ μr σm without type
                m.type = 1;
                m.epsr = d(0); m.esgm = d(1); m.amur = d(2); m.msgm = d(3);
            }
            m.name = name;
            project.materials().push_back(m);
        }
        else if (key == "geometry") {
            Geometry ge;
            ge.materialId = n(0);
            ge.shape      = n(1);
            const int np = Geometry::paramCount(ge.shape);
            for (int i = 0; i < np && i < 8; ++i) ge.g[i] = d(2 + i);
            ge.name = name;
            project.geometries().push_back(ge);
        }
        else if (key == "name") {
            // 本家カーネルは無視する — GUI も読み飛ばす
        }
        else if (key == "feed" && t.size() >= 7) {
            Feed fd;
            fd.dir = t[0].toUpper().at(0);
            fd.x = d(1); fd.y = d(2); fd.z = d(3);
            fd.volt = d(4); fd.delay = d(5); fd.z0 = d(6);
            project.feeds().push_back(fd);
        }
        else if (key == "planewave" && t.size() >= 3) {
            project.planewave().enabled = true;
            project.planewave().theta = d(0);
            project.planewave().phi   = d(1);
            project.planewave().pol   = n(2);
        }
        else if (key == "point" && t.size() >= 4) {
            Probe pr;
            pr.dir = t[0].toUpper().at(0);
            pr.x = d(1); pr.y = d(2); pr.z = d(3);
            if (project.probes().isEmpty() && t.size() >= 5)
                pr.propagation = t[4];
            project.probes().push_back(pr);
        }
        else if (key == "load" && t.size() >= 6) {
            Load l;
            l.dir = t[0].toUpper().at(0);
            l.x = d(1); l.y = d(2); l.z = d(3);
            l.kind = t[4].toUpper().at(0);
            l.value = d(5);
            project.loads().push_back(l);
        }
        else if (key == "rfeed")       { g.rfeed = d(0); }
        else if (key == "abc") {
            if (t.size() >= 1 && t[0].startsWith('0')) {
                g.abc = 0;
            } else if (t.size() >= 4 && t[0].startsWith('1')) {
                g.abc = 1;
                g.pmlL = n(1); g.pmlM = d(2); g.pmlR0 = d(3);
            }
        }
        else if (key == "pbc" && t.size() >= 3) {
            g.pbcX = n(0); g.pbcY = n(1); g.pbcZ = n(2);
        }
        else if (key == "frequency1" && t.size() >= 3) {
            g.hasF1 = true;
            g.f1min = d(0); g.f1max = d(1); g.f1div = n(2);
        }
        else if ((key == "frequency2" || key == "frequency") && t.size() >= 3) {
            g.hasF2 = true;
            g.f2min = d(0); g.f2max = d(1); g.f2div = n(2);
        }
        else if (key == "solver" && t.size() >= 3) {
            g.maxiter = n(0); g.nout = n(1); g.converg = d(2);
        }
        else if (key == "timestep")    { g.dt = d(0); }
        else if (key == "pulsewidth")  { g.tw = d(0); }
        else if (key == "plot3dgeom")  { g.plot3dgeom = n(0); }

        // ── post-processing keys ────────────────────────────────────────
        else if (key == "matchingloss") { po.matchingloss = n(0); }
        else if (key == "plotiter")     { po.plotiter  = n(0); }
        else if (key == "plotfeed")     { po.plotfeed  = n(0); }
        else if (key == "plotpoint")    { po.plotpoint = n(0); }
        else if (key == "plotsmith")    { po.plotsmith = n(0); }
        else if (key == "plotzin" || key == "plotyin" || key == "plotref" ||
                 key == "plotspara" || key == "plotcoupling") {
            FreqPlot &fp = (key == "plotzin")   ? po.zin
                         : (key == "plotyin")   ? po.yin
                         : (key == "plotref")   ? po.ref
                         : (key == "plotspara") ? po.spara : po.coupling;
            if (t.size() >= 1 && t[0] == "1") {
                fp.enabled = true; fp.userScale = false;
            } else if (t.size() >= 4 && t[0] == "2") {
                fp.enabled = true; fp.userScale = true;
                fp.min = d(1); fp.max = d(2); fp.div = n(3);
            }
        }
        else if (key == "freqdiv")      { po.freqdiv = n(0); }
        else if (key == "plotfar0d" && t.size() >= 3) {
            po.far0d = true;
            po.far0dTheta = d(0); po.far0dPhi = d(1);
            if (t[2] == "2" && t.size() >= 6) {
                po.far0dUserScale = true;
                po.far0dMin = d(3); po.far0dMax = d(4); po.far0dDiv = n(5);
            }
        }
        else if (key == "plotfar1d" && t.size() >= 2) {
            Far1d f;
            f.dir = t[0].toUpper().at(0);
            f.div = n(1);
            if ((f.dir == 'V' || f.dir == 'H') && t.size() >= 3) f.angle = d(2);
            po.far1d.push_back(f);
        }
        else if (key == "plotfar2d" && t.size() >= 2) {
            po.far2d = true;
            po.far2dDivTheta = n(0); po.far2dDivPhi = n(1);
        }
        else if (key == "plotnear1d" && t.size() >= 4) {
            Near1d nr;
            nr.cmp = t[0]; nr.dir = t[1].toUpper().at(0);
            nr.pos1 = d(2); nr.pos2 = d(3);
            po.near1d.push_back(nr);
        }
        else if (key == "plotnear2d" && t.size() >= 3) {
            Near2d nr;
            nr.cmp = t[0]; nr.dir = t[1].toUpper().at(0);
            nr.pos = d(2);
            po.near2d.push_back(nr);
        }
        else if (key == "far1dcomponent" && t.size() >= 3) {
            for (int i = 0; i < 3; ++i) po.far1dComp[i] = n(i);
        }
        else if (key == "far1dstyle")   { po.far1dStyle = n(0); }
        else if (key == "far1ddb")      { po.far1dDb = n(0); }
        else if (key == "far1dnorm")    { po.far1dNorm = n(0); }
        else if (key == "far1dscale" && t.size() >= 3) {
            po.far1dUserScale = true;
            po.far1dMin = d(0); po.far1dMax = d(1); po.far1dDiv = n(2);
        }
        else if (key == "far2dcomponent" && t.size() >= 7) {
            for (int i = 0; i < 7; ++i) po.far2dComp[i] = n(i);
        }
        else if (key == "far2ddb")      { po.far2dDb = n(0); }
        else if (key == "far2dscale" && t.size() >= 2) {
            po.far2dUserScale = true;
            po.far2dMin = d(0); po.far2dMax = d(1);
        }
        else if (key == "far2dobj")     { po.far2dObj = d(0); }
        else if (key == "near1ddb")     { po.near1dDb = n(0); }
        else if (key == "near1dscale" && t.size() >= 3) {
            po.near1dUserScale = true;
            po.near1dMin = d(0); po.near1dMax = d(1); po.near1dDiv = n(2);
        }
        else if (key == "near1dnoinc")  { po.near1dNoinc = n(0); }
        else if (key == "near2ddim" && t.size() >= 2) {
            po.near2dDim[0] = n(0); po.near2dDim[1] = n(1);
        }
        else if (key == "near2dframe")  { po.near2dFrame = n(0); }
        else if (key == "near2ddb")     { po.near2dDb = n(0); }
        else if (key == "near2dscale" && t.size() >= 2) {
            po.near2dUserScale = true;
            po.near2dMin = d(0); po.near2dMax = d(1);
        }
        else if (key == "near2dcontour"){ po.near2dContour = n(0); }
        else if (key == "near2dobj")    { po.near2dObj = n(0); }
        else if (key == "near2dnoinc")  { po.near2dNoinc = n(0); }
        else if (key == "near2dzoom" && t.size() >= 4) {
            po.near2dZoom = true;
            po.near2dHzoom[0] = qMin(d(0), d(1));
            po.near2dHzoom[1] = qMax(d(0), d(1));
            po.near2dVzoom[0] = qMin(d(2), d(3));
            po.near2dVzoom[1] = qMax(d(2), d(3));
        }
        // ── RCWA コア設定 (OpenRCWA: rcwa / rcwalayer) ──────────────────
        // rcwa キーが在るファイルは RCWA カーネル用の入力なので、ソルバー
        // も RCWA にする (planewave/tpa と同じ「キーの存在 = 機能有効」の
        // 流儀)。.ofdx は .ofd の後に読まれるため、サイドカーに明示的な
        // solver があればそちらが優先される。
        else if (key == "rcwa" && t.size() >= 2) {
            OpticalOpts &o = project.optical();
            o.solver = OpticalSolver::RCWA;
            o.rcwaNx = n(0);
            o.rcwaPeriodX = d(1) * 1e9;    // m → nm
        }
        else if (key == "rcwalayer" && t.size() >= 4) {
            RcwaLayer l;
            l.eps1 = d(0);
            l.eps2 = d(1);
            l.fill = d(2);
            l.thickness_nm = d(3) * 1e9;   // m → nm
            project.optical().rcwaLayerList.push_back(l);
        }
        // ── 非線形 (TPA) / ONN 活性化キー ───────────────────────────────
        else if (key == "tpa" && t.size() >= 2) {
            OpticalOpts &o = project.optical();
            o.tpaEnabled = true;
            o.tpaMaterialId = n(0);
            o.tpaBeta_cmGW = d(1);
        }
        else if (key == "powersweep" && t.size() >= 3) {
            OpticalOpts &o = project.optical();
            o.powerSweepEnabled = true;
            o.psPmin_W = d(0);
            o.psPmax_W = d(1);
            o.psPoints = n(2);
            if (t.size() >= 4)
                o.psLog = (t[3].compare("lin", Qt::CaseInsensitive) != 0);
        }
        else {
            // unknown key — keep verbatim for round-trip safety
            project.extraLines().push_back(line);
        }
    }

    if (!header) {
        if (err) *err = "empty file";
        return false;
    }
    return true;
}

// ── .ofdx (JSON sidecar) ────────────────────────────────────────────────────
bool OfdxIO::save(const QString &path, const Project &p, QString *err)
{
    QJsonObject root;
    root["schemaVersion"] = "1.0";
    root["domain"] = domainKey(p.activeDomain());

    {
        const OpticalOpts &o = p.optical();
        QJsonObject opt;
        opt["solver"] = int(o.solver);
        opt["mode"]   = int(o.mode);
        opt["wavelength"] = QJsonObject{
            {"min_nm", o.lambdaMin}, {"max_nm", o.lambdaMax}, {"div", o.lambdaDiv} };
        // RCWA 層スタック — "layer_list" は追加キー。既存の "layers"
        // (GUI 上の層分割数) は意味が違うので残す (改名・削除は禁止)。
        QJsonArray rcwaLayerList;
        for (const RcwaLayer &l : o.rcwaLayerList)
            rcwaLayerList.append(QJsonObject{
                {"eps1", l.eps1}, {"eps2", l.eps2},
                {"fill", l.fill}, {"thickness_nm", l.thickness_nm} });
        opt["rcwa"] = QJsonObject{
            {"nx", o.rcwaNx}, {"ny", o.rcwaNy},
            {"period_x_nm", o.rcwaPeriodX}, {"period_y_nm", o.rcwaPeriodY},
            {"layers", o.rcwaLayers},
            {"layer_list", rcwaLayerList} };
        opt["bpm"] = QJsonObject{
            {"algorithm", o.bpmAlgorithm}, {"dz_nm", o.bpmDz},
            {"ref_index", o.bpmRefIndex}, {"input_mode", o.bpmInputMode} };
        opt["fmm"] = QJsonObject{
            {"harmonics", o.fmmHarmonics}, {"li_rules", o.fmmLiRules} };
        // BPF: il_db / stop_db (設計目標) は既存キーの後ろへの追加のみ。
        opt["bpf"] = QJsonObject{
            {"band_nm", QJsonArray{ o.bpfBandMin, o.bpfBandMax }}, {"Q", o.bpfQ},
            {"il_db", o.bpfIL_dB}, {"stop_db", o.bpfStop_dB} };
        opt["ring"] = QJsonObject{
            {"radius_um", o.ringRadius_um}, {"gap_nm", o.ringGap_nm},
            {"thru_port", o.ringThruPort}, {"drop_port", o.ringDropPort} };
        // ── 光解析モード別設定 (mock のモード別セクション) — 追加キーのみ。
        // .ofd (カーネル入力) には出力しない: 設計目標 / 解析対象の記録。
        opt["waveguide"] = QJsonObject{
            {"te0", o.wgTE0}, {"te1", o.wgTE1},
            {"tm0", o.wgTM0}, {"tm1", o.wgTM1},
            {"loss_db_cm", o.wgLoss_dBcm} };
        opt["mzi"] = QJsonObject{
            {"delta_l_um", o.mziDeltaL_um},
            {"thermo", o.mziThermo}, {"electro", o.mziElectro} };
        opt["metasurface"] = QJsonObject{
            {"period_nm", o.metaPeriod_nm},
            {"shape", o.metaShape}, {"phase", o.metaPhase} };
        opt["phc"] = QJsonObject{
            {"lattice", o.phcLattice}, {"a_nm", o.phcA_nm},
            {"r_over_a", o.phcRoverA},
            {"band", o.phcBand}, {"defect", o.phcDefect} };
        opt["nf2ff"] = QJsonObject{
            {"surface", o.nfffSurface},
            {"distance_lambda", o.nfffDistance_lambda} };
        opt["sparam"] = QJsonObject{
            {"ports", o.spPorts},
            {"port_in", o.spPortIn}, {"port_out", o.spPortOut},
            {"s11", o.spS11}, {"s21", o.spS21},
            {"phase", o.spPhase}, {"group_delay", o.spGroupDelay} };
        // 非線形 (TPA) / ONN 活性化 (Opt. Lett. 49, 5811 (2024)) — 追加キー
        // のみ。既存キーの改名・削除・順序変更は後方互換のため禁止。
        opt["tpa"] = QJsonObject{
            {"enabled", o.tpaEnabled},
            {"material_id", o.tpaMaterialId},
            {"beta_cm_gw", o.tpaBeta_cmGW} };
        opt["powersweep"] = QJsonObject{
            {"enabled", o.powerSweepEnabled},
            {"pmin_w", o.psPmin_W},
            {"pmax_w", o.psPmax_W},
            {"points", o.psPoints},
            {"scale", o.psLog ? QStringLiteral("log") : QStringLiteral("lin")} };
        root["optical"] = opt;
    }
    {
        const AcousticOpts &a = p.acoustic();
        QJsonArray budget;
        for (const AbsorptionRow &r : a.absorption) {
            QJsonArray alpha;
            for (double v : r.alpha) alpha.append(v);
            budget.append(QJsonObject{
                {"enabled", r.enabled}, {"role", r.role}, {"name", r.name},
                {"area", r.area}, {"alpha", alpha}, {"air_a", r.airA} });
        }
        QJsonArray noise;
        for (double v : a.noiseLevels) noise.append(v);
        // 騒音源内訳 (mock room-acoustics.jsx 騒音源テーブル) — 追加キーのみ。
        // 既存キーの改名・削除・型変更は後方互換のため禁止。
        QJsonArray noiseSrc;
        for (const NoiseSourceRow &r : a.noiseSources)
            noiseSrc.append(QJsonObject{
                {"enabled", r.enabled}, {"name", r.name},
                {"level_dba", r.level_dBA}, {"measure", r.measure} });
        // 受音点リスト (AcousticTab の受音点表) — 追加キーのみ。
        // 既存の mic_count は受音点数として残す (行数と常に一致する)。
        QJsonArray recv;
        for (const ReceiverRow &r : a.receivers)
            recv.append(QJsonObject{
                {"enabled", r.enabled},
                {"pos_m", QJsonArray{ r.x, r.y, r.z }},
                {"type", r.type}, {"name", r.name},
                // 受音点ごとの RIR WAV (可聴化の一括レンダリング入力) —
                // 追加キーのみ。欠落時は空 (旧ファイル互換)
                {"rir_file", r.rirFile} });
        // 音源リスト (AcousticSourceTab の音源一覧) — 追加キーのみ。
        QJsonArray srcList;
        for (const AcousticSourceRow &r : a.sources)
            srcList.append(sourceRowToJson(r));
        QJsonObject ac{
            {"rt60", a.rt60}, {"c80", a.c80}, {"d50", a.d50},
            {"sti", a.sti}, {"edt", a.edt},
            {"impulse_response", a.impulseResponse},
            {"auralization", a.auralization},
            {"sample_rate", a.sampleRate},
            {"src_directivity", a.srcDirectivity},
            {"src_spl_db", a.srcSPL_dB},
            {"mic_count", a.micCount},
            // AcousticTab 追加設定 (LF 指標 / 音源位置・向き / 解析タイプ /
            // 周波数帯域) — 追加キーのみ。改名・削除・型変更は禁止。
            {"lf", a.lf},
            {"src_pos_m", QJsonArray{ a.srcX_m, a.srcY_m, a.srcZ_m }},
            {"src_aim_deg", QJsonArray{ a.srcAimTheta_deg, a.srcAimPhi_deg }},
            {"analysis_type", a.analysisType},
            {"third_octave", a.thirdOctave},
            {"band_range", a.bandRange},
            {"room_l", a.roomL}, {"room_w", a.roomW}, {"room_h", a.roomH},
            {"volume", a.volume}, {"surface", a.surface},
            {"occupancy", a.occupancy}, {"rt_formula", a.rtFormula},
            {"absorption", budget}, {"noise_levels", noise},
            {"noise_sources", noiseSrc}, {"receivers", recv},
            {"sources", srcList} };
        // 実測 RIR 分析 (RirAnalysisTab, 指示書 §15) — 追加キーのみ。
        // 既存キーの改名・削除・型変更は後方互換のため禁止。
        const OperaAcousticSettings &oa = p.operaAcoustic();
        ac["opera_analysis"] = QJsonObject{
            {"enabled", oa.enabled},
            {"rir_file", oa.rirPath},
            {"voice_file", oa.voicePath},
            {"voice_type", oa.voiceType},
            {"calibration_state", oa.calibrationState},
            // dBFS→dB SPL オフセット (docs 予約キー、負債 #1)。
            // 既存キーの後ろへの追加のみ。欠落時は 0.0 (旧ファイル互換)。
            {"calibration_offset_db", oa.calibrationOffsetDb},
            {"direct_sound_method", oa.directSoundMethod},
            {"band_mode", oa.bandMode},
            {"channel_mode", oa.channelMode},
            {"analysis_settings", QJsonObject{
                {"noise_correction", oa.noiseCorrection},
                {"minimum_dynamic_range_db", oa.minimumDynamicRangeDb} }},
            // 可聴化 (フェーズ4) — ネスト追加のみ (docs §2.1)。RIR は
            // rir_file を共用する (単一ソース原則)。分析結果は保存しない。
            {"auralization", QJsonObject{
                {"dry_file", oa.auralizationDryFile},
                {"output_file", oa.auralizationOutputFile},
                {"gain_mode", oa.auralizationGainMode} }},
            // 歌声分析 (フェーズ3) — F0 探索範囲の上書き (0 = 声種プリセット)
            {"vocal", QJsonObject{
                {"f0_min_hz", oa.vocalF0MinHz},
                {"f0_max_hz", oa.vocalF0MaxHz} }},
            // 音響ソルバー連携 (AcousticSolverTab) — ネスト追加のみ。
            // backend は AcousticBackend と同順 (並び替え・挿入は禁止)
            {"solver", QJsonObject{
                {"backend", oa.solverBackend},
                {"executable", oa.solverExecutable},
                {"threads", oa.solverThreads},
                {"processes", oa.solverProcesses} }} };
        root["acoustic"] = ac;
    }
    {
        const UnderwaterOpts &u = p.underwater();
        QJsonArray ssp;
        for (const SSPPoint &pt : u.ssp)
            ssp.append(QJsonObject{ {"depth_m", pt.depth_m}, {"c_mps", pt.c_mps} });
        // ソナー送信源リスト (AcousticSourceTab の音源一覧・水中) — 追加キーのみ
        QJsonArray uwSrc;
        for (const AcousticSourceRow &r : u.sources)
            uwSrc.append(sourceRowToJson(r));
        QJsonObject uwObj{
            {"temp_C", u.waterTemp_C}, {"salinity_psu", u.salinity_psu},
            {"ssp", ssp}, {"sofar", u.sofar},
            {"bottom_type", u.bottomType},
            {"bottom_c_mps", u.bottomC_mps},
            {"bottom_rho_kgm3", u.bottomRho_kgm3},
            // 底質吸収係数 α [dB/λ] — 追加キーのみ。欠落時は 0.5 (旧ファイル
            // 互換 = 従来 BellhopIO がハードコードしていた値)。
            {"bottom_alpha_db_lambda", u.bottomAlpha_dBlambda},
            {"sonar_freq_khz", u.sonarFreq_kHz},
            {"sonar_sl_db", u.sonarSL_dB},
            {"range_max_km", u.rangeMax_km},
            {"sources", uwSrc} };
        // ── 以下は追加キー。既定値のままなら **キー自体を書かない** ので、
        //    測点・地形・Bellhop 設定を触らない限り .ofdx は従来とバイト一致。
        {
            const UnderwaterOpts d;   // 既定値
            if (u.siteLat_deg != d.siteLat_deg || u.siteLon_deg != d.siteLon_deg
                || u.trackBearing_deg != d.trackBearing_deg) {
                uwObj["site"] = QJsonObject{
                    {"lat_deg", u.siteLat_deg}, {"lon_deg", u.siteLon_deg},
                    {"bearing_deg", u.trackBearing_deg} };
            }
            if (!u.bathymetry.isEmpty()) {
                QJsonArray bty;
                for (const BathyPoint &b : u.bathymetry)
                    bty.append(QJsonObject{ {"range_km", b.range_km},
                                            {"depth_m", b.depth_m} });
                uwObj["bathymetry"] =
                    QJsonObject{ {"source", u.bathySource}, {"points", bty} };
            }
            if (u.runMode != d.runMode || u.beamType != d.beamType
                || u.numRays != d.numRays || u.angleMin_deg != d.angleMin_deg
                || u.angleMax_deg != d.angleMax_deg
                || u.srcDepth_m != d.srcDepth_m
                || u.numRcvDepth != d.numRcvDepth
                || u.numRcvRange != d.numRcvRange) {
                uwObj["bellhop"] = QJsonObject{
                    {"run_mode", u.runMode}, {"beam_type", u.beamType},
                    {"num_rays", u.numRays},
                    {"angle_min_deg", u.angleMin_deg},
                    {"angle_max_deg", u.angleMax_deg},
                    {"src_depth_m", u.srcDepth_m},
                    {"num_rcv_depth", u.numRcvDepth},
                    {"num_rcv_range", u.numRcvRange} };
            }
        }
        root["underwater"] = uwObj;
    }
    {
        // API key is NOT persisted here — it lives in QSettings
        const Tidy3dOpts &t = p.tidy3d();
        root["tidy3d"] = QJsonObject{
            {"project_name", t.projectName},
            {"resolution", t.resolution},
            {"auto_pml", t.autoPml} };
    }
    // ── ジオメトリ拡張 (.ofdx "geometry") — 追加キーのみ ────────────────────
    // メッシュ細分化領域 (GeometryTab の「細分化領域」表)。領域が 1 つも
    // 定義されていない既定状態では **キー自体を書かない** ので、この機能を
    // 使わない限り .ofdx の出力は従来とバイト一致になる。
    if (!p.refineRegions().isEmpty()) {
        QJsonArray regs;
        for (const RefineRegion &r : p.refineRegions())
            regs.append(QJsonObject{
                {"enabled", r.enabled}, {"name", r.name},
                {"min_m", QJsonArray{ r.min_m[0], r.min_m[1], r.min_m[2] }},
                {"max_m", QJsonArray{ r.max_m[0], r.max_m[1], r.max_m[2] }},
                {"ratio", r.ratio} });
        root["geometry"] = QJsonObject{ {"refine_regions", regs} };
    }

    // ── 回路系電磁解析のポート定義 (.ofdx "circuit") — 追加キーのみ ──────────
    // 既定 3 行のままなら **キー自体を書かない** ので、表を編集しない限り
    // .ofdx の出力は従来とバイト一致になる。
    {
        auto toJson = [](const QVector<CircuitPortRow> &ports) {
            QJsonArray a;
            for (const CircuitPortRow &r : ports)
                a.append(QJsonObject{
                    {"enabled", r.enabled}, {"name", r.name},
                    {"kind", r.kind}, {"net", r.net}, {"ref", r.ref} });
            return a;
        };
        const QJsonArray cur = toJson(p.circuitPorts());
        if (cur != toJson(defaultCircuitPorts()))
            root["circuit"] = QJsonObject{ {"ports", cur} };
    }

    // ── フォトニック回路のネットリスト (.ofdx "schematic") — 追加キーのみ ────
    // 同上: 既定 5 行のままならキー自体を書かない。
    {
        auto toJson = [](const QVector<PhotonicNetRow> &net) {
            QJsonArray a;
            for (const PhotonicNetRow &r : net)
                a.append(QJsonObject{
                    {"enabled", r.enabled}, {"from", r.from}, {"to", r.to},
                    {"wavelength", r.wavelength} });
            return a;
        };
        const QJsonArray cur = toJson(p.photonicNetlist());
        if (cur != toJson(defaultPhotonicNetlist()))
            root["schematic"] = QJsonObject{ {"netlist", cur} };
    }

    // ── モニター定義 (.ofdx "monitors") — 追加キーのみ ──────────────────────
    // 現ドメインの既定行のままなら **キー自体を書かない** (旧 .ofdx と
    // バイト一致)。読み込み側もキーが無ければドメインの既定行で埋める。
    {
        auto toJson = [](const QVector<MonitorRow> &rows) {
            QJsonArray a;
            for (const MonitorRow &r : rows)
                a.append(QJsonObject{
                    {"enabled", r.enabled}, {"type", r.type}, {"name", r.name},
                    {"region", r.region}, {"band", r.band} });
            return a;
        };
        const QJsonArray cur = toJson(p.monitors());
        if (cur != toJson(defaultMonitors(p.activeDomain())))
            root["monitors"] = cur;
    }

    // ── 解析グループ (.ofdx "analysis_groups") — 追加キーのみ ────────────────
    // 同上: 現ドメインの既定行のままならキー自体を書かない。
    {
        auto toJson = [](const QVector<AnalysisGroupRow> &rows) {
            QJsonArray a;
            for (const AnalysisGroupRow &r : rows)
                a.append(QJsonObject{
                    {"enabled", r.enabled}, {"name", r.name},
                    {"monitors", r.monitors}, {"output", r.output} });
            return a;
        };
        const QJsonArray cur = toJson(p.analysisGroups());
        if (cur != toJson(defaultAnalysisGroups(p.activeDomain())))
            root["analysis_groups"] = cur;
    }

    // ── ディスプレイ / AR-VR 光学 (.ofdx "display_optics") — 追加キーのみ ────
    // 既定値のままなら **キー自体を書かない** ので、この機能を使わない限り
    // .ofdx の出力は従来とバイト一致になる (既定値との比較で判定する)。
    {
        auto toJson = [](const DisplayOpticsOpts &d) {
            return QJsonObject{
                {"device", d.device},
                {"waveguide", QJsonObject{
                    {"type", d.wgType},
                    {"substrate_thickness_mm", d.subThick_mm},
                    {"substrate_index", d.subIndex},
                    {"period_nm", d.gratPeriod_nm},
                    {"depth_nm", d.gratDepth_nm},
                    {"slant_deg", d.gratSlant_deg},
                    {"three_gratings", d.threeGratings},
                    {"rcwa_optimize", d.rcwaOptimize},
                    {"design_lambda_nm", d.designLambda_nm},
                    {"guide_max_angle_deg", d.guideMaxAngle_deg},
                    {"outcoupler_len_mm", d.outcouplerLen_mm},
                    {"eye_relief_mm", d.eyeRelief_mm} }},
                {"targets", QJsonObject{
                    {"fov_deg", d.fovTarget_deg},
                    {"eyebox_mm", d.eyeboxTarget_mm},
                    {"see_through_pct", d.seeThroughTarget_pct} }},
                {"oled", QJsonObject{
                    {"bottom_emission", d.bottomEmission},
                    {"top_emission", d.topEmission},
                    {"microcavity", d.microcavity},
                    {"separate_iqe", d.sepIqe},
                    {"separate_spp", d.sepSpp},
                    {"separate_waveguide", d.sepWaveguide},
                    {"structure", d.outcouplingStruct},
                    {"index", d.oledIndex},
                    {"iqe", d.oledIqe} }},
                {"microled", QJsonObject{
                    {"chip_size_um", d.chipSize_um},
                    {"sidewall_recomb", d.sidewallRecomb},
                    {"sidewall_dbr", d.sidewallDbr},
                    {"directional", d.directional},
                    {"index", d.mlIndex},
                    {"iqe", d.mlIqe},
                    {"surface_velocity_cm_s", d.mlSurfVel_cm_s},
                    {"lifetime_ns", d.mlLifetime_ns} }},
                {"lcd", QJsonObject{
                    {"mode", d.lcdMode},
                    {"anisotropy", d.lcAnisotropy},
                    {"comp_film", d.compFilm},
                    {"peak_luminance_cdm2", d.lcdPeakLum_cdm2},
                    {"darkroom_cr", d.lcdDarkroomCr},
                    {"ambient_lx", d.lcdAmbient_lx},
                    {"reflectance", d.lcdReflectance} }} };
        };
        const QJsonObject cur = toJson(p.displayOptics());
        if (cur != toJson(DisplayOpticsOpts{})) root["display_optics"] = cur;
    }

    // ── 照明光学・測色 (.ofdx "illumination") — 追加キーのみ ────────────────
    // 同上: 既定値のままならキー自体を書かない。
    {
        auto toJson = [](const IlluminationOpts &i) {
            return QJsonObject{
                {"app", i.app},
                {"source_model", i.srcModel},
                {"ray_file", i.rayFile},
                {"spectrum", i.spectrum},
                {"flux_lm", i.flux_lm},
                {"rays", i.rays},
                {"optics", QJsonObject{
                    {"reflector", i.reflector},
                    {"tir_lens", i.tirLens},
                    {"diffuser", i.diffuser},
                    {"light_guide", i.lightGuide},
                    {"phosphor", i.phosphor},
                    {"surface", i.surface} }},
                {"white_led", QJsonObject{
                    {"blue_peak_nm", i.bluePeak_nm},
                    {"blue_fwhm_nm", i.blueFwhm_nm},
                    {"phosphor_peak_nm", i.phosPeak_nm},
                    {"phosphor_fwhm_nm", i.phosFwhm_nm},
                    {"phosphor_ratio", i.phosRatio} }},
                {"rgb", QJsonObject{
                    {"r_peak_nm", i.rPeak_nm}, {"r_fwhm_nm", i.rFwhm_nm},
                    {"r_ratio", i.rRatio},
                    {"g_peak_nm", i.gPeak_nm}, {"g_fwhm_nm", i.gFwhm_nm},
                    {"g_ratio", i.gRatio},
                    {"b_peak_nm", i.bPeak_nm}, {"b_fwhm_nm", i.bFwhm_nm},
                    {"b_ratio", i.bRatio} }},
                {"blackbody_k", i.blackbody_K},
                {"mono", QJsonObject{
                    {"peak_nm", i.monoPeak_nm}, {"fwhm_nm", i.monoFwhm_nm} }},
                {"targets", QJsonObject{
                    {"cct_k", i.cctTarget_K},
                    {"cct_tol_k", i.cctTol_K},
                    {"duv_tol", i.duvTol} }} };
        };
        const QJsonObject cur = toJson(p.illumination());
        if (cur != toJson(IlluminationOpts{})) root["illumination"] = cur;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = f.errorString();
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool OfdxIO::load(const QString &path, Project &p, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = f.errorString();
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        if (err) *err = "invalid JSON";
        return false;
    }
    const QJsonObject root = doc.object();
    p.setActiveDomain(domainFromKey(root.value("domain").toString("em")));

    if (root.contains("optical")) {
        const QJsonObject opt = root["optical"].toObject();
        OpticalOpts &o = p.optical();
        o.solver = OpticalSolver(opt.value("solver").toInt(int(o.solver)));
        o.mode   = OpticalMode(opt.value("mode").toInt(int(o.mode)));
        const QJsonObject wl = opt["wavelength"].toObject();
        o.lambdaMin = wl.value("min_nm").toDouble(o.lambdaMin);
        o.lambdaMax = wl.value("max_nm").toDouble(o.lambdaMax);
        o.lambdaDiv = wl.value("div").toInt(o.lambdaDiv);
        const QJsonObject rc = opt["rcwa"].toObject();
        o.rcwaNx = rc.value("nx").toInt(o.rcwaNx);
        o.rcwaNy = rc.value("ny").toInt(o.rcwaNy);
        o.rcwaPeriodX = rc.value("period_x_nm").toDouble(o.rcwaPeriodX);
        o.rcwaPeriodY = rc.value("period_y_nm").toDouble(o.rcwaPeriodY);
        o.rcwaLayers = rc.value("layers").toInt(o.rcwaLayers);
        // 層スタック — 欠落していれば .ofd から読んだ値 (無ければ空リスト)
        // をそのまま残す (旧ファイル互換)。
        if (rc.contains("layer_list")) {
            o.rcwaLayerList.clear();
            for (const QJsonValue &v : rc["layer_list"].toArray()) {
                const QJsonObject lo = v.toObject();
                RcwaLayer l;
                l.eps1 = lo.value("eps1").toDouble(l.eps1);
                l.eps2 = lo.value("eps2").toDouble(l.eps2);
                l.fill = lo.value("fill").toDouble(l.fill);
                l.thickness_nm =
                    lo.value("thickness_nm").toDouble(l.thickness_nm);
                o.rcwaLayerList.push_back(l);
            }
        }
        const QJsonObject bp = opt["bpm"].toObject();
        o.bpmAlgorithm = bp.value("algorithm").toInt(o.bpmAlgorithm);
        o.bpmDz = bp.value("dz_nm").toDouble(o.bpmDz);
        o.bpmRefIndex = bp.value("ref_index").toDouble(o.bpmRefIndex);
        o.bpmInputMode = bp.value("input_mode").toInt(o.bpmInputMode);
        const QJsonObject fm = opt["fmm"].toObject();
        o.fmmHarmonics = fm.value("harmonics").toInt(o.fmmHarmonics);
        o.fmmLiRules = fm.value("li_rules").toBool(o.fmmLiRules);
        const QJsonObject bpf = opt["bpf"].toObject();
        const QJsonArray band = bpf["band_nm"].toArray();
        if (band.size() >= 2) {
            o.bpfBandMin = band[0].toDouble(o.bpfBandMin);
            o.bpfBandMax = band[1].toDouble(o.bpfBandMax);
        }
        o.bpfQ = bpf.value("Q").toDouble(o.bpfQ);
        // BPF 設計目標 — 欠落キーは既定値のまま (旧ファイル互換)
        o.bpfIL_dB = bpf.value("il_db").toDouble(o.bpfIL_dB);
        o.bpfStop_dB = bpf.value("stop_db").toDouble(o.bpfStop_dB);
        const QJsonObject ring = opt["ring"].toObject();
        o.ringRadius_um = ring.value("radius_um").toDouble(o.ringRadius_um);
        o.ringGap_nm = ring.value("gap_nm").toDouble(o.ringGap_nm);
        o.ringThruPort = ring.value("thru_port").toBool(o.ringThruPort);
        o.ringDropPort = ring.value("drop_port").toBool(o.ringDropPort);
        // ── 光解析モード別設定 — 欠落キーは既定値のまま (旧ファイル互換)。
        // コンボの index になる int は壊れたファイルの範囲外値で不正な
        // 選択を作らないようクランプする (solverBackend と同じ流儀)。
        const QJsonObject wg = opt["waveguide"].toObject();
        o.wgTE0 = wg.value("te0").toBool(o.wgTE0);
        o.wgTE1 = wg.value("te1").toBool(o.wgTE1);
        o.wgTM0 = wg.value("tm0").toBool(o.wgTM0);
        o.wgTM1 = wg.value("tm1").toBool(o.wgTM1);
        o.wgLoss_dBcm = wg.value("loss_db_cm").toDouble(o.wgLoss_dBcm);
        const QJsonObject mzi = opt["mzi"].toObject();
        o.mziDeltaL_um = mzi.value("delta_l_um").toDouble(o.mziDeltaL_um);
        o.mziThermo = mzi.value("thermo").toBool(o.mziThermo);
        o.mziElectro = mzi.value("electro").toBool(o.mziElectro);
        const QJsonObject meta = opt["metasurface"].toObject();
        o.metaPeriod_nm = meta.value("period_nm").toDouble(o.metaPeriod_nm);
        o.metaShape = qBound(0, meta.value("shape").toInt(o.metaShape), 2);
        o.metaPhase = qBound(0, meta.value("phase").toInt(o.metaPhase), 2);
        const QJsonObject phc = opt["phc"].toObject();
        o.phcLattice = qBound(0, phc.value("lattice").toInt(o.phcLattice), 2);
        o.phcA_nm = phc.value("a_nm").toDouble(o.phcA_nm);
        o.phcRoverA = phc.value("r_over_a").toDouble(o.phcRoverA);
        o.phcBand = phc.value("band").toBool(o.phcBand);
        o.phcDefect = phc.value("defect").toBool(o.phcDefect);
        const QJsonObject nfff = opt["nf2ff"].toObject();
        o.nfffSurface = qBound(0, nfff.value("surface").toInt(o.nfffSurface), 1);
        o.nfffDistance_lambda =
            nfff.value("distance_lambda").toDouble(o.nfffDistance_lambda);
        const QJsonObject sp = opt["sparam"].toObject();
        o.spPorts = qBound(1, sp.value("ports").toInt(o.spPorts), 64);
        o.spPortIn = qBound(1, sp.value("port_in").toInt(o.spPortIn), o.spPorts);
        o.spPortOut =
            qBound(1, sp.value("port_out").toInt(o.spPortOut), o.spPorts);
        o.spS11 = sp.value("s11").toBool(o.spS11);
        o.spS21 = sp.value("s21").toBool(o.spS21);
        o.spPhase = sp.value("phase").toBool(o.spPhase);
        o.spGroupDelay = sp.value("group_delay").toBool(o.spGroupDelay);
        // 非線形 (TPA) / ONN 活性化 — 欠落キーは既定値のまま (旧ファイル互換)
        const QJsonObject tpa = opt["tpa"].toObject();
        o.tpaEnabled = tpa.value("enabled").toBool(o.tpaEnabled);
        o.tpaMaterialId = tpa.value("material_id").toInt(o.tpaMaterialId);
        o.tpaBeta_cmGW = tpa.value("beta_cm_gw").toDouble(o.tpaBeta_cmGW);
        const QJsonObject ps = opt["powersweep"].toObject();
        o.powerSweepEnabled = ps.value("enabled").toBool(o.powerSweepEnabled);
        o.psPmin_W = ps.value("pmin_w").toDouble(o.psPmin_W);
        o.psPmax_W = ps.value("pmax_w").toDouble(o.psPmax_W);
        o.psPoints = ps.value("points").toInt(o.psPoints);
        if (ps.contains("scale"))
            o.psLog = (ps.value("scale").toString() != QLatin1String("lin"));
    }
    if (root.contains("acoustic")) {
        const QJsonObject ac = root["acoustic"].toObject();
        AcousticOpts &a = p.acoustic();
        a.rt60 = ac.value("rt60").toBool(a.rt60);
        a.c80  = ac.value("c80").toBool(a.c80);
        a.d50  = ac.value("d50").toBool(a.d50);
        a.sti  = ac.value("sti").toBool(a.sti);
        a.edt  = ac.value("edt").toBool(a.edt);
        a.impulseResponse = ac.value("impulse_response").toBool(a.impulseResponse);
        a.auralization = ac.value("auralization").toBool(a.auralization);
        a.sampleRate = ac.value("sample_rate").toInt(a.sampleRate);
        a.srcDirectivity = ac.value("src_directivity").toString(a.srcDirectivity);
        a.srcSPL_dB = ac.value("src_spl_db").toDouble(a.srcSPL_dB);
        a.micCount = ac.value("mic_count").toInt(a.micCount);
        // AcousticTab 追加設定 — 欠落キーは既定値のまま (旧ファイル互換)。
        // コンボの index になる int は範囲外値で不正な選択を作らないよう
        // クランプする (metaShape / solverBackend と同じ流儀)。
        a.lf = ac.value("lf").toBool(a.lf);
        const QJsonArray srcPos = ac["src_pos_m"].toArray();
        if (srcPos.size() >= 3) {
            a.srcX_m = srcPos[0].toDouble(a.srcX_m);
            a.srcY_m = srcPos[1].toDouble(a.srcY_m);
            a.srcZ_m = srcPos[2].toDouble(a.srcZ_m);
        }
        const QJsonArray srcAim = ac["src_aim_deg"].toArray();
        if (srcAim.size() >= 2) {
            a.srcAimTheta_deg = srcAim[0].toDouble(a.srcAimTheta_deg);
            a.srcAimPhi_deg   = srcAim[1].toDouble(a.srcAimPhi_deg);
        }
        a.analysisType =
            qBound(0, ac.value("analysis_type").toInt(a.analysisType), 2);
        a.thirdOctave = ac.value("third_octave").toBool(a.thirdOctave);
        a.bandRange = qBound(0, ac.value("band_range").toInt(a.bandRange), 2);
        a.roomL = ac.value("room_l").toDouble(a.roomL);
        a.roomW = ac.value("room_w").toDouble(a.roomW);
        a.roomH = ac.value("room_h").toDouble(a.roomH);
        a.volume = ac.value("volume").toDouble(a.volume);
        a.surface = ac.value("surface").toDouble(a.surface);
        a.occupancy = ac.value("occupancy").toInt(a.occupancy);
        a.rtFormula = ac.value("rt_formula").toInt(a.rtFormula);
        if (ac.contains("absorption")) {
            a.absorption.clear();
            for (const QJsonValue &v : ac["absorption"].toArray()) {
                const QJsonObject o = v.toObject();
                AbsorptionRow r;
                r.enabled = o.value("enabled").toBool(true);
                r.role = o.value("role").toInt(AbsorptionRow::Other);
                r.name = o.value("name").toString();
                r.area = o.value("area").toDouble();
                const QJsonArray alpha = o["alpha"].toArray();
                for (int i = 0; i < 6 && i < alpha.size(); ++i)
                    r.alpha[i] = alpha[i].toDouble();
                r.airA = o.value("air_a").toDouble();
                a.absorption.push_back(r);
            }
        }
        if (ac.contains("noise_levels")) {
            const QJsonArray noise = ac["noise_levels"].toArray();
            for (int i = 0; i < 7 && i < noise.size(); ++i)
                a.noiseLevels[i] = noise[i].toDouble();
        }
        // 騒音源内訳 — キー欠落時は既定 4 行のまま (旧ファイル互換)
        if (ac.contains("noise_sources")) {
            a.noiseSources.clear();
            for (const QJsonValue &v : ac["noise_sources"].toArray()) {
                const QJsonObject o = v.toObject();
                NoiseSourceRow r;
                r.enabled = o.value("enabled").toBool(true);
                r.name = o.value("name").toString();
                r.level_dBA = o.value("level_dba").toDouble();
                r.measure = o.value("measure").toString();
                a.noiseSources.push_back(r);
            }
        }
        // 音源リスト — 追加キー。キーが無い旧ファイルは既定 3 行のまま
        // (旧ファイル互換)。空配列は「音源なし」として尊重する。
        if (ac.contains("sources")) {
            a.sources.clear();
            for (const QJsonValue &v : ac["sources"].toArray())
                a.sources.push_back(sourceRowFromJson(v.toObject()));
        }
        // 受音点リスト — 追加キー。キーが無い旧ファイルは既存の mic_count 個
        // の既定点で埋め、「受音点数」と表の行数が食い違わないようにする。
        // 読み込み後は必ず micCount = receivers.size() (同一データの不変条件)。
        if (ac.contains("receivers")) {
            a.receivers.clear();
            for (const QJsonValue &v : ac["receivers"].toArray()) {
                const QJsonObject o = v.toObject();
                ReceiverRow r;
                r.enabled = o.value("enabled").toBool(true);
                const QJsonArray pos = o["pos_m"].toArray();
                if (pos.size() >= 3) {
                    r.x = pos[0].toDouble(r.x);
                    r.y = pos[1].toDouble(r.y);
                    r.z = pos[2].toDouble(r.z);
                }
                r.type = qBound(0, o.value("type").toInt(r.type), 2);
                r.name = o.value("name").toString();
                // 追加キー rir_file — 無い旧ファイルは空 (未指定) のまま
                r.rirFile = o.value("rir_file").toString();
                a.receivers.push_back(r);
            }
        }
        // キーが無い / 空の旧ファイルは mic_count 個の既定点へ (受音点は 1 点以上)
        if (!ac.contains("receivers") || a.receivers.isEmpty())
            a.receivers = defaultReceivers(qBound(1, a.micCount, 256));
        a.micCount = a.receivers.size();
        // 実測 RIR 分析設定 — 欠落キーは既定値のまま (旧ファイル互換)
        if (ac.contains("opera_analysis")) {
            const QJsonObject oa = ac["opera_analysis"].toObject();
            OperaAcousticSettings &s = p.operaAcoustic();
            s.enabled = oa.value("enabled").toBool(s.enabled);
            s.rirPath = oa.value("rir_file").toString(s.rirPath);
            s.voicePath = oa.value("voice_file").toString(s.voicePath);
            s.voiceType = oa.value("voice_type").toInt(s.voiceType);
            s.calibrationState =
                oa.value("calibration_state").toInt(s.calibrationState);
            s.calibrationOffsetDb =
                oa.value("calibration_offset_db").toDouble(s.calibrationOffsetDb);
            s.directSoundMethod =
                oa.value("direct_sound_method").toInt(s.directSoundMethod);
            s.bandMode = oa.value("band_mode").toInt(s.bandMode);
            s.channelMode = oa.value("channel_mode").toInt(s.channelMode);
            const QJsonObject as = oa["analysis_settings"].toObject();
            s.noiseCorrection =
                as.value("noise_correction").toBool(s.noiseCorrection);
            s.minimumDynamicRangeDb =
                as.value("minimum_dynamic_range_db")
                    .toDouble(s.minimumDynamicRangeDb);
            // 可聴化 / 歌声分析 — 欠落キーは既定値のまま (旧ファイル互換)
            const QJsonObject au = oa["auralization"].toObject();
            s.auralizationDryFile =
                au.value("dry_file").toString(s.auralizationDryFile);
            s.auralizationOutputFile =
                au.value("output_file").toString(s.auralizationOutputFile);
            s.auralizationGainMode =
                au.value("gain_mode").toInt(s.auralizationGainMode);
            const QJsonObject vo = oa["vocal"].toObject();
            s.vocalF0MinHz = vo.value("f0_min_hz").toDouble(s.vocalF0MinHz);
            s.vocalF0MaxHz = vo.value("f0_max_hz").toDouble(s.vocalF0MaxHz);
            // 音響ソルバー連携 — 欠落キーは既定値のまま (旧ファイル互換)
            const QJsonObject so = oa["solver"].toObject();
            // 壊れたファイルの範囲外値で不正 enum を作らない (0..4 にクランプ)
            s.solverBackend =
                qBound(0, so.value("backend").toInt(s.solverBackend), 4);
            s.solverExecutable =
                so.value("executable").toString(s.solverExecutable);
            s.solverThreads = so.value("threads").toInt(s.solverThreads);
            s.solverProcesses = so.value("processes").toInt(s.solverProcesses);
        }
    }
    if (root.contains("underwater")) {
        const QJsonObject uw = root["underwater"].toObject();
        UnderwaterOpts &u = p.underwater();
        u.waterTemp_C = uw.value("temp_C").toDouble(u.waterTemp_C);
        u.salinity_psu = uw.value("salinity_psu").toDouble(u.salinity_psu);
        if (uw.contains("ssp")) {
            u.ssp.clear();
            for (const QJsonValue &v : uw["ssp"].toArray()) {
                const QJsonObject o = v.toObject();
                u.ssp.push_back({ o.value("depth_m").toDouble(),
                                  o.value("c_mps").toDouble() });
            }
        }
        u.sofar = uw.value("sofar").toBool(u.sofar);
        u.bottomType = uw.value("bottom_type").toString(u.bottomType);
        u.bottomC_mps = uw.value("bottom_c_mps").toDouble(u.bottomC_mps);
        u.bottomRho_kgm3 = uw.value("bottom_rho_kgm3").toDouble(u.bottomRho_kgm3);
        u.bottomAlpha_dBlambda =
            uw.value("bottom_alpha_db_lambda").toDouble(u.bottomAlpha_dBlambda);
        u.sonarFreq_kHz = uw.value("sonar_freq_khz").toDouble(u.sonarFreq_kHz);
        u.sonarSL_dB = uw.value("sonar_sl_db").toDouble(u.sonarSL_dB);
        u.rangeMax_km = uw.value("range_max_km").toDouble(u.rangeMax_km);
        // ソナー送信源リスト — 追加キー。欠落時は既定 2 行のまま (旧ファイル互換)
        if (uw.contains("sources")) {
            u.sources.clear();
            for (const QJsonValue &v : uw["sources"].toArray())
                u.sources.push_back(sourceRowFromJson(v.toObject()));
        }
        // 測点・地形・Bellhop 設定 — いずれも追加キー。欠落時は既定値のまま。
        if (uw.contains("site")) {
            const QJsonObject st = uw["site"].toObject();
            u.siteLat_deg = st.value("lat_deg").toDouble(u.siteLat_deg);
            u.siteLon_deg = st.value("lon_deg").toDouble(u.siteLon_deg);
            u.trackBearing_deg =
                st.value("bearing_deg").toDouble(u.trackBearing_deg);
        }
        if (uw.contains("bathymetry")) {
            const QJsonObject bt = uw["bathymetry"].toObject();
            u.bathySource = bt.value("source").toString(u.bathySource);
            u.bathymetry.clear();
            for (const QJsonValue &v : bt["points"].toArray()) {
                const QJsonObject o = v.toObject();
                u.bathymetry.push_back({ o.value("range_km").toDouble(),
                                         o.value("depth_m").toDouble() });
            }
        }
        if (uw.contains("bellhop")) {
            const QJsonObject bh = uw["bellhop"].toObject();
            u.runMode = bh.value("run_mode").toString(u.runMode);
            u.beamType = bh.value("beam_type").toString(u.beamType);
            u.numRays = bh.value("num_rays").toInt(u.numRays);
            u.angleMin_deg = bh.value("angle_min_deg").toDouble(u.angleMin_deg);
            u.angleMax_deg = bh.value("angle_max_deg").toDouble(u.angleMax_deg);
            u.srcDepth_m = bh.value("src_depth_m").toDouble(u.srcDepth_m);
            u.numRcvDepth = bh.value("num_rcv_depth").toInt(u.numRcvDepth);
            u.numRcvRange = bh.value("num_rcv_range").toInt(u.numRcvRange);
        }
    }
    if (root.contains("tidy3d")) {
        const QJsonObject t3 = root["tidy3d"].toObject();
        Tidy3dOpts &t = p.tidy3d();
        t.projectName = t3.value("project_name").toString(t.projectName);
        t.resolution = t3.value("resolution").toString(t.resolution);
        t.autoPml = t3.value("auto_pml").toBool(t.autoPml);
    }
    // ジオメトリ拡張 — キーが無い旧ファイルは細分化領域なし (既定値のまま)
    if (root.contains("geometry")) {
        const QJsonObject geo = root["geometry"].toObject();
        if (geo.contains("refine_regions")) {
            QVector<RefineRegion> &regs = p.refineRegions();
            regs.clear();
            for (const QJsonValue &v : geo["refine_regions"].toArray()) {
                const QJsonObject o = v.toObject();
                RefineRegion r;
                r.enabled = o.value("enabled").toBool(true);
                r.name = o.value("name").toString();
                const QJsonArray lo = o["min_m"].toArray();
                const QJsonArray hi = o["max_m"].toArray();
                for (int a = 0; a < 3; ++a) {
                    if (a < lo.size()) r.min_m[a] = lo[a].toDouble(r.min_m[a]);
                    if (a < hi.size()) r.max_m[a] = hi[a].toDouble(r.max_m[a]);
                }
                r.ratio = o.value("ratio").toDouble(r.ratio);
                regs.push_back(r);
            }
        }
    }
    // 回路系電磁解析のポート定義 — キーが無い旧ファイルは既定 3 行のまま
    if (root.contains("circuit")) {
        const QJsonObject cj = root["circuit"].toObject();
        if (cj.contains("ports")) {
            QVector<CircuitPortRow> &ports = p.circuitPorts();
            ports.clear();
            for (const QJsonValue &v : cj["ports"].toArray()) {
                const QJsonObject o = v.toObject();
                CircuitPortRow r;
                r.enabled = o.value("enabled").toBool(true);
                r.name = o.value("name").toString();
                r.kind = qBound(0, o.value("kind").toInt(r.kind), 1);
                r.net = o.value("net").toString();
                r.ref = o.value("ref").toString();
                ports.push_back(r);
            }
        }
    }
    // フォトニック回路のネットリスト — キーが無い旧ファイルは既定 5 行のまま
    if (root.contains("schematic")) {
        const QJsonObject sj = root["schematic"].toObject();
        if (sj.contains("netlist")) {
            QVector<PhotonicNetRow> &net = p.photonicNetlist();
            net.clear();
            for (const QJsonValue &v : sj["netlist"].toArray()) {
                const QJsonObject o = v.toObject();
                PhotonicNetRow r;
                r.enabled = o.value("enabled").toBool(true);
                r.from = o.value("from").toString();
                r.to = o.value("to").toString();
                r.wavelength = o.value("wavelength").toString();
                net.push_back(r);
            }
        }
    }
    // モニター定義 — キーが無い旧ファイルは *読み込んだドメイン* の既定行。
    // (ドメインは冒頭の setActiveDomain で確定済み)
    if (root.contains("monitors")) {
        QVector<MonitorRow> &mons = p.monitors();
        mons.clear();
        for (const QJsonValue &v : root["monitors"].toArray()) {
            const QJsonObject o = v.toObject();
            MonitorRow r;
            r.enabled = o.value("enabled").toBool(true);
            r.type = o.value("type").toString();
            r.name = o.value("name").toString();
            r.region = o.value("region").toString();
            r.band = o.value("band").toString();
            mons.push_back(r);
        }
    } else {
        p.monitors() = defaultMonitors(p.activeDomain());
    }
    // 解析グループ — 同上
    if (root.contains("analysis_groups")) {
        QVector<AnalysisGroupRow> &grps = p.analysisGroups();
        grps.clear();
        for (const QJsonValue &v : root["analysis_groups"].toArray()) {
            const QJsonObject o = v.toObject();
            AnalysisGroupRow r;
            r.enabled = o.value("enabled").toBool(false);
            r.name = o.value("name").toString();
            r.monitors = o.value("monitors").toString();
            r.output = o.value("output").toString();
            grps.push_back(r);
        }
    } else {
        p.analysisGroups() = defaultAnalysisGroups(p.activeDomain());
    }
    // ディスプレイ / AR-VR 光学 — キーが無い旧ファイルは既定値のまま
    if (root.contains("display_optics")) {
        const QJsonObject dj = root["display_optics"].toObject();
        DisplayOpticsOpts &d = p.displayOptics();
        d.device = qBound(0, dj.value("device").toInt(d.device), 3);
        const QJsonObject wg = dj["waveguide"].toObject();
        d.wgType = qBound(0, wg.value("type").toInt(d.wgType), 3);
        d.subThick_mm = wg.value("substrate_thickness_mm").toDouble(d.subThick_mm);
        d.subIndex = wg.value("substrate_index").toDouble(d.subIndex);
        d.gratPeriod_nm = wg.value("period_nm").toDouble(d.gratPeriod_nm);
        d.gratDepth_nm = wg.value("depth_nm").toDouble(d.gratDepth_nm);
        d.gratSlant_deg = wg.value("slant_deg").toDouble(d.gratSlant_deg);
        d.threeGratings = wg.value("three_gratings").toBool(d.threeGratings);
        d.rcwaOptimize = wg.value("rcwa_optimize").toBool(d.rcwaOptimize);
        d.designLambda_nm = wg.value("design_lambda_nm").toDouble(d.designLambda_nm);
        d.guideMaxAngle_deg =
            wg.value("guide_max_angle_deg").toDouble(d.guideMaxAngle_deg);
        d.outcouplerLen_mm = wg.value("outcoupler_len_mm").toDouble(d.outcouplerLen_mm);
        d.eyeRelief_mm = wg.value("eye_relief_mm").toDouble(d.eyeRelief_mm);
        const QJsonObject tg = dj["targets"].toObject();
        d.fovTarget_deg = tg.value("fov_deg").toDouble(d.fovTarget_deg);
        d.eyeboxTarget_mm = tg.value("eyebox_mm").toDouble(d.eyeboxTarget_mm);
        d.seeThroughTarget_pct =
            tg.value("see_through_pct").toDouble(d.seeThroughTarget_pct);
        const QJsonObject ol = dj["oled"].toObject();
        d.bottomEmission = ol.value("bottom_emission").toBool(d.bottomEmission);
        d.topEmission = ol.value("top_emission").toBool(d.topEmission);
        d.microcavity = ol.value("microcavity").toBool(d.microcavity);
        d.sepIqe = ol.value("separate_iqe").toBool(d.sepIqe);
        d.sepSpp = ol.value("separate_spp").toBool(d.sepSpp);
        d.sepWaveguide = ol.value("separate_waveguide").toBool(d.sepWaveguide);
        d.outcouplingStruct =
            qBound(0, ol.value("structure").toInt(d.outcouplingStruct), 3);
        d.oledIndex = ol.value("index").toDouble(d.oledIndex);
        d.oledIqe = ol.value("iqe").toDouble(d.oledIqe);
        const QJsonObject ml = dj["microled"].toObject();
        d.chipSize_um = ml.value("chip_size_um").toDouble(d.chipSize_um);
        d.sidewallRecomb = ml.value("sidewall_recomb").toBool(d.sidewallRecomb);
        d.sidewallDbr = ml.value("sidewall_dbr").toBool(d.sidewallDbr);
        d.directional = ml.value("directional").toBool(d.directional);
        d.mlIndex = ml.value("index").toDouble(d.mlIndex);
        d.mlIqe = ml.value("iqe").toDouble(d.mlIqe);
        d.mlSurfVel_cm_s =
            ml.value("surface_velocity_cm_s").toDouble(d.mlSurfVel_cm_s);
        d.mlLifetime_ns = ml.value("lifetime_ns").toDouble(d.mlLifetime_ns);
        const QJsonObject lc = dj["lcd"].toObject();
        d.lcdMode = qBound(0, lc.value("mode").toInt(d.lcdMode), 2);
        d.lcAnisotropy = lc.value("anisotropy").toBool(d.lcAnisotropy);
        d.compFilm = lc.value("comp_film").toBool(d.compFilm);
        d.lcdPeakLum_cdm2 =
            lc.value("peak_luminance_cdm2").toDouble(d.lcdPeakLum_cdm2);
        d.lcdDarkroomCr = lc.value("darkroom_cr").toDouble(d.lcdDarkroomCr);
        d.lcdAmbient_lx = lc.value("ambient_lx").toDouble(d.lcdAmbient_lx);
        d.lcdReflectance = lc.value("reflectance").toDouble(d.lcdReflectance);
    }
    // 照明光学・測色 — キーが無い旧ファイルは既定値のまま
    if (root.contains("illumination")) {
        const QJsonObject ij = root["illumination"].toObject();
        IlluminationOpts &i = p.illumination();
        i.app = qBound(0, ij.value("app").toInt(i.app), 3);
        i.srcModel = qBound(0, ij.value("source_model").toInt(i.srcModel), 2);
        i.rayFile = ij.value("ray_file").toString(i.rayFile);
        i.spectrum = qBound(0, ij.value("spectrum").toInt(i.spectrum), 3);
        i.flux_lm = ij.value("flux_lm").toDouble(i.flux_lm);
        i.rays = ij.value("rays").toDouble(i.rays);
        const QJsonObject op = ij["optics"].toObject();
        i.reflector = op.value("reflector").toBool(i.reflector);
        i.tirLens = op.value("tir_lens").toBool(i.tirLens);
        i.diffuser = op.value("diffuser").toBool(i.diffuser);
        i.lightGuide = op.value("light_guide").toBool(i.lightGuide);
        i.phosphor = op.value("phosphor").toBool(i.phosphor);
        i.surface = qBound(0, op.value("surface").toInt(i.surface), 3);
        const QJsonObject wl = ij["white_led"].toObject();
        i.bluePeak_nm = wl.value("blue_peak_nm").toDouble(i.bluePeak_nm);
        i.blueFwhm_nm = wl.value("blue_fwhm_nm").toDouble(i.blueFwhm_nm);
        i.phosPeak_nm = wl.value("phosphor_peak_nm").toDouble(i.phosPeak_nm);
        i.phosFwhm_nm = wl.value("phosphor_fwhm_nm").toDouble(i.phosFwhm_nm);
        i.phosRatio = wl.value("phosphor_ratio").toDouble(i.phosRatio);
        const QJsonObject rgb = ij["rgb"].toObject();
        i.rPeak_nm = rgb.value("r_peak_nm").toDouble(i.rPeak_nm);
        i.rFwhm_nm = rgb.value("r_fwhm_nm").toDouble(i.rFwhm_nm);
        i.rRatio = rgb.value("r_ratio").toDouble(i.rRatio);
        i.gPeak_nm = rgb.value("g_peak_nm").toDouble(i.gPeak_nm);
        i.gFwhm_nm = rgb.value("g_fwhm_nm").toDouble(i.gFwhm_nm);
        i.gRatio = rgb.value("g_ratio").toDouble(i.gRatio);
        i.bPeak_nm = rgb.value("b_peak_nm").toDouble(i.bPeak_nm);
        i.bFwhm_nm = rgb.value("b_fwhm_nm").toDouble(i.bFwhm_nm);
        i.bRatio = rgb.value("b_ratio").toDouble(i.bRatio);
        i.blackbody_K = ij.value("blackbody_k").toDouble(i.blackbody_K);
        const QJsonObject mo = ij["mono"].toObject();
        i.monoPeak_nm = mo.value("peak_nm").toDouble(i.monoPeak_nm);
        i.monoFwhm_nm = mo.value("fwhm_nm").toDouble(i.monoFwhm_nm);
        const QJsonObject tg = ij["targets"].toObject();
        i.cctTarget_K = tg.value("cct_k").toDouble(i.cctTarget_K);
        i.cctTol_K = tg.value("cct_tol_k").toDouble(i.cctTol_K);
        i.duvTol = tg.value("duv_tol").toDouble(i.duvTol);
    }
    return true;
}
