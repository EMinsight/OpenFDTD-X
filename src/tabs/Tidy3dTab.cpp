// Tidy3dTab.cpp
#include "Tidy3dTab.h"
#include "../core/Project.h"
#include "../io/Tidy3dExporter.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ固有の翻訳キー (t3x_) — file-local 登録 ─────────────────────────────
// 既存 t3_ キーの一部は I18n.cpp にあり (reg は既存優先なので衝突しても無害)。
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // i18n.js にあって I18n.cpp に未登録のキー
    I18n::reg("t3_credits", "クレジット残高", "Credits");
    I18n::reg("t3_cost", "見積コスト", "Estimated cost");
    I18n::reg("t3_runtime", "推定実行時間", "Est. runtime");
    I18n::reg("t3_submit", "ジョブ送信", "Submit job");
    I18n::reg("t3_jobs", "ジョブ一覧", "Jobs");
    I18n::reg("t3_done", "完了", "Done");
    I18n::reg("t3_running", "実行中", "Running");
    I18n::reg("t3_failed", "失敗", "Failed");
    I18n::reg("t3_status", "ジョブ状態", "Job status");
    I18n::reg("t3_download", "結果ダウンロード", "Download results");
    I18n::reg("t3_pending", "待機中", "Pending");
    // t3_export は I18n.cpp 側が「Pythonスクリプト生成…」で先取りしており
    // (reg は既存優先 / I18n.cpp は編集不可)、このタブのボタンだけ mock 表記に
    // 揃えるためタブ固有キーで持つ。
    I18n::reg("t3x_export", "tidy3dへエクスポート", "Export to tidy3d");
    // ☁ 概要
    I18n::reg("t3x_cloud_section", "tidy3d クラウド計算 (光FDTD専用バックエンド)",
              "tidy3d cloud compute (photonic-FDTD-only backend)");
    I18n::reg("t3x_cloud_hint",
              "Flexcompute社の光FDTD専用クラウドソルバ。GPUで~100倍速く、"
              "メタサーフェス・PIC・大規模光学系を秒~分単位で解析。",
              "Flexcompute's photonic-FDTD cloud solver. ~100× faster on GPUs — "
              "metasurfaces, PICs and large optical systems in seconds to minutes.");
    I18n::reg("t3x_optical_only",
              "※ 光ドメイン専用です。電磁/音響/水中では使用できません。",
              "* Optical domain only — not available for EM / acoustic / "
              "underwater.");
    // 接続
    I18n::reg("t3x_conn_section", "接続 / Connection", "Connection");
    I18n::reg("t3x_verify", "検証", "Verify");
    I18n::reg("t3x_connected", "接続中", "Connected");
    I18n::reg("t3x_no_key", "APIキー未設定", "No API key");
    I18n::reg("t3x_tier", "tier: Standard", "tier: Standard");
    // 自動変換マッピング
    I18n::reg("t3x_map_hint", "ローカル設定 → tidy3d API への自動変換状況",
              "Auto-conversion status: local settings → tidy3d API");
    I18n::reg("t3x_h_state", "状態", "Status");
    I18n::reg("t3x_partial", "部分対応", "Partial");
    I18n::reg("t3x_unsupported", "非対応 (光のみ)", "Unsupported (optical only)");
    I18n::reg("t3x_m_shape", "形状ユニット (Brick/Sphere/...)",
              "Shape units (Brick/Sphere/…)");
    I18n::reg("t3x_m_mesh3d", "取込3Dモデル (STL/OBJ)",
              "Imported 3D model (STL/OBJ)");
    I18n::reg("t3x_m_nk", "物性値 (n, k)", "Optical constants (n, k)");
    I18n::reg("t3x_m_disp", "分散材料 (Drude/Lorentz)",
              "Dispersive media (Drude/Lorentz)");
    I18n::reg("t3x_m_source", "波源 (Gaussian/Mode)", "Sources (Gaussian/Mode)");
    I18n::reg("t3x_m_monitor", "モニター (Field/Mode/Flux)",
              "Monitors (Field/Mode/Flux)");
    I18n::reg("t3x_m_bc", "境界条件 (PML/Periodic/Bloch)",
              "Boundaries (PML/Periodic/Bloch)");
    I18n::reg("t3x_m_grid", "非均一メッシュ", "Non-uniform mesh");
    I18n::reg("t3x_m_lumped", "集中定数 (R/L/C)", "Lumped elements (R/L/C)");
    I18n::reg("t3x_m_acoustic", "音響パラメータ", "Acoustic parameters");
    // エクスポート設定
    I18n::reg("t3x_export_section", "エクスポート設定", "Export settings");
    I18n::reg("t3x_res_low", "低 (λ/10)", "Low (λ/10)");
    I18n::reg("t3x_res_med", "中 (λ/20)", "Medium (λ/20)");
    I18n::reg("t3x_res_high", "高 (λ/40)", "High (λ/40)");
    I18n::reg("t3x_subpixel", "サブピクセル平均化 (推奨)",
              "Sub-pixel averaging (recommended)");
    I18n::reg("t3x_dft", "モニターで時間DFT記録", "Record time DFT at monitors");
    I18n::reg("t3x_preview", "プレビュー (.json)", "Preview (.json)");
    I18n::reg("t3x_preview_title", "tidy3d スクリプトプレビュー",
              "tidy3d script preview");
    // ジョブ送信
    I18n::reg("t3x_submit_section", "ジョブ送信", "Job submission");
    I18n::reg("t3x_cost_remain", "(残: 1,245.8)", "(left: 1,245.8)");
    I18n::reg("t3x_runtime_val", "~2分 30秒", "~2 min 30 s");
    I18n::reg("t3x_priority", "優先度", "Priority");
    I18n::reg("t3x_prio_normal", "通常", "Normal");
    I18n::reg("t3x_prio_high", "高 (+25%)", "High (+25%)");
    I18n::reg("t3x_pause", "⏸ 一時停止", "⏸ Pause");
    I18n::reg("t3x_submit_note",
              "スクリプトを生成し `python <name>.py` で送信してください "
              "(GUI からの直接送信は未対応 — APIキーは tidy3d CLI 設定を使用)。",
              "Generate the script and submit it with `python <name>.py` "
              "(direct submission from the GUI is not supported — the tidy3d CLI "
              "configuration provides the API key).");
    // ジョブ一覧
    I18n::reg("t3x_h_name", "名前", "Name");
    I18n::reg("t3x_h_time", "時間", "Time");
    // ローカル ↔ クラウド比較
    I18n::reg("t3x_cmp_section", "ローカル ↔ クラウド比較",
              "Local ↔ cloud comparison");
    I18n::reg("t3x_cmp_hint",
              "同じ設定でローカル/クラウド両方実行して結果を比較できます",
              "Run the same setup locally and in the cloud, then compare the "
              "results");
    I18n::reg("t3x_cmp_parallel", "ローカル CPU と並列実行",
              "Run in parallel with the local CPU");
    I18n::reg("t3x_cmp_diff", "結果差分の自動チェック",
              "Auto-check result differences");
    I18n::reg("t3x_cmp_notify", "完了時に通知", "Notify when finished");
    return true;
}();

// mock の CSS クラス色 (badge ok / warn / err / acc)
QString badgeCss(const char *kind)
{
    QString css = "border-radius:3px; padding:1px 6px; font-size:11px;";
    if (qstrcmp(kind, "ok") == 0)        css += "background:#DFF6DD; color:#0F7B0F;";
    else if (qstrcmp(kind, "warn") == 0) css += "background:#FFF4CE; color:#9D5D00;";
    else if (qstrcmp(kind, "err") == 0)  css += "background:#FDE7E9; color:#B91C1C;";
    else if (qstrcmp(kind, "acc") == 0)  css += "background:#DEECF9; color:#0078D4;";
    else                                  css += "background:palette(midlight);";
    return css;
}

QLabel *makeBadge(const QString &text, const char *kind, QWidget *parent)
{
    auto *b = new QLabel(text, parent);
    b->setStyleSheet(badgeCss(kind));
    return b;
}

// 表セル内バッジ (左寄せ / 付記テキスト付き — mock の "実行中 64%")
QWidget *badgeCell(const QString &text, const char *kind,
                   const QString &extra = QString())
{
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(4, 1, 4, 1);
    h->setSpacing(4);
    h->addWidget(makeBadge(text, kind, w));
    if (!extra.isEmpty()) h->addWidget(new QLabel(extra, w));
    h->addStretch(1);
    return w;
}

QFont monoFont()
{
    QFont f("Consolas");
    f.setStyleHint(QFont::Monospace);
    return f;
}

QLabel *monoLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setFont(monoFont());
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

QTableWidgetItem *monoItem(const QString &text)
{
    auto *it = new QTableWidgetItem(text);
    it->setFont(monoFont());
    return it;
}

QLabel *mutedLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet("color:#888888;");
    return l;
}

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}

QCheckBox *makeCheck(const QString &text, bool on, QWidget *parent)
{
    auto *c = new QCheckBox(text, parent);
    c->setChecked(on);
    return c;
}

// 読取専用テーブル (q-table 相当)
QTableWidget *makeStaticTable(QWidget *parent, const QStringList &headers,
                              int rows)
{
    auto *t = new QTableWidget(rows, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(rows * 30 + 42);
    return t;
}

// mock の自動変換マッピング表 (10行) — 表示テキストはモックのまま
struct MapRow { const char *key; const char *api; const char *state; const char *kind; };
const MapRow kMap[] = {
    { "t3x_m_shape",    "td.Box / td.Sphere",                  "OK",              "ok"   },
    { "t3x_m_mesh3d",   "td.TriangleMesh",                     "OK",              "ok"   },
    { "t3x_m_nk",       "td.medium.Medium",                    "OK",              "ok"   },
    { "t3x_m_disp",     "td.medium.PoleResidue",               "OK",              "ok"   },
    { "t3x_m_source",   "td.GaussianPulse / td.ModeSource",    "OK",              "ok"   },
    { "t3x_m_monitor",  "td.FieldMonitor / td.FluxMonitor",    "OK",              "ok"   },
    { "t3x_m_bc",       "td.PML / td.BlochBoundary",           "OK",              "ok"   },
    { "t3x_m_grid",     "td.AutoGrid",                         "OK",              "ok"   },
    { "t3x_m_lumped",   "td.LumpedElement",                    "t3x_partial",     "warn" },
    { "t3x_m_acoustic", "—",                                   "t3x_unsupported", "err"  },
};
const int kMapCount = int(sizeof(kMap) / sizeof(kMap[0]));

// 解像度コンボの index ↔ Tidy3dOpts::resolution 値 (Tidy3dExporter が解釈する語)
const char *kResKeys[3] = { "coarse", "medium", "fine" };

int resIndexOf(const QString &key)
{
    for (int i = 0; i < 3; ++i)
        if (key == QLatin1String(kResKeys[i])) return i;
    return 1;   // 既定 medium
}

// mock のジョブ一覧 (静的プロトタイプの4行)
struct JobRow {
    const char *id; const char *name; const char *stateKey; const char *kind;
    const char *extra; const char *time; const char *btn;
};
const JobRow kJobs[] = {
    { "fdtd_241015_a", "BPF design v3",   "t3_done",    "ok",  "",     "2m18s", "DL"  },
    { "fdtd_241015_b", "Ring resonator",  "t3_running", "acc", "64%",  "1m02s", "…"   },
    { "fdtd_241014_c", "Metasurface 3x3", "t3_done",    "ok",  "",     "8m44s", "DL"  },
    { "fdtd_241014_d", "PhC cavity",      "t3_failed",  "err", "",     "-",     "log" },
};
const int kJobCount = int(sizeof(kJobs) / sizeof(kJobs[0]));
} // namespace

Tidy3dTab::Tidy3dTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── ☁ tidy3d クラウド計算 (光FDTD専用バックエンド) ──────────────────────
    auto *s = new SectionBox("☁ " + I18n::tr("t3x_cloud_section"), body);
    auto *hint = new QLabel(I18n::tr("t3_hint"), s);
    hint->setWordWrap(true);
    s->vbox()->addWidget(hint);
    auto *cloudHint = hintLabel(I18n::tr("t3x_cloud_hint"), s);
    cloudHint->setStyleSheet("color:#888888; font-size:11px;");  // mock: muted text-sm
    s->vbox()->addWidget(cloudHint);
    auto *only = hintLabel(I18n::tr("t3x_optical_only"), s);
    only->setStyleSheet("color:#7C3AED;");     // mock: var(--acc-tidy3d)
    s->vbox()->addWidget(only);
    v->addWidget(s);

    // ── 接続 / Connection ────────────────────────────────────────────────────
    auto *sc = new SectionBox(I18n::tr("t3x_conn_section"), body);
    m_apiKey = new QLineEdit(sc);
    m_apiKey->setEchoMode(QLineEdit::Password);
    auto *verifyBtn = new QPushButton(I18n::tr("t3x_verify"), sc);
    auto *keyRow = new QHBoxLayout();
    keyRow->addWidget(m_apiKey, 1);
    keyRow->addWidget(verifyBtn);
    sc->form()->addRow(I18n::tr("t3_apikey"), keyRow);
    m_project = new QLineEdit(sc);
    // mock の select 既定値をプレースホルダで示す (保存先は Tidy3dOpts::projectName)
    m_project->setPlaceholderText("my-photonics-2026");
    sc->form()->addRow(I18n::tr("t3_project"), m_project);

    auto *credRow = new QHBoxLayout();
    credRow->addWidget(mutedLabel(I18n::tr("t3_credits") + ":", sc));
    credRow->addWidget(monoLabel("1,245.8 FlexCredits", sc));
    m_connBadge = makeBadge(I18n::tr("t3x_connected"), "ok", sc);
    credRow->addWidget(m_connBadge);
    credRow->addStretch(1);
    credRow->addWidget(mutedLabel(I18n::tr("t3x_tier"), sc));
    sc->vbox()->addLayout(credRow);
    v->addWidget(sc);

    // ── 自動変換マッピング / Auto-conversion (informational) ─────────────────
    auto *sm = new SectionBox(I18n::tr("t3_mapping"), body);
    sm->vbox()->addWidget(hintLabel(I18n::tr("t3x_map_hint"), sm));
    auto *table = makeStaticTable(sm, { "OpenFDTD-X", "→", "tidy3d",
                                        I18n::tr("t3x_h_state") }, kMapCount);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    for (int r = 0; r < kMapCount; ++r) {
        table->setItem(r, 0, new QTableWidgetItem(I18n::tr(kMap[r].key)));
        table->setItem(r, 1, new QTableWidgetItem("→"));
        table->setItem(r, 2, monoItem(QString::fromUtf8(kMap[r].api)));
        const QString st = QString::fromUtf8(kMap[r].state);
        table->setCellWidget(r, 3, badgeCell(st.startsWith("t3x_")
                                                 ? I18n::tr(st) : st,
                                             kMap[r].kind));
    }
    table->resizeRowsToContents();
    sm->vbox()->addWidget(table);
    v->addWidget(sm);

    // ── エクスポート設定 ─────────────────────────────────────────────────────
    auto *se = new SectionBox(I18n::tr("t3x_export_section"), body);
    m_resolution = new QComboBox(se);
    // 表示は mock の λ/N ラベル、保存値は従来どおり coarse / medium / fine
    m_resolution->addItems({ I18n::tr("t3x_res_low"), I18n::tr("t3x_res_med"),
                             I18n::tr("t3x_res_high") });
    se->form()->addRow(I18n::tr("t3_resolution"), m_resolution);
    m_autoPml = makeCheck(I18n::tr("t3_auto_pml"), true, se);
    m_subpixel = makeCheck(I18n::tr("t3x_subpixel"), true, se);
    m_dft = makeCheck(I18n::tr("t3x_dft"), true, se);
    se->vbox()->addWidget(m_autoPml);
    se->vbox()->addWidget(m_subpixel);
    se->vbox()->addWidget(m_dft);

    // mock: "📤 {t3_export} (.py)" (primary) + "プレビュー (.json)"
    auto *exportBtn = new QPushButton("📤 " + I18n::tr("t3x_export") + " (.py)", se);
    exportBtn->setStyleSheet("font-weight:600;");
    auto *previewBtn = new QPushButton(I18n::tr("t3x_preview"), se);
    auto *expRow = new QHBoxLayout();
    expRow->addWidget(exportBtn);
    expRow->addWidget(previewBtn);
    expRow->addStretch(1);
    se->vbox()->addLayout(expRow);
    m_status = new QLabel(se);
    m_status->setWordWrap(true);
    se->vbox()->addWidget(m_status);
    v->addWidget(se);

    // ── ジョブ送信 ───────────────────────────────────────────────────────────
    auto *sj = new SectionBox(I18n::tr("t3x_submit_section"), body);
    auto *costRow = new QHBoxLayout();
    costRow->addWidget(monoLabel("~8.4 FlexCredits", sj));
    costRow->addWidget(mutedLabel(I18n::tr("t3x_cost_remain"), sj));
    costRow->addStretch(1);
    sj->form()->addRow(I18n::tr("t3_cost"), costRow);
    sj->form()->addRow(I18n::tr("t3_runtime"),
                       monoLabel(I18n::tr("t3x_runtime_val"), sj));
    m_priority = new QComboBox(sj);
    m_priority->addItems({ I18n::tr("t3x_prio_normal"),
                           I18n::tr("t3x_prio_high") });
    sj->form()->addRow(I18n::tr("t3x_priority"), m_priority);
    // ジョブ状態: GUI から直接送信はしないので既定は「待機中」
    auto *stateRow = new QHBoxLayout();
    stateRow->addWidget(makeBadge(I18n::tr("t3_pending"), "warn", sj));
    stateRow->addStretch(1);
    sj->form()->addRow(I18n::tr("t3_status"), stateRow);
    auto *submitBtn = new QPushButton("🚀 " + I18n::tr("t3_submit"), sj);
    submitBtn->setStyleSheet("font-weight:600;");
    auto *pauseBtn = new QPushButton(I18n::tr("t3x_pause"), sj);
    auto *subRow = new QHBoxLayout();
    subRow->addWidget(submitBtn);
    subRow->addWidget(pauseBtn);
    subRow->addStretch(1);
    sj->vbox()->addLayout(subRow);
    m_jobStatus = hintLabel(QString(), sj);
    sj->vbox()->addWidget(m_jobStatus);
    v->addWidget(sj);

    // ── ジョブ一覧 (mock の静的プロトタイプ) ────────────────────────────────
    auto *sl = new SectionBox(I18n::tr("t3_jobs"), body);
    m_jobs = makeStaticTable(sl, { "ID", I18n::tr("t3x_h_name"),
                                   I18n::tr("t3x_h_state"),
                                   I18n::tr("t3x_h_time"), "" }, kJobCount);
    m_jobs->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int r = 0; r < kJobCount; ++r) {
        m_jobs->setItem(r, 0, monoItem(QString::fromUtf8(kJobs[r].id)));
        m_jobs->setItem(r, 1, new QTableWidgetItem(QString::fromUtf8(kJobs[r].name)));
        m_jobs->setCellWidget(r, 2, badgeCell(I18n::tr(kJobs[r].stateKey),
                                              kJobs[r].kind,
                                              QString::fromUtf8(kJobs[r].extra)));
        m_jobs->setItem(r, 3, monoItem(QString::fromUtf8(kJobs[r].time)));
        // mock の行ボタンは "DL" / "…" / "log"。DL は結果ダウンロードなので
        // 正式名称 (t3_download) をヒントに出す。
        auto *rowBtn = new QPushButton(QString::fromUtf8(kJobs[r].btn), m_jobs);
        if (qstrcmp(kJobs[r].btn, "DL") == 0)
            rowBtn->setToolTip(I18n::tr("t3_download"));
        m_jobs->setCellWidget(r, 4, rowBtn);
    }
    m_jobs->resizeRowsToContents();
    sl->vbox()->addWidget(m_jobs);
    v->addWidget(sl);

    // ── ローカル ↔ クラウド比較 ─────────────────────────────────────────────
    auto *sp = new SectionBox(I18n::tr("t3x_cmp_section"), body);
    sp->vbox()->addWidget(hintLabel(I18n::tr("t3x_cmp_hint"), sp));
    m_cmpParallel = makeCheck(I18n::tr("t3x_cmp_parallel"), false, sp);
    m_cmpDiff = makeCheck(I18n::tr("t3x_cmp_diff"), false, sp);
    m_cmpNotify = makeCheck(I18n::tr("t3x_cmp_notify"), true, sp);
    auto *cmpRow = new QHBoxLayout();
    cmpRow->addWidget(m_cmpParallel);
    cmpRow->addWidget(m_cmpDiff);
    cmpRow->addWidget(m_cmpNotify);
    cmpRow->addStretch(1);
    sp->vbox()->addLayout(cmpRow);
    v->addWidget(sp);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // API key lives in QSettings, not in the project files
    connect(m_apiKey, &QLineEdit::editingFinished, this, [this] {
        QSettings().setValue("tidy3d/apiKey", m_apiKey->text());
        updateConnBadge();
    });
    auto applyCb = [this] { apply(); };
    connect(m_project, &QLineEdit::editingFinished, this, applyCb);
    connect(m_resolution, &QComboBox::currentIndexChanged, this, applyCb);
    connect(m_autoPml, &QCheckBox::toggled, this, applyCb);
    connect(exportBtn, &QPushButton::clicked, this, &Tidy3dTab::exportScript);
    connect(previewBtn, &QPushButton::clicked, this, &Tidy3dTab::previewScript);
    connect(verifyBtn, &QPushButton::clicked, this, &Tidy3dTab::verifyKey);
    connect(submitBtn, &QPushButton::clicked, this, &Tidy3dTab::submitJob);

    connect(project, &Project::loaded, this, &Tidy3dTab::refresh);
    refresh();
}

void Tidy3dTab::apply()
{
    if (m_updating) return;
    Tidy3dOpts &t = m_p->tidy3d();
    t.projectName = m_project->text();
    t.resolution = QLatin1String(kResKeys[qBound(0, m_resolution->currentIndex(), 2)]);
    t.autoPml = m_autoPml->isChecked();
    m_p->touch();
}

void Tidy3dTab::exportScript()
{
    const QString path = QFileDialog::getSaveFileName(
        this, I18n::tr("t3x_export"),
        m_p->tidy3d().projectName + ".py", "Python (*.py)");
    if (path.isEmpty()) return;
    QString err;
    if (Tidy3dExporter::exportTo(path, *m_p, &err))
        m_status->setText("OK: " + path);
    else
        m_status->setText("error: " + err);
}

// プレビュー: 生成される tidy3d スクリプトをそのまま表示 (Tidy3dExporter が唯一の生成元)
void Tidy3dTab::previewScript()
{
    QDialog dlg(this);
    dlg.setWindowTitle(I18n::tr("t3x_preview_title"));
    auto *lay = new QVBoxLayout(&dlg);
    auto *text = new QPlainTextEdit(Tidy3dExporter::generatePython(*m_p), &dlg);
    text->setReadOnly(true);
    text->setLineWrapMode(QPlainTextEdit::NoWrap);
    text->setFont(monoFont());
    lay->addWidget(text);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(bb);
    dlg.resize(760, 540);
    dlg.exec();
}

void Tidy3dTab::verifyKey()
{
    QSettings().setValue("tidy3d/apiKey", m_apiKey->text());
    updateConnBadge();
}

void Tidy3dTab::submitJob()
{
    m_jobStatus->setText(I18n::tr("t3x_submit_note"));
}

void Tidy3dTab::updateConnBadge()
{
    const bool ok = !m_apiKey->text().trimmed().isEmpty();
    m_connBadge->setText(I18n::tr(ok ? "t3x_connected" : "t3x_no_key"));
    m_connBadge->setStyleSheet(badgeCss(ok ? "ok" : "err"));
}

void Tidy3dTab::refresh()
{
    m_updating = true;
    m_apiKey->setText(QSettings().value("tidy3d/apiKey").toString());
    m_project->setText(m_p->tidy3d().projectName);
    m_resolution->setCurrentIndex(resIndexOf(m_p->tidy3d().resolution));
    m_autoPml->setChecked(m_p->tidy3d().autoPml);
    updateConnBadge();
    m_updating = false;
}
