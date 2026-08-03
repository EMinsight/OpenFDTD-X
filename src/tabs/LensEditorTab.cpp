// LensEditorTab.cpp
#include "LensEditorTab.h"
#include "TabHelpers.h"
#include "../core/GlassCatalog.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QCompleter>
#include <QFont>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

using namespace ofd;

// ── タブ専用の翻訳キー (接頭辞 lde_) ────────────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("lde_lde_section", "Lens Data Editor (Zemax風)",
              "Lens Data Editor (Zemax style)");
    I18n::reg("lde_table_hint",
              "順次光線追跡用の光学系定義。各行が1つの面 (R=曲率半径、厚さ、ガラス材質、半径)。最上行=物体、最下行=像面。\n"
              "ガラス欄はカタログ自動補完 (N-BK7, S-LAH64… と入力)。「🔷 ガラスカタログ」タブでガラスマップから選択も可能。",
              "Optical system definition for sequential ray tracing. One row per surface (R=radius, thickness, glass, semi-diameter). Top row = object, bottom row = image plane.\n"
              "The glass column auto-completes from the catalog (type N-BK7, S-LAH64…). You can also pick from the glass map in the 🔷 Glass Catalog tab.");
    I18n::reg("lde_col_type",    "種別",        "Type");
    I18n::reg("lde_col_r",       "曲率R [mm]",  "Radius R [mm]");
    I18n::reg("lde_col_thick",   "厚さ [mm]",   "Thickness [mm]");
    I18n::reg("lde_col_glass",   "ガラス",      "Glass");
    I18n::reg("lde_col_semid",   "半径",        "Semi-dia.");
    I18n::reg("lde_col_conic",   "コーニック",  "Conic");
    I18n::reg("lde_col_comment", "コメント",    "Comment");
    I18n::reg("lde_ins_tip",     "次に挿入",    "Insert after");
    I18n::reg("lde_del_tip",     "削除",        "Delete");
    I18n::reg("lde_types_label", "面種別:",     "Surface types:");
    I18n::reg("lde_type_obj",    "OBJ=物体",    "OBJ=object");
    I18n::reg("lde_type_std",    "STD=球面",    "STD=sphere");
    I18n::reg("lde_type_sto",    "STO=絞り",    "STO=stop");
    I18n::reg("lde_type_asp",    "ASP=非球面",  "ASP=asphere");
    I18n::reg("lde_type_fre",    "FRE=自由曲面","FRE=freeform");
    I18n::reg("lde_type_img",    "IMG=像面",    "IMG=image");
    I18n::reg("lde_sys_section", "システム諸元 / System spec", "System spec");
    I18n::reg("lde_epd",         "入射瞳径",    "Entrance pupil dia.");
    I18n::reg("lde_field",       "視野",        "Field");
    I18n::reg("lde_field_unit",  "° (半角)",    "° (half angle)");
    I18n::reg("lde_waves",       "波長サンプル", "Wavelength samples");
    I18n::reg("lde_wave_add",    "+ 追加",      "+ Add");
    I18n::reg("lde_wave_note",   "(プレビューは d 線のみ)",
              "(preview traces the d line only)");
    I18n::reg("lde_coord",       "座標系",      "Coordinate system");
    I18n::reg("lde_coord_seq",   "順次光線追跡", "Sequential ray tracing");
    I18n::reg("lde_coord_nonseq","非順次 (LightTools/TracePro) (未実装)",
              "Non-sequential (LightTools/TracePro) (not implemented)");
    I18n::reg("lde_coord_hybrid","ハイブリッド (未実装)",
              "Hybrid (not implemented)");
    I18n::reg("lde_merit_section","Merit Function (FoM)", "Merit Function (FoM)");
    I18n::reg("lde_merit_hint",
              "最適化評価関数の定義 (最適化は未実装 — 定義の記録のみ)。",
              "Merit-function definition (optimization is not implemented — "
              "the definition is only recorded).");
    I18n::reg("lde_col_operand", "オペランド",  "Operand");
    I18n::reg("lde_col_target",  "目標",        "Target");
    I18n::reg("lde_col_weight",  "重み",        "Weight");
    I18n::reg("lde_col_value",   "値",          "Value");
    I18n::reg("lde_op_spha",     "SPHA (球面収差)", "SPHA (spherical aberration)");
    I18n::reg("lde_op_asti",     "ASTI (非点)",     "ASTI (astigmatism)");
    I18n::reg("lde_op_effl",     "EFFL (有効焦点)", "EFFL (effective focal length)");
    I18n::reg("lde_op_dist",     "DIST (歪曲)",     "DIST (distortion)");
    I18n::reg("lde_optimize",    "▶ 最適化実行",  "▶ Run optimization");
    I18n::reg("lde_analyses_section", "解析プロット / Analyses", "Analyses");
    I18n::reg("lde_an_mtf",      "MTF (変調伝達関数)",
              "MTF (modulation transfer function)");
    I18n::reg("lde_preview_section", "レイトレース プレビュー / Raytrace preview",
              "Raytrace preview");
    I18n::reg("lde_preview_hint",
              "面テーブルから子午面 2D 光線追跡 (軸上=マゼンタ、±視野=赤/青、瞳内5本)。テーブル編集で自動更新。"
              "カタログ外ガラスは n=1.6 と仮定。",
              "2D meridional ray trace from the surface table (on-axis = magenta, ±field = red/blue, 5 pupil rays). Updates as you edit the table. "
              "Glasses not in the catalog are assumed to have n = 1.6.");
    I18n::reg("lde_air",         "空気",        "Air");
    return true;
}();

// mock の <span className="badge"> 相当
QLabel *makeBadge(const QString &text, const char *variant, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    QString ss = QStringLiteral("border-radius:8px; padding:1px 7px; font-size:11px;");
    if (qstrcmp(variant, "acc") == 0)
        ss += "background:#B83280; color:white; border:1px solid transparent;";
    else
        ss += "border:1px solid palette(mid);";
    l->setStyleSheet(ss);
    return l;
}

// muted text-sm 相当
QLabel *mutedLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setStyleSheet("color:#7A7A7A; font-size:11px;");
    return l;
}

// ガラス名 → 屈折率 (nd)。AIR/空欄/"-" は 1.0、カタログ外は代表値 1.6
double indexAfter(const QString &glass)
{
    const QString g = glass.trimmed();
    if (g.isEmpty() || g == QStringLiteral("-") || g == QString::fromUtf8("—")
        || g.compare(QStringLiteral("AIR"), Qt::CaseInsensitive) == 0
        || g == QString::fromUtf8("空気"))
        return 1.0;
    for (const Glass &c : GlassCatalog::all())
        if (c.name.compare(g, Qt::CaseInsensitive) == 0)
            return c.nd;
    return 1.6;
}
} // namespace

// ── LensLayoutView ──────────────────────────────────────────────────────────
LensLayoutView::LensLayoutView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(360, 200);
}

void LensLayoutView::setSystem(const QVector<LensSurface> &rows,
                               double epd, double fieldDeg)
{
    m_surfs.clear();
    m_epd = epd > 0 ? epd : 12.0;
    m_field = fieldDeg;
    double z = 0.0;
    for (const LensSurface &r : rows) {
        if (!r.enabled) continue;
        if (r.type == QStringLiteral("OBJ")) continue;   // 無限遠物体 → 平行光
        Surf s;
        s.z = z;
        bool ok = false;
        const double R = r.R.toDouble(&ok);
        s.plane = !ok || R == 0.0;                        // "Infinity" → 平面
        s.R = s.plane ? 0.0 : R;
        const double sd = r.semiD.toDouble(&ok);
        s.semiD = (ok && sd > 0) ? sd : 7.0;
        s.conic = r.conic.toDouble();
        s.stop  = r.type == QStringLiteral("STO");
        s.image = r.type == QStringLiteral("IMG");
        s.n2 = indexAfter(r.glass);
        m_surfs.push_back(s);
        if (s.image) break;
        const double t = r.thick.toDouble(&ok);
        z += ok ? t : 0.0;
    }
    update();
}

void LensLayoutView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    if (m_surfs.size() < 2) return;

    const double zEnd = m_surfs.last().z;
    if (zEnd <= 0) return;
    double yMax = 1.0;
    for (const Surf &s : m_surfs) yMax = std::max(yMax, s.semiD);
    yMax *= 1.3;
    const double z0 = -std::max(4.0, zEnd * 0.06);   // 入射光の助走区間
    const double W = width(), H = height();
    const double sx = (W - 24) / (zEnd - z0);
    const double sy = (H - 16) / (2.0 * yMax);
    auto X = [&](double z) { return 12.0 + (z - z0) * sx; };
    auto Y = [&](double y) { return H / 2.0 - y * sy; };
    auto sag = [](const Surf &s, double y) {
        if (s.plane) return 0.0;
        const double c = 1.0 / s.R;
        const double arg = 1.0 - (1.0 + s.conic) * c * c * y * y;
        if (arg <= 0.0) return 0.0;
        return c * y * y / (1.0 + std::sqrt(arg));
    };

    // 光軸
    p.setPen(QPen(palette().midlight().color(), 1, Qt::DashLine));
    p.drawLine(QPointF(X(z0), Y(0)), QPointF(X(zEnd), Y(0)));

    // レンズ縁 (ガラス区間の上下端を接続)
    p.setPen(QPen(palette().text().color(), 1.0));
    for (int i = 0; i + 1 < m_surfs.size(); ++i) {
        const Surf &a = m_surfs[i], &b = m_surfs[i + 1];
        if (a.n2 <= 1.001) continue;
        for (double sgn : { 1.0, -1.0 })
            p.drawLine(QPointF(X(a.z + sag(a, a.semiD)), Y(sgn * a.semiD)),
                       QPointF(X(b.z + sag(b, b.semiD)), Y(sgn * b.semiD)));
    }

    // 面
    for (const Surf &s : m_surfs) {
        if (s.image) {                                   // 像面: 縦実線
            p.setPen(QPen(palette().text().color(), 1.6));
            p.drawLine(QPointF(X(s.z), Y(-s.semiD)), QPointF(X(s.z), Y(s.semiD)));
            continue;
        }
        if (s.stop && s.plane) {                         // 絞り: 上下のティック
            p.setPen(QPen(palette().text().color(), 2.0));
            for (double sgn : { 1.0, -1.0 })
                p.drawLine(QPointF(X(s.z), Y(sgn * s.semiD)),
                           QPointF(X(s.z), Y(sgn * s.semiD * 1.35)));
            continue;
        }
        QPolygonF arc;
        const int N = 32;
        for (int k = 0; k <= N; ++k) {
            const double y = -s.semiD + 2.0 * s.semiD * k / N;
            if (!s.plane) {
                const double c = 1.0 / s.R;
                if (1.0 - (1.0 + s.conic) * c * c * y * y <= 0.0) continue;
            }
            arc << QPointF(X(s.z + sag(s, y)), Y(y));
        }
        p.setPen(QPen(palette().text().color(), 1.2));
        p.drawPolyline(arc);
    }

    // 光線追跡 (軸上 + ±視野、各5本)
    struct FieldSpec { double deg; QColor col; };
    const FieldSpec fields[3] = {
        {       0.0, QColor("#B83280") },
        {  m_field,  QColor("#DC2626") },
        { -m_field,  QColor("#2563EB") },
    };
    double zStop = m_surfs[0].z;
    for (const Surf &s : m_surfs)
        if (s.stop) { zStop = s.z; break; }

    for (const FieldSpec &fs : fields) {
        const double a = fs.deg * M_PI / 180.0;
        const double slope = -std::tan(a);
        p.setPen(QPen(QColor(fs.col.red(), fs.col.green(), fs.col.blue(), 170), 1.1));
        for (int k = 0; k < 5; ++k) {
            const double h = (k - 2) / 2.0 * m_epd / 2.0;   // 瞳内高さ
            QPointF pos(z0, h + slope * (z0 - zStop));      // 絞り位置で h を通す
            QPointF dir(std::cos(a), -std::sin(a));
            double n1 = 1.0;
            QPolygonF poly;
            poly << QPointF(X(pos.x()), Y(pos.y()));
            for (const Surf &s : m_surfs) {
                double t = -1;
                QPointF hit, nrm;
                if (s.plane) {
                    if (std::fabs(dir.x()) < 1e-9) break;
                    t = (s.z - pos.x()) / dir.x();
                    if (t < 1e-9) break;
                    hit = pos + t * dir;
                    nrm = QPointF(-1, 0);
                } else {
                    const QPointF C(s.z + s.R, 0.0);
                    const QPointF oc = pos - C;
                    const double b = 2.0 * QPointF::dotProduct(dir, oc);
                    const double c = QPointF::dotProduct(oc, oc) - s.R * s.R;
                    const double disc = b * b - 4.0 * c;
                    if (disc < 0) break;
                    const double sq = std::sqrt(disc);
                    const double t1 = (-b - sq) / 2.0, t2 = (-b + sq) / 2.0;
                    t = (s.R > 0) ? t1 : t2;                // 頂点側の交点
                    if (t < 1e-9) t = (s.R > 0) ? t2 : t1;
                    if (t < 1e-9) break;
                    hit = pos + t * dir;
                    nrm = (hit - C) / s.R;
                }
                poly << QPointF(X(hit.x()), Y(hit.y()));
                if (s.image) break;
                // Snell (2D ベクトル形)
                QPointF n = nrm;
                double cosi = -(dir.x() * n.x() + dir.y() * n.y());
                if (cosi < 0) { n = -n; cosi = -cosi; }
                const double eta = n1 / s.n2;
                const double sin2t = eta * eta * (1.0 - cosi * cosi);
                if (sin2t > 1.0) break;                     // 全反射
                const double cost = std::sqrt(1.0 - sin2t);
                dir = eta * dir + (eta * cosi - cost) * n;
                const double len = std::hypot(dir.x(), dir.y());
                if (len < 1e-12) break;
                dir /= len;
                pos = hit;
                n1 = s.n2;
            }
            p.drawPolyline(poly);
        }
    }
}

// ── LensEditorTab ───────────────────────────────────────────────────────────
LensEditorTab::LensEditorTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    // 初期値: Cooke triplet (mock と同一)
    m_rows = {
        { true, "OBJ", "Infinity", "Infinity", "AIR",     "-",    "0", QString::fromUtf8("Object") },
        { true, "STO", "Infinity", "5.00",     "AIR",     "6.00", "0", QString::fromUtf8("Stop") },
        { true, "STD", "50.230",   "3.260",    "N-LAK10", "7.10", "0", QString::fromUtf8("L1 front") },
        { true, "STD", "-83.430",  "1.250",    "AIR",     "7.05", "0", QString::fromUtf8("L1 back") },
        { true, "STD", "-39.270",  "1.000",    "N-SF10",  "6.30", "0", QString::fromUtf8("L2 front (neg)") },
        { true, "STD", "40.500",   "5.300",    "AIR",     "6.30", "0", QString::fromUtf8("L2 back") },
        { true, "STD", "83.430",   "3.260",    "N-LAK10", "7.50", "0", QString::fromUtf8("L3 front") },
        { true, "STD", "-50.230",  "42.100",   "AIR",     "7.50", "0", QString::fromUtf8("L3 back") },
        { true, "IMG", "Infinity", "-",        "-",       "7.65", "0", QString::fromUtf8("Image plane") },
    };

    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // Lens Data Editor
    auto *sLde = new SectionBox(I18n::tr("lde_lde_section"), body);
    sLde->vbox()->addWidget(mutedLabel(I18n::tr("lde_table_hint"), sLde));

    m_table = new QTableWidget(0, 10, sLde);
    m_table->setHorizontalHeaderLabels({
        "", "#", I18n::tr("lde_col_type"), I18n::tr("lde_col_r"),
        I18n::tr("lde_col_thick"), I18n::tr("lde_col_glass"),
        I18n::tr("lde_col_semid"), I18n::tr("lde_col_conic"),
        I18n::tr("lde_col_comment"), "" });
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(30);
    m_table->setColumnWidth(0, 26);
    m_table->setColumnWidth(1, 26);
    m_table->setColumnWidth(2, 70);
    m_table->setColumnWidth(3, 82);
    m_table->setColumnWidth(4, 82);
    m_table->setColumnWidth(5, 96);
    m_table->setColumnWidth(6, 60);
    m_table->setColumnWidth(7, 64);
    m_table->setColumnWidth(9, 56);
    m_table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);
    m_table->setMinimumHeight(320);
    sLde->vbox()->addWidget(m_table);

    auto *badges = new QHBoxLayout();
    badges->setSpacing(4);
    badges->addWidget(mutedLabel(I18n::tr("lde_types_label"), sLde));
    for (const char *key : { "lde_type_obj", "lde_type_std", "lde_type_sto",
                             "lde_type_asp", "lde_type_fre", "lde_type_img" })
        badges->addWidget(makeBadge(I18n::tr(key), "", sLde));
    badges->addStretch(1);
    sLde->vbox()->addLayout(badges);
    v->addWidget(sLde);

    // システム諸元 / System spec
    auto *sSys = new SectionBox(I18n::tr("lde_sys_section"), body);
    m_epd = new QLineEdit("12.0", sSys);
    m_epd->setMaximumWidth(100);
    auto *epdRow = new QHBoxLayout();
    epdRow->addWidget(m_epd);
    epdRow->addWidget(new QLabel("mm", sSys));
    epdRow->addStretch(1);
    sSys->form()->addRow(I18n::tr("lde_epd"), epdRow);

    m_field = new QLineEdit("20", sSys);
    m_field->setMaximumWidth(60);
    auto *fieldRow = new QHBoxLayout();
    fieldRow->addWidget(m_field);
    fieldRow->addWidget(new QLabel(I18n::tr("lde_field_unit"), sSys));
    fieldRow->addStretch(1);
    sSys->form()->addRow(I18n::tr("lde_field"), fieldRow);

    auto *waveRow = new QHBoxLayout();
    waveRow->setSpacing(4);
    waveRow->addWidget(makeBadge("486.1nm (F)", "", sSys));
    waveRow->addWidget(makeBadge("587.6nm (d)", "acc", sSys));
    waveRow->addWidget(makeBadge("656.3nm (C)", "", sSys));
    auto *waveAdd = new QPushButton(I18n::tr("lde_wave_add"), sSys);
    waveAdd->setFixedHeight(22);
    // 波長追加は未実装 — 無効化して明示する (絶対規則 5)
    tabhelp::markNotImplemented(waveAdd);
    waveRow->addWidget(waveAdd);
    // 3 波長を並べているがプレビューのトレースは d 線単一
    waveRow->addWidget(mutedLabel(I18n::tr("lde_wave_note"), sSys));
    waveRow->addStretch(1);
    sSys->form()->addRow(I18n::tr("lde_waves"), waveRow);

    m_coord = new QComboBox(sSys);
    m_coord->addItem(I18n::tr("lde_coord_seq"));
    m_coord->addItem(I18n::tr("lde_coord_nonseq"));
    m_coord->addItem(I18n::tr("lde_coord_hybrid"));
    // 実装があるのは順次のみ — 未実装の選択肢は選べなくする (絶対規則 5)
    if (auto *model = qobject_cast<QStandardItemModel *>(m_coord->model())) {
        for (int i : { 1, 2 })
            if (auto *item = model->item(i))
                item->setEnabled(false);
    }
    sSys->form()->addRow(I18n::tr("lde_coord"), m_coord);
    v->addWidget(sSys);

    // Merit Function (FoM)
    auto *sMerit = new SectionBox(I18n::tr("lde_merit_section"), body);
    sMerit->vbox()->addWidget(mutedLabel(I18n::tr("lde_merit_hint"), sMerit));
    auto *merit = new QTableWidget(5, 5, sMerit);
    merit->setHorizontalHeaderLabels({
        "#", I18n::tr("lde_col_operand"), I18n::tr("lde_col_target"),
        I18n::tr("lde_col_weight"), I18n::tr("lde_col_value") });
    merit->verticalHeader()->setVisible(false);
    merit->setEditTriggers(QAbstractItemView::NoEditTriggers);
    merit->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    merit->setMinimumHeight(170);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    const struct { QString op; const char *target, *weight, *value; } fom[5] = {
        { I18n::tr("lde_op_spha"), "0.000",  "1.0", "0.018"  },
        { "COMA",                  "0.000",  "1.0", "-0.005" },
        { I18n::tr("lde_op_asti"), "0.000",  "0.8", "0.012"  },
        { I18n::tr("lde_op_effl"), "50.000", "1.0", "50.21"  },
        { I18n::tr("lde_op_dist"), "0.000",  "0.5", "-1.2%"  },
    };
    for (int i = 0; i < 5; ++i) {
        auto num = [](const QString &t) {
            auto *it = new QTableWidgetItem(t);
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return it;
        };
        merit->setItem(i, 0, num(QString::number(i + 1)));
        auto *op = new QTableWidgetItem(fom[i].op);
        op->setFont(mono);
        merit->setItem(i, 1, op);
        merit->setItem(i, 2, num(QString::fromUtf8(fom[i].target)));
        merit->setItem(i, 3, num(QString::fromUtf8(fom[i].weight)));
        merit->setItem(i, 4, num(QString::fromUtf8(fom[i].value)));
    }
    sMerit->vbox()->addWidget(merit);
    // 値列はモック由来の固定値 (絶対規則 5)
    sMerit->vbox()->addWidget(tabhelp::sampleNote(sMerit));
    auto *optRow = new QHBoxLayout();
    // 最適化は未実装 — primary (実行可能な見た目) を外して無効化 (絶対規則 5)
    auto *optBtn = new QPushButton(I18n::tr("lde_optimize"), sMerit);
    tabhelp::markNotImplemented(optBtn);
    optRow->addWidget(optBtn);
    optRow->addWidget(mutedLabel("Damped Least-Squares / Hammer", sMerit));
    optRow->addStretch(1);
    sMerit->vbox()->addLayout(optRow);
    v->addWidget(sMerit);

    // 解析プロット / Analyses
    auto *sAn = new SectionBox(I18n::tr("lde_analyses_section"), body);
    auto *grid = new QGridLayout();
    grid->setSpacing(6);
    const QString analyses[8] = {
        QString::fromUtf8("⊙ Spot Diagram"),
        QString::fromUtf8("📐 Ray Fan"),
        QString::fromUtf8("📊 ") + I18n::tr("lde_an_mtf"),
        QString::fromUtf8("⬡ Encircled Energy"),
        QString::fromUtf8("⌖ Field Curvature"),
        QString::fromUtf8("▦ Distortion grid"),
        QString::fromUtf8("🌈 Chromatic Focal Shift"),
        QString::fromUtf8("⊕ Wavefront map (Zernike)"),
    };
    for (int i = 0; i < 8; ++i) {
        auto *b = new QPushButton(analyses[i], sAn);
        b->setFixedHeight(30);
        b->setStyleSheet("text-align:left; padding-left:8px;");
        // 解析プロットは未実装 — 無効化して明示する (絶対規則 5)
        tabhelp::markNotImplemented(b);
        grid->addWidget(b, i / 2, i % 2);
    }
    sAn->vbox()->addLayout(grid);
    v->addWidget(sAn);

    // レイトレース プレビュー (面テーブル連動)
    auto *sPre = new SectionBox(I18n::tr("lde_preview_section"), body);
    sPre->vbox()->addWidget(mutedLabel(I18n::tr("lde_preview_hint"), sPre));
    m_layout = new LensLayoutView(sPre);
    sPre->vbox()->addWidget(m_layout);
    v->addWidget(sPre);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_table, &QTableWidget::cellChanged, this, [this](int row, int) {
        if (m_updating) return;
        if (row < 0 || row >= m_rows.size()) return;
        syncRowFromTable(row);
        retrace();
    });
    connect(m_epd, &QLineEdit::editingFinished, this, &LensEditorTab::retrace);
    connect(m_field, &QLineEdit::editingFinished, this, &LensEditorTab::retrace);

    rebuildTable();
    retrace();
}

// m_rows → QTableWidget。行の挿入/削除時のみ全再構築する
void LensEditorTab::rebuildTable()
{
    m_updating = true;
    m_table->clearContents();
    m_table->setRowCount(m_rows.size());

    // ガラス補完候補 (mock の datalist 相当): カタログ全銘柄 + AIR
    QStringList glassNames;
    for (const Glass &g : GlassCatalog::all())
        glassNames << g.name;
    glassNames << QStringLiteral("AIR");

    for (int i = 0; i < m_rows.size(); ++i) {
        const LensSurface &r = m_rows[i];

        auto *en = new QTableWidgetItem;
        en->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        en->setCheckState(r.enabled ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(i, 0, en);

        auto *no = new QTableWidgetItem(QString::number(i));
        no->setFlags(Qt::ItemIsEnabled);
        no->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(i, 1, no);

        auto *type = new QComboBox(m_table);
        type->addItems({ "OBJ", "STD", "STO", "ASP", "BIN", "EVN",
                         "HOL", "FRE", "IMG" });
        type->setCurrentText(r.type);
        m_table->setCellWidget(i, 2, type);
        connect(type, &QComboBox::currentTextChanged, this,
                [this, i](const QString &t) {
            if (m_updating || i >= m_rows.size()) return;
            m_rows[i].type = t;
            applyStopHighlight();
            retrace();
        });

        auto num = [](const QString &t) {
            auto *it = new QTableWidgetItem(t);
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return it;
        };
        m_table->setItem(i, 3, num(r.R));
        m_table->setItem(i, 4, num(r.thick));

        auto *gl = new QLineEdit(r.glass, m_table);
        gl->setFrame(false);
        auto *comp = new QCompleter(glassNames, gl);
        comp->setCaseSensitivity(Qt::CaseInsensitive);
        gl->setCompleter(comp);
        m_table->setCellWidget(i, 5, gl);
        connect(gl, &QLineEdit::textChanged, this, [this, i](const QString &t) {
            if (m_updating || i >= m_rows.size()) return;
            m_rows[i].glass = t;
            retrace();
        });

        m_table->setItem(i, 6, num(r.semiD));
        m_table->setItem(i, 7, num(r.conic));
        m_table->setItem(i, 8, new QTableWidgetItem(r.comment));

        // 挿入/削除ボタン (再構築はシグナル脱出後に遅延実行)
        auto *ops = new QWidget(m_table);
        auto *oh = new QHBoxLayout(ops);
        oh->setContentsMargins(2, 2, 2, 2);
        oh->setSpacing(2);
        auto *bIns = new QPushButton("+", ops);
        bIns->setFixedSize(22, 20);
        bIns->setToolTip(I18n::tr("lde_ins_tip"));
        auto *bDel = new QPushButton(QString::fromUtf8("×"), ops);
        bDel->setFixedSize(22, 20);
        bDel->setToolTip(I18n::tr("lde_del_tip"));
        oh->addWidget(bIns);
        oh->addWidget(bDel);
        m_table->setCellWidget(i, 9, ops);
        connect(bIns, &QPushButton::clicked, this, [this, i] {
            QTimer::singleShot(0, this, [this, i] {
                LensSurface s;
                s.type = "STD"; s.R = "Infinity"; s.thick = "0";
                s.glass = "AIR"; s.semiD = "-"; s.conic = "0";
                m_rows.insert(qMin(i + 1, int(m_rows.size())), s);
                rebuildTable();
                retrace();
            });
        });
        connect(bDel, &QPushButton::clicked, this, [this, i] {
            QTimer::singleShot(0, this, [this, i] {
                if (i < 0 || i >= m_rows.size()) return;
                m_rows.remove(i);
                rebuildTable();
                retrace();
            });
        });
    }
    m_updating = false;
    applyStopHighlight();
}

void LensEditorTab::syncRowFromTable(int row)
{
    LensSurface &r = m_rows[row];
    if (auto *en = m_table->item(row, 0))
        r.enabled = en->checkState() == Qt::Checked;
    auto cell = [this, row](int col) {
        auto *it = m_table->item(row, col);
        return it ? it->text() : QString();
    };
    r.R       = cell(3);
    r.thick   = cell(4);
    r.semiD   = cell(6);
    r.conic   = cell(7);
    r.comment = cell(8);
}

// mock の tr className="sel" 相当: STO 行をアクセント色でハイライト
void LensEditorTab::applyStopHighlight()
{
    const bool was = m_updating;
    m_updating = true;
    for (int i = 0; i < m_rows.size(); ++i) {
        const QBrush bg = (m_rows[i].type == QStringLiteral("STO"))
            ? QBrush(QColor(184, 50, 128, 36)) : QBrush();
        for (int c : { 0, 1, 3, 4, 6, 7, 8 })
            if (auto *it = m_table->item(i, c))
                it->setBackground(bg);
    }
    m_updating = was;
}

void LensEditorTab::retrace()
{
    bool ok = false;
    double epd = m_epd->text().toDouble(&ok);
    if (!ok || epd <= 0) epd = 12.0;
    double field = m_field->text().toDouble(&ok);
    if (!ok) field = 20.0;
    m_layout->setSystem(m_rows, epd, field);
}
