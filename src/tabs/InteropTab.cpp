// InteropTab.cpp
#include "InteropTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

namespace {
// タブ専用語彙 (接頭辞 iop_) — file-local 登録
const bool s_i18n = [] {
    ofd::I18n::reg("iop_detect_title", "🧰 インストール済みツール検出", "🧰 Detected tools");
    ofd::I18n::reg("iop_detect_hint",
        "起動時にローカル環境をスキャン。未検出のツール連携はファイル形式変換のみ "
        "(相手ツールの起動・実行は不可) として動作します。",
        "The local environment is scanned at start-up. For tools that are not "
        "detected, only file-format conversion is available (the tool itself "
        "cannot be launched or run).");
    ofd::I18n::reg("iop_h_tool", "ツール", "Tool");
    ofd::I18n::reg("iop_h_kind", "種別", "Type");
    ofd::I18n::reg("iop_h_found", "検出", "Detected");
    ofd::I18n::reg("iop_h_alt", "未検出時の代替", "Fallback when not detected");
    ofd::I18n::reg("iop_found", "✓ 検出", "✓ Detected");
    ofd::I18n::reg("iop_notfound", "未検出", "Not found");
    ofd::I18n::reg("iop_kind_oss", "OSS", "OSS");
    ofd::I18n::reg("iop_kind_free", "無償", "Freeware");
    ofd::I18n::reg("iop_kind_comm", "商用", "Commercial");
    ofd::I18n::reg("iop_kind_cloud", "商用クラウド", "Commercial cloud");
    ofd::I18n::reg("iop_rescan", "↻ 再スキャン", "↻ Rescan");
    ofd::I18n::reg("iop_setpath", "📁 ツールパスを手動設定…",
                   "📁 Set tool paths manually…");
    ofd::I18n::reg("iop_policy",
        "▸ 方針: 商用ツールがなくても全ワークフローが内蔵ソルバで完結する設計。"
        "外部ツールは「あれば相互検証に使う」位置づけ。"
        "ファイル取込・書出は相手ツールのインストール不要 (形式を直接パース)。",
        "▸ Policy: every workflow can be completed with the built-in solvers even "
        "without commercial tools. External tools are there \"for cross-validation "
        "if available\". Import/export needs no installation of the other tool "
        "(the formats are parsed directly).");

    ofd::I18n::reg("iop_bridge_title", "🔗 外部ツール連携", "🔗 Tool interoperability");
    ofd::I18n::reg("iop_bridge_hint",
        "サードパーティ製ツールとの入出力ブリッジ。すべてローカルファイル経由 "
        "(オフライン運用前提)。",
        "Import/export bridges to third-party tools — all through local files "
        "(offline operation assumed).");
    ofd::I18n::reg("iop_import", "⬅ インポート", "⬅ Import");
    ofd::I18n::reg("iop_export", "➡ エクスポート", "➡ Export");
    ofd::I18n::reg("iop_h_fmt", "形式", "Format");
    ofd::I18n::reg("iop_h_target", "対象ツール", "Target tool");
    ofd::I18n::reg("iop_h_what", "内容", "Contents");
    ofd::I18n::reg("iop_h_support", "対応", "Support");
    ofd::I18n::reg("iop_sup_ok", "対応", "Supported");
    ofd::I18n::reg("iop_sup_partial", "一部対応", "Partial");
    ofd::I18n::reg("iop_do_import", "📁 取込…", "📁 Import…");
    ofd::I18n::reg("iop_do_export", "💾 書出…", "💾 Export…");
    ofd::I18n::reg("iop_bridge_note",
        "▸ 取込/書出はすべて本体内蔵のパーサ・ライタで処理 — "
        "対象ツールのインストールは不要です。",
        "▸ Import and export are handled entirely by built-in parsers and writers "
        "— no installation of the target tool is required.");

    ofd::I18n::reg("iop_batch_title", "一括変換", "Batch conversion");
    ofd::I18n::reg("iop_watch", "監視フォルダ", "Watch folder");
    ofd::I18n::reg("iop_browse", "📁 参照…", "📁 Browse…");
    ofd::I18n::reg("iop_auto_detect",
        "フォルダ内の対応形式を自動検出して取込キューへ",
        "Auto-detect supported formats in the folder and queue them for import");
    ofd::I18n::reg("iop_save_log", "取込後に変換ログを保存",
                   "Save a conversion log after importing");
    ofd::I18n::reg("iop_run_batch", "▶ 一括変換実行", "▶ Run batch conversion");

    ofd::I18n::reg("iop_script_title", "スクリプトAPI連携", "Scripting bridges");
    ofd::I18n::reg("iop_bd_matlab", "MATLAB (.mat 読書き)", "MATLAB (.mat read/write)");
    ofd::I18n::reg("iop_bd_jupyter", "Jupyter (HDF5経由)", "Jupyter (via HDF5)");
    ofd::I18n::reg("iop_script_note",
        "▸ 詳細はスクリプトタブ参照。全結果は HDF5 (.h5) が正本 — "
        "外部ツールはそこから読むのが最も確実。",
        "▸ See the Script tab for details. HDF5 (.h5) is the master copy of every "
        "result — reading from there is the most reliable route for external tools.");

    ofd::I18n::reg("iop_oss_title", "OSS代替スタック",
                   "Free && open-source alternatives");
    ofd::I18n::reg("iop_oss_hint",
        "商用ツールを持たない環境向けの推奨無償スタック (相互検証・後処理用)。",
        "Recommended free stack for environments without commercial tools "
        "(cross-validation and post-processing).");
    ofd::I18n::reg("iop_h_commercial", "商用ツール", "Commercial tool");
    ofd::I18n::reg("iop_h_oss", "OSS代替", "OSS alternative");
    ofd::I18n::reg("iop_h_role", "本体での位置づけ", "Role in OpenFDTD-X");
    return true;
}();

// ── バッジ (VerificationTab と同じ流儀) ─────────────────────────────────────
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

// ── mock の DATA / 検出表 / OSS表をそのまま転記 ─────────────────────────────
struct BridgeRow { const char *fmt, *tool, *what; bool ok; };   // ok=false → 一部対応
struct ToolRow   { const char *name; const char *kindKey; bool found; const char *alt; };
struct OssRow    { const char *commercial, *oss, *role; };

// EM
const BridgeRow kEmIn[] = {
    { ".hfss / .aedt",   "Ansys HFSS/AEDT", "形状・材料・ポート",       true  },
    { ".cst",            "CST Studio",      "形状・境界条件",           true  },
    { ".pre / .cfx",     "FEKO",            "形状・MoM設定",            false },
    { ".kicad_pcb",      "KiCad",           "PCB導体層 → PEEC/FDTD",    true  },
    { "ODB++ / Gerber",  "Altium/Eagle",    "PCBスタックアップ",        true  },
    { ".s*p Touchstone", "VNA実測/他ソルバ", "測定Sパラ (検証比較)",     true  },
};
const BridgeRow kEmOut[] = {
    { ".s*p Touchstone",   "ADS / AWR / QUCS",   "Sパラメータ",              true  },
    { ".ffe / .ffs",       "FEKO / CST",         "遠方界パターン",           true  },
    { ".msi (MSI Planet)", "電波伝搬ツール",      "アンテナパターン",         true  },
    { ".cir / .subckt",    "SPICE系",            "等価回路 (PEEC)",          true  },
    { ".uan",              "Savant/EMIT",        "3D放射パターン",           false },
    { "EMC report",        "CISPR32/FCC測定所",  "放射エミッション比較表",    true  },
};
const ToolRow kEmTools[] = {
    { "ngspice",   "iop_kind_oss",  true,  "— (内蔵)" },
    { "KiCad",     "iop_kind_oss",  true,  ".kicad_pcb 直接パース" },
    { "Ansys HFSS","iop_kind_comm", false, "内蔵FEM波動ソルバで代替 / .aedtは形状のみ取込" },
    { "CST Studio","iop_kind_comm", false, "本体FDTDで同等解析 / .cst形状取込" },
    { "openEMS",   "iop_kind_oss",  false, "インストール推奨 (相互検証用の無償FDTD)" },
    { "scikit-rf", "iop_kind_oss",  true,  "— (Touchstone処理)" },
};
const OssRow kEmOss[] = {
    { "HFSS / CST", "openEMS, gprMax, scikit-rf", "相互検証 (本体FDTDが主)" },
    { "ADS / AWR",  "QUCS-S + ngspice",           "回路側の共シミュレーション" },
    { "Altium",     "KiCad",                      "PCB取込元として全対応" },
};

// Optical
const BridgeRow kOptIn[] = {
    { ".zmx / .zar", "Zemax OpticStudio",   "レンズ系 → Lens Editor",    true  },
    { ".agf / .xml", "Zemax/CODE V ガラス", "ガラスカタログ (取込済)",    true  },
    { ".seq",        "CODE V",              "順次光学系",                false },
    { ".fsp",        "Lumerical FDTD",      "形状・波源・モニター",       true  },
    { ".py (MEEP)",  "MEEP",                "Python形状定義の解釈",       false },
    { "GDSII/OASIS", "KLayout/SiEPIC",      "フォトニックICレイアウト",   true  },
};
const BridgeRow kOptOut[] = {
    { ".json (td.Simulation)", "tidy3d",              "クラウド解析 (専用タブ)",       true  },
    { ".zmx",                  "Zemax OpticStudio",   "設計逆輸出 (レンズ)",           false },
    { ".s*p / .dat",           "INTERCONNECT/Aspic",  "光Sパラ・コンパクトモデル",     true  },
    { ".fsp",                  "Lumerical",           "相互検証用",                    false },
    { "GDSII",                 "ファウンドリ (AMF等)", "製造テープアウト (DRC済)",      true  },
    { ".mat / .npz",           "MATLAB / Python",     "場データ・スペクトル",          true  },
};
const ToolRow kOptTools[] = {
    { "tidy3d client",       "iop_kind_cloud", false,
      "ローカルFDTDで縮小モデル検証→後日送信用 .json を保存" },
    { "Lumerical",           "iop_kind_comm",  false,
      "本体FDTD/RCWAで同等解析 / .fspは形状のみ取込" },
    { "Zemax OpticStudio",   "iop_kind_comm",  false,
      "内蔵 Lens Editor (順次光線追跡) で代替" },
    { "MEEP",                "iop_kind_oss",   false, "インストール推奨 (相互検証用)" },
    { "KLayout",             "iop_kind_oss",   true,  "— (GDS確認に使用可)" },
    { "RayOptics (Python)",  "iop_kind_oss",   true,  "—" },
};
const OssRow kOptOss[] = {
    { "Lumerical",       "MEEP, Tidy3D無償枠, EMpy",
      "相互検証 (本体FDTD/RCWAが主)" },
    { "Zemax / CODE V",  "RayOptics, ray-optics (Python), Goptical",
      "内蔵Lens Editorの検証" },
    { "INTERCONNECT",    "SAX (Python S-matrix), Simphony", "回路レベル検証" },
};

// Acoustic
const BridgeRow kAcIn[] = {
    { ".xhn / .frd", "AFMG EASE",              "ホールモデル・スピーカー設定", true  },
    { ".par / .odm", "Odeon",                   "室形状・材料割当",            false },
    { ".geo / .md9", "CATT-Acoustic",           "室形状",                      false },
    { ".ifc",        "Revit / ArchiCAD (BIM)",  "建築モデル+材料属性",         true  },
    { ".skp / .dae", "SketchUp",                "ホール3D形状",                true  },
    { ".gll / .clf", "スピーカーメーカー",       "指向性 (音源タブ)",           true  },
    { ".sofa",       "HRTF DB (可聴化タブ)",     "個人化HRTF",                  true  },
};
const BridgeRow kAcOut[] = {
    { ".wav (IR)",        "REW / Smaart / DAW",   "インパルス応答 (畳込リバーブ)",         true  },
    { ".sofa",            "SPARTA / Reaper",      "空間IR (Ambisonics/バイノーラル)",      true  },
    { ".etx / .txt",      "EASERA / ARTA",        "実測解析ツールとの比較",                true  },
    { ".xhn",             "AFMG EASE",            "モデル逆輸出",                          false },
    { "ISO 3382 report",  "報告書 (PDF/HTML)",    "全指標の準拠レポート",                  true  },
};
const ToolRow kAcTools[] = {
    { "AFMG EASE",     "iop_kind_comm", false,
      "内蔵幾何音響+電気音響タブで代替 / .xhn取込は可" },
    { "Odeon",         "iop_kind_comm", false, "内蔵レイトレース/ISMで同等解析" },
    { "REW",           "iop_kind_free", true,  "— (IR検証に推奨)" },
    { "SPARTA (VST)",  "iop_kind_oss",  false, "内蔵バイノーラルレンダラで可聴化" },
    { "SketchUp",      "iop_kind_comm", false,
      ".skpは直接パース (起動不要) / IFC・STL経由も可" },
    { "Blender",       "iop_kind_oss",  true,  "— (形状編集に推奨)" },
};
const OssRow kAcOss[] = {
    { "EASE / Odeon / CATT", "内蔵ソルバ + pyroomacoustics, RAVEN(学術)",
      "本体で完結、OSS で追検証" },
    { "EASERA / Smaart",     "REW (無償), ARTA(シェアウェア)", "実測IRの取得・比較" },
    { "SketchUp Pro",        "Blender, FreeCAD",               "形状編集→STL/IFC取込" },
};

// Underwater
const BridgeRow kUwIn[] = {
    { ".env / .bty",   "Bellhop (AT)",       "SSP・海底地形・レイ設定",       true },
    { ".in (RAM)",     "RAM PE",             "放物線方程式入力",              true },
    { ".nc (NetCDF)",  "WOA/CMEMS/HYCOM",    "水温・塩分格子 (海洋環境タブ)", true },
    { ".xyz / .grd",   "GEBCO/ETOPO",        "海底地形 (海洋環境タブ)",       true },
    { ".csv (CTD)",    "CTD観測データ",       "実測プロファイル → SSP",        true },
};
const BridgeRow kUwOut[] = {
    { ".env",         "Bellhop / Kraken",    "解析条件の相互検証",              true },
    { ".shd",         "AT plotshd",          "伝搬損失場",                      true },
    { ".nc (NetCDF)", "MATLAB/Python/GIS",   "TL格子・時系列",                  true },
    { ".wav",         "ソナー信号処理",       "受信波形 (整合フィルタ検証)",     true },
    { ".h5",          "本体H5アニメ/外部",   "時系列場データ",                  true },
};
const ToolRow kUwTools[] = {
    { "Bellhop (AT)",     "iop_kind_oss",  true,  "— (内蔵ポートもあり)" },
    { "RAM PE",           "iop_kind_oss",  false, "内蔵PEソルバで代替 / .in書出は可" },
    { "Kraken",           "iop_kind_oss",  false, "内蔵法線モードソルバで代替" },
    { "MATLAB",           "iop_kind_comm", false, "Python/NumPy ブリッジ (.npz/.h5) で代替" },
    { "Python+netCDF4",   "iop_kind_oss",  true,  "—" },
};
const OssRow kUwOss[] = {
    { "—(商用少数)",
      "Bellhop/Kraken/RAM (AT一式), arlpy, UnderwaterAcoustics.jl",
      "標準ツール群がOSS — 本体ポート内蔵" },
    { "MATLAB", "Python + NumPy/SciPy/netCDF4", ".npz/.h5/.nc ブリッジ" },
};

// ドメイン → データ束 (mock の DATA[domain] || DATA.em 相当)
struct DomainData {
    const BridgeRow *imports; int nImp;
    const BridgeRow *exports; int nExp;
    const ToolRow   *tools;   int nTools;
    const OssRow    *oss;     int nOss;
};

DomainData dataFor(ofd::Domain d)
{
    switch (d) {
    case ofd::Domain::Optical:
        return { kOptIn, 6, kOptOut, 6, kOptTools, 6, kOptOss, 3 };
    case ofd::Domain::Acoustic:
        return { kAcIn, 7, kAcOut, 5, kAcTools, 6, kAcOss, 3 };
    case ofd::Domain::Underwater:
        return { kUwIn, 5, kUwOut, 5, kUwTools, 5, kUwOss, 2 };
    default:
        return { kEmIn, 6, kEmOut, 6, kEmTools, 6, kEmOss, 3 };
    }
}

// 種別バッジは OSS / 無償 のとき ok 色 (mock: k==="OSS"||k==="無償")
bool kindIsFree(const char *kindKey)
{
    return qstrcmp(kindKey, "iop_kind_oss") == 0
        || qstrcmp(kindKey, "iop_kind_free") == 0;
}

QTableWidgetItem *monoItem(const QString &text)
{
    auto *it = new QTableWidgetItem(text);
    QFont f = it->font();
    f.setFamily("Consolas");
    f.setStyleHint(QFont::Monospace);
    it->setFont(f);
    return it;
}

QTableWidgetItem *mutedItem(const QString &text)
{
    auto *it = new QTableWidgetItem(text);
    it->setForeground(QColor("#888888"));
    return it;
}
} // namespace

// ── InteropTab ──────────────────────────────────────────────────────────────
InteropTab::InteropTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // 🧰 インストール済みツール検出 / Detected tools
    auto *sd = new SectionBox(I18n::tr("iop_detect_title"), body);
    sd->vbox()->addWidget(hintLabel(I18n::tr("iop_detect_hint"), sd));
    m_detected = new QTableWidget(0, 4, sd);
    m_detected->setHorizontalHeaderLabels({ I18n::tr("iop_h_tool"),
                                            I18n::tr("iop_h_kind"),
                                            I18n::tr("iop_h_found"),
                                            I18n::tr("iop_h_alt") });
    m_detected->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_detected->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_detected->verticalHeader()->setVisible(false);
    m_detected->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_detected->setWordWrap(false);
    sd->vbox()->addWidget(m_detected);
    auto *drow = new QHBoxLayout();
    drow->addWidget(new QPushButton(I18n::tr("iop_rescan"), sd));
    drow->addWidget(new QPushButton(I18n::tr("iop_setpath"), sd));
    drow->addStretch(1);
    sd->vbox()->addLayout(drow);
    sd->vbox()->addWidget(hintLabel(I18n::tr("iop_policy"), sd));
    v->addWidget(sd);

    // 🔗 外部ツール連携 / Tool interoperability
    auto *sb = new SectionBox(I18n::tr("iop_bridge_title"), body);
    sb->vbox()->addWidget(hintLabel(I18n::tr("iop_bridge_hint"), sb));
    auto *segRow = new QHBoxLayout();
    m_dirImport = new QPushButton(I18n::tr("iop_import"), sb);
    m_dirImport->setCheckable(true);
    m_dirImport->setChecked(true);
    m_dirExport = new QPushButton(I18n::tr("iop_export"), sb);
    m_dirExport->setCheckable(true);
    auto *dirGroup = new QButtonGroup(this);      // <Seg> 相当 (排他トグル)
    dirGroup->setExclusive(true);
    dirGroup->addButton(m_dirImport, 0);
    dirGroup->addButton(m_dirExport, 1);
    segRow->addWidget(m_dirImport);
    segRow->addWidget(m_dirExport);
    segRow->addStretch(1);
    sb->vbox()->addLayout(segRow);

    m_bridges = new QTableWidget(0, 5, sb);
    m_bridges->setHorizontalHeaderLabels({ I18n::tr("iop_h_fmt"),
                                           I18n::tr("iop_h_target"),
                                           I18n::tr("iop_h_what"),
                                           I18n::tr("iop_h_support"), "" });
    m_bridges->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_bridges->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_bridges->verticalHeader()->setVisible(false);
    m_bridges->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sb->vbox()->addWidget(m_bridges);
    sb->vbox()->addWidget(hintLabel(I18n::tr("iop_bridge_note"), sb));
    v->addWidget(sb);

    // 一括変換 / Batch conversion
    auto *sc = new SectionBox(I18n::tr("iop_batch_title"), body);
    auto *wrow = new QHBoxLayout();
    m_watchDir = new QLineEdit("D:/exchange/inbox/", sc);
    wrow->addWidget(m_watchDir, 1);
    wrow->addWidget(new QPushButton(I18n::tr("iop_browse"), sc));
    sc->form()->addRow(I18n::tr("iop_watch"), wrow);
    auto *ckAuto = new QCheckBox(I18n::tr("iop_auto_detect"), sc);
    ckAuto->setChecked(true);
    auto *ckLog = new QCheckBox(I18n::tr("iop_save_log"), sc);
    ckLog->setChecked(true);
    sc->form()->addRow(ckAuto);
    sc->form()->addRow(ckLog);
    auto *brow = new QHBoxLayout();
    brow->addWidget(new QPushButton(I18n::tr("iop_run_batch"), sc));
    auto *cli = new QLabel("CLI: ofdx-convert --in model.zmx --out project.ofd", sc);
    cli->setStyleSheet("font-family:'Consolas','Menlo',monospace; color:#888888;");
    cli->setTextInteractionFlags(Qt::TextSelectableByMouse);
    brow->addWidget(cli);
    brow->addStretch(1);
    sc->vbox()->addLayout(brow);
    v->addWidget(sc);

    // スクリプトAPI連携 / Scripting bridges
    auto *ss = new SectionBox(I18n::tr("iop_script_title"), body);
    auto *srow = new QHBoxLayout();
    srow->addWidget(makeBadge("Python API", "acc", ss));
    srow->addWidget(makeBadge(I18n::tr("iop_bd_matlab"), "", ss));
    srow->addWidget(makeBadge(I18n::tr("iop_bd_jupyter"), "", ss));
    srow->addWidget(makeBadge("ParaView (.xdmf)", "", ss));
    srow->addStretch(1);
    ss->vbox()->addLayout(srow);
    ss->vbox()->addWidget(hintLabel(I18n::tr("iop_script_note"), ss));
    v->addWidget(ss);

    // OSS代替スタック / Free & open-source alternatives
    auto *so = new SectionBox(I18n::tr("iop_oss_title"), body);
    so->vbox()->addWidget(hintLabel(I18n::tr("iop_oss_hint"), so));
    m_oss = new QTableWidget(0, 3, so);
    m_oss->setHorizontalHeaderLabels({ I18n::tr("iop_h_commercial"),
                                       I18n::tr("iop_h_oss"),
                                       I18n::tr("iop_h_role") });
    m_oss->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_oss->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_oss->verticalHeader()->setVisible(false);
    m_oss->setEditTriggers(QAbstractItemView::NoEditTriggers);
    so->vbox()->addWidget(m_oss);
    v->addWidget(so);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // ── 接続 ──
    connect(dirGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_dir = id;
        rebuildBridges();
    });
    connect(project, &Project::domainChanged, this, &InteropTab::refresh);
    connect(project, &Project::loaded, this, &InteropTab::refresh);
    refresh();
}

void InteropTab::refresh()
{
    m_updating = true;
    rebuildDetected();
    rebuildBridges();
    rebuildOss();
    m_updating = false;
}

// 🧰 インストール済みツール検出 (mock: domain 分岐の配列)
void InteropTab::rebuildDetected()
{
    const DomainData d = dataFor(m_p->activeDomain());
    m_detected->setRowCount(0);
    m_detected->setRowCount(d.nTools);
    for (int r = 0; r < d.nTools; ++r) {
        const ToolRow &t = d.tools[r];
        m_detected->setItem(r, 0, new QTableWidgetItem(QString::fromUtf8(t.name)));
        m_detected->setCellWidget(r, 1,
            badgeCell(I18n::tr(t.kindKey), kindIsFree(t.kindKey) ? "ok" : ""));
        m_detected->setCellWidget(r, 2,
            t.found ? badgeCell(I18n::tr("iop_found"), "ok")
                    : badgeCell(I18n::tr("iop_notfound"), "warn"));
        m_detected->setItem(r, 3, mutedItem(QString::fromUtf8(t.alt)));
    }
    m_detected->resizeRowsToContents();
    m_detected->setMinimumHeight(30 * d.nTools + 38);
}

// 🔗 インポート / エクスポート形式 (dir トグルで rows を差し替え)
void InteropTab::rebuildBridges()
{
    const DomainData d = dataFor(m_p->activeDomain());
    const BridgeRow *rows = (m_dir == 0) ? d.imports : d.exports;
    const int n           = (m_dir == 0) ? d.nImp : d.nExp;
    const QString btn = I18n::tr(m_dir == 0 ? "iop_do_import" : "iop_do_export");

    m_bridges->setRowCount(0);
    m_bridges->setRowCount(n);
    for (int r = 0; r < n; ++r) {
        m_bridges->setItem(r, 0, monoItem(QString::fromUtf8(rows[r].fmt)));
        m_bridges->setItem(r, 1, new QTableWidgetItem(QString::fromUtf8(rows[r].tool)));
        m_bridges->setItem(r, 2, new QTableWidgetItem(QString::fromUtf8(rows[r].what)));
        m_bridges->setCellWidget(r, 3,
            rows[r].ok ? badgeCell(I18n::tr("iop_sup_ok"), "ok")
                       : badgeCell(I18n::tr("iop_sup_partial"), "warn"));
        m_bridges->setCellWidget(r, 4, new QPushButton(btn, m_bridges));
    }
    m_bridges->resizeRowsToContents();
    m_bridges->setMinimumHeight(30 * n + 38);
}

// OSS代替スタック (mock: domain 分岐の配列)
void InteropTab::rebuildOss()
{
    const DomainData d = dataFor(m_p->activeDomain());
    m_oss->setRowCount(0);
    m_oss->setRowCount(d.nOss);
    for (int r = 0; r < d.nOss; ++r) {
        m_oss->setItem(r, 0,
            new QTableWidgetItem(QString::fromUtf8(d.oss[r].commercial)));
        m_oss->setItem(r, 1, mutedItem(QString::fromUtf8(d.oss[r].oss)));
        m_oss->setItem(r, 2, mutedItem(QString::fromUtf8(d.oss[r].role)));
    }
    m_oss->resizeRowsToContents();
    m_oss->setMinimumHeight(30 * d.nOss + 38);
}
