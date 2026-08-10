// OceanEnvironmentTab.cpp
#include "OceanEnvironmentTab.h"
#include "../core/Project.h"
#include "../io/BellhopIO.h"
#include "../io/BathymetryIO.h"
#include "../io/PageLinkScanner.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFileDialog>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QSet>
#include <QRegularExpression>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace ofd;

// ── file-local vocabulary (oe_) ─────────────────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // location query
    I18n::reg("oe_loc_section", "位置指定", "Location query");
    I18n::reg("oe_loc_hint",
              "緯度・経度から代表海域プロファイル (同梱の WOA 気候値近似) を"
              "選択し、Mackenzie 式で SSP を自動生成します。\n"
              "配置済みローカルデータセット (実ファイル) の照会は未実装です "
              "(下の一覧は配置状態の表示のみ)。",
              "Selects a representative sea-area profile (built-in WOA "
              "climatology approximation) for the latitude/longitude and "
              "generates the SSP with the Mackenzie formula.\n"
              "Querying the staged local dataset files is not implemented "
              "(the list below only shows staging state).");
    I18n::reg("oe_lat", "緯度", "Latitude");
    I18n::reg("oe_lon", "経度", "Longitude");
    I18n::reg("oe_month", "月 (季節)", "Month (season)");
    I18n::reg("oe_month_fmt", "%1月", "%1");
    I18n::reg("oe_annual", "年平均も併記", "Also show annual mean");
    I18n::reg("oe_query_btn", "🔍 データ照会", "🔍 Query data");
    // 「照会成功」は実データ照会を連想させるため、実態 (代表プロファイルの
    // 選択) に合わせた文言にする
    I18n::reg("oe_query_ok", "代表海域プロファイル: %1",
              "Representative profile: %1");
    // local datasets
    I18n::reg("oe_ds_section", "ローカルデータセット", "Local datasets");
    I18n::reg("oe_ds_hint",
              "データセットフォルダを走査し、実在するファイルだけを"
              "「配置済 (実サイズ)」として表示します。取得は"
              "「データセット取得」から (公式配布ページ + フォルダへの取込)。",
              "The dataset folder is scanned and only files that actually "
              "exist are shown as staged (with their real size). Fetch via "
              "the dataset-fetch dialog (official pages + folder import).");
    I18n::reg("oe_col_dataset", "データセット", "Dataset");
    I18n::reg("oe_col_provider", "提供元", "Provider");
    I18n::reg("oe_col_content", "内容", "Content");
    I18n::reg("oe_col_res", "解像度", "Resolution");
    I18n::reg("oe_col_state", "状態", "State");
    I18n::reg("oe_ds1", "JODC 各層観測 (J-DOSS)",
              "JODC layered observations (J-DOSS)");
    I18n::reg("oe_ds1c", "水温・塩分", "Temperature / salinity");
    I18n::reg("oe_ds1r", "観測点", "Station");
    I18n::reg("oe_ds2c", "海底地形 (日本周辺)", "Bathymetry (around Japan)");
    I18n::reg("oe_ds3c", "水温・塩分 月別気候値 (全球)",
              "Monthly climatology of temperature / salinity (global)");
    I18n::reg("oe_ds4c", "海底地形 (全球)", "Bathymetry (global)");
    I18n::reg("oe_ds5c", "海底地形 (全球・航海精度)",
              "Bathymetry (global, navigational accuracy)");
    I18n::reg("oe_ds6", "Argo フロート (GDAC)", "Argo floats (GDAC)");
    I18n::reg("oe_ds6c", "水温・塩分 実測プロファイル (準リアルタイム)",
              "Measured T/S profiles (near real time)");
    I18n::reg("oe_ds6r", "フロート位置", "Float position");
    I18n::reg("oe_ds7", "Copernicus CMEMS 再解析", "Copernicus CMEMS reanalysis");
    I18n::reg("oe_ds7c", "水温・塩分・海流 (時系列)",
              "Temperature / salinity / currents (time series)");
    I18n::reg("oe_ds8", "HYCOM 再解析", "HYCOM reanalysis");
    I18n::reg("oe_ds8c", "海流・渦 (時系列)", "Currents / eddies (time series)");
    I18n::reg("oe_ds9", "GDEM-V (音響用気候値)",
              "GDEM-V (acoustic climatology)");
    I18n::reg("oe_ds9c", "水温・塩分 (ソナー解析向け)",
              "Temperature / salinity (for sonar analysis)");
    I18n::reg("oe_ds10c", "底質 (粒度・音響パラメータ)",
              "Sediment (grain size / acoustic parameters)");
    I18n::reg("oe_staged", "配置済 %1", "Staged %1");
    I18n::reg("oe_notstaged", "未配置", "Not staged");
    I18n::reg("oe_restricted", "公開制限あり", "Distribution restricted");
    I18n::reg("oe_ds_folder", "📁 データセットフォルダ設定…",
              "📁 Dataset folder…");
    I18n::reg("oe_ds_fetch", "📦 データセット取得 (取込/配布ページ)",
              "📦 Fetch datasets (import / official pages)");
    I18n::reg("oe_ds_prio",
              "▸ 実データ照会の実装方針 (将来): 実測 (JODC/Argo) > 再解析 "
              "(CMEMS/HYCOM) > 気候値 (WOA23) の順に採用し出典を記録する。"
              "現状は同梱の代表プロファイルのみを使用。",
              "▸ Planned policy for real-data queries: measured (JODC/Argo) > "
              "reanalysis (CMEMS/HYCOM) > climatology (WOA23), recording "
              "provenance. Currently only the built-in representative "
              "profiles are used.");
    // query result
    I18n::reg("oe_result", "照会結果", "Query result");
    I18n::reg("oe_b_depth", "水深 %1 m", "Depth %1 m");
    I18n::reg("oe_b_sst", "表層水温 %1°C (%2月)",
              "Surface temperature %1°C (month %2)");
    I18n::reg("oe_b_sss", "表層塩分 %1 psu", "Surface salinity %1 psu");
    I18n::reg("oe_b_bottom", "底質: %1", "Sediment: %1");
    I18n::reg("oe_bottom_shelf", "砂泥 (大陸棚)", "Sandy mud (shelf)");
    I18n::reg("oe_bottom_deep", "泥 (深海平原)", "Mud (abyssal plain)");
    I18n::reg("oe_col_z", "深度 [m]", "Depth [m]");
    I18n::reg("oe_col_t", "水温 [°C]", "Temperature [°C]");
    I18n::reg("oe_col_s", "塩分 [psu]", "Salinity [psu]");
    I18n::reg("oe_col_c", "音速 [m/s]", "Sound speed [m/s]");
    // UNESCO (Chen-Millero) のセレクタは存在しないため「選択可」とは書かない
    I18n::reg("oe_layer_note",
              "▸ 音速式: Mackenzie (1981)。全%1層。",
              "▸ Sound-speed formula: Mackenzie (1981). %1 layers total.");
    // SSP preview
    I18n::reg("oe_ssp_section", "音速プロファイル", "SSP preview");
    I18n::reg("oe_ssp_okhotsk",
              "▸ 中冷水層による浅い音道 (表層ダクト) が形成されています",
              "▸ The dichothermal layer forms a shallow surface duct");
    I18n::reg("oe_ssp_shelf",
              "▸ 浅海のため海底反射が支配的。底質減衰の設定が重要",
              "▸ Shallow water: bottom reflection dominates, so the sediment "
              "attenuation setting matters");
    I18n::reg("oe_ssp_sofar", "▸ SOFAR チャネル軸: 深度 %1m",
              "▸ SOFAR channel axis: depth %1 m");
    // bathymetry
    I18n::reg("oe_bty_section", "地形断面", "Bathymetry along track");
    I18n::reg("oe_bearing", "伝搬方位", "Propagation bearing");
    I18n::reg("oe_bearing_unit", "° (東向き)", "° (eastward)");
    I18n::reg("oe_dist", "距離", "Distance");
    // apply
    I18n::reg("oe_apply_section", "ソルバへ反映", "Apply to solver");
    I18n::reg("oe_chk_ssp", "SSPを水中音響タブへ転送",
              "Transfer the SSP to the underwater acoustics tab");
    I18n::reg("oe_chk_bty", "地形断面をBellhop .btyへ",
              "Write the bathymetry section to a Bellhop .bty");
    I18n::reg("oe_chk_bottom", "底質パラメータも設定",
              "Also set the sediment parameters");
    I18n::reg("oe_apply_btn", "✓ 環境を適用 (SSP + 地形 + 底質)",
              "✓ Apply environment (SSP + bathymetry + sediment)");
    I18n::reg("oe_export_btn", "📄 .env 書出し", "📄 Export .env");
    I18n::reg("oe_apply_note",
              "▸ 適用: SSP・底質・伝搬距離を水中音響タブへ反映。Bellhop "
              "環境ファイル (.env) と地形 (.bty) は計算実行時にも自動生成"
              "される。SSP ファイル (.ssp) の書出しは未実装。",
              "▸ Apply transfers the SSP, sediment and range to the "
              "underwater acoustics tab. The Bellhop environment file (.env) "
              "and the bathymetry (.bty) are generated automatically when a "
              "run starts. Writing an SSP (.ssp) file is not implemented.");
    I18n::reg("oe_bty_note",
              "▸ 海域水深から合成した参考断面です (実地形データ未使用)。",
              "▸ Synthetic reference section derived from the area depth "
              "(no real bathymetry data — to be replaced once real-data "
              "queries are implemented). The bearing is currently unused.");
    I18n::reg("oe_notimpl", "未実装", "Not implemented");
    I18n::reg("oe_bty_surface", "海面", "Sea surface");
    I18n::reg("oe_bty_synth_tag", "合成断面 (実データではない)",
              "synthetic (not real data)");
    I18n::reg("oe_bty_real",
              "▸ %1 から伝搬経路 (方位 %2°, %3 km) に沿って %4 点を"
              "サンプリングしました。「地形断面をBellhop .btyへ」を入れて"
              "適用すると計算に反映されます。",
              "▸ Sampled %4 points along the track (bearing %2°, %3 km) from "
              "%1. Tick \u201cwrite the bathymetry to a Bellhop .bty\u201d and apply "
              "to use it in the run.");
    I18n::reg("oe_bty_synth",
              "▸ 水深データセットが見つからないため、海域代表水深からの"
              "「合成断面」を表示しています (実地形ではない)。"
              "「データセット取得」で GEBCO / ETOPO / J-EGG500 を配置すると"
              "実地形に置き換わります。",
              "▸ No bathymetry dataset was found, so this is a synthetic "
              "section derived from the area depth (not real terrain). Stage "
              "GEBCO / ETOPO / J-EGG500 via the fetch dialog to replace it.");
    I18n::reg("oe_bty_err", "▸ %1 (合成断面で代用しています)",
              "\u25b8 %1 (falling back to a synthetic section)");
    I18n::reg("oe_chk_bty_tip",
              "適用時に断面を .ofdx へ保存し、計算実行時に <ケース名>.bty を"
              "書き出して BELLHOP に読ませます (底面オプション 'A~')。",
              "Stores the section in the .ofdx on apply and writes "
              "<case>.bty at run time so BELLHOP reads it (bottom option "
              "'A~').");
    // download manager
    I18n::reg("oe_dl_title", "📦 データセット取得マネージャ",
              "📦 Dataset fetch manager");
    I18n::reg("oe_dl_standalone",
              "スタンドアロン運用 (外部ネットワーク非接続前提)",
              "Standalone operation (external network assumed disconnected)");
    I18n::reg("oe_dl_dest", "保存先: %1", "Destination: %1");
    I18n::reg("oe_dl_s1", "① 取得済みファイルをフォルダへ取込",
              "① Import fetched files into the folder");
    I18n::reg("oe_dl_s1_hint",
              "別環境や下の配布ページで取得したデータファイル (.nc / .grd / "
              ".csv / .zip 等) をデータセットフォルダへコピーします。"
              "コピー後に一覧の配置状態が更新されます。",
              "Copies dataset files (.nc / .grd / .csv / .zip etc.) fetched "
              "elsewhere or from the pages below into the dataset folder. "
              "The staging list refreshes after the copy.");
    I18n::reg("oe_dl_browse", "📁 ファイルを選んで取込…",
              "📁 Choose files and import…");
    I18n::reg("oe_dl_imported", "%1 件をコピーしました (失敗 %2 件)",
              "Copied %1 file(s) (%2 failed)");
    I18n::reg("oe_dl_s3", "③ URL から直接ダウンロード",
              "\u2462 Download directly from a URL");
    I18n::reg("oe_dl_s3_hint",
              "配布ページで得た直リンクを貼り付けるとデータセットフォルダへ"
              "保存します。ファイル名は URL から決まります (既存ファイルは"
              "上書きしません)。全球グリッドは数 GB あるので回線と空き容量に"
              "注意してください。",
              "Paste a direct link obtained from a distribution page and the "
              "file is saved into the dataset folder. The name comes from the "
              "URL (an existing file is never overwritten). Global grids are "
              "several GB \u2014 mind your link and free space.");
    I18n::reg("oe_dl_s3_off",
              "このビルドは Qt6::Network 無しで構成されているため、"
              "アプリ内ダウンロードは使えません (① の取込と ② の配布ページは"
              "使えます)。",
              "This build was configured without Qt6::Network, so in-app "
              "downloading is unavailable (\u2460 import and \u2461 pages still work).");
    I18n::reg("oe_dl_go", "⬇ ダウンロード", "\u2b07 Download");
    I18n::reg("oe_dl_scan_short", "🔍", "\U0001f50d");
    I18n::reg("oe_dl_scan", "🔍 ページ内のデータを探す",
              "\U0001f50d Find data on the page");
    I18n::reg("oe_dl_scan_hint",
              "配布ページの URL を入れて「探す」を押すと、そのページから"
              "データファイル (.nc / .asc / .csv / .zip 等) へのリンクを"
              "抜き出します。フォルダ/ページの行をダブルクリックすると"
              "その先を辿れます (NOAA や JODC のようにディレクトリを"
              "降りていく配布形態向け)。ファイルの行をダブルクリックすると"
              "ダウンロードします。",
              "Enter a distribution page URL and press Find: links to data "
              "files (.nc / .asc / .csv / .zip \u2026) on that page are extracted. "
              "Double-click a folder/page row to descend into it (for the "
              "directory-style layouts NOAA and JODC use); double-click a file "
              "row to download it.");
    I18n::reg("oe_dl_scanning", "取得中: %1", "Fetching: %1");
    I18n::reg("oe_dl_scan_ok",
              "%1 : データファイル %2 件 / フォルダ・ページ %3 件",
              "%1 : %2 data file(s), %3 folder(s)/page(s)");
    I18n::reg("oe_dl_scan_none",
              "%1 : データファイルへのリンクが見つかりませんでした。"
              "JavaScript で組み立てるページや検索フォーム経由の配布は"
              "ここからは辿れません — 「②」でブラウザを開いて直リンクを"
              "取得してください。",
              "%1 : no links to data files were found. Pages that build their "
              "links with JavaScript, or distribute through a search form, "
              "cannot be followed from here \u2014 open the page in a browser "
              "(section \u2461) and grab the direct link.");
    I18n::reg("oe_dl_scan_failed", "ページを取得できませんでした: %1",
              "Could not fetch the page: %1");
    I18n::reg("oe_dl_scan_trunc", " (先頭 %1 件のみ表示)",
              " (showing the first %1)");
    I18n::reg("oe_col_kind", "種別", "Kind");
    I18n::reg("oe_kind_file", "📄 データ", "\U0001f4c4 data");
    I18n::reg("oe_kind_dir", "📁 フォルダ/ページ", "\U0001f4c1 folder/page");
    I18n::reg("oe_dl_abort", "中断", "Abort");
    I18n::reg("oe_dl_badurl", "URL が不正です (http/https のみ)",
              "Invalid URL (http/https only)");
    I18n::reg("oe_dl_exists", "同名のファイルが既にあります: %1",
              "A file with that name already exists: %1");
    I18n::reg("oe_dl_running", "受信中 %1 / %2", "Receiving %1 / %2");
    I18n::reg("oe_dl_running_unknown", "受信中 %1 (総量不明)",
              "Receiving %1 (total unknown)");
    I18n::reg("oe_dl_done", "保存しました: %1 (%2)", "Saved: %1 (%2)");
    I18n::reg("oe_dl_failed", "ダウンロードに失敗しました: %1",
              "Download failed: %1");
    I18n::reg("oe_dl_aborted", "中断しました", "Aborted");
    I18n::reg("oe_dl_s2", "② 公式配布ページ (ブラウザで開く)",
              "② Official distribution pages (opens in a browser)");
    I18n::reg("oe_dl_s2_hint",
              "アプリ内での直接ダウンロードは未実装です。ボタンを押したとき"
              "だけ既定ブラウザで公式配布ページを開きます (それ以外の通信は"
              "行いません)。取得したファイルは ① で取込んでください。",
              "In-app direct download is not implemented. Pressing a button "
              "opens the official page in your default browser (no other "
              "network traffic). Import the fetched files via ①.");
    I18n::reg("oe_dl_open_page", "🌐 配布ページを開く", "🌐 Open page");
    I18n::reg("oe_col_src", "取得元", "Source");
    I18n::reg("oe_col_size", "サイズ", "Size");
    I18n::reg("oe_col_size_nominal", "公称サイズ", "Nominal size");
    I18n::reg("oe_job_argo", "Argo フロート GDAC", "Argo floats GDAC");
    I18n::reg("oe_job_argo_sz", "~180MB (選択海域)", "~180 MB (selected area)");
    I18n::reg("oe_job_cmems_sz", "~2.4GB (海域×12ヶ月)",
              "~2.4 GB (area × 12 months)");
    I18n::reg("oe_job_hycom_sz", "~1.8GB (海域切出し)", "~1.8 GB (area subset)");
    I18n::reg("oe_job_seabed", "usSEABED 底質", "usSEABED sediment");
    I18n::reg("oe_dl_note",
              "▸ 自動更新確認・定期通信は行いません (オフライン前提のため)。"
              "GDEM-V は公開制限のため対象外。",
              "▸ No automatic update checks or periodic traffic (offline by "
              "design). GDEM-V is excluded because its distribution is "
              "restricted.");
    I18n::reg("oe_close", "閉じる", "Close");
    // 照会結果の出典 (実データ照会は未実装であることの明示)
    I18n::reg("oe_src_note",
              "▸ 出典: 同梱の代表海域プロファイル (WOA 気候値を基にした近似) "
              "と Mackenzie 音速式。ローカルデータセット (実ファイル) の照会は"
              "未実装です。",
              "▸ Source: built-in representative sea-area profiles "
              "(approximation based on WOA climatology) and the Mackenzie "
              "sound-speed formula. Querying the local dataset files is not "
              "implemented yet.");
    return true;
}();

// ── 配色 (mock の badge / var(--acc) 相当) ──────────────────────────────────
const char *kAcc  = "#0078D4";
const char *kOk   = "#2E8B57";
const char *kWarn = "#B8860B";
const char *kErr  = "#C62828";
const char *kRed  = "#E53935";

// ── データセットフォルダ (実在ファイルの走査) ───────────────────────────────
// モックの固定「配置済 2.1GB」「D:/ocean_data/ (25.9GB)」は実機と乖離する
// ため使わない。実際のフォルダを走査し、見つかったファイルだけを配置済と
// して実サイズ付きで表示する。フォルダは QSettings に永続化 (openuwa と共有)。

QString oeDataDir()
{
    QSettings s(QSettings::UserScope, QStringLiteral("OpenFDTD"),
                QStringLiteral("OceanData"));
    const QString def = QStandardPaths::writableLocation(
                            QStandardPaths::GenericDataLocation)
                        + QStringLiteral("/OpenFDTD-X/ocean_data");
    return s.value(QStringLiteral("dir"), def).toString();
}

void setOeDataDir(const QString &dir)
{
    QSettings s(QSettings::UserScope, QStringLiteral("OpenFDTD"),
                QStringLiteral("OceanData"));
    s.setValue(QStringLiteral("dir"), dir);
}

// データセット定義 (表示・走査キーワード・公式配布ページを 1 箇所に集約)
struct OeDatasetDef {
    const char *nameKey, *nameRaw;   // 表示名 (I18n キー or 生文字列)
    const char *provider;
    const char *contentKey;
    const char *resKey, *resRaw;     // 解像度 (I18n キー or 生文字列)
    const char *keywords;   // ';' 区切り — ファイル名の小文字部分一致で走査
    const char *url;        // 公式配布ページ (nullptr = 公開制限で対象外)
    const char *nominal;    // 公称サイズ (配布ページの目安。実サイズではない)
};
const OeDatasetDef kOeDatasets[] = {
    { "oe_ds1", nullptr, "🇯🇵 JODC", "oe_ds1c", "oe_ds1r", nullptr,
      "jodc", "https://www.jodc.go.jp/", "~2.1GB" },
    { nullptr, "J-EGG500", "🇯🇵 JODC", "oe_ds2c", nullptr, "500m",
      "egg500;jegg", "https://www.jodc.go.jp/vpage/depth500_file_j.html",
      "~1.2GB" },
    { nullptr, "WOA23 (World Ocean Atlas)", "🇺🇸 NOAA/NCEI", "oe_ds3c",
      nullptr, "0.25°", "woa",
      "https://www.ncei.noaa.gov/products/world-ocean-atlas", "~8.4GB" },
    { nullptr, "ETOPO 2022", "🇺🇸 NOAA", "oe_ds4c", nullptr, "15秒",
      "etopo", "https://www.ncei.noaa.gov/products/etopo-global-relief-model",
      "~6.7GB" },
    { nullptr, "GEBCO 2024", "🇬🇧 IHO/IOC", "oe_ds5c", nullptr, "15秒",
      "gebco",
      "https://www.gebco.net/data_and_products/gridded_bathymetry_data/",
      "~7.5GB" },
    { "oe_ds6", nullptr, "🌐 Argo/Ifremer", "oe_ds6c", "oe_ds6r", nullptr,
      "argo", "https://argo.ucsd.edu/data/", "~180MB" },
    { "oe_ds7", nullptr, "🇪🇺 EU", "oe_ds7c", nullptr, "1/12°",
      "cmems;glorys;copernicus", "https://data.marine.copernicus.eu/",
      "~2.4GB" },
    { "oe_ds8", nullptr, "🇺🇸 US Navy/NOPP", "oe_ds8c", nullptr, "1/12°",
      "hycom", "https://www.hycom.org/dataserver", "~1.8GB" },
    { "oe_ds9", nullptr, "🇺🇸 US Navy", "oe_ds9c", nullptr, "0.25°",
      "gdem", nullptr, nullptr },   // 公開制限 — 配布ページなし
    { nullptr, "usSEABED / dbSEABED", "🇺🇸 USGS", "oe_ds10c", "oe_ds1r",
      nullptr, "seabed", "https://www.usgs.gov/programs/cmhrp", "~45MB" },
};
const int kOeDatasetCount = int(sizeof(kOeDatasets) / sizeof(kOeDatasets[0]));

QString oeDatasetName(const OeDatasetDef &d)
{
    return d.nameKey ? I18n::tr(d.nameKey) : QString::fromUtf8(d.nameRaw);
}

// フォルダを 1 回走査して各データセットの一致サイズと総サイズを求める
struct OeScanResult {
    qint64 bytes[sizeof(kOeDatasets) / sizeof(kOeDatasets[0])] = {};
    qint64 total = 0;
    bool   dirExists = false;
};

OeScanResult oeScanAll(const QString &dir)
{
    OeScanResult res;
    res.dirExists = QDir(dir).exists();
    if (!res.dirExists) return res;
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString name = it.fileName().toLower();
        const qint64 sz = it.fileInfo().size();
        res.total += sz;
        for (int d = 0; d < kOeDatasetCount; ++d) {
            for (const QString &k :
                 QString::fromLatin1(kOeDatasets[d].keywords)
                     .split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
                if (name.contains(k)) { res.bytes[d] += sz; break; }
            }
        }
    }
    return res;
}

QString oeHumanSize(qint64 bytes)
{
    const double gb = 1024.0 * 1024.0 * 1024.0;
    if (bytes >= qint64(gb))
        return QStringLiteral("%1GB").arg(double(bytes) / gb, 0, 'f', 1);
    if (bytes >= 1024 * 1024)
        return QStringLiteral("%1MB").arg(double(bytes) / (1024.0 * 1024.0),
                                          0, 'f', 1);
    return QStringLiteral("%1KB").arg(double(bytes) / 1024.0, 0, 'f', 1);
}

// バッジ風 QLabel (badge / badge ok / badge acc / badge warn / badge err 相当)
QLabel *oeBadge(const QString &text, QWidget *parent, const char *color = nullptr)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(QStringLiteral(
        "QLabel { border: 1px solid palette(mid); border-radius: 3px;"
        " padding: 1px 6px; %1 }")
        .arg(color ? QStringLiteral("color: %1;")
                         .arg(QString::fromLatin1(color))
                   : QString()));
    return l;
}

// 読取専用データ表 (q-table 相当) の共通初期化
void oeSetupTable(QTableWidget *t, const QStringList &headers, int minH)
{
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(22);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(minH);
}

// ── 局所データセットのモック: 代表海域の WOA 風プロファイル (OE_REGIONS) ────
const OeRegion kRegions[] = {
    // name,                     latMin, latMax, lonMin, lonMax, depth, sst, sss, type
    { "日本海 (大和堆周辺)",        36,  42,  130,   139,    1650, 18, 34.1, "japan_sea" },
    { "太平洋 (黒潮域)",            30,  36,  135,   145,    4200, 24, 34.7, "kuroshio" },
    { "オホーツク海",               44,  55,  142,   155,     850,  8, 32.9, "okhotsk" },
    { "東シナ海 (大陸棚)",          26,  32,  122,   130,     120, 26, 34.3, "shelf" },
    { "フィリピン海 (深海)",        15,  26,  125,   140,    5600, 28, 34.5, "deep_pacific" },
    { "津軽海峡",                   41,  42,  139.5, 141.5,   250, 15, 33.8, "strait" },
    // ── 海外海域 (全球データセット WOA23/GEBCO から) ──
    { "北大西洋 (メキシコ湾流域)",  35,  45,  -70,   -40,    4800, 20, 36.2, "deep_pacific" },
    { "地中海 (深海平原)",          33,  40,    5,    25,    2600, 22, 38.4, "med" },
    { "メキシコ湾",                 22,  29,  -96,   -84,    3200, 27, 36.3, "deep_pacific" },
    { "北極海 (バレンツ海)",        70,  80,   20,    50,     350,  3, 34.8, "okhotsk" },
    { "南シナ海",                    5,  20,  108,   120,    4000, 29, 33.8, "deep_pacific" },
    { "インド洋 (中央海岺域)",     -20,   5,   60,    90,    4500, 28, 34.9, "deep_pacific" },
};
const int kRegionCount = int(sizeof(kRegions) / sizeof(kRegions[0]));

// Mackenzie (1981) 音速式 (mock の oeSoundSpeed をそのまま移植)
double oeSoundSpeed(double T, double S, double z)
{
    return 1448.96 + 4.591 * T - 0.05304 * T * T + 2.374e-4 * T * T * T
         + 1.340 * (S - 35) + 0.0163 * z + 1.675e-7 * z * z
         - 0.01025 * T * (S - 35) - 7.139e-13 * T * z * z * z;
}

// 小数丸め (mock の +x.toFixed(n) 相当)
double oeRound(double v, int digits)
{
    const double f = std::pow(10.0, digits);
    return std::round(v * f) / f;
}
} // namespace

// ── OeSspView — mock の SSP SVG を QPainter で再現 ──────────────────────────
OeSspView::OeSspView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(280, 200);
    setMaximumWidth(340);
}

void OeSspView::setProfile(const QVector<OeSspPoint> &ssp, double depth,
                           int cMinIdx)
{
    m_ssp = ssp;
    m_depth = depth > 0 ? depth : 1;
    m_cMinIdx = cMinIdx;
    update();
}

void OeSspView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    if (m_ssp.size() < 2) return;

    // viewBox "0 0 300 200" → ウィジェット座標
    const double sx = width() / 300.0;
    const double sy = height() / 200.0;
    auto vx = [sx](double x) { return x * sx; };
    auto vy = [sy](double y) { return y * sy; };

    double cLo = m_ssp.first().c, cHi = m_ssp.first().c;
    for (const OeSspPoint &q : m_ssp) {
        cLo = std::min(cLo, q.c);
        cHi = std::max(cHi, q.c);
    }
    const double span = (cHi - cLo) != 0.0 ? (cHi - cLo) : 1.0;
    auto X = [&](double c) { return vx(30.0 + (c - cLo) / span * 250.0); };
    auto Y = [&](double z) { return vy(15.0 + (z / m_depth) * 170.0); };

    QPainterPath path;
    for (int i = 0; i < m_ssp.size(); ++i) {
        const QPointF pt(X(m_ssp[i].c), Y(m_ssp[i].z));
        if (i == 0) path.moveTo(pt); else path.lineTo(pt);
    }
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(kAcc), 1.8));
    p.drawPath(path);

    QFont f = p.font();
    f.setPointSizeF(7.5);
    p.setFont(f);

    // SOFAR 軸マーカ
    const OeSspPoint &cm = m_ssp.at(qBound(0, m_cMinIdx, int(m_ssp.size()) - 1));
    p.setPen(QPen(QColor(kRed), 1.5));
    p.drawLine(QPointF(X(cm.c), Y(cm.z) - 8 * sy),
               QPointF(X(cm.c), Y(cm.z) + 8 * sy));
    p.setPen(QColor(kRed));
    p.drawText(QPointF(X(cm.c) + 5 * sx, Y(cm.z) + 4 * sy),
               QStringLiteral("SOFAR軸 %1m / %2m/s")
                   .arg(cm.z).arg(cm.c, 0, 'f', 1));

    // 目盛りラベル (最小 / 最大音速・最大深度)
    p.setPen(palette().mid().color());
    p.drawText(QPointF(vx(32), vy(12)),
               QStringLiteral("%1 m/s").arg(cLo, 0, 'f', 0));
    p.drawText(QRectF(vx(170), 0, vx(126), vy(14)),
               Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("%1 m/s").arg(cHi, 0, 'f', 0));
    p.drawText(QPointF(vx(4), vy(190)),
               QStringLiteral("%1m").arg(m_depth, 0, 'f', 0));
}

// ── OeBathyView — mock の合成海底断面 SVG を QPainter で再現 ────────────────
OeBathyView::OeBathyView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(300, 120);
    setMaximumWidth(380);
}

void OeBathyView::setSection(const QVector<BathyPoint> &pts,
                             const QString &source)
{
    m_pts = pts;
    m_source = source;
    update();
}

void OeBathyView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    if (m_pts.size() < 2) return;

    // viewBox "0 0 340 120" → ウィジェット座標
    const double sx = width() / 340.0;
    const double sy = height() / 120.0;
    auto vx = [sx](double x) { return x * sx; };
    auto vy = [sy](double y) { return y * sy; };

    double rMax = 0.0, dMax = 0.0;
    for (const BathyPoint &q : m_pts) {
        rMax = std::max(rMax, q.range_km);
        dMax = std::max(dMax, q.depth_m);
    }
    if (rMax <= 0 || dMax <= 0) return;

    QPolygonF line;
    for (const BathyPoint &q : m_pts)
        line << QPointF(vx(10 + q.range_km / rMax * 320),
                        vy(18 + q.depth_m / dMax * 80));

    QPolygonF fillPoly;
    fillPoly << QPointF(line.first().x(), vy(18));
    fillPoly << line;
    fillPoly << QPointF(line.last().x(), vy(110))
             << QPointF(line.first().x(), vy(110));
    QColor sed("#5D4037");
    sed.setAlphaF(0.55);
    p.setPen(Qt::NoPen);
    p.setBrush(sed);
    p.drawPolygon(fillPoly);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#8D6E63"), 1.5));
    p.drawPolyline(line);

    // 海面 (accent 破線)
    QPen sea(QColor(kAcc), 1);
    sea.setDashPattern({ 3, 2 });
    p.setPen(sea);
    p.drawLine(QPointF(vx(10), vy(18)), QPointF(vx(330), vy(18)));

    QFont f = p.font();
    f.setPointSizeF(7.5);
    p.setFont(f);
    p.setPen(QColor(kAcc));
    p.drawText(QPointF(vx(12), vy(14)), I18n::tr("oe_bty_surface"));
    p.setPen(palette().mid().color());
    // 距離・最大水深の目盛り
    p.drawText(QPointF(vx(12), vy(118)), QStringLiteral("0 km"));
    p.drawText(QRectF(vx(150), vy(112), vx(180), vy(10)),
               Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("%1 km  /  max %2 m")
                   .arg(rMax, 0, 'f', rMax < 10 ? 1 : 0)
                   .arg(dMax, 0, 'f', 0));
    // 出所 — 実データか合成かを図の中で必ず区別する
    const bool synthetic = m_source.isEmpty();
    p.setPen(synthetic ? QColor("#B8860B") : QColor(kAcc));
    p.drawText(QRectF(vx(150), vy(2), vx(180), vy(12)),
               Qt::AlignRight | Qt::AlignVCenter,
               synthetic ? I18n::tr("oe_bty_synth_tag") : m_source);
}


// ── ③ 直接ダウンロード (Qt6::Network がある構成のみ) ────────────────────────
// 進捗は QNetworkReply の実受信バイト数だけを出す (擬似進捗は出さない)。
// 保存はテンポラリ (.part) へ書き、完了時にリネームする — 途中で切れた
// ファイルを「配置済み」として拾わないため。
#ifdef OFD_USE_NETWORK
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

class OeDownloadManager::Impl : public QObject {
public:
    QNetworkAccessManager nam;
    QNetworkReply *reply = nullptr;
    QFile file;
    QString finalPath;
};
#else
class OeDownloadManager::Impl : public QObject {};
#endif


namespace { const int kScanLimit = 400; }



void OeDownloadManager::scanPage(const QString &pageUrl)
{
#ifdef OFD_USE_NETWORK
    if (!m_impl) return;
    const QString target = pageUrl.isEmpty() ? m_url->text().trimmed() : pageUrl;
    const QUrl url(target);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || (scheme != QLatin1String("http")
                           && scheme != QLatin1String("https"))) {
        m_scanNote->setText(I18n::tr("oe_dl_badurl"));
        return;
    }
    m_url->setText(target);
    m_scanBtn->setEnabled(false);
    m_scanNote->setText(I18n::tr("oe_dl_scanning").arg(target));

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *r = m_impl->nam.get(req);
    connect(r, &QNetworkReply::finished, this, [this, r, target] {
        m_scanBtn->setEnabled(true);
        if (r->error() != QNetworkReply::NoError) {
            m_scanNote->setText(I18n::tr("oe_dl_scan_failed").arg(r->errorString()));
            r->deleteLater();
            return;
        }
        // 巨大なファイルを誤って掴んだときのために上限を設ける (2 MB)
        const QByteArray body = r->read(2 * 1024 * 1024);
        const QUrl finalUrl = r->url();   // リダイレクト後を相対解決の基準にする
        r->deleteLater();
        showScanResult(finalUrl.isEmpty() ? target : finalUrl.toString(), body);
    });
#else
    Q_UNUSED(pageUrl);
#endif
}

void OeDownloadManager::showScanResult(const QString &pageUrl,
                                       const QByteArray &html)
{
    if (!m_scanTable) return;
    bool truncated = false;
    const QVector<PageLink> links =
        scanPageLinks(pageUrl, html, kScanLimit, &truncated);

    m_scanTable->setRowCount(0);
    int nFile = 0, nDir = 0;
    for (const PageLink &l : links) {
        const int r = m_scanTable->rowCount();
        m_scanTable->insertRow(r);
        auto *kind = new QTableWidgetItem(
            I18n::tr(l.isDir ? "oe_kind_dir" : "oe_kind_file"));
        kind->setData(Qt::UserRole, l.url);
        kind->setData(Qt::UserRole + 1, l.isDir);
        m_scanTable->setItem(r, 0, kind);
        m_scanTable->setItem(r, 1, new QTableWidgetItem(l.name));
        m_scanTable->setItem(r, 2, new QTableWidgetItem(l.url));
        (l.isDir ? nDir : nFile)++;
    }
    QString note;
    if (nFile == 0 && nDir == 0)
        note = I18n::tr("oe_dl_scan_none").arg(pageUrl);
    else
        note = I18n::tr("oe_dl_scan_ok").arg(pageUrl).arg(nFile).arg(nDir);
    if (truncated) note += I18n::tr("oe_dl_scan_trunc").arg(kScanLimit);
    m_scanNote->setText(note);
}

OeDownloadManager::~OeDownloadManager()
{
    abortDownload();
    delete m_impl;
}

void OeDownloadManager::abortDownload()
{
#ifdef OFD_USE_NETWORK
    if (m_impl && m_impl->reply) {
        m_impl->reply->abort();   // finished ハンドラが後始末する
    }
#endif
}

void OeDownloadManager::startDownload()
{
#ifdef OFD_USE_NETWORK
    if (!m_impl || m_impl->reply) return;
    const QUrl url(m_url->text().trimmed());
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || (scheme != QLatin1String("http")
                           && scheme != QLatin1String("https"))) {
        m_dlResult->setText(I18n::tr("oe_dl_badurl"));
        return;
    }
    QString name = QFileInfo(url.path()).fileName();
    if (name.isEmpty()) name = QStringLiteral("download.bin");
    const QString dir = oeDataDir();
    QDir().mkpath(dir);
    const QString target = QDir(dir).filePath(name);
    if (QFile::exists(target)) {
        m_dlResult->setText(I18n::tr("oe_dl_exists")
                                .arg(QDir::toNativeSeparators(target)));
        return;
    }
    m_impl->finalPath = target;
    m_impl->file.setFileName(target + QStringLiteral(".part"));
    if (!m_impl->file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_dlResult->setText(I18n::tr("oe_dl_failed")
                                .arg(m_impl->file.errorString()));
        return;
    }
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_impl->reply = m_impl->nam.get(req);
    m_dlBtn->setEnabled(false);
    m_abortBtn->setEnabled(true);
    m_dlProgress->setRange(0, 0);
    m_dlProgress->setValue(0);
    m_dlResult->setText(QString());

    connect(m_impl->reply, &QNetworkReply::readyRead, this, [this] {
        m_impl->file.write(m_impl->reply->readAll());
    });
    connect(m_impl->reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 got, qint64 total) {
                if (total > 0) {
                    m_dlProgress->setRange(0, 100);
                    m_dlProgress->setValue(int(got * 100 / total));
                    m_dlResult->setText(
                        I18n::tr("oe_dl_running")
                            .arg(QLocale().formattedDataSize(got))
                            .arg(QLocale().formattedDataSize(total)));
                } else {
                    m_dlResult->setText(
                        I18n::tr("oe_dl_running_unknown")
                            .arg(QLocale().formattedDataSize(got)));
                }
            });
    connect(m_impl->reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *r = m_impl->reply;
        m_impl->reply = nullptr;
        m_impl->file.write(r->readAll());
        m_impl->file.close();
        const bool aborted = (r->error() == QNetworkReply::OperationCanceledError);
        const bool ok = (r->error() == QNetworkReply::NoError);
        const QString errStr = r->errorString();
        r->deleteLater();
        m_dlBtn->setEnabled(true);
        m_abortBtn->setEnabled(false);
        m_dlProgress->setRange(0, 100);
        if (!ok) {
            m_dlProgress->setValue(0);
            QFile::remove(m_impl->file.fileName());   // 途中のファイルは残さない
            m_dlResult->setText(aborted ? I18n::tr("oe_dl_aborted")
                                        : I18n::tr("oe_dl_failed").arg(errStr));
            return;
        }
        QFile::remove(m_impl->finalPath);
        if (!QFile::rename(m_impl->file.fileName(), m_impl->finalPath)) {
            m_dlResult->setText(I18n::tr("oe_dl_failed")
                                    .arg(m_impl->finalPath));
            return;
        }
        m_dlProgress->setValue(100);
        m_dlResult->setText(
            I18n::tr("oe_dl_done")
                .arg(QFileInfo(m_impl->finalPath).fileName())
                .arg(QLocale().formattedDataSize(
                    QFileInfo(m_impl->finalPath).size())));
    });
#endif
}

// ── OeDownloadManager — データセット取得マネージャ (オフラインファースト) ───
// 実際に起きることだけを表示する:
//   ① 取得済みファイルのフォルダ取込 = 実コピー
//   ② 公式配布ページをブラウザで開く (アプリ内直接 DL は未実装)
// 旧実装の進捗シミュレーション (乱数で完了表示) は実環境と乖離するため廃止。
OeDownloadManager::OeDownloadManager(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(I18n::tr("oe_dl_title"));
    setModal(true);
    resize(660, 560);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *body = new QWidget(scroll);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(10, 10, 10, 10);
    v->setSpacing(8);

    auto *standalone = new QLabel(I18n::tr("oe_dl_standalone"), body);
    standalone->setWordWrap(true);
    v->addWidget(standalone);
    auto *dest = new QLabel(
        I18n::tr("oe_dl_dest").arg(QDir::toNativeSeparators(oeDataDir())),
        body);
    dest->setWordWrap(true);
    v->addWidget(dest);

    // ── ① 取得済みファイルをフォルダへ取込 (実コピー) ────────────────────
    auto *s1 = new SectionBox(I18n::tr("oe_dl_s1"), body);
    auto *h1 = new QLabel(I18n::tr("oe_dl_s1_hint"), s1);
    h1->setWordWrap(true);
    s1->vbox()->addWidget(h1);
    auto *impRow = new QHBoxLayout();
    auto *impBtn = new QPushButton(I18n::tr("oe_dl_browse"), s1);
    connect(impBtn, &QPushButton::clicked,
            this, &OeDownloadManager::importFiles);
    impRow->addWidget(impBtn);
    m_importResult = new QLabel(s1);
    impRow->addWidget(m_importResult);
    impRow->addStretch(1);
    s1->vbox()->addLayout(impRow);
    v->addWidget(s1);

    // ── ③ URL から直接ダウンロード / 配布ページの走査 ─────────────────────
    {
        auto *s3 = new SectionBox(I18n::tr("oe_dl_s3"), body);
#ifdef OFD_USE_NETWORK
        m_impl = new Impl();
        auto *h3 = new QLabel(I18n::tr("oe_dl_s3_hint"), s3);
        h3->setWordWrap(true);
        s3->vbox()->addWidget(h3);
        auto *urlRow = new QHBoxLayout();
        m_url = new QLineEdit(s3);
        m_url->setPlaceholderText(QStringLiteral("https://…/GEBCO_2024.nc"));
        m_dlBtn = new QPushButton(I18n::tr("oe_dl_go"), s3);
        m_abortBtn = new QPushButton(I18n::tr("oe_dl_abort"), s3);
        m_abortBtn->setEnabled(false);
        urlRow->addWidget(m_url, 1);
        urlRow->addWidget(m_dlBtn);
        urlRow->addWidget(m_abortBtn);
        s3->vbox()->addLayout(urlRow);
        m_dlProgress = new QProgressBar(s3);
        m_dlProgress->setRange(0, 100);
        m_dlProgress->setValue(0);
        s3->vbox()->addWidget(m_dlProgress);
        m_dlResult = new QLabel(s3);
        m_dlResult->setWordWrap(true);
        s3->vbox()->addWidget(m_dlResult);
        connect(m_dlBtn, &QPushButton::clicked,
                this, &OeDownloadManager::startDownload);
        connect(m_abortBtn, &QPushButton::clicked,
                this, &OeDownloadManager::abortDownload);

        // 配布ページからデータ URL を探す
        auto *scanHint = new QLabel(I18n::tr("oe_dl_scan_hint"), s3);
        scanHint->setWordWrap(true);
        scanHint->setStyleSheet("font-size:11px; color:palette(mid);");
        s3->vbox()->addWidget(scanHint);
        m_scanBtn = new QPushButton(I18n::tr("oe_dl_scan"), s3);
        urlRow->addWidget(m_scanBtn);
        m_scanTable = new QTableWidget(0, 3, s3);
        oeSetupTable(m_scanTable, { I18n::tr("oe_col_kind"),
                                    I18n::tr("oe_col_dataset"),
                                    QStringLiteral("URL") }, 160);
        m_scanTable->setMinimumHeight(180);
        s3->vbox()->addWidget(m_scanTable);
        m_scanNote = new QLabel(s3);
        m_scanNote->setWordWrap(true);
        s3->vbox()->addWidget(m_scanNote);
        connect(m_scanBtn, &QPushButton::clicked, this, [this] { scanPage(); });
        // フォルダ/ページ行 = 辿る、データ行 = URL 欄へ入れてダウンロード
        connect(m_scanTable, &QTableWidget::cellDoubleClicked, this,
                [this](int row, int) {
                    auto *it = m_scanTable->item(row, 0);
                    if (!it) return;
                    const QString url = it->data(Qt::UserRole).toString();
                    if (it->data(Qt::UserRole + 1).toBool()) {
                        scanPage(url);
                    } else {
                        m_url->setText(url);
                        startDownload();
                    }
                });
#else
        auto *off = new QLabel(I18n::tr("oe_dl_s3_off"), s3);
        off->setWordWrap(true);
        s3->vbox()->addWidget(off);
#endif
        v->addWidget(s3);
    }

    // ── ② 公式配布ページ (ブラウザで開く) ────────────────────────────────
    auto *s2 = new SectionBox(I18n::tr("oe_dl_s2"), body);
    auto *h2 = new QLabel(I18n::tr("oe_dl_s2_hint"), s2);
    h2->setWordWrap(true);
    s2->vbox()->addWidget(h2);

    int nPages = 0;
    for (int d = 0; d < kOeDatasetCount; ++d)
        if (kOeDatasets[d].url) ++nPages;
    auto *pages = new QTableWidget(nPages, 4, s2);
    oeSetupTable(pages, { I18n::tr("oe_col_dataset"), I18n::tr("oe_col_src"),
                          I18n::tr("oe_col_size_nominal"), "" }, 220);
    int r = 0;
    for (int d = 0; d < kOeDatasetCount; ++d) {
        const OeDatasetDef &def = kOeDatasets[d];
        if (!def.url) continue;
        pages->setItem(r, 0, new QTableWidgetItem(oeDatasetName(def)));
        pages->setItem(r, 1,
                       new QTableWidgetItem(QUrl(QString::fromLatin1(def.url))
                                                .host()));
        pages->setItem(r, 2, new QTableWidgetItem(
            QString::fromLatin1(def.nominal ? def.nominal : "-")));
        // ブラウザで開く / アプリ内でデータリンクを探す の 2 択
        auto *cell = new QWidget(pages);
        auto *ch = new QHBoxLayout(cell);
        ch->setContentsMargins(0, 0, 0, 0);
        ch->setSpacing(4);
        auto *open = new QPushButton(I18n::tr("oe_dl_open_page"), cell);
        connect(open, &QPushButton::clicked, this, [def] {
            QDesktopServices::openUrl(QUrl(QString::fromLatin1(def.url)));
        });
        ch->addWidget(open);
#ifdef OFD_USE_NETWORK
        auto *scan = new QPushButton(I18n::tr("oe_dl_scan_short"), cell);
        scan->setToolTip(I18n::tr("oe_dl_scan"));
        connect(scan, &QPushButton::clicked, this, [this, def] {
            scanPage(QString::fromLatin1(def.url));
        });
        ch->addWidget(scan);
#endif
        pages->setCellWidget(r, 3, cell);
        ++r;
    }
    s2->vbox()->addWidget(pages);
    v->addWidget(s2);

    auto *note = new QLabel(I18n::tr("oe_dl_note"), body);
    note->setWordWrap(true);
    v->addWidget(note);
    v->addStretch(1);

    scroll->setWidget(body);
    outer->addWidget(scroll, 1);

    // ── フッタ ──────────────────────────────────────────────────────────
    auto *foot = new QWidget(this);
    auto *fh = new QHBoxLayout(foot);
    fh->setContentsMargins(10, 8, 10, 8);
    fh->addStretch(1);
    auto *closeBtn = new QPushButton(I18n::tr("oe_close"), foot);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    fh->addWidget(closeBtn);
    outer->addWidget(foot);
}

// ファイル選択 → データセットフォルダへコピー。実際のコピー結果だけを表示する。
void OeDownloadManager::importFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, I18n::tr("oe_dl_browse"), QString(),
        "Dataset files (*.nc *.grd *.xyz *.zip *.csv *.tif *.asc);;"
        "All files (*)");
    if (files.isEmpty()) return;
    const QString dir = oeDataDir();
    QDir().mkpath(dir);
    int ok = 0, fail = 0;
    for (const QString &src : files) {
        const QString dst = QDir(dir).filePath(QFileInfo(src).fileName());
        if (QFileInfo::exists(dst)) QFile::remove(dst);   // 上書き取込
        if (QFile::copy(src, dst)) ++ok; else ++fail;
    }
    m_importResult->setText(I18n::tr("oe_dl_imported").arg(ok).arg(fail));
    m_importResult->setStyleSheet(QStringLiteral("color: %1;")
        .arg(QString::fromLatin1(fail == 0 ? kOk : kErr)));
}


// ── OceanEnvironmentTab ─────────────────────────────────────────────────────
OceanEnvironmentTab::OceanEnvironmentTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 位置指定 / Location query ───────────────────────────────────────────
    auto *sl = new SectionBox(I18n::tr("oe_loc_section"), body);
    auto *locHint = new QLabel(I18n::tr("oe_loc_hint"), sl);
    locHint->setWordWrap(true);
    sl->vbox()->addWidget(locHint);

    auto *llRow = new QHBoxLayout();
    m_lat = new QLineEdit("34.5", sl);
    m_lat->setMaximumWidth(90);
    m_lon = new QLineEdit("139.2", sl);
    m_lon->setMaximumWidth(90);
    llRow->addWidget(m_lat);
    llRow->addWidget(new QLabel(QStringLiteral("°N"), sl));
    llRow->addSpacing(8);
    llRow->addWidget(new QLabel(I18n::tr("oe_lon"), sl));
    llRow->addWidget(m_lon);
    llRow->addWidget(new QLabel(QStringLiteral("°E"), sl));
    llRow->addStretch(1);
    sl->form()->addRow(I18n::tr("oe_lat"), llRow);

    auto *monthRow = new QHBoxLayout();
    m_month = new QComboBox(sl);
    for (int i = 1; i <= 12; ++i)
        m_month->addItem(I18n::tr("oe_month_fmt").arg(i));
    m_month->setCurrentIndex(6);          // 既定 7月
    monthRow->addWidget(m_month);
    auto *annualCk = new QCheckBox(I18n::tr("oe_annual"), sl);
    tabhelp::markNotImplemented(annualCk);   // 年平均の併記は未実装 (どこにも読まれない)
    monthRow->addWidget(annualCk);
    monthRow->addStretch(1);
    sl->form()->addRow(I18n::tr("oe_month"), monthRow);

    auto *qRow = new QHBoxLayout();
    auto *queryBtn = new QPushButton(I18n::tr("oe_query_btn"), sl);
    m_queryBadge = oeBadge(QString(), sl, kOk);
    qRow->addWidget(queryBtn);
    qRow->addWidget(m_queryBadge);
    qRow->addStretch(1);
    sl->vbox()->addLayout(qRow);
    v->addWidget(sl);

    // ── ローカルデータセット / Local datasets ───────────────────────────────
    auto *sd = new SectionBox(I18n::tr("oe_ds_section"), body);
    auto *dsHint = new QLabel(I18n::tr("oe_ds_hint"), sd);
    dsHint->setWordWrap(true);
    sd->vbox()->addWidget(dsHint);

    m_dsTable = new QTableWidget(kOeDatasetCount, 6, sd);
    oeSetupTable(m_dsTable, { "", I18n::tr("oe_col_dataset"),
                              I18n::tr("oe_col_provider"),
                              I18n::tr("oe_col_content"),
                              I18n::tr("oe_col_res"),
                              I18n::tr("oe_col_state") }, 250);
    sd->vbox()->addWidget(m_dsTable);

    auto *dsRow = new QHBoxLayout();
    auto *folderBtn = new QPushButton(I18n::tr("oe_ds_folder"), sd);
    connect(folderBtn, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(
            this, I18n::tr("oe_ds_folder"), oeDataDir());
        if (d.isEmpty()) return;
        setOeDataDir(d);
        rebuildDatasetTable();
    });
    auto *fetchBtn = new QPushButton(I18n::tr("oe_ds_fetch"), sd);
    m_dsFolderLabel = new QLabel(sd);
    dsRow->addWidget(folderBtn);
    dsRow->addWidget(fetchBtn);
    dsRow->addWidget(m_dsFolderLabel);
    dsRow->addStretch(1);
    sd->vbox()->addLayout(dsRow);
    rebuildDatasetTable();
    auto *prio = new QLabel(I18n::tr("oe_ds_prio"), sd);
    prio->setWordWrap(true);
    sd->vbox()->addWidget(prio);
    v->addWidget(sd);

    // ── 照会結果 / Query result ─────────────────────────────────────────────
    m_resultSection = new SectionBox(I18n::tr("oe_result"), body);
    auto *badgeRow = new QHBoxLayout();
    m_bDepth  = oeBadge(QString(), m_resultSection, kAcc);
    m_bSst    = oeBadge(QString(), m_resultSection);
    m_bSss    = oeBadge(QString(), m_resultSection);
    m_bBottom = oeBadge(QString(), m_resultSection);
    badgeRow->addWidget(m_bDepth);
    badgeRow->addWidget(m_bSst);
    badgeRow->addWidget(m_bSss);
    badgeRow->addWidget(m_bBottom);
    badgeRow->addStretch(1);
    m_resultSection->vbox()->addLayout(badgeRow);

    m_sspTable = new QTableWidget(8, 4, m_resultSection);
    oeSetupTable(m_sspTable, { I18n::tr("oe_col_z"), I18n::tr("oe_col_t"),
                               I18n::tr("oe_col_s"), I18n::tr("oe_col_c") },
                 210);
    m_resultSection->vbox()->addWidget(m_sspTable);
    m_layerNote = new QLabel(m_resultSection);
    m_layerNote->setWordWrap(true);
    m_resultSection->vbox()->addWidget(m_layerNote);
    // 出典の明示: 同梱の代表プロファイルであり、ローカルデータセットの
    // 実照会ではない (実装されたら差し替える)
    auto *srcNote = new QLabel(I18n::tr("oe_src_note"), m_resultSection);
    srcNote->setWordWrap(true);
    srcNote->setStyleSheet("font-size:11px; color:palette(mid);");
    m_resultSection->vbox()->addWidget(srcNote);
    v->addWidget(m_resultSection);

    // ── 音速プロファイル / SSP preview ──────────────────────────────────────
    auto *sp = new SectionBox(I18n::tr("oe_ssp_section"), body);
    m_sspView = new OeSspView(sp);
    sp->vbox()->addWidget(m_sspView);
    m_sspNote = new QLabel(sp);
    m_sspNote->setWordWrap(true);
    sp->vbox()->addWidget(m_sspNote);
    v->addWidget(sp);

    // ── 地形断面 / Bathymetry along track ───────────────────────────────────
    auto *sb = new SectionBox(I18n::tr("oe_bty_section"), body);
    auto *brRow = new QHBoxLayout();
    m_bearing = new QLineEdit("90", sb);
    m_bearing->setMaximumWidth(60);
    m_dist = new QLineEdit("50", sb);
    m_dist->setMaximumWidth(60);
    brRow->addWidget(m_bearing);
    brRow->addWidget(new QLabel(I18n::tr("oe_bearing_unit"), sb));
    brRow->addSpacing(8);
    brRow->addWidget(new QLabel(I18n::tr("oe_dist"), sb));
    brRow->addWidget(m_dist);
    brRow->addWidget(new QLabel(QStringLiteral("km"), sb));
    brRow->addStretch(1);
    sb->form()->addRow(I18n::tr("oe_bearing"), brRow);
    m_bathy = new OeBathyView(sb);
    sb->vbox()->addWidget(m_bathy);
    // 実データ断面か合成断面かを毎回明示する (computeSection() が書き換える)
    m_btyNote = new QLabel(I18n::tr("oe_bty_synth"), sb);
    m_btyNote->setWordWrap(true);
    m_btyNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sb->vbox()->addWidget(m_btyNote);
    v->addWidget(sb);

    // ── ソルバへ反映 / Apply to solver ──────────────────────────────────────
    auto *sa = new SectionBox(I18n::tr("oe_apply_section"), body);
    auto *chkRow = new QHBoxLayout();
    m_chkSsp    = new QCheckBox(I18n::tr("oe_chk_ssp"), sa);
    m_chkBty    = new QCheckBox(I18n::tr("oe_chk_bty"), sa);
    m_chkBottom = new QCheckBox(I18n::tr("oe_chk_bottom"), sa);
    m_chkSsp->setChecked(true);
    // 地形断面の .bty 書出しは実装済み (BellhopIO::btyText → Runner が
    // <ケース名>.bty を書き、.env 側は底面オプション 'A~' になる)
    m_chkBty->setChecked(true);
    m_chkBty->setToolTip(I18n::tr("oe_chk_bty_tip"));
    m_chkBottom->setChecked(true);
    chkRow->addWidget(m_chkSsp);
    chkRow->addWidget(m_chkBty);
    chkRow->addWidget(m_chkBottom);
    chkRow->addStretch(1);
    sa->vbox()->addLayout(chkRow);
    auto *btnRow = new QHBoxLayout();
    auto *applyBtn = new QPushButton(I18n::tr("oe_apply_btn"), sa);
    // .env は実際に書出せる (計算実行時と同じ BellhopIO::envText)。
    // .bty / .ssp は未実装のためボタン名から外す
    auto *exportBtn = new QPushButton(I18n::tr("oe_export_btn"), sa);
    connect(exportBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(
            this, I18n::tr("oe_export_btn"),
            QStringLiteral("underwater.env"), "BELLHOP env (*.env)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
        f.write(BellhopIO::envText(*m_p).toUtf8());
    });
    btnRow->addWidget(applyBtn);
    btnRow->addWidget(exportBtn);
    btnRow->addStretch(1);
    sa->vbox()->addLayout(btnRow);
    auto *applyNote = new QLabel(I18n::tr("oe_apply_note"), sa);
    applyNote->setWordWrap(true);
    sa->vbox()->addWidget(applyNote);
    v->addWidget(sa);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(queryBtn, &QPushButton::clicked, this,
            &OceanEnvironmentTab::requery);
    connect(m_lat, &QLineEdit::editingFinished, this,
            &OceanEnvironmentTab::requery);
    connect(m_lon, &QLineEdit::editingFinished, this,
            &OceanEnvironmentTab::requery);
    connect(m_month, &QComboBox::currentIndexChanged, this,
            [this] { requery(); });
    connect(fetchBtn, &QPushButton::clicked, this,
            &OceanEnvironmentTab::openDownloadManager);
    connect(applyBtn, &QPushButton::clicked, this,
            &OceanEnvironmentTab::applyToSolver);
    // 伝搬方位・距離を変えたら断面を引き直す (旧: 方位はどこにも効かなかった)
    connect(m_bearing, &QLineEdit::editingFinished, this,
            [this] { computeSection(); });
    connect(m_dist, &QLineEdit::editingFinished, this,
            [this] { computeSection(); });
    // データセットを取り込んだ直後にも実データへ切り替わるようにする
    connect(project, &Project::loaded, this, [this] {
        loadSiteFromProject();
        requery();
    });

    loadSiteFromProject();
    requery();
}

// 緯度・経度 → 海域 (mock の oeFindRegion。該当なしは黒潮域にフォールバック)
const OeRegion &OceanEnvironmentTab::findRegion(double lat, double lon)
{
    for (int i = 0; i < kRegionCount; ++i) {
        const OeRegion &r = kRegions[i];
        if (lat >= r.latMin && lat <= r.latMax
            && lon >= r.lonMin && lon <= r.lonMax)
            return r;
    }
    return kRegions[1];
}

// 水温プロファイル生成 (型別の典型形状) + Mackenzie 式で音速
// mock の oeTempProfile / oeSoundSpeed をそのまま移植 (N=40 → 41層)
void OceanEnvironmentTab::computeSsp()
{
    m_ssp.clear();
    if (!m_region) return;
    const OeRegion &r = *m_region;
    const double depth = r.depth;
    const QString type = QString::fromUtf8(r.type);
    const int N = 40;

    for (int i = 0; i <= N; ++i) {
        const double z = depth * std::pow(double(i) / N, 1.6);  // 浅部を密に
        double T;
        if (type == QLatin1String("okhotsk")) {
            // 中冷水 (dichothermal layer)
            T = z < 30  ? r.sst - z * 0.15
              : z < 120 ? 3.5 - (z - 30) * 0.025
              : z < 400 ? 1.2 + (z - 120) * 0.003
                        : 2.0;
        } else if (type == QLatin1String("shelf")) {
            T = z < 40 ? r.sst - z * 0.12
                       : r.sst - 4.8 - (z - 40) * 0.05;
            T = std::max(T, 16.0);
        } else if (type == QLatin1String("med")) {
            // 地中海: 深層が暖かく (~13°C) 塩分が高い
            const double mixed = 30;
            T = z < mixed ? r.sst
              : z < 500   ? r.sst - (z - mixed) * (r.sst - 13.5) / 470
                          : 13.5;
        } else {
            // 標準: 混合層 → 主躍層 → 深層
            const double mixed = 45;
            T = z < mixed ? r.sst
              : z < 900   ? r.sst - (z - mixed) * (r.sst - 4) / 855
                          : std::max(1.6, 4 - (z - 900) * 0.0005);
        }
        // 塩分: 表層値 → 深層 34.62
        const double S = z < 100
            ? r.sss
            : r.sss + (34.62 - r.sss) * std::min(1.0, (z - 100) / 1500.0);

        OeSspPoint pt;
        pt.z = int(std::round(z));
        pt.T = oeRound(T, 2);
        pt.S = oeRound(S, 2);
        pt.c = oeRound(oeSoundSpeed(pt.T, pt.S, pt.z), 1);
        m_ssp.push_back(pt);
    }

    // 最小音速層 (SOFAR 軸) — 最初に現れる最小値 (mock の reduce と同じ)
    m_cMinIdx = 0;
    for (int i = 1; i < m_ssp.size(); ++i)
        if (m_ssp[i].c < m_ssp[m_cMinIdx].c) m_cMinIdx = i;
}

void OceanEnvironmentTab::requery()
{
    const double lat = m_lat->text().toDouble();
    const double lon = m_lon->text().toDouble();
    m_region = &findRegion(lat, lon);
    computeSsp();

    const OeRegion &r = *m_region;
    const QString name = QString::fromUtf8(r.name);
    const bool shelf = (QString::fromUtf8(r.type) == QLatin1String("shelf"));
    const int month = m_month->currentIndex() + 1;

    m_queryBadge->setText(I18n::tr("oe_query_ok").arg(name));
    m_resultSection->setTitle(I18n::tr("oe_result") + QStringLiteral(" — ")
                              + name);

    m_bDepth->setText(I18n::tr("oe_b_depth")
        .arg(QLocale(QLocale::English).toString(qlonglong(r.depth))));
    m_bSst->setText(I18n::tr("oe_b_sst")
        .arg(QString::number(r.sst, 'g', 4)).arg(month));
    m_bSss->setText(I18n::tr("oe_b_sss").arg(QString::number(r.sss, 'g', 4)));
    m_bBottom->setText(I18n::tr("oe_b_bottom")
        .arg(I18n::tr(shelf ? "oe_bottom_shelf" : "oe_bottom_deep")));

    // 代表 8 層 (mock の [0,4,8,14,20,27,34,40])
    static const int kRows[8] = { 0, 4, 8, 14, 20, 27, 34, 40 };
    m_sspTable->setRowCount(8);
    const int cMinZ = m_ssp.isEmpty() ? -1 : m_ssp[m_cMinIdx].z;
    for (int i = 0; i < 8; ++i) {
        const int k = kRows[i];
        const bool valid = (k < m_ssp.size());
        const OeSspPoint p = valid ? m_ssp[k] : OeSspPoint{ 0, 0, 0, 0 };
        const QString cells[4] = {
            valid ? QString::number(p.z) : QString(),
            valid ? QString::number(p.T, 'f', 1) : QString(),
            valid ? QString::number(p.S, 'f', 2) : QString(),
            valid ? QString::number(p.c, 'f', 1) : QString(),
        };
        for (int c = 0; c < 4; ++c) {
            auto *it = new QTableWidgetItem(cells[c]);
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (valid && p.z == cMinZ) {          // "sel" 行 (SOFAR 軸)
                QFont f = it->font();
                f.setBold(true);
                it->setFont(f);
                it->setForeground(QBrush(QColor(kAcc)));
            }
            m_sspTable->setItem(i, c, it);
        }
    }
    m_layerNote->setText(I18n::tr("oe_layer_note").arg(m_ssp.size()));

    m_sspView->setProfile(m_ssp, r.depth, m_cMinIdx);
    computeSection();

    const QString type = QString::fromUtf8(r.type);
    if (type == QLatin1String("okhotsk"))
        m_sspNote->setText(I18n::tr("oe_ssp_okhotsk"));
    else if (shelf)
        m_sspNote->setText(I18n::tr("oe_ssp_shelf"));
    else
        m_sspNote->setText(I18n::tr("oe_ssp_sofar").arg(cMinZ));
}

// SSP / 底質 / 伝搬距離を UnderwaterOpts に転送 (地形は .bty 書出し側の担当)
// 測点 (緯度経度) と伝搬方位から地形断面を作る。
// ローカルデータセットに水深グリッドがあれば大圏に沿ってサンプリングし、
// 無ければ (あるいは読めなければ) 海域代表水深からの合成断面にする。
// **どちらなのかは必ず画面に出す** — 合成を実地形として扱わせない。
// .ofdx に保存された測点・方位・距離を入力欄へ戻す (既定値ならそのまま)
void OceanEnvironmentTab::loadSiteFromProject()
{
    const UnderwaterOpts &u = m_p->underwater();
    m_lat->setText(QString::number(u.siteLat_deg, 'g', 8));
    m_lon->setText(QString::number(u.siteLon_deg, 'g', 8));
    m_bearing->setText(QString::number(u.trackBearing_deg, 'g', 6));
    if (u.rangeMax_km > 0.0)
        m_dist->setText(QString::number(u.rangeMax_km, 'g', 8));
}

void OceanEnvironmentTab::computeSection()
{
    const double lat = m_lat->text().toDouble();
    const double lon = m_lon->text().toDouble();
    const double bearing = m_bearing->text().toDouble();
    double km = m_dist->text().toDouble();
    if (!(km > 0.0)) km = m_region ? 50.0 : 0.0;
    const double areaDepth = m_region ? m_region->depth : 0.0;

    m_section.clear();
    m_sectionSource.clear();
    QString firstErr;

    if (km > 0.0) {
        // 経路が通る緯度経度の外接矩形 (端点まで実際に辿って求める)
        const GeoPoint site{ lat, lon };
        double laLo = lat, laHi = lat, loLo = lon, loHi = lon;
        for (int i = 1; i <= 16; ++i) {
            const GeoPoint q = geoDestination(site, bearing, km * i / 16.0);
            laLo = qMin(laLo, q.lat_deg); laHi = qMax(laHi, q.lat_deg);
            loLo = qMin(loLo, q.lon_deg); loHi = qMax(loHi, q.lon_deg);
        }
        const QStringList grids = BathymetryIO::findGrids(oeDataDir());
        for (const QString &g : grids) {
            BathyGrid grid;
            QString err;
            if (!BathymetryIO::readGrid(g, laLo, laHi, loLo, loHi, grid, &err)) {
                if (firstErr.isEmpty()) firstErr = err;
                continue;
            }
            const QVector<BathyPoint> pts =
                BathymetryIO::sampleTrack(grid, site, bearing, km, 101);
            // 経路の大半が陸・欠測なら断面として使わない (次の候補へ)
            if (pts.size() < 50) {
                if (firstErr.isEmpty())
                    firstErr = QStringLiteral("%1: the track is mostly land or "
                                              "outside the grid").arg(grid.source);
                continue;
            }
            m_section = pts;
            m_sectionSource = grid.source;
            break;
        }
    }
    if (m_section.isEmpty() && areaDepth > 0.0 && km > 0.0)
        m_section = BathymetryIO::syntheticTrack(areaDepth, km, 51);

    m_bathy->setSection(m_section, m_sectionSource);
    if (!m_btyNote) return;
    if (!m_sectionSource.isEmpty()) {
        m_btyNote->setText(I18n::tr("oe_bty_real")
                               .arg(m_sectionSource)
                               .arg(bearing, 0, 'f', 0)
                               .arg(km, 0, 'f', 0)
                               .arg(m_section.size()));
    } else if (!firstErr.isEmpty()) {
        m_btyNote->setText(I18n::tr("oe_bty_err").arg(firstErr));
    } else {
        m_btyNote->setText(I18n::tr("oe_bty_synth"));
    }
}

void OceanEnvironmentTab::applyToSolver()
{
    if (!m_region || m_ssp.isEmpty()) return;
    UnderwaterOpts &u = m_p->underwater();
    const bool shelf =
        (QString::fromUtf8(m_region->type) == QLatin1String("shelf"));

    if (m_chkSsp->isChecked()) {
        u.ssp.clear();
        u.ssp.reserve(m_ssp.size());
        for (const OeSspPoint &p : m_ssp)
            u.ssp.push_back({ double(p.z), p.c });
        u.waterTemp_C  = m_ssp.first().T;
        u.salinity_psu = m_ssp.first().S;
        // 深海で SOFAR 軸が中層にあれば音道解析を有効化
        u.sofar = !shelf && m_cMinIdx > 0 && m_cMinIdx < m_ssp.size() - 1;
    }
    if (m_chkBottom->isChecked()) {
        u.bottomType     = shelf ? QStringLiteral("sand") : QStringLiteral("mud");
        u.bottomC_mps    = shelf ? 1650.0 : 1520.0;
        u.bottomRho_kgm3 = shelf ? 1900.0 : 1500.0;
    }
    const double km = m_dist->text().toDouble();
    if (km > 0) u.rangeMax_km = km;
    // 測点と伝搬方位は常に記録する (断面の再現に要る)
    u.siteLat_deg = m_lat->text().toDouble();
    u.siteLon_deg = m_lon->text().toDouble();
    u.trackBearing_deg = m_bearing->text().toDouble();
    if (m_chkBty->isChecked()) {
        // 合成断面でも「合成である」と分かる出所を残す
        u.bathymetry = m_section;
        u.bathySource = m_sectionSource.isEmpty()
                            ? QStringLiteral("synthetic")
                            : m_sectionSource;
    } else {
        u.bathymetry.clear();
        u.bathySource.clear();
    }
    m_p->touch();
}

void OceanEnvironmentTab::openDownloadManager()
{
    OeDownloadManager dlg(this);
    dlg.exec();
    rebuildDatasetTable();   // 取込があれば配置状態に即反映
}

// データセットフォルダを走査し、実在するファイルの配置状態だけを表示する
void OceanEnvironmentTab::rebuildDatasetTable()
{
    const QString dir = oeDataDir();
    const OeScanResult scan = oeScanAll(dir);

    for (int r = 0; r < kOeDatasetCount; ++r) {
        const OeDatasetDef &d = kOeDatasets[r];
        const bool found = scan.bytes[r] > 0;

        auto *ck = new QTableWidgetItem;
        ck->setCheckState(found ? Qt::Checked : Qt::Unchecked);
        ck->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        m_dsTable->setItem(r, 0, ck);
        m_dsTable->setItem(r, 1, new QTableWidgetItem(oeDatasetName(d)));
        m_dsTable->setItem(r, 2, new QTableWidgetItem(
            QString::fromUtf8(d.provider)));
        m_dsTable->setItem(r, 3, new QTableWidgetItem(I18n::tr(d.contentKey)));
        m_dsTable->setItem(r, 4, new QTableWidgetItem(
            d.resKey ? I18n::tr(d.resKey) : QString::fromUtf8(d.resRaw)));

        QString state;
        const char *color;
        if (found) {
            state = I18n::tr("oe_staged").arg(oeHumanSize(scan.bytes[r]));
            color = kOk;
        } else if (!d.url) {
            state = I18n::tr("oe_restricted");
            color = kErr;
        } else {
            state = I18n::tr("oe_notstaged");
            color = kWarn;
        }
        auto *st = new QTableWidgetItem(state);
        st->setForeground(QBrush(QColor(color)));
        m_dsTable->setItem(r, 5, st);
    }

    m_dsFolderLabel->setText(QStringLiteral("%1 (%2)")
        .arg(QDir::toNativeSeparators(dir),
             scan.dirExists ? oeHumanSize(scan.total)
                            : I18n::tr("oe_notstaged")));
}
