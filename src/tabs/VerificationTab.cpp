// VerificationTab.cpp
#include "VerificationTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <random>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 ver_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("ver_title", "精度検証", "Result Verification");
    // 誇大ヒントの是正 (CLAUDE.md 絶対規則 5): 未実装であることを明記する。
    // 文言は全ドメイン共通表示のためドメイン非依存 (「ソルバ結果」) にする
    I18n::reg("ver_hint",
              "ソルバ結果の信頼性を複数の観点からチェックする画面 (未実装)。",
              "Screen for checking solver result reliability from several angles "
              "(not implemented).");
    // タブ全体がモックであることの強い注記 (このタブは全出力が固定サンプル)。
    // 列挙もドメイン非依存の表現 (PML→境界吸収) にする
    I18n::reg("ver_mock_note",
              "⚠ このタブの表示は設計モックです — 検証機能 (収束テスト/境界吸収/"
              "エネルギー減衰/自動診断) は未実装で、表示中の数値・判定はすべて"
              "サンプルです (②の対策ボタン 2 個のみプロジェクト設定を実際に"
              "変更します)",
              "⚠ This tab is a design mock — the verification features "
              "(convergence test / boundary absorption / energy decay / "
              "auto-diagnostics) are not implemented, and every value and "
              "verdict shown is sample data (only the two remedy buttons in "
              "section 2 actually modify the project settings)");

    I18n::reg("ver_mesh_title", "① メッシュ収束", "① Mesh convergence");
    I18n::reg("ver_mesh_hint", "メッシュ精度を段階的に上げて結果の収束を確認",
              "Raise mesh accuracy stepwise and confirm the result converges");
    I18n::reg("ver_mesh_qty", "チェックする量", "Quantity to check");
    I18n::reg("ver_h_mesh", "メッシュ", "Mesh");
    I18n::reg("ver_h_cells", "セル数", "Cells");
    I18n::reg("ver_h_result", "結果", "Result");
    I18n::reg("ver_h_err", "誤差 vs 最高", "Error vs finest");
    I18n::reg("ver_baseline", "0.0% (基準)", "0.0% (reference)");
    I18n::reg("ver_notrun", "(未実行)", "(not run)");
    I18n::reg("ver_mesh_ok", "レベル3 で収束 (誤差 <1%)", "Converged at level 3 (error <1%)");
    I18n::reg("ver_mesh_run", "▶ 自動収束テスト実行", "▶ Run auto-convergence test");

    I18n::reg("ver_pml_title", "② PML吸収品質", "② PML reflection");
    I18n::reg("ver_pml_hint", "PML境界での反射が結果を汚染していないか確認",
              "Check that PML boundary reflections are not contaminating the result");
    I18n::reg("ver_h_face", "面", "Face");
    I18n::reg("ver_h_refl", "反射率", "Reflection");
    I18n::reg("ver_h_verdict", "判定", "Verdict");
    I18n::reg("ver_pml_warn", "境界に近接 → PML層数を増加推奨",
              "Close to the boundary → increase PML layers");
    // 対策ボタン (配線済み — Project の実設定を変更する)。層数は現在値から表示
    I18n::reg("ver_pml_btn1_fmt", "PML層数を%1→%2に増加",
              "Increase PML layers %1→%2");
    I18n::reg("ver_pml_btn1_tip",
              "PML層数を増やして境界反射を低減します (プロジェクト設定を実際に"
              "変更。境界条件が Mur の場合は PML へ切り替えます)",
              "Increase the PML layer count to reduce boundary reflection "
              "(actually modifies the project settings; switches a Mur "
              "boundary to PML)");
    I18n::reg("ver_pml_btn2", "境界余裕を増加 (+λ/4)", "Increase boundary margin (+λ/4)");
    I18n::reg("ver_pml_btn2_tip",
              "解析周波数1の中心 %1 の λ/4 = %2 をメッシュ各軸の両端に追加します"
              " (プロジェクト設定を実際に変更。周期境界の軸は除外)",
              "Extend the mesh on both ends of each axis by λ/4 = %2 at %1 "
              "(center of frequency1). Actually modifies the project settings; "
              "periodic-boundary axes are excluded");
    I18n::reg("ver_pml_margin_na",
              "メッシュが未定義/不正、または解析周波数1が無いため使用できません",
              "Unavailable: the mesh is undefined/invalid or frequency1 is absent");

    I18n::reg("ver_time_title", "③ 時間精度", "③ Time accuracy");
    I18n::reg("ver_time_hint", "自動シャットオフ前にエネルギーが十分減衰しているか確認",
              "Confirm the energy decayed sufficiently before auto-shutoff");
    I18n::reg("ver_time_ok", "エネルギー残量 1.2e-6 (シャットオフ閾値 1e-5)",
              "Residual energy 1.2e-6 (shutoff threshold 1e-5)");
    I18n::reg("ver_time_note", "▸ 高Q構造では時間を延長してください",
              "▸ Extend the simulation time for high-Q structures");

    I18n::reg("ver_cross_title", "④ 周波数領域比較", "④ Cross-validation");
    I18n::reg("ver_cross_hint", "同じ問題をFEM/RCWA等の異なるソルバで解いて結果を比較",
              "Solve the same problem with a different solver (FEM/RCWA…) and compare");
    I18n::reg("ver_cross_solver", "比較ソルバ", "Comparison solver");
    I18n::reg("ver_cross_run", "▶ クロスバリデーション実行", "▶ Run cross-validation");
    // ドメイン別の比較ソルバ名 (モック表示。実行連携は未実装)
    I18n::reg("ver_cross_ism", "ISM (鏡像法)", "ISM (image source)");
    I18n::reg("ver_cross_ray", "レイトレース", "Ray tracing");
    I18n::reg("ver_cross_pe", "PE (放物型方程式)", "PE (parabolic equation)");

    I18n::reg("ver_diag_title", "自動診断", "Auto-diagnostics");
    I18n::reg("ver_h_item", "項目", "Item");
    I18n::reg("ver_h_note", "備考", "Note");
    return true;
}();

QLabel *makeBadge(const QString &text, const char *kind, QWidget *parent)
{
    auto *b = new QLabel(text, parent);
    QString css = "border-radius:3px; padding:1px 6px; font-size:11px;";
    if (qstrcmp(kind, "ok") == 0)        css += "background:#DFF6DD; color:#0F7B0F;";
    else if (qstrcmp(kind, "warn") == 0) css += "background:#FFF4CE; color:#9D5D00;";
    else if (qstrcmp(kind, "acc") == 0)  css += "background:#DEECF9; color:#0078D4;";
    else                                  css += "background:palette(midlight);";
    b->setStyleSheet(css);
    return b;
}

// 表セル内バッジ (左寄せ)
QWidget *badgeCell(const QString &text, const char *kind)
{
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(4, 2, 4, 2);
    h->addWidget(makeBadge(text, kind, w));
    h->addStretch(1);
    return w;
}

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}

// ドメイン別「チェックする量」(mock の三項演算子をそのまま転記)
const char *meshQuantity(ofd::Domain d)
{
    switch (d) {
        case ofd::Domain::Optical:    return "T_drop @ 1550nm";
        case ofd::Domain::Acoustic:   return "RT60 @ 1kHz";
        case ofd::Domain::Underwater: return "TL @ 50km";
        default:                      return "S11 @ 2.45GHz";
    }
}

// ── λ/4 境界余裕の計算 (MeshTab.cpp の λ/N 評価と同じ定義) ──────────────────
// 波長評価に使うドメイン別の伝搬速度 [m/s]。MeshTab の file-local ヘルパーと
// 同値 (数行のためここも file-local に留める)。水中は PML セクション自体を
// 隠すので到達しないが、既定 (光速) のままにしておく。
double domainWaveSpeed(ofd::Domain d)
{
    switch (d) {
    case ofd::Domain::Acoustic:   return 343.0;         // 空気中の音速 (20℃)
    case ofd::Domain::Underwater: return 1500.0;        // 海水中の代表音速
    default:                      return 2.99792458e8;  // EM / 光: 真空光速
    }
}

// 解析周波数1の中心 [Hz] (MeshTab の λ/N 評価と同じ)。無ければ 0
double centerFreq1(const ofd::GeneralOpts &g)
{
    return g.hasF1 ? 0.5 * (g.f1min + g.f1max) : 0.0;
}

// 周波数の簡易表示 (MeshTab::formatFreq と同形式)
QString formatFreq(double f)
{
    if (f >= 1e9) return QStringLiteral("%1 GHz").arg(f / 1e9, 0, 'g', 3);
    if (f >= 1e6) return QStringLiteral("%1 MHz").arg(f / 1e6, 0, 'g', 3);
    if (f >= 1e3) return QStringLiteral("%1 kHz").arg(f / 1e3, 0, 'g', 3);
    return QStringLiteral("%1 Hz").arg(f, 0, 'g', 3);
}

// λ/4 マージン [m]。周波数が無く計算できないときは 0
double quarterWaveMargin(const ofd::Project &p)
{
    const double fc = centerFreq1(p.general());
    return (fc > 0) ? 0.25 * domainWaveSpeed(p.activeDomain()) / fc : 0.0;
}

// 軸 a (0=x 1=y 2=z) が +λ/4 拡張の対象か。
// 周期境界 (PBC) の軸は端を延ばすと周期そのものが変わってしまうため除外する
bool axisExtendable(const ofd::Project &p, int a)
{
    const ofd::GeneralOpts &g = p.general();
    const bool pbc = (a == 0) ? g.pbcX : (a == 1) ? g.pbcY : g.pbcZ;
    return !pbc;
}

// +λ/4 拡張が今すぐ安全に実行できるか (対象軸が 1 本以上あり、全て妥当)
bool canAddMargin(const ofd::Project &p)
{
    if (!(quarterWaveMargin(p) > 0)) return false;
    bool any = false;
    for (int a = 0; a < 3; ++a) {
        if (!axisExtendable(p, a)) continue;
        if (!p.mesh(a).isValid()) return false;
        any = true;
    }
    return any;
}
} // namespace

// ── VerificationTab ─────────────────────────────────────────────────────────
VerificationTab::VerificationTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // 精度検証
    auto *sTop = new SectionBox(I18n::tr("ver_title"), body);
    sTop->vbox()->addWidget(hintLabel(I18n::tr("ver_hint"), sTop));
    // タブ先頭の強い注記: 全出力が固定サンプル (捏造値を実行結果と誤認させない)
    auto *mockNote = hintLabel(I18n::tr("ver_mock_note"), sTop);
    mockNote->setStyleSheet(
        "background:#FFF4CE; color:#9D5D00; border-radius:3px; "
        "padding:4px 8px; font-weight:600;");
    sTop->vbox()->addWidget(mockNote);
    v->addWidget(sTop);

    // ① メッシュ収束
    auto *sMesh = new SectionBox(I18n::tr("ver_mesh_title"), body);
    sMesh->vbox()->addWidget(hintLabel(I18n::tr("ver_mesh_hint"), sMesh));
    m_qtyBox = new QComboBox(sMesh);
    sMesh->form()->addRow(I18n::tr("ver_mesh_qty"), m_qtyBox);
    // 「チェックする量」はどこにも読まれない
    sMesh->form()->addRow(tabhelp::unwiredNote(sMesh));

    auto *meshTbl = new QTableWidget(5, 4, sMesh);
    meshTbl->setHorizontalHeaderLabels({
        I18n::tr("ver_h_mesh"), I18n::tr("ver_h_cells"),
        I18n::tr("ver_h_result"), I18n::tr("ver_h_err") });
    struct { const char *mesh, *cells, *result, *err; } kMeshRows[4] = {
        { "1", "8,000",   "0.7421", "-12.4%" },
        { "2", "27,900",  "0.8104", "-4.4%"  },
        { "3", "88,000",  "0.8408", "-0.8%"  },
        { "4", "280,000", "0.8472", nullptr  },   // 誤差は ver_baseline
    };
    for (int r = 0; r < 4; ++r) {
        meshTbl->setItem(r, 0, new QTableWidgetItem(QString::fromUtf8(kMeshRows[r].mesh)));
        meshTbl->setItem(r, 1, new QTableWidgetItem(QString::fromUtf8(kMeshRows[r].cells)));
        meshTbl->setItem(r, 2, new QTableWidgetItem(QString::fromUtf8(kMeshRows[r].result)));
        meshTbl->setItem(r, 3, new QTableWidgetItem(
            kMeshRows[r].err ? QString::fromUtf8(kMeshRows[r].err)
                             : I18n::tr("ver_baseline")));
    }
    // 行3 (レベル3) が選択状態 (mock の className="sel")
    for (int c = 0; c < 4; ++c)
        if (auto *it = meshTbl->item(2, c))
            it->setBackground(QColor(0, 120, 212, 36));
    // 「5+ (未実行)」行
    auto *lv5 = new QTableWidgetItem("5+");
    lv5->setForeground(QColor(128, 128, 128));
    meshTbl->setItem(4, 0, lv5);
    auto *notRun = new QTableWidgetItem(I18n::tr("ver_notrun"));
    notRun->setForeground(QColor(128, 128, 128));
    meshTbl->setItem(4, 1, notRun);
    meshTbl->setSpan(4, 1, 1, 3);
    meshTbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    meshTbl->verticalHeader()->setVisible(false);
    meshTbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    meshTbl->setMinimumHeight(170);
    sMesh->vbox()->addWidget(meshTbl);

    auto *meshRow = new QHBoxLayout();
    meshRow->addWidget(makeBadge(I18n::tr("ver_mesh_ok"), "ok", sMesh));
    auto *meshRunBtn = new QPushButton(I18n::tr("ver_mesh_run"), sMesh);
    tabhelp::markNotImplemented(meshRunBtn);   // 収束テストは未実装
    meshRow->addWidget(meshRunBtn);
    meshRow->addStretch(1);
    sMesh->vbox()->addLayout(meshRow);
    // 収束表と「収束」バッジはモック固定値 (実行していない)
    sMesh->vbox()->addWidget(tabhelp::sampleNote(sMesh));
    v->addWidget(sMesh);

    // ② PML吸収品質 (FDTD 系のみ — 水中ドメインでは refreshDomain が隠す)
    auto *sPml = new SectionBox(I18n::tr("ver_pml_title"), body);
    m_pmlSection = sPml;
    sPml->vbox()->addWidget(hintLabel(I18n::tr("ver_pml_hint"), sPml));
    auto *pmlTbl = new QTableWidget(5, 3, sPml);
    pmlTbl->setHorizontalHeaderLabels({
        I18n::tr("ver_h_face"), I18n::tr("ver_h_refl"), I18n::tr("ver_h_verdict") });
    struct { const char *face, *refl; bool ok; } kPmlRows[5] = {
        { "X- PML", "-72.4 dB", true  },
        { "X+ PML", "-71.8 dB", true  },
        { "Y- PML", "-68.9 dB", true  },
        { "Y+ PML", "-68.5 dB", true  },
        { "Z+ PML", "-42.1 dB", false },
    };
    for (int r = 0; r < 5; ++r) {
        pmlTbl->setItem(r, 0, new QTableWidgetItem(QString::fromUtf8(kPmlRows[r].face)));
        pmlTbl->setItem(r, 1, new QTableWidgetItem(QString::fromUtf8(kPmlRows[r].refl)));
        pmlTbl->setCellWidget(r, 2, kPmlRows[r].ok
            ? badgeCell("OK", "ok")
            : badgeCell(I18n::tr("ver_pml_warn"), "warn"));
    }
    pmlTbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    pmlTbl->verticalHeader()->setVisible(false);
    pmlTbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pmlTbl->setMinimumHeight(170);
    sPml->vbox()->addWidget(pmlTbl);
    auto *pmlRow = new QHBoxLayout();
    // 対策 2 ボタンは配線済み: Project の実設定 (PML 層数 / メッシュ端) を
    // 変更する。文言・有効状態は refreshPmlButtons() が現状に合わせる
    m_pmlBtn1 = new QPushButton(sPml);
    m_pmlBtn2 = new QPushButton(I18n::tr("ver_pml_btn2"), sPml);
    connect(m_pmlBtn1, &QPushButton::clicked,
            this, &VerificationTab::increasePmlLayers);
    connect(m_pmlBtn2, &QPushButton::clicked,
            this, &VerificationTab::addBoundaryMargin);
    pmlRow->addWidget(m_pmlBtn1);
    pmlRow->addWidget(m_pmlBtn2);
    pmlRow->addStretch(1);
    sPml->vbox()->addLayout(pmlRow);
    // PML 反射表はモック固定値 (測定していない)
    sPml->vbox()->addWidget(tabhelp::sampleNote(sPml));
    v->addWidget(sPml);

    // ③ 時間精度 (時間領域ソルバのみ — 水中ドメインでは refreshDomain が隠す)
    auto *sTime = new SectionBox(I18n::tr("ver_time_title"), body);
    m_timeSection = sTime;
    sTime->vbox()->addWidget(hintLabel(I18n::tr("ver_time_hint"), sTime));
    m_energyPlot = new MiniPlot(sTime);
    {
        // mock: y = log10(max(1e-7, exp(-i*0.18) + random()*0.01)), 50点
        std::mt19937 rng(20260726u);   // 固定シードで再現性を確保
        std::uniform_real_distribution<double> uni(0.0, 1.0);
        MiniSeries s;
        for (int i = 0; i < 50; ++i) {
            const double y = std::log10(std::max(1e-7,
                1.0 * std::exp(-i * 0.18) + uni(rng) * 0.01));
            s.pts.append(QPointF(i, y));
        }
        m_energyPlot->setSeries({ s });
    }
    m_energyPlot->setLabels("time [×1000 steps]", "log E");
    m_energyPlot->setYRange(-7, 0);
    m_energyPlot->setMinimumHeight(120);
    sTime->vbox()->addWidget(m_energyPlot);
    auto *timeRow = new QHBoxLayout();
    timeRow->addWidget(makeBadge(I18n::tr("ver_time_ok"), "ok", sTime));
    timeRow->addStretch(1);
    sTime->vbox()->addLayout(timeRow);
    // 減衰曲線は乱数合成のモック、「エネルギー残量」バッジも固定値
    sTime->vbox()->addWidget(tabhelp::sampleNote(sTime));
    sTime->vbox()->addWidget(hintLabel(I18n::tr("ver_time_note"), sTime));
    v->addWidget(sTime);

    // ④ クロスバリデーション
    auto *sCross = new SectionBox(I18n::tr("ver_cross_title"), body);
    sCross->vbox()->addWidget(hintLabel(I18n::tr("ver_cross_hint"), sCross));
    // 項目はドメイン別 (refreshDomain が入れる)
    m_crossBox = new QComboBox(sCross);
    sCross->form()->addRow(I18n::tr("ver_cross_solver"), m_crossBox);
    // 比較ソルバの選択はどこにも読まれない
    sCross->form()->addRow(tabhelp::unwiredNote(sCross));
    auto *crossRow = new QHBoxLayout();
    auto *crossRunBtn = new QPushButton(I18n::tr("ver_cross_run"), sCross);
    tabhelp::markNotImplemented(crossRunBtn);   // クロスバリデーションは未実装
    crossRow->addWidget(crossRunBtn);
    crossRow->addStretch(1);
    sCross->vbox()->addLayout(crossRow);
    v->addWidget(sCross);

    // 自動診断
    auto *sDiag = new SectionBox(I18n::tr("ver_diag_title"), body);
    m_diag = new QTableWidget(7, 3, sDiag);
    m_diag->setHorizontalHeaderLabels({
        I18n::tr("ver_h_item"), I18n::tr("ver_h_verdict"), I18n::tr("ver_h_note") });
    struct { const char *item, *badge, *kind, *note; } kDiagRows[7] = {
        { "λ/Δx ≥ 10",                 "OK", "ok",   nullptr },   // 備考はドメイン別
        { "CFL 安定条件",               "OK", "ok",   "0.99 < 1.0" },
        { "PML 反射 < -60 dB",          "!",  "warn", "Z+ 面が -42 dB" },
        { "シャットオフ到達",           "OK", "ok",   "96% 経過時に到達" },
        { "モニター範囲が領域内",       "OK", "ok",   "—" },
        { "波源とモニターの距離 > λ",   "OK", "ok",   "4λ" },
        { "サブピクセル平均化",         "ON", "ok",   "形状誤差 ~λ/200" },
    };
    for (int r = 0; r < 7; ++r) {
        m_diag->setItem(r, 0, new QTableWidgetItem(QString::fromUtf8(kDiagRows[r].item)));
        m_diag->setCellWidget(r, 1, badgeCell(QString::fromUtf8(kDiagRows[r].badge),
                                              kDiagRows[r].kind));
        m_diag->setItem(r, 2, new QTableWidgetItem(
            kDiagRows[r].note ? QString::fromUtf8(kDiagRows[r].note) : QString()));
    }
    m_diag->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_diag->verticalHeader()->setVisible(false);
    m_diag->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_diag->setMinimumHeight(230);
    sDiag->vbox()->addWidget(m_diag);
    // 自動診断の 7 行は判定・備考ともモック固定値 (診断していない)
    sDiag->vbox()->addWidget(tabhelp::sampleNote(sDiag));
    v->addWidget(sDiag);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::domainChanged, this, &VerificationTab::refreshDomain);
    // 他タブでの編集 (pmlL・メッシュ・周波数・PBC) をボタン表示へ追従させる
    connect(project, &Project::changed, this, &VerificationTab::refreshPmlButtons);
    connect(project, &Project::loaded, this, &VerificationTab::refreshPmlButtons);
    refreshDomain();
}

// ② 対策ボタン 1: PML 層数を増加 (既に 12 以上なら +4)。
// Mur (abc=0) のまま層数だけ増やしても効果が無いので PML へ切り替える
void VerificationTab::increasePmlLayers()
{
    GeneralOpts &g = m_p->general();
    g.abc = 1;                                    // PML を使用
    g.pmlL = (g.pmlL >= 12) ? g.pmlL + 4 : 12;
    m_p->touch();   // changed() → refreshPmlButtons() で表示も追従
}

// ② 対策ボタン 2: メッシュ各軸の両端を λ/4 (解析周波数1の中心) だけ拡張する。
// 追加区間の分割数は端の区間のセル幅を保つように選ぶ (最低 1 分割)。
// PBC の軸は周期が変わってしまうため対象外 (axisExtendable)
void VerificationTab::addBoundaryMargin()
{
    if (!canAddMargin(*m_p)) return;   // 途中で失敗して半端に拡張しない
    const double margin = quarterWaveMargin(*m_p);
    // 端のセル幅を保つ分割数。周波数とメッシュスケールが極端に不整合でも
    // int を溢れさせない (上限 1e6 — その場合は総セル数の統計表示で気付ける)
    const auto ndiv = [](double ratio) {
        return int(qBound(1.0, std::floor(ratio + 0.5), 1e6));
    };
    for (int a = 0; a < 3; ++a) {
        if (!axisExtendable(*m_p, a)) continue;
        MeshAxis &ax = m_p->mesh(a);
        const int n = ax.nodes.size();
        const double dLo = (ax.nodes[1] - ax.nodes[0]) / ax.divs.first();
        const double dHi = (ax.nodes[n - 1] - ax.nodes[n - 2]) / ax.divs.last();
        ax.nodes.prepend(ax.nodes.first() - margin);
        ax.divs.prepend(ndiv(margin / dLo));
        ax.nodes.append(ax.nodes.last() + margin);
        ax.divs.append(ndiv(margin / dHi));
    }
    m_p->touch();
}

// ② 対策ボタンの表示更新: 層数は現在値→目標値で表示し、+λ/4 は実行できる
// ときだけ有効化してツールチップに実際の追加量を示す
void VerificationTab::refreshPmlButtons()
{
    const GeneralOpts &g = m_p->general();
    const int target = (g.pmlL >= 12) ? g.pmlL + 4 : 12;
    m_pmlBtn1->setText(I18n::tr("ver_pml_btn1_fmt").arg(g.pmlL).arg(target));
    m_pmlBtn1->setToolTip(I18n::tr("ver_pml_btn1_tip"));

    const bool ok = canAddMargin(*m_p);
    m_pmlBtn2->setEnabled(ok);
    m_pmlBtn2->setToolTip(ok
        ? I18n::tr("ver_pml_btn2_tip")
              .arg(formatFreq(centerFreq1(g)))
              .arg(QStringLiteral("%1 m")
                       .arg(quarterWaveMargin(*m_p), 0, 'g', 3))
        : I18n::tr("ver_pml_margin_na"));
}

void VerificationTab::refreshDomain()
{
    const Domain d = m_p->activeDomain();
    const bool uw = (d == Domain::Underwater);
    m_qtyBox->clear();
    m_qtyBox->addItem(QString::fromUtf8(meshQuantity(d)));

    // ② PML吸収品質 / ③ 時間精度は FDTD 系 (時間領域 + PML) 前提の画面。
    // 水中ドメイン (BELLHOP レイトレース) には対応概念が無い → セクションごと隠す
    m_pmlSection->setVisible(!uw);
    m_timeSection->setVisible(!uw);

    // ④ 比較ソルバ: ドメインごとの妥当な候補だけを見せる (モック表示・未配線)
    m_crossBox->clear();
    switch (d) {
        case Domain::Optical:
            m_crossBox->addItems({ "RCWA", "STACK", "tidy3d (Cloud)" });
            break;
        case Domain::Acoustic:
            m_crossBox->addItems({ I18n::tr("ver_cross_ism"),
                                   I18n::tr("ver_cross_ray") });
            break;
        case Domain::Underwater:
            m_crossBox->addItems({ I18n::tr("ver_cross_pe"), "BEM" });
            break;
        default:   // EM
            m_crossBox->addItem("FEM (Frequency)");
            break;
    }

    // 自動診断: CFL (行1)・PML 反射 (行2)・サブピクセル平均化 (行6) は
    // FDTD 系の項目で水中では無意味 → 行を隠す
    m_diag->setRowHidden(1, uw);
    m_diag->setRowHidden(2, uw);
    m_diag->setRowHidden(6, uw);

    // 自動診断 行0 備考: 例示周波数をドメインに合わせる
    QString freq;
    switch (d) {
        case Domain::Optical:    freq = QStringLiteral("1550nm"); break;
        case Domain::Acoustic:   freq = QStringLiteral("1kHz");   break;
        case Domain::Underwater: freq = QStringLiteral("3.5kHz"); break;
        default:                 freq = QStringLiteral("2.5GHz"); break;
    }
    if (auto *it = m_diag->item(0, 2))
        it->setText(QString("λ/Δx = 22 @ %1").arg(freq));

    // ドメインで伝搬速度 (λ/4 の量) が変わるためボタン表示も更新
    refreshPmlButtons();
}
