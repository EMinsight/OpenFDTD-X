// Tidy3dTab.cpp
#include "Tidy3dTab.h"
#include "../core/SeriesCompare.h"
#include "../io/SeriesCsv.h"
#include "../io/KernelResultReader.h"
#include "../kernel/Runner.h"
#include <QDir>
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../io/Tidy3dExporter.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "../Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
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
    I18n::reg("t3_status", "ジョブ状態", "Job status");
    I18n::reg("t3_pending", "待機中", "Pending");
    // 完了/実行中/失敗/ダウンロード (mock のジョブ一覧の語彙) は、クラウド API
    // からジョブ状態を取得できるようになるまで表示しないので登録もしない。
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
    // 接続 — API との実疎通確認は未実装なので「検証/接続中」を名乗らない
    // (CLAUDE.md 絶対規則 5)。ボタンは保存のみ、バッジはキー設定の有無のみ。
    I18n::reg("t3x_conn_section", "接続 / Connection", "Connection");
    I18n::reg("t3x_verify", "保存", "Save");
    I18n::reg("t3x_verify_tip",
              "APIキーを保存するだけです (tidy3d への実疎通確認は未実装)",
              "Only stores the API key (no actual connectivity check with "
              "tidy3d — not implemented)");
    I18n::reg("t3x_connected", "APIキー設定済み", "API key set");
    I18n::reg("t3x_no_key", "APIキー未設定", "No API key");
    I18n::reg("t3x_tier", "tier: 未取得", "tier: not fetched");
    // 残高・見積は API 未接続のため取得できない (偽装値を表示しない)
    I18n::reg("t3x_credits_na", "未取得 (API 未接続)",
              "Not fetched (API not connected)");
    I18n::reg("t3x_cost_na", "未算出 (tidy3d 側で見積)",
              "Not estimated (estimated on the tidy3d side)");
    // 変換対応の一覧 (静的表 — 実プロジェクトの自動判定ではない)
    I18n::reg("t3x_map_hint", "ローカル設定 → tidy3d API の変換対応の一覧 (静的)",
              "Conversion support list: local settings → tidy3d API (static)");
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
    I18n::reg("t3x_cost_remain", "(残高: 未取得)", "(balance: not fetched)");
    I18n::reg("t3x_runtime_val", "未算出 (tidy3d 側で見積)",
              "Not estimated (estimated on the tidy3d side)");
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
    // ジョブ一覧 — クラウド側の状態は取得していないので、ここに出せる実データは
    // 「このマシンで書き出したスクリプト」だけ (偽のジョブ行は出さない)。
    I18n::reg("t3x_scripts_section", "書き出したジョブスクリプト (ローカル)",
              "Exported job scripts (local)");
    I18n::reg("t3x_h_file", "ファイル", "File");
    I18n::reg("t3x_h_created", "書き出し日時", "Exported at");
    I18n::reg("t3x_state_local", "未送信 (ローカル生成)",
              "Not submitted (generated locally)");
    I18n::reg("t3x_state_missing", "ファイルなし", "File missing");
    I18n::reg("t3x_jobs_empty",
              "まだスクリプトを書き出していません — 上の "
              "「📤 tidy3dへエクスポート (.py)」でここに履歴が残ります。",
              "No scripts exported yet — use \"📤 Export to tidy3d (.py)\" above "
              "and the history will appear here.");
    I18n::reg("t3x_jobs_note",
              "クラウド上のジョブ状態 (実行中/完了/課金) は取得していません "
              "(GUI からの送信・状態取得は未実装)。tidy3d 側の状態は web コンソール"
              "または tidy3d CLI で確認してください。",
              "Cloud job status (running / done / cost) is not fetched — "
              "submission and status polling from the GUI are not implemented. "
              "Check the tidy3d web console or the tidy3d CLI instead.");
    I18n::reg("t3x_jobs_clear", "履歴を消去", "Clear history");
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
    // 結果差分の自動チェック (io/SeriesCsv + core/SeriesCompare)
    I18n::reg("t3x_cmp_load", "クラウドの結果を読む (CSV)",
              "Load the cloud result (CSV)");
    I18n::reg("t3x_cmp_hint2",
              "tidy3d の結果を 2 列の CSV (1 列目 = 周波数 [Hz]、2 列目 = 値) で"
              "書き出して読ませると、ローカルの <kernel>.log の給電点掃引 "
              "(反射 Ref[dB]) と共通の周波数軸で突き合わせます。",
              "Export the tidy3d result as a two-column CSV (column 1 = "
              "frequency in Hz, column 2 = value) and load it here; it is then "
              "compared against the local feed sweep (reflection Ref[dB]) in "
              "<kernel>.log on a common frequency axis.");
    I18n::reg("t3x_cmp_scale", "クラウド側の単位", "Scale of the cloud data");
    I18n::reg("t3x_cmp_lin", "線形", "Linear");
    I18n::reg("t3x_cmp_pdb", "dB (電力 10log10)", "dB (power, 10log10)");
    I18n::reg("t3x_cmp_adb", "dB (振幅 20log10)", "dB (amplitude, 20log10)");
    I18n::reg("t3x_cmp_res",
              "%1 (%2 点) と %3 (%4 点): 重なり %5 点 / 最大差 %6 / RMS %7 / "
              "系統差 %8 / 相関 %9。系統差が大きく相関が高いときは正規化の"
              "ずれ、相関が低いときは設定そのものが違います。",
              "%1 (%2 points) versus %3 (%4 points): %5 overlapping points / "
              "worst difference %6 / rms %7 / bias %8 / correlation %9. A large "
              "bias with a high correlation means a normalisation difference; a "
              "low correlation means the setups differ.");
    I18n::reg("t3x_cmp_nolocal",
              "ローカルの結果がありません。先にこのプロジェクトを実行して"
              "<kernel>.log を作ってください。",
              "There is no local result yet - run this project first so that "
              "<kernel>.log exists.");
    I18n::reg("t3x_cmp_badcsv",
              "CSV から数値の列を 2 つ以上読めませんでした "
              "(1 列目 = 周波数 [Hz]、2 列目 = 値)。",
              "Could not read two numeric columns from the CSV (column 1 = "
              "frequency in Hz, column 2 = value).");
    I18n::reg("t3x_cmp_nooverlap",
              "周波数の範囲が重なっていないので比較できません "
              "(クラウド側 %1 〜 %2 Hz、ローカル %3 〜 %4 Hz)。",
              "The frequency ranges do not overlap, so nothing can be compared "
              "(cloud %1 to %2 Hz, local %3 to %4 Hz).");
    I18n::reg("t3x_uw_cmp2",
              "「ローカル CPU と並列実行」と「完了時に通知」(このタブは"
              "スクリプトを書き出すだけで送信しないので、並列に走らせる相手も"
              "完了を知る手段もありません)",
              "the \"run in parallel with the local CPU\" and \"notify when "
              "finished\" options (this tab only writes the script and never "
              "submits, so there is nothing to run in parallel with and no "
              "completion to be notified of)");
    I18n::reg("t3x_uw_cmp2_ok",
              "「結果差分の自動チェック」— クラウドの結果 CSV を読み、"
              "ローカルの給電点掃引と共通の周波数軸・共通の単位で突き合わせます "
              "(io/SeriesCsv + core/SeriesCompare)",
              "the automatic result-difference check - it loads the cloud "
              "result CSV and compares it against the local feed sweep on a "
              "common frequency axis and in common units (io/SeriesCsv + "
              "core/SeriesCompare)");
    I18n::reg("t3x_exp_note",
              "▸ 3 つとも生成スクリプトへ渡ります。自動 PML は "
              "boundary_spec、サブピクセル平均化は subpixel "
              "(tidy3d 自身の既定が有効なので、外したときだけ subpixel=False を"
              "書きます)、DFT 記録は td.FieldMonitor (周波数領域) と "
              "td.FieldTimeMonitor (時間波形) の切替です。",
              "▸ All three reach the generated script. Automatic PML becomes "
              "boundary_spec; sub-pixel averaging becomes subpixel (tidy3d "
              "enables it by default, so subpixel=False is written only when "
              "you clear it); the DFT setting switches between td.FieldMonitor "
              "(frequency domain) and td.FieldTimeMonitor (time waveform).");
    I18n::reg("t3x_uw_prio",
              "優先度をジョブ API へ渡すこと (このスクリプトは送信までは"
              "行わないため、優先度は tidy3d 側で設定します)",
              "passing the priority to the job API (this script does not "
              "submit, so the priority is set on the tidy3d side)");
    I18n::reg("t3x_uw_prio_ok",
              "優先度の選択そのもの (プロジェクトに保存し、生成スクリプトへ"
              "注記として書き出します)",
              "the priority selection itself (saved with the project and "
              "written into the generated script as a note)");
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
    // 実在するファミリのみ指定する (Theme が環境ごとに解決済み)
    QFont f;
    const QString mono = Theme::monoFontFamily();
    if (!mono.isEmpty())
        f.setFamily(mono);
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

// 書き出し履歴の QSettings キー。1 件 = "ISO日時\tパス" (タブ区切り)。
// API キーと同じくプロジェクトファイルではなくアプリ設定に置く。
const char kHistoryKey[] = "tidy3d/exportHistory";
const int  kHistoryMax = 20;
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
    verifyBtn->setToolTip(I18n::tr("t3x_verify_tip"));   // 実疎通確認はしない
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
    // 残高は API 未接続なので取得できない (固定値の偽装をしない)
    credRow->addWidget(monoLabel(I18n::tr("t3x_credits_na"), sc));
    m_connBadge = makeBadge(I18n::tr("t3x_connected"), "ok", sc);
    m_connBadge->setToolTip(I18n::tr("t3x_verify_tip"));
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
    // サブピクセル平均化・DFT 記録・自動 PML はすべてエクスポートへ渡る。
    // 生成スクリプトの意味は下の注記に書く (何がどう出るかを隠さない)。
    se->vbox()->addWidget(mutedLabel(I18n::tr("t3x_exp_note"), se));

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
    // コスト見積は未実装 (tidy3d 側で見積されるため偽装値を出さない)
    costRow->addWidget(monoLabel(I18n::tr("t3x_cost_na"), sj));
    costRow->addWidget(mutedLabel(I18n::tr("t3x_cost_remain"), sj));
    costRow->addStretch(1);
    sj->form()->addRow(I18n::tr("t3_cost"), costRow);
    sj->form()->addRow(I18n::tr("t3_runtime"),
                       monoLabel(I18n::tr("t3x_runtime_val"), sj));
    m_priority = new QComboBox(sj);
    m_priority->addItems({ I18n::tr("t3x_prio_normal"),
                           I18n::tr("t3x_prio_high") });
    sj->form()->addRow(I18n::tr("t3x_priority"), m_priority);
    // 優先度はプロジェクトへ保存し、スクリプトには注記として出す。
    // ジョブ API へ渡す先をここは持たない (送信は tidy3d 側の操作)。
    sj->form()->addRow(tabhelp::unwiredNote(sj, I18n::tr("t3x_uw_prio"),
                                            I18n::tr("t3x_uw_prio_ok")));
    // ジョブ状態: GUI から直接送信はしないので既定は「待機中」
    auto *stateRow = new QHBoxLayout();
    stateRow->addWidget(makeBadge(I18n::tr("t3_pending"), "warn", sj));
    stateRow->addStretch(1);
    sj->form()->addRow(I18n::tr("t3_status"), stateRow);
    auto *submitBtn = new QPushButton("🚀 " + I18n::tr("t3_submit"), sj);
    submitBtn->setStyleSheet("font-weight:600;");
    auto *pauseBtn = new QPushButton(I18n::tr("t3x_pause"), sj);
    tabhelp::markNotImplemented(pauseBtn, I18n::tr(tabhelp::notimpl::kControl));   // ジョブ制御は未実装
    auto *subRow = new QHBoxLayout();
    subRow->addWidget(submitBtn);
    subRow->addWidget(pauseBtn);
    subRow->addStretch(1);
    sj->vbox()->addLayout(subRow);
    m_jobStatus = hintLabel(QString(), sj);
    sj->vbox()->addWidget(m_jobStatus);
    v->addWidget(sj);

    // ── 書き出したジョブスクリプト (ローカル履歴) ──────────────────────────
    // クラウドのジョブ状態は API 未接続で取得できないため、実在するデータ =
    // 「このマシンで書き出した .py」だけを出す (絶対規則 5)。
    auto *sl = new SectionBox(I18n::tr("t3x_scripts_section"), body);
    sl->vbox()->addWidget(hintLabel(I18n::tr("t3x_jobs_note"), sl));
    m_jobs = makeStaticTable(sl, { I18n::tr("t3x_h_file"),
                                   I18n::tr("t3x_h_created"),
                                   I18n::tr("t3x_h_state") }, 0);
    m_jobs->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    sl->vbox()->addWidget(m_jobs);
    auto *histRow = new QHBoxLayout();
    auto *clearBtn = new QPushButton(I18n::tr("t3x_jobs_clear"), sl);
    connect(clearBtn, &QPushButton::clicked, this, [this] {
        QSettings().setValue(QString::fromLatin1(kHistoryKey), QStringList());
        rebuildJobs();
    });
    histRow->addWidget(clearBtn);
    histRow->addStretch(1);
    sl->vbox()->addLayout(histRow);
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

    // ── 結果差分の自動チェック (io/SeriesCsv + core/SeriesCompare) ────────
    // このタブは送信しないので、クラウド側の結果はファイルで受け取る。
    // 並列実行と通知は相手がいないので理由を添えて無効にする。
    for (QCheckBox *c : { m_cmpParallel, m_cmpNotify }) {
        c->setChecked(false);
        c->setEnabled(false);
    }
    sp->vbox()->addWidget(hintLabel(I18n::tr("t3x_cmp_hint2"), sp));
    m_cmpScale = new QComboBox(sp);
    for (const char *k : { "t3x_cmp_lin", "t3x_cmp_pdb", "t3x_cmp_adb" })
        m_cmpScale->addItem(I18n::tr(k));
    m_cmpScale->setCurrentIndex(1);              // 反射は電力 dB
    sp->form()->addRow(I18n::tr("t3x_cmp_scale"), m_cmpScale);
    auto *loadRow = new QHBoxLayout();
    m_cmpLoad = new QPushButton(I18n::tr("t3x_cmp_load"), sp);
    connect(m_cmpLoad, &QPushButton::clicked, this, &Tidy3dTab::loadCloudResult);
    loadRow->addWidget(m_cmpLoad);
    loadRow->addStretch(1);
    sp->vbox()->addLayout(loadRow);
    m_cmpResult = hintLabel(QString(), sp);
    m_cmpResult->setWordWrap(true);
    sp->vbox()->addWidget(m_cmpResult);
    connect(m_cmpScale, &QComboBox::currentIndexChanged,
            this, &Tidy3dTab::updateCloudCompare);
    connect(m_cmpDiff, &QCheckBox::toggled, this, [this](bool on) {
        m_cmpLoad->setEnabled(on);
        m_cmpScale->setEnabled(on);
        if (on) updateCloudCompare(); else m_cmpResult->clear();
    });
    m_cmpLoad->setEnabled(m_cmpDiff->isChecked());
    m_cmpScale->setEnabled(m_cmpDiff->isChecked());

    sp->vbox()->addWidget(tabhelp::unwiredNote(sp, I18n::tr("t3x_uw_cmp2"),
                                               I18n::tr("t3x_uw_cmp2_ok")));
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
    // 新しくエクスポートへ効く設定も同じ経路で保存する
    connect(m_subpixel, &QCheckBox::toggled, this, applyCb);
    connect(m_dft, &QCheckBox::toggled, this, applyCb);
    connect(m_priority, &QComboBox::currentIndexChanged, this, applyCb);
    connect(exportBtn, &QPushButton::clicked, this, &Tidy3dTab::exportScript);
    connect(previewBtn, &QPushButton::clicked, this, &Tidy3dTab::previewScript);
    connect(verifyBtn, &QPushButton::clicked, this, &Tidy3dTab::verifyKey);
    connect(submitBtn, &QPushButton::clicked, this, &Tidy3dTab::submitJob);

    connect(project, &Project::loaded, this, &Tidy3dTab::refresh);
    refresh();
}

// ── 結果差分の自動チェック (io/SeriesCsv + core/SeriesCompare) ────────────
// このタブはスクリプトを書き出すだけで送信しないので、クラウド側の結果は
// ファイルで受け取る。ローカル側は <kernel>.log の給電点掃引 (周波数 vs
// 反射 Ref[dB]) — 実行すれば実際に出る物理量。
void Tidy3dTab::loadCloudResult()
{
    const QString path = QFileDialog::getOpenFileName(
        this, I18n::tr("t3x_cmp_load"), QString(),
        QStringLiteral("CSV (*.csv *.txt);;All files (*)"));
    if (path.isEmpty()) return;
    const cmp::Series s = ofd::io::readSeriesCsv(path);
    if (!s.valid()) {
        m_cloud = cmp::Series();
        m_cloudName.clear();
        m_cmpResult->setText(I18n::tr("t3x_cmp_badcsv"));
        return;
    }
    m_cloud = s;
    m_cloudName = QFileInfo(path).fileName();
    updateCloudCompare();
}

void Tidy3dTab::updateCloudCompare()
{
    if (!m_cmpResult) return;
    if (!m_cmpDiff->isChecked() || !m_cloud.valid()) { m_cmpResult->clear(); return; }

    // ローカル側 = <kernel>.log の給電点掃引
    cmp::Series local;
    QString localName;
    {
        RunConfig cfg;
        const QString dir = Runner::resolveWorkingDir(m_p, cfg);
        const Kernel k = Runner::kernelForProject(*m_p);
        const QString name = Runner::runLogName(k);
        if (!dir.isEmpty() && !name.isEmpty()) {
            const QVector<FeedSweep> fs =
                KernelResultReader::readFeedSweeps(QDir(dir).filePath(name));
            if (!fs.isEmpty() && fs[0].points.size() >= 2) {
                for (const FeedSweepPoint &p : fs[0].points) {
                    local.x.push_back(p.freqHz);
                    local.y.push_back(p.refDb);
                }
                localName = name;
            }
        }
    }
    if (!local.valid()) {
        m_cmpResult->setText(I18n::tr("t3x_cmp_nolocal"));
        return;
    }

    // 単位を揃える (ローカルは電力 dB。10 と 20 の取り違えは値を 2 乗ずらす)
    const cmp::Scale sb = (m_cmpScale->currentIndex() == 2) ? cmp::Scale::AmplitudeDb
                        : (m_cmpScale->currentIndex() == 1) ? cmp::Scale::PowerDb
                                                            : cmp::Scale::Linear;
    const cmp::Series cloud = cmp::convert(m_cloud, sb, cmp::Scale::PowerDb);
    const cmp::Agreement g = cmp::compare(local, cloud);
    if (!g.valid) {
        m_cmpResult->setText(I18n::tr("t3x_cmp_nooverlap")
                                 .arg(m_cloud.x.front()).arg(m_cloud.x.back())
                                 .arg(local.x.front()).arg(local.x.back()));
        return;
    }
    m_cmpResult->setText(I18n::tr("t3x_cmp_res")
                             .arg(localName).arg(local.x.size())
                             .arg(m_cloudName).arg(m_cloud.x.size())
                             .arg(g.n)
                             .arg(g.maxAbs, 0, 'g', 4)
                             .arg(g.rms, 0, 'g', 4)
                             .arg(g.bias, 0, 'g', 4)
                             .arg(g.correlation, 0, 'f', 4));
}

void Tidy3dTab::apply()
{
    if (m_updating) return;
    Tidy3dOpts &t = m_p->tidy3d();
    t.projectName = m_project->text();
    t.resolution = QLatin1String(kResKeys[qBound(0, m_resolution->currentIndex(), 2)]);
    t.autoPml = m_autoPml->isChecked();
    t.subpixel = m_subpixel->isChecked();
    t.dftMonitors = m_dft->isChecked();
    t.priority = qBound(0, m_priority->currentIndex(), 1);
    m_p->touch();
}

void Tidy3dTab::exportScript()
{
    const QString path = QFileDialog::getSaveFileName(
        this, I18n::tr("t3x_export"),
        m_p->tidy3d().projectName + ".py", "Python (*.py)");
    if (path.isEmpty()) return;
    QString err;
    if (Tidy3dExporter::exportTo(path, *m_p, &err)) {
        m_status->setText("OK: " + path);
        // 実際に書き出せたものだけを履歴へ入れる (書き出し日時 + パス)
        QStringList hist =
            QSettings().value(QString::fromLatin1(kHistoryKey)).toStringList();
        const QString stamp =
            QDateTime::currentDateTime().toString(Qt::ISODate);
        // 同じパスへの再書き出しは 1 件に畳んで先頭へ
        for (int i = hist.size() - 1; i >= 0; --i)
            if (hist[i].section('\t', 1) == path) hist.removeAt(i);
        hist.prepend(stamp + '\t' + path);
        while (hist.size() > kHistoryMax) hist.removeLast();
        QSettings().setValue(QString::fromLatin1(kHistoryKey), hist);
        rebuildJobs();
    } else {
        m_status->setText("error: " + err);
    }
}

// 書き出し履歴 (QSettings) → 表。ファイルが消えていれば実状態を出す。
void Tidy3dTab::rebuildJobs()
{
    const QStringList hist =
        QSettings().value(QString::fromLatin1(kHistoryKey)).toStringList();
    m_jobs->clearContents();
    m_jobs->clearSpans();   // 前回の結合セルを解除
    if (hist.isEmpty()) {
        // 空表ではなく「何をすれば埋まるか」を出す
        m_jobs->setRowCount(1);
        m_jobs->setItem(0, 0, new QTableWidgetItem(I18n::tr("t3x_jobs_empty")));
        m_jobs->setSpan(0, 0, 1, 3);
        m_jobs->setMinimumHeight(72);
        m_jobs->resizeRowsToContents();
        return;
    }
    m_jobs->setRowCount(hist.size());
    for (int r = 0; r < hist.size(); ++r) {
        const QString stamp = hist[r].section('\t', 0, 0);
        const QString path  = hist[r].section('\t', 1);
        const QFileInfo fi(path);
        auto *file = monoItem(fi.fileName());
        file->setToolTip(path);
        m_jobs->setItem(r, 0, file);
        const QDateTime dt = QDateTime::fromString(stamp, Qt::ISODate);
        m_jobs->setItem(r, 1, monoItem(dt.isValid()
                                           ? dt.toString("yyyy-MM-dd hh:mm")
                                           : stamp));
        const bool exists = fi.exists();
        m_jobs->setCellWidget(r, 2,
            badgeCell(I18n::tr(exists ? "t3x_state_local" : "t3x_state_missing"),
                      exists ? "warn" : "err"));
    }
    m_jobs->setMinimumHeight(hist.size() * 30 + 42);
    m_jobs->resizeRowsToContents();
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

// APIキーの保存のみ (tidy3d への実疎通確認は未実装 — ボタン表記も「保存」)
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
    m_subpixel->setChecked(m_p->tidy3d().subpixel);
    m_dft->setChecked(m_p->tidy3d().dftMonitors);
    m_priority->setCurrentIndex(qBound(0, m_p->tidy3d().priority, 1));
    updateConnBadge();
    rebuildJobs();
    m_updating = false;
}
