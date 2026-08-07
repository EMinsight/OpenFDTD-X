// ScriptsTab.cpp
#include "ScriptsTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QPushButton>
#include <QStringConverter>
#include <QTableWidget>
#include <QTextCursor>
#include <QTextStream>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 scr_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("scr_title_fmt", "スクリプトエディタ (%1サンプル)",
              "Script editor (%1 sample)");
    I18n::reg("scr_python", "Python", "Python");
    I18n::reg("scr_lsf", "Script (LSF風)", "Script (LSF-style)");
    I18n::reg("scr_load", "📁 読込", "📁 Load");
    I18n::reg("scr_save", "💾 保存", "💾 Save");
    I18n::reg("scr_examples", "📚 サンプル", "📚 Examples");
    I18n::reg("scr_load_title", "スクリプトを読込", "Load script");
    I18n::reg("scr_save_title", "スクリプトを保存", "Save script");
    I18n::reg("scr_filter_py", "Pythonスクリプト (*.py);;すべてのファイル (*)",
              "Python scripts (*.py);;All files (*)");
    I18n::reg("scr_filter_lsf", "スクリプト (*.lsf *.m);;すべてのファイル (*)",
              "Scripts (*.lsf *.m);;All files (*)");
    I18n::reg("scr_run", "▶ 実行", "▶ Run");
    I18n::reg("scr_abort", "⏸ 中断", "⏸ Abort");
    I18n::reg("scr_status_fmt", "行 %1 列 %2 · UTF-8", "Ln %1 Col %2 · UTF-8");
    I18n::reg("scr_console", "コンソール", "Console");
    I18n::reg("scr_run_tip", "python3 で実行します (%1)",
              "Runs with python3 (%1)");
    I18n::reg("scr_run_lsf_na",
              "LSF 風スクリプトは実行できません (Lumerical 専用言語で"
              "インタプリタが存在しないため)。編集と保存のみできます。",
              "The LSF-style script cannot be run (it is Lumerical's own "
              "language and no interpreter exists here). Edit and save only.");
    I18n::reg("scr_run_nopython",
              "python3 が見つかりません (PATH に無いため実行できません)",
              "python3 was not found on PATH, so the script cannot be run");
    I18n::reg("scr_run_start",
              "=== 実行 %1 (作業ディレクトリ %2) ===\n"
              "環境変数 OFDX_PROJECT / OFDX_WORKDIR を渡しています。",
              "=== running %1 (working directory %2) ===\n"
              "OFDX_PROJECT / OFDX_WORKDIR are passed in the environment.");
    I18n::reg("scr_run_done", "=== 終了 (終了コード %1) ===",
              "=== finished (exit code %1) ===");
    I18n::reg("scr_run_crash", "=== 異常終了 ===", "=== crashed ===");
    I18n::reg("scr_run_aborted", "=== 中断しました ===", "=== aborted ===");
    I18n::reg("scr_run_error", "エラー: %1", "error: %1");
    I18n::reg("scr_run_writefail", "スクリプトを書けません: %1 (%2)",
              "cannot write the script: %1 (%2)");
    I18n::reg("scr_console_hint",
              "「▶ 実行」で python3 に渡します (LSF は実行できません)。\n"
              "出力はここに逐次表示されます。",
              "Press \"▶ Run\" to hand the script to python3 (LSF cannot be "
              "run).\nOutput is streamed here.");
    I18n::reg("scr_api_title", "スクリプトから触れるもの (ドメイン別)",
              "What a script can touch (per domain)");
    I18n::reg("scr_c_func", "名前", "Name");
    I18n::reg("scr_c_desc", "説明", "Description");
    return true;
}();

// ── samples[lang][domain] ───────────────────────────────────────────────────
// LSF 側は mock の転記 (実行しないので雰囲気のまま)。
// Python サンプルは **そのまま実行できるもの** に限る。
// 埋め込み Python API (openfdtd_x モジュール) は存在しない — 動かない API を
// 例示すると「あるもの」と誤解されるため書かない (絶対規則 5)。
// GUI が渡す環境変数:
//   OFDX_PROJECT … 開いている .ofd のパス (未保存なら空)
//   OFDX_WORKDIR … cwd。カーネルの出力ファイルが置かれる場所
// 依存は標準ライブラリのみ (numpy が無い環境でも落ちない)。
const char *kPyEm = R"CODE(# OpenFDTD-X — 計算結果の後処理 (電磁)
# 埋め込み API はありません。ofd / ofd_post が書いたファイルを読みます。
import os

work = os.environ.get("OFDX_WORKDIR", ".")
print("workdir :", work)
print("project :", os.environ.get("OFDX_PROJECT") or "(未保存)")

# ofd.log の給電点表 ("feed #1 (Z0[ohm] = 50.00)" の後に続く数値行):
#   frequency[Hz] Rin Xin Gin Bin Ref[dB] VSWR
log = os.path.join(work, "ofd.log")
rows, inside = [], False
if os.path.exists(log):
    with open(log, encoding="utf-8", errors="replace") as f:
        for line in f:
            if line.startswith("feed #"):
                inside = True
                continue
            if not inside:
                continue
            v = line.split()
            try:
                nums = [float(x) for x in v]
            except ValueError:
                inside = line.strip().startswith("frequency[Hz]")
                continue
            if len(nums) >= 7:
                rows.append(nums[:7])

if not rows:
    print("給電点の周波数特性が見つかりません (計算を実行してください)")
else:
    best = min(rows, key=lambda r: r[5])          # 反射が最小の点
    print("点数 : %d" % len(rows))
    print("最良 : f = %.4f GHz, Zin = %.2f%+.2fj ohm, Ref = %.2f dB, VSWR = %.3f"
          % (best[0] / 1e9, best[1], best[2], best[5], best[6]))
    band = [r[0] for r in rows if r[5] <= -10.0]  # -10 dB 帯域
    if band:
        print("-10dB 帯域 : %.4f ~ %.4f GHz" % (min(band) / 1e9, max(band) / 1e9))
)CODE";

const char *kPyOptical = R"CODE(# OpenFDTD-X — 計算結果の後処理 (光)
# orcwa の rcwa_efficiency.csv / obpm の activation_curve.csv を読みます。
import csv, os

work = os.environ.get("OFDX_WORKDIR", ".")
print("workdir :", work)

def read_csv(name):
    path = os.path.join(work, name)
    if not os.path.exists(path):
        return None, None
    with open(path, encoding="utf-8", errors="replace") as f:
        rows = [r for r in csv.reader(f) if r]
    return rows[0], rows[1:]

head, rows = read_csv("rcwa_efficiency.csv")
if rows:
    print("rcwa_efficiency.csv :", ", ".join(head))
    for r in rows[:5]:
        print("  " + "  ".join(r))
    # 無損失なら R+T=1 になるはず (エネルギー保存の目視確認)
    print("  行数 %d" % len(rows))
else:
    print("rcwa_efficiency.csv がありません")

head, rows = read_csv("activation_curve.csv")
if rows:
    print("activation_curve.csv :", ", ".join(head))
    print("  先頭 %s / 末尾 %s" % (rows[0], rows[-1]))
)CODE";

const char *kPyAcoustic = R"CODE(# OpenFDTD-X — 計算結果の後処理 (室内音響)
# AcousticRunner の契約: metrics.json (指標) + rir.wav (インパルス応答)。
import json, os, wave

work = os.environ.get("OFDX_WORKDIR", ".")
print("workdir :", work)

path = os.path.join(work, "metrics.json")
if os.path.exists(path):
    with open(path, encoding="utf-8") as f:
        m = json.load(f)
    for key in ("t30", "t20", "edt", "c50", "c80", "d50", "ts"):
        if key in m:
            print("%-4s : %s" % (key.upper(), m[key]))
else:
    print("metrics.json がありません (音響ソルバを実行してください)")

path = os.path.join(work, "rir.wav")
if os.path.exists(path):
    with wave.open(path, "rb") as w:
        ch, sr, n = w.getnchannels(), w.getframerate(), w.getnframes()
    print("rir.wav : %d ch, %d Hz, %.3f s" % (ch, sr, n / float(sr)))
)CODE";

const char *kPyUnderwater = R"CODE(# OpenFDTD-X — 計算結果の後処理 (水中音響)
# BellhopIO が書いた .env と bellhopcxx の出力 (.prt / .arr / .ray) を読みます。
import os

work = os.environ.get("OFDX_WORKDIR", ".")
print("workdir :", work)

found = sorted(f for f in (os.listdir(work) if os.path.isdir(work) else [])
               if os.path.splitext(f)[1] in (".env", ".prt", ".arr", ".ray",
                                             ".shd", ".ssp"))
if not found:
    print("BELLHOP の入出力ファイルがありません (計算を実行してください)")
for name in found:
    path = os.path.join(work, name)
    print("--- %s (%d bytes)" % (name, os.path.getsize(path)))
    if os.path.splitext(name)[1] in (".env", ".prt", ".ssp"):
        with open(path, encoding="utf-8", errors="replace") as f:
            for i, line in enumerate(f):
                if i >= 8:
                    break
                print("   " + line.rstrip())
)CODE";

const char *kLsfEm = R"CODE(## OpenFDTD-X LSF — EM antenna sweep
setnamed("patch", "length", 30e-3);
setnamed("feed",  "offset", 5e-3);
run;

S11 = getresult("input_port", "S");
?"Center freq: " + num2str(getresult("input_port","f0")/1e9) + " GHz";
)CODE";

const char *kLsfOptical = R"CODE(## OpenFDTD-X LSF — Optical ring resonator
switchtolayout;
setnamed("ring",    "radius", 5.2e-6);
setnamed("coupler", "gap",    180e-9);
run;

T = getresult("T_drop", "T");
?T;
)CODE";

const char *kLsfAcoustic = R"CODE(## OpenFDTD-X LSF — Room acoustic RT60
setnamed("absorber", "thickness", 50e-3);
run;

irf = getresult("P1_center", "impulse_response");
?"RT60 = " + num2str(rt60(irf,1000)) + " s";
)CODE";

const char *kLsfUnderwater = R"CODE(## OpenFDTD-X LSF — Underwater Bellhop run
setnamed("sonar", "depth", 50);
setnamed("sonar", "angle", 15);
setsolver("bellhop");
run;

TL = getresult("TL_range", "TL");
?"TL @ 50km = " + num2str(TL_at_range(TL,50e3)) + " dB";
)CODE";

// 言語に応じたファイルダイアログのフィルタ (python → .py, lsf → .lsf/.m)
QString scriptFilter(const QString &lang)
{
    using ofd::I18n;
    return I18n::tr(lang == "python" ? "scr_filter_py" : "scr_filter_lsf");
}

const char *sampleCode(const QString &lang, ofd::Domain d)
{
    const bool py = (lang == "python");
    switch (d) {
        case ofd::Domain::Optical:    return py ? kPyOptical    : kLsfOptical;
        case ofd::Domain::Acoustic:   return py ? kPyAcoustic   : kLsfAcoustic;
        case ofd::Domain::Underwater: return py ? kPyUnderwater : kLsfUnderwater;
        default:                      return py ? kPyEm         : kLsfEm;
    }
}

// 早見表 = **スクリプトから実際に触れるもの**。
// 埋め込み API は無いので、渡される環境変数とカーネル出力のファイル名を並べる。
struct ApiRow { const char *func, *desc; };
const ApiRow kApiHead[2] = {
    { "os.environ[\"OFDX_PROJECT\"]", "開いている .ofd のパス (未保存なら空)" },
    { "os.environ[\"OFDX_WORKDIR\"]", "作業ディレクトリ (= cwd。出力の置き場)" },
};
const ApiRow kApiEm[3] = {
    { "ofd.log",    "給電点表 (f, Rin, Xin, …, Ref[dB], VSWR)" },
    { "far1d.log",  "遠方界パターン (plotfar1d)" },
    { "far2d.log / near2d.log", "2 次元の場マップ" },
};
const ApiRow kApiOptical[3] = {
    { "rcwa_efficiency.csv", "RCWA の回折効率 (orcwa)" },
    { "activation_curve.csv", "ONN 活性化カーブ (obpm, powersweep)" },
    { "time_series_data.h5", "BPM の場 (/field, HDF5)" },
};
const ApiRow kApiAcoustic[3] = {
    { "metrics.json", "室内音響指標 (T30/C80/D50 …)" },
    { "rir.wav",      "インパルス応答 (wave モジュールで読める)" },
    { "solver.log",   "音響ソルバのログ" },
};
const ApiRow kApiUnderwater[3] = {
    { "*.env",  "BELLHOP 入力 (BellhopIO が生成)" },
    { "*.prt",  "BELLHOP の出力ログ" },
    { "*.arr / *.ray", "到達時間・線追跡の結果" },
};
const ApiRow kApiTail[2] = {
    { "python3 -u", "実行はこれ。標準出力はコンソールへ逐次表示" },
    { "(標準ライブラリのみ)", "numpy 等は入っていれば使える (必須ではない)" },
};
} // namespace

// ── ScriptsTab ──────────────────────────────────────────────────────────────
ScriptsTab::ScriptsTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    // ── スクリプトエディタ ─────────────────────────────────────────────────
    m_editorSec = new SectionBox(QString(), body);
    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(4);
    auto addLangBtn = [this, topRow](const char *key, const char *lang,
                                     QWidget *owner) {
        auto *b = new QPushButton(I18n::tr(key), owner);
        b->setCheckable(true);
        b->setStyleSheet("font-size:11px; padding:2px 8px;");
        b->setProperty("lang", QString::fromUtf8(lang));
        const QString l = QString::fromUtf8(lang);
        connect(b, &QPushButton::clicked, this, [this, l] { setLang(l); });
        topRow->addWidget(b);
        m_langBtns.push_back(b);
    };
    addLangBtn("scr_python", "python", m_editorSec);
    addLangBtn("scr_lsf", "lsf", m_editorSec);
    topRow->addStretch(1);
    auto addToolBtn = [this, topRow](const char *key, void (ScriptsTab::*slot)()) {
        auto *b = new QPushButton(I18n::tr(key), m_editorSec);
        b->setStyleSheet("font-size:11px; padding:2px 8px;");
        connect(b, &QPushButton::clicked, this, slot);
        topRow->addWidget(b);
    };
    addToolBtn("scr_load", &ScriptsTab::loadScript);
    addToolBtn("scr_save", &ScriptsTab::saveScript);
    addToolBtn("scr_examples", &ScriptsTab::insertSample);
    m_editorSec->vbox()->addLayout(topRow);

    m_editor = new QPlainTextEdit(m_editorSec);
    m_editor->setFont(mono);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setMinimumHeight(280);
    m_editor->setStyleSheet("background:#0E1116; color:#DDE2E8;"
                            "border:1px solid palette(dark); font-size:11px;");
    m_editorSec->vbox()->addWidget(m_editor);

    auto *runRow = new QHBoxLayout();
    m_runBtn = new QPushButton(I18n::tr("scr_run"), m_editorSec);
    m_runBtn->setStyleSheet("font-weight:600;");
    runRow->addWidget(m_runBtn);
    m_abortBtn = new QPushButton(I18n::tr("scr_abort"), m_editorSec);
    m_abortBtn->setEnabled(false);
    runRow->addWidget(m_abortBtn);
    connect(m_runBtn, &QPushButton::clicked, this, &ScriptsTab::runScript);
    connect(m_abortBtn, &QPushButton::clicked, this, &ScriptsTab::abortScript);
    runRow->addStretch(1);
    // ステータスは固定文言ではなく実カーソル位置を表示する
    auto *statusLbl = new QLabel(m_editorSec);
    auto updateStatus = [this, statusLbl] {
        const QTextCursor c = m_editor->textCursor();
        statusLbl->setText(I18n::tr("scr_status_fmt")
                               .arg(c.blockNumber() + 1)
                               .arg(c.columnNumber() + 1));
    };
    connect(m_editor, &QPlainTextEdit::cursorPositionChanged, this, updateStatus);
    updateStatus();
    runRow->addWidget(statusLbl);
    m_editorSec->vbox()->addLayout(runRow);
    v->addWidget(m_editorSec);

    // ── コンソール ─────────────────────────────────────────────────────────
    auto *sCon = new SectionBox(I18n::tr("scr_console"), body);
    m_console = new QPlainTextEdit(sCon);
    m_console->setReadOnly(true);
    m_console->setFont(mono);
    m_console->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_console->setMinimumHeight(140);
    m_console->setStyleSheet("background:#0E1116; color:#DDE2E8;"
                             "border:1px solid palette(dark); font-size:11px;");
    sCon->vbox()->addWidget(m_console);
    v->addWidget(sCon);

    // ── API 早見表 ─────────────────────────────────────────────────────────
    auto *sApi = new SectionBox(I18n::tr("scr_api_title"), body);
    m_api = new QTableWidget(0, 2, sApi);
    m_api->setHorizontalHeaderLabels({ I18n::tr("scr_c_func"),
                                       I18n::tr("scr_c_desc") });
    m_api->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_api->verticalHeader()->setVisible(false);
    m_api->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_api->setMinimumHeight(230);
    sApi->vbox()->addWidget(m_api);
    v->addWidget(sApi);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::domainChanged, this, &ScriptsTab::rebuild);
    rebuild();
}

void ScriptsTab::setLang(const QString &lang)
{
    m_lang = lang;
    rebuild();
}

// ── 読込 / 保存 / サンプル再挿入 ────────────────────────────────────────────
void ScriptsTab::loadScript()
{
    const QString title = I18n::tr("scr_load_title");
    const QString path = QFileDialog::getOpenFileName(this, title, QString(),
                                                      scriptFilter(m_lang));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, title, f.errorString());
        return;
    }
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    m_editor->setPlainText(in.readAll());
    // 読み込んだ内容をドメイン切替時のサンプル差し替えで潰さない
    // (rebuild() は未編集時のみサンプルを流し込む)
    m_editor->document()->setModified(true);
}

void ScriptsTab::saveScript()
{
    const QString suggested = (m_lang == "python") ? QStringLiteral("script.py")
                                                   : QStringLiteral("script.lsf");
    tabhelp::saveTextFile(this, I18n::tr("scr_save_title"), suggested,
                          scriptFilter(m_lang), m_editor->toPlainText());
}

void ScriptsTab::insertSample()
{
    // 現在の言語×ドメインのサンプルをエディタへ再挿入。
    // setPlainText は modified フラグをリセットするので、以後はドメイン切替に
    // 追従してサンプルが更新される (ctor 直後と同じ状態に戻る)。
    m_editor->setPlainText(
        QString::fromUtf8(sampleCode(m_lang, m_p->activeDomain())));
}

void ScriptsTab::rebuild()
{
    const Domain d = m_p->activeDomain();
    for (QPushButton *b : m_langBtns)
        b->setChecked(b->property("lang").toString() == m_lang);

    m_editorSec->setTitle(I18n::tr("scr_title_fmt").arg(domainKey(d).toUpper()));
    // サンプルコードはユーザーが編集していないときだけ差し替える
    // (setPlainText は modified フラグをリセットするので初回・未編集時のみ更新)
    if (!m_editor->document()->isModified())
        m_editor->setPlainText(QString::fromUtf8(sampleCode(m_lang, d)));

    // ── コンソール ─────────────────────────────────────────────────────────
    // 実行結果は消さない (ドメイン/言語切替は実行と無関係) — 空のときだけ案内を出す
    if (!m_proc && m_console->toPlainText().isEmpty())
        m_console->setPlainText(I18n::tr("scr_console_hint"));

    // ── API 早見表 ─────────────────────────────────────────────────────────
    const ApiRow *mid = kApiEm;
    switch (d) {
        case Domain::Optical:    mid = kApiOptical;    break;
        case Domain::Acoustic:   mid = kApiAcoustic;   break;
        case Domain::Underwater: mid = kApiUnderwater; break;
        default:                 mid = kApiEm;         break;
    }
    m_api->setRowCount(7);
    int r = 0;
    auto addRow = [this, &r](const ApiRow &a) {
        auto *f = new QTableWidgetItem(QString::fromUtf8(a.func));
        f->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        m_api->setItem(r, 0, f);
        m_api->setItem(r, 1, new QTableWidgetItem(QString::fromUtf8(a.desc)));
        ++r;
    };
    for (const ApiRow &a : kApiHead) addRow(a);
    for (int i = 0; i < 3; ++i)      addRow(mid[i]);
    for (const ApiRow &a : kApiTail) addRow(a);

    updateRunButtons();   // 言語切替で実行可否が変わる
}

// ── スクリプト実行 ─────────────────────────────────────────────────────────
// 外部の python3 を subprocess で起動する。組込インタプリタは持たない:
// Qt6 Widgets のみという制約で QJSEngine (Qt Qml 依存) が使えず、独自言語を
// 作っても numpy / h5py が使えないと結果の後処理には足りないため。
// パラメータ掃引は kernel/SweepRunner が担うので、こちらの役目は
// **出力ファイルの自由な後処理**。
//
// スクリプトへは環境変数で文脈を渡す:
//   OFDX_PROJECT — 開いている .ofd のパス (未保存なら空)
//   OFDX_WORKDIR — 実行の作業ディレクトリ (出力ファイルがある場所)
// 作業ディレクトリを cwd にして起動するので、相対パスで ofd.log 等を開ける。

// python3 の場所。無ければ空 (実行ボタンを無効化する理由に使う)。
static QString findPython()
{
    for (const char *name : { "python3", "python" }) {
        const QString p = QStandardPaths::findExecutable(QLatin1String(name));
        if (!p.isEmpty()) return p;
    }
    return QString();
}

void ScriptsTab::updateRunButtons()
{
    if (!m_runBtn) return;
    const bool busy = (m_proc != nullptr);
    const bool isPy = (m_lang == QLatin1String("python"));
    const QString py = findPython();

    m_abortBtn->setEnabled(busy);
    if (busy) { m_runBtn->setEnabled(false); return; }

    // LSF はインタプリタが存在しない (Lumerical 専用言語) — 実行しない。
    // python3 が無い環境も、押してから失敗させず理由を出す (絶対規則 5)。
    if (!isPy) {
        m_runBtn->setEnabled(false);
        m_runBtn->setToolTip(I18n::tr("scr_run_lsf_na"));
    } else if (py.isEmpty()) {
        m_runBtn->setEnabled(false);
        m_runBtn->setToolTip(I18n::tr("scr_run_nopython"));
    } else {
        m_runBtn->setEnabled(true);
        m_runBtn->setToolTip(I18n::tr("scr_run_tip").arg(py));
    }
}

void ScriptsTab::runScript()
{
    if (m_proc) return;
    const QString py = findPython();
    if (py.isEmpty() || m_lang != QLatin1String("python")) {
        updateRunButtons();
        return;
    }
    // 一時ファイルへ書いてから渡す (エディタの内容をそのまま実行する)
    const QString dir = QStandardPaths::writableLocation(
                            QStandardPaths::TempLocation)
                        + QStringLiteral("/openfdtd-x");
    QDir().mkpath(dir);
    m_scriptPath = QDir(dir).filePath(QStringLiteral("script.py"));
    QFile f(m_scriptPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_console->appendPlainText(I18n::tr("scr_run_writefail")
                                       .arg(m_scriptPath, f.errorString()));
        return;
    }
    f.write(m_editor->toPlainText().toUtf8());
    f.close();

    // 文脈: 開いているプロジェクトと、その出力があるディレクトリ
    const QString proj = m_p->filePath();
    const QString work = proj.isEmpty()
        ? dir : QFileInfo(proj).path();

    m_console->clear();
    m_console->appendPlainText(I18n::tr("scr_run_start").arg(py, work));

    m_proc = new QProcess(this);
    m_proc->setWorkingDirectory(work);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("OFDX_PROJECT"), proj);
    env.insert(QStringLiteral("OFDX_WORKDIR"), work);
    m_proc->setProcessEnvironment(env);

    connect(m_proc, &QProcess::readyRead, this, [this] {
        if (!m_proc) return;
        const QString out = QString::fromUtf8(m_proc->readAll());
        if (!out.isEmpty()) m_console->appendPlainText(out.trimmed());
    });
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(
                        &QProcess::finished),
            this, [this](int code, QProcess::ExitStatus st) {
        m_console->appendPlainText(
            st == QProcess::CrashExit ? I18n::tr("scr_run_crash")
                                      : I18n::tr("scr_run_done").arg(code));
        m_proc->deleteLater();
        m_proc = nullptr;
        updateRunButtons();
    });
    connect(m_proc, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError e) {
        if (!m_proc) return;
        m_console->appendPlainText(I18n::tr("scr_run_error")
                                       .arg(m_proc->errorString()));
        // FailedToStart では finished が来ないのでここで後始末する
        if (e == QProcess::FailedToStart) {
            m_proc->deleteLater();
            m_proc = nullptr;
            updateRunButtons();
        }
    });

    // -u で出力をバッファさせない (進捗が逐次コンソールへ出る)
    m_proc->start(py, { QStringLiteral("-u"), m_scriptPath });
    updateRunButtons();
}

void ScriptsTab::abortScript()
{
    if (!m_proc) return;
    m_console->appendPlainText(I18n::tr("scr_run_aborted"));
    m_proc->kill();     // finished が来て後始末される
}
