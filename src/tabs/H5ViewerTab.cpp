// H5ViewerTab.cpp
#include "H5ViewerTab.h"
#include "../core/Project.h"
#include "../io/MovieExport.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QStringList>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>

using namespace ofd;

namespace {
// タブ専用語彙 (接頭辞 h5_) — file-local 登録
const bool s_i18n = [] {
    ofd::I18n::reg("h5_file_section", "HDF5 ファイル", "HDF5 File");
    ofd::I18n::reg("h5_file", "ファイル", "File");
    ofd::I18n::reg("h5_browse", "📁 参照…", "📁 Browse…");
    ofd::I18n::reg("h5_reload", "↻ 再読込", "↻ Reload");
    ofd::I18n::reg("h5_file_ph", "例: time_series_data.h5",
                   "e.g. time_series_data.h5");
    ofd::I18n::reg("h5_formats", "対応形式", "Supported formats");
    ofd::I18n::reg("h5_file_hint",
        "▸ 2D / 3D (frames×rows×cols) データセットのヒートマップ表示と、"
        "ofd の伝搬時系列 (/timeseries/E|H の瞬時値、旧 /data*/E|H) の"
        "z 中央断面アニメ再生に対応。1D 等は表示未対応。",
        "▸ Displays 2D and 3D (frames×rows×cols) datasets as heatmaps, and "
        "plays ofd propagation series (instantaneous /timeseries/E|H, legacy "
        "/data*/E|H) as z-mid-slice animations. 1D etc. are not supported.");
    ofd::I18n::reg("h5_series_title",
        "伝搬アニメ |%1| %2 断面 — %3",
        "Propagation |%1| %2 slice — %3");
    ofd::I18n::reg("h5_sec_note_on",
        "▸ 断面の軸と位置は伝搬時系列 (E/H) の表示に反映されます",
        "▸ The slice plane and position apply to the propagation series "
        "(E/H) view");
    ofd::I18n::reg("h5_sec_note_off",
        "▸ 断面の選択は伝搬時系列 (E/H) を選択したときに有効になります",
        "▸ Slice selection becomes active when a propagation series (E/H) "
        "is selected");
    ofd::I18n::reg("h5_exp_running", "書き出し中… %1 / %2",
                   "Exporting… %1 / %2");
    ofd::I18n::reg("h5_exp_encoding", "ffmpeg でエンコード中…",
                   "Encoding with ffmpeg…");
    ofd::I18n::reg("h5_exp_done", "書き出しました: %1", "Exported: %1");
    ofd::I18n::reg("h5_exp_fail", "書き出し失敗: %1", "Export failed: %1");
    ofd::I18n::reg("h5_exp_noffmpeg",
        "ffmpeg が見つかりません (PATH を確認)。動画化には ffmpeg の導入が"
        "必要です — macOS: brew install ffmpeg / Linux: apt install ffmpeg。"
        "代わりに「PNG連番」で書き出せます。",
        "ffmpeg not found on PATH. Install it for video export — macOS: "
        "brew install ffmpeg / Linux: apt install ffmpeg. You can export a "
        "PNG sequence instead.");
    ofd::I18n::reg("h5_exp_cancelled", "書き出しを中止しました",
                   "Export cancelled");
    ofd::I18n::reg("h5_disabled",
        "HDF5 読取はこのビルドでは無効です (-DUSE_HDF5=ON でビルドしてください)",
        "HDF5 reading is disabled in this build (rebuild with -DUSE_HDF5=ON)");
    ofd::I18n::reg("h5_tree_section", "データセット", "Dataset tree");
    ofd::I18n::reg("h5_no_datasets", "(データセットなし)", "(no datasets)");
    ofd::I18n::reg("h5_selected", "選択中:", "Selected:");
    ofd::I18n::reg("h5_no_data",
        "データ未読込 — .h5 を開いてデータセットを選択してください",
        "No data loaded — open an .h5 file and select a dataset");
    ofd::I18n::reg("h5_unsupported",
        "このデータセットは表示未対応 (2D/3D のみ)",
        "This dataset cannot be displayed (only 2D/3D supported)");
    ofd::I18n::reg("h5_load_error", "読込エラー:", "Load error:");
    ofd::I18n::reg("h5_vis_section", "可視化", "Visualization");
    ofd::I18n::reg("h5_colormap", "カラーマップ", "Colormap");
    ofd::I18n::reg("h5_scale", "スケール", "Scale");
    ofd::I18n::reg("h5_auto", "自動", "Auto");
    ofd::I18n::reg("h5_show_grid", "グリッド表示", "Show grid");
    ofd::I18n::reg("h5_show_axes", "軸ラベル", "Axis labels");
    ofd::I18n::reg("h5_overlay_geom", "物体オーバーレイ", "Geometry overlay");
    ofd::I18n::reg("h5_overlay_mon", "モニター位置", "Monitor positions");
    ofd::I18n::reg("h5_preview", "プレビュー", "Preview");
    ofd::I18n::reg("h5_playback", "再生コントロール", "Playback");
    ofd::I18n::reg("h5_play", "▶ 再生", "▶ Play");
    ofd::I18n::reg("h5_pause", "⏸ 一時停止", "⏸ Pause");
    ofd::I18n::reg("h5_first", "⏮ 先頭", "⏮ First");
    ofd::I18n::reg("h5_prev", "◀ 前", "◀ Prev");
    ofd::I18n::reg("h5_next", "次 ▶", "Next ▶");
    ofd::I18n::reg("h5_last", "末尾 ⏭", "Last ⏭");
    ofd::I18n::reg("h5_loop", "⟳ ループ", "⟳ Loop");
    ofd::I18n::reg("h5_loop_tip",
        "ON: 末尾フレームの後は先頭へ戻って再生を続けます / "
        "OFF: 末尾フレームで再生を停止します",
        "ON: playback wraps to the first frame after the last / "
        "OFF: playback stops at the last frame");
    ofd::I18n::reg("h5_frame", "フレーム", "Frame");
    ofd::I18n::reg("h5_speed", "速度", "Speed");
    ofd::I18n::reg("h5_time_range", "時間範囲", "Time range");
    ofd::I18n::reg("h5_range_only", "範囲限定再生", "Play range only");
    ofd::I18n::reg("h5_xsec_section", "時間断面", "Cross-sections (XY/XZ/YZ)");
    ofd::I18n::reg("h5_sec_xy", "XY 面 (Z=固定)", "XY plane (Z fixed)");
    ofd::I18n::reg("h5_sec_xz", "XZ 面 (Y=固定)", "XZ plane (Y fixed)");
    ofd::I18n::reg("h5_sec_yz", "YZ 面 (X=固定)", "YZ plane (X fixed)");
    ofd::I18n::reg("h5_sec_pos", "位置", "Position");
    ofd::I18n::reg("h5_scene_chk", "3D シーンに重ねる",
                   "Overlay on the 3D scene");
    ofd::I18n::reg("h5_scene_tip_on",
                   "表示中のフレームを 3D シーンの同じ位置へ重ねます",
                   "Overlay the frame being shown at the matching position "
                   "in the 3D scene");
    ofd::I18n::reg("h5_scene_tip_off",
                   "伝搬時系列で、断面の座標 (/metadata/Xn・Yn・Zn) が "
                   "読めるファイルのときだけ使えます",
                   "Available only for a propagation time series whose slice "
                   "coordinates (/metadata/Xn, Yn, Zn) can be read");
    ofd::I18n::reg("h5_scene_note",
                   "表示中のフレームを 3D シーンの同じ位置へ重ねます "
                   "(再生・コマ送り・断面の変更に追従します)。"
                   "3D 側は「結果断面を重ねる」が入っていると見えます。"
                   "座標が読めないファイルでは位置を決められないので重ねません "
                   "(適当な場所に置くと結果を読み違えるため)。",
                   "The frame being shown is overlaid at the matching position "
                   "in the 3D scene, following playback, stepping and slice "
                   "changes. Turn on \"Overlay result slice\" in the 3D view "
                   "to see it. If the file has no coordinates the slice is not "
                   "placed at all, because putting it at a guessed position "
                   "would misrepresent the result.");
    ofd::I18n::reg("h5_scene_label", "H5アニメ |%1| %2 断面 %3 (%4 m)",
                   "H5 animation |%1| %2 slice %3 (%4 m)");
    ofd::I18n::reg("h5_scene_auto",
                   "  ※ 明るさはフレームごとの最大値で正規化しています "
                   "(2D と同じ)。フレーム間で強さを比べるには手動スケールに "
                   "してください。",
                   "  Note: the brightness is normalised per frame, the same "
                   "as the 2D view. Switch to a manual scale to compare "
                   "strength between frames.");
    ofd::I18n::reg("h5_sec_multi", "複数断面同時表示 (3面ビュー)",
                   "Show multiple sections (3-plane view)");
    ofd::I18n::reg("h5_multi_tip_on",
        "XY / XZ / YZ の 3 断面を同時に表示します "
        "(カラースケールは 3 面共通 — 面どうしの強度を比較できます)",
        "Shows the XY / XZ / YZ slices side by side "
        "(one shared color scale, so intensities are comparable)");
    ofd::I18n::reg("h5_multi_tip_off",
        "3面ビューは 3 次元の伝搬時系列 (E/H) でのみ有効です。"
        "2D データセットや汎用 3D (frames×rows×cols) データセットには"
        "直交 3 断面が定義できません。",
        "The 3-plane view requires a 3-D propagation series (E/H). "
        "2-D datasets and generic 3-D (frames×rows×cols) datasets have no "
        "orthogonal slice planes.");
    ofd::I18n::reg("h5_sec_note_multi",
        "▸ 3面ビュー: XY / XZ / YZ を共通カラースケール (3 面の合成 min/max) "
        "で同時表示しています。位置スライダは上のコンボで選んだ面 (太字) の"
        "断面位置を動かします。統計値も 3 面の合成です。",
        "▸ 3-plane view: XY / XZ / YZ are shown together with one shared "
        "color scale (min/max over all three). The position slider moves the "
        "slice of the plane selected above (shown in bold). Statistics are "
        "over all three planes as well.");
    ofd::I18n::reg("h5_multi_title",
        "伝搬アニメ |%1| 3面ビュー (XY / XZ / YZ) — %2",
        "Propagation |%1| 3-plane view (XY / XZ / YZ) — %2");
    ofd::I18n::reg("h5_multi_exp_note",
        "▸ 3面ビュー表示中: PNG (現フレーム) / PNG連番 / MP4 / GIF は"
        "「3 面を並べた画像」を書き出します。CSV は主断面 (%1) の行列のみです。",
        "▸ 3-plane view is on: PNG (current frame) / PNG sequence / MP4 / GIF "
        "export the three planes side by side. CSV writes only the primary "
        "plane (%1).");
    ofd::I18n::reg("h5_export", "エクスポート", "Export");
    ofd::I18n::reg("h5_exp_mp4", "🎥 MP4 動画", "🎥 MP4 movie");
    ofd::I18n::reg("h5_exp_gif", "🎞 GIF アニメ", "🎞 GIF animation");
    ofd::I18n::reg("h5_exp_png", "📸 PNG (現フレーム)", "📸 PNG (current frame)");
    ofd::I18n::reg("h5_exp_pngseq", "🖼 PNG 連番 (全フレーム)",
                   "🖼 PNG sequence (all frames)");
    ofd::I18n::reg("h5_exp_csv", "📊 CSV (時系列)", "📊 CSV (time series)");
    ofd::I18n::reg("h5_res_native", "元のまま", "Native");
    ofd::I18n::reg("h5_range_all",
                   "▸ 全 %1 〜 %2 %3 を再生します (範囲限定なし)。",
                   "▸ Playing the whole %1 – %2 %3 (no range limit).");
    ofd::I18n::reg("h5_range_applied",
                   "▸ %1 フレーム (#%2〜#%3, %4 〜 %5 %6) だけを再生・書き出し"
                   "します。",
                   "▸ Playing and exporting only %1 frames (#%2–#%3, "
                   "%4 – %5 %6).");
    ofd::I18n::reg("h5_range_empty",
                   "▸ その時間範囲に入るフレームがありません — 範囲限定を"
                   "解除しました。",
                   "▸ No frame falls in that time range — the range limit was "
                   "switched off.");
    ofd::I18n::reg("h5_range_notime",
                   "▸ このファイルにはフレームの時刻 (/timeseries/time) が"
                   "無いため、時間範囲では絞り込めません "
                   "(フレーム番号を時刻とみなすようなことはしません)。",
                   "▸ This file has no per-frame time (/timeseries/time), so "
                   "the range cannot be resolved (frame numbers are not "
                   "treated as times).");
    ofd::I18n::reg("h5_range_noseries",
                   "▸ 時系列データセットを選ぶと時間範囲を指定できます。",
                   "▸ Select a time-series dataset to use the time range.");
    ofd::I18n::reg("h5_movie_note",
                   "▸ FPS・解像度・コーデックは ffmpeg へ渡ります "
                   "(FPS 未入力なら再生速度から決まります)。解像度「元のまま」"
                   "は拡大しません。図中への凡例・形状の焼き込みは未実装です。",
                   "▸ The FPS, resolution and codec are passed to ffmpeg (an "
                   "empty FPS falls back to the playback speed). \"Native\" "
                   "resolution does not rescale. Burning the colour bar or the "
                   "geometry into the frames is not implemented.");
    ofd::I18n::reg("h5_movie", "動画設定", "Movie settings");
    ofd::I18n::reg("h5_resolution", "解像度", "Resolution");
    ofd::I18n::reg("h5_codec", "コーデック", "Codec");
    ofd::I18n::reg("h5_embed_bar", "タイムスタンプ・カラーバー埋込",
                   "Embed timestamp && colorbar");
    ofd::I18n::reg("h5_embed_geom", "物体オーバーレイ埋込",
                   "Embed geometry overlay");
    ofd::I18n::reg("h5_stats_section", "統計・派生量", "Statistics && derived");
    ofd::I18n::reg("h5_stat_mean", "平均", "mean");
    ofd::I18n::reg("h5_stat_series", "📈 全体時系列 (max/RMS)",
                   "📈 Overall time series (max/RMS)");
    ofd::I18n::reg("h5_stat_schroeder", "📈 Schroeder減衰", "📈 Schroeder decay");
    ofd::I18n::reg("h5_stat_fft", "📈 FFT スペクトログラム", "📈 FFT spectrogram");
    ofd::I18n::reg("h5_stat_lineint", "📈 線積分", "📈 Line integral");
    ofd::I18n::reg("h5_integration", "連携", "Integration");
    ofd::I18n::reg("h5_int_python", "🐍 Python (h5py) で開く",
                   "🐍 Open in Python (h5py)");
    ofd::I18n::reg("h5_int_jupyter", "📊 Jupyter Notebook",
                   "📊 Jupyter Notebook");
    ofd::I18n::reg("h5_int_need_file",
        "先に .h5 ファイルを開いてください (スクリプトは開いているファイルの"
        "実スキーマから生成します)",
        "Open an .h5 file first (the script is generated from the actual "
        "schema of the open file)");
    ofd::I18n::reg("h5_int_hint",
        "▸ Python / Jupyter は開いている .h5 の実スキーマに合わせた h5py "
        "読み込みコード (|E| プロットまで) をファイルへ生成します"
        " (外部ツールの起動は行いません)",
        "▸ Python / Jupyter generate h5py loading code (through an |E| plot) "
        "matched to the actual schema of the open .h5 file "
        "(no external tool is launched)");
    ofd::I18n::reg("h5_int_paraview", "🌐 ParaView ファイル出力 (.vtk)",
                   "🌐 ParaView file export (.vtk)");
    ofd::I18n::reg("h5_int_matlab", "📦 Matlab .mat 変換",
                   "📦 Convert to Matlab .mat");
    return true;
}();

const double kSpeeds[5] = { 0.25, 0.5, 1.0, 2.0, 5.0 };

// planeBox の並び (0=XY, 1=XZ, 2=YZ) → 固定軸 (0=X, 1=Y, 2=Z)。
// 3 面ビューのパネル並びもこの順 (XY / XZ / YZ)
const int kPlaneAxis[3] = { 2, 1, 0 };

// 固定軸 → 軸名 / 面の i18n キー
const char *const kAxisName[3] = { "X", "Y", "Z" };
const char *const kPlaneKey[3] = { "h5_sec_yz", "h5_sec_xz", "h5_sec_xy" };

// planeBox の並び順のパネル名 (キャンバス左上のオーバーレイ)
const char *const kPlaneShort[3] = { "XY", "XZ", "YZ" };

// バッジ風ラベル (mock .badge 相当, 最小限のスタイル)
QLabel *makeBadge(const QString &text, QWidget *parent, const char *kind = "")
{
    auto *l = new QLabel(text, parent);
    const char *color = "#8A8A8A";
    if (qstrcmp(kind, "ok") == 0)  color = "#2E8B57";
    if (qstrcmp(kind, "acc") == 0) color = "#0078D4";
    l->setStyleSheet(QString("padding:1px 6px; border:1px solid %1; "
                             "border-radius:3px; color:%1; font-size:11px;")
                         .arg(QString::fromUtf8(color)));
    return l;
}

// 次元表示 "(200 × 128)"
QString dimsText(const QVector<qlonglong> &dims)
{
    QStringList parts;
    for (const qlonglong d : dims) parts << QString::number(d);
    return QString("(%1)").arg(parts.join(QString::fromUtf8(" × ")));
}

// ── Python (h5py) コード生成 (連携ボタン) ──────────────────────────────────
// Python の文字列リテラルとしてパスを埋め込む (バックスラッシュ・引用符を
// エスケープ — Windows パス対策)
QString pyQuote(const QString &s)
{
    QString t = s;
    t.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    t.replace(QLatin1Char('"'), QLatin1String("\\\""));
    return QLatin1Char('"') + t + QLatin1Char('"');
}

// 列挙結果の 1 行表記 "  /timeseries/E  float32 (100 × 41 × 41 × 41 × 3)"
QStringList schemaLines(const QVector<ofd::H5DatasetInfo> &dsets)
{
    QStringList out;
    for (const ofd::H5DatasetInfo &ds : dsets)
        out << QStringLiteral("  %1  %2 %3")
                   .arg(ds.path, ds.typeName, dimsText(ds.dims));
    return out;
}

// 開いているファイルの実スキーマから h5py 読み込みコードを組み立てる。
// プロット対象は列挙結果から実在するものを選ぶ:
//   (a) /timeseries/E|H (新レイアウト {Nt,Nx+1,Ny+1,Nz+1,3}) → 最終フレームの
//       |E| z 中央断面
//   (b) 旧 /data%06d/E|H ({1,NFreq2,NN,6}) → /metadata の格子定数で空間展開
//   (c) それ以外の最初の 2D データセット → そのまま表示
// どれも無ければ列挙のみのコードを返す。戻り値はコード行 (改行なし)。
QStringList buildH5PyCode(const QString &filePath,
                          const QVector<ofd::H5DatasetInfo> &dsets)
{
    QStringList c;
    c << QStringLiteral("import h5py")
      << QStringLiteral("import numpy as np")
      << QStringLiteral("import matplotlib.pyplot as plt")
      << QString()
      << QStringLiteral("path = %1").arg(pyQuote(filePath))
      << QString()
      << QStringLiteral("with h5py.File(path, \"r\") as f:")
      << QStringLiteral("    # 全データセットの確認 (パス・形状・型)")
      << QStringLiteral("    f.visititems(lambda name, obj: print(")
      << QStringLiteral("        name, getattr(obj, \"shape\", ()), "
                        "getattr(obj, \"dtype\", \"\")))");

    // (a) 新レイアウトの伝搬時系列 (E 優先)
    QString seriesPath;
    for (const char *p : { "/timeseries/E", "/timeseries/H" }) {
        for (const ofd::H5DatasetInfo &ds : dsets)
            if (ds.path == QLatin1String(p)) { seriesPath = ds.path; break; }
        if (!seriesPath.isEmpty()) break;
    }
    // (b) 旧レイアウト /data%06d/E|H — 最大 (最終) グループ番号を選ぶ
    QString dataPath;
    if (seriesPath.isEmpty()) {
        static const QRegularExpression dataRe(
            QStringLiteral("^/data(\\d+)/(E|H)$"));
        for (const char *comp : { "E", "H" }) {
            qlonglong best = -1;
            for (const ofd::H5DatasetInfo &ds : dsets) {
                const QRegularExpressionMatch m = dataRe.match(ds.path);
                if (!m.hasMatch()
                        || m.captured(2) != QLatin1String(comp)) continue;
                const qlonglong num = m.captured(1).toLongLong();
                if (num > best) { best = num; dataPath = ds.path; }
            }
            if (best >= 0) break;
        }
    }
    // (c) 最初の 2D データセット
    QString flatPath;
    if (seriesPath.isEmpty() && dataPath.isEmpty()) {
        for (const ofd::H5DatasetInfo &ds : dsets)
            if (ds.dims.size() == 2) { flatPath = ds.path; break; }
    }

    bool hasPlot = true;
    QString xlab = QStringLiteral("x index"), ylab = QStringLiteral("y index");
    if (!seriesPath.isEmpty()) {
        const QString comp = seriesPath.section(QLatin1Char('/'), -1);
        c << QString()
          << QStringLiteral("    # 伝搬時系列 (瞬時値): "
                            "(Nt, Nx+1, Ny+1, Nz+1, 3)")
          << QStringLiteral("    ds = f[%1]").arg(pyQuote(seriesPath))
          << QStringLiteral("    nt, nx1, ny1, nz1, _ = ds.shape")
          << QStringLiteral("    frame = np.asarray(ds[nt - 1], "
                            "dtype=np.float64)  # 最終フレーム")
          << QStringLiteral("    amp = np.sqrt((frame ** 2).sum(axis=-1))"
                            "     # |%1| (成分の RSS)").arg(comp)
          << QStringLiteral("    img = amp[:, :, nz1 // 2].T"
                            "                    # z 中央断面 (行 = y)")
          << QStringLiteral("    title = f\"%1  |%2|  frame {nt - 1}  "
                            "z index {nz1 // 2}\"").arg(seriesPath, comp);
    } else if (!dataPath.isEmpty()) {
        const QString comp = dataPath.section(QLatin1Char('/'), -1);
        c << QString()
          << QStringLiteral("    # 旧レイアウト %1: (1, NFreq2, NN, 6)。"
                            "/metadata の格子定数で").arg(dataPath)
          << QStringLiteral("    # ノード番号 n = Ni*i + Nj*j + Nk*k + N0 "
                            "を空間へ展開する")
          << QStringLiteral("    md = f[\"/metadata\"]")
          << QStringLiteral("    Nx, Ny, Nz = (int(md[k][()]) for k in "
                            "(\"Nx\", \"Ny\", \"Nz\"))")
          << QStringLiteral("    Ni, Nj, Nk, N0 = (int(md[k][()]) for k in "
                            "(\"Ni\", \"Nj\", \"Nk\", \"N0\"))")
          << QStringLiteral("    e = np.asarray(f[%1][0, 0], "
                            "dtype=np.float64)  # 周波数 0: (NN, 6)")
                 .arg(pyQuote(dataPath))
          << QStringLiteral("    amp = np.sqrt((e ** 2).sum(axis=1))"
                            "   # |%1| (実部 3 + 虚部 3 の RSS)").arg(comp)
          << QStringLiteral("    kz = Nz // 2"
                            "                          # z 中央断面")
          << QStringLiteral("    ii = np.arange(Nx + 1)[None, :]")
          << QStringLiteral("    jj = np.arange(Ny + 1)[:, None]")
          << QStringLiteral("    img = amp[Ni * ii + Nj * jj + Nk * kz + N0]"
                            "  # (Ny+1, Nx+1)")
          << QStringLiteral("    title = f\"%1  |%2|  z index {kz}\"")
                 .arg(dataPath, comp);
    } else if (!flatPath.isEmpty()) {
        xlab = QStringLiteral("col");
        ylab = QStringLiteral("row");
        c << QString()
          << QStringLiteral("    # 2D データセットをそのまま表示")
          << QStringLiteral("    img = np.asarray(f[%1], dtype=np.float64)")
                 .arg(pyQuote(flatPath))
          << QStringLiteral("    title = %1").arg(pyQuote(flatPath));
    } else {
        hasPlot = false;
        c << QString()
          << QStringLiteral("    # 表示に適した 2D / 時系列データセットが"
                            "見つからないため列挙のみ");
    }

    if (hasPlot) {
        c << QString()
          << QStringLiteral("plt.figure(figsize=(6, 5))")
          << QStringLiteral("plt.imshow(img, origin=\"lower\", cmap=\"jet\")")
          << QStringLiteral("plt.colorbar()")
          << QStringLiteral("plt.title(title)")
          << QStringLiteral("plt.xlabel(%1)").arg(pyQuote(xlab))
          << QStringLiteral("plt.ylabel(%1)").arg(pyQuote(ylab))
          << QStringLiteral("plt.tight_layout()")
          << QStringLiteral("plt.show()");
    }
    return c;
}

// コード行 → ipynb の "source" 配列 (各行に改行を付ける。最終行は付けない)
QJsonArray ipynbSource(const QStringList &lines)
{
    QJsonArray arr;
    for (int i = 0; i < lines.size(); ++i)
        arr.append(i + 1 < lines.size() ? lines[i] + QLatin1Char('\n')
                                        : lines[i]);
    return arr;
}
} // namespace

// ── FieldCanvas ─────────────────────────────────────────────────────────────
FieldCanvas::FieldCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 320);
}

// mock の colormap_fn を転記。v は m_lo..m_hi で 0..1 へ正規化して着色する。
QColor FieldCanvas::mapColor(double v) const
{
    // レンジが潰れている (全セル同値) ときは中央色で塗る
    double t = (m_hi - m_lo > 0.0) ? (v - m_lo) / (m_hi - m_lo) : 0.5;
    t = std::max(0.0, std::min(1.0, t));
    auto c255 = [](double x) {
        return int(std::lround(255.0 * std::max(0.0, std::min(1.0, x))));
    };
    switch (m_cmap) {
    case 0:   // jet
        return QColor(c255(1.5 - std::fabs(4.0 * t - 3.0)),
                      c255(1.5 - std::fabs(4.0 * t - 2.0)),
                      c255(1.5 - std::fabs(4.0 * t - 1.0)));
    case 1:   // viridis (近似)
        return QColor(c255(0.27 + 0.65 * t),
                      c255(0.0 + 0.9 * t),
                      c255(0.33 + 0.4 * (1.0 - t)));
    case 2:   // seismic: blue-white-red (diverging)
        if (t < 0.5) {
            const int k = c255(t * 2.0);
            return QColor(k, k, 255);
        } else {
            const int k = c255((1.0 - t) * 2.0);
            return QColor(255, k, k);
        }
    default: {  // grayscale
        const int g = c255(t);
        return QColor(g, g, g);
    }
    }
}

void FieldCanvas::setData(const QVector<double> &d, int rows, int cols)
{
    m_data = d;
    m_rows = rows;
    m_cols = cols;
    m_msg.clear();
    rebuildImage();
    update();
}

void FieldCanvas::setMessage(const QString &msg)
{
    m_data.clear();
    m_rows = m_cols = 0;
    m_img = QImage();
    m_msg = msg;
    update();
}

void FieldCanvas::setColormap(int c)
{
    m_cmap = c;
    rebuildImage();
    update();
}

void FieldCanvas::setScale(double lo, double hi)
{
    m_lo = lo;
    m_hi = hi;
    rebuildImage();
    update();
}

// 行列 → 1 セル 1 ピクセルの QImage (paintEvent でウィジェット全面へ拡大)
void FieldCanvas::rebuildImage()
{
    if (m_rows <= 0 || m_cols <= 0
            || m_data.size() < qsizetype(m_rows) * m_cols) {
        m_img = QImage();
        return;
    }
    m_img = QImage(m_cols, m_rows, QImage::Format_RGB32);
    for (int r = 0; r < m_rows; ++r) {
        QRgb *line = reinterpret_cast<QRgb *>(m_img.scanLine(r));
        const double *src = m_data.constData() + qsizetype(r) * m_cols;
        for (int c = 0; c < m_cols; ++c)
            line[c] = mapColor(src[c]).rgb();
    }
}

QImage FieldCanvas::renderImage(const QVector<double> &d, int rows, int cols,
                                int cellPx, double lo, double hi)
{
    if (rows <= 0 || cols <= 0 || d.size() < qsizetype(rows) * cols)
        return QImage();
    // mapColor は m_lo/m_hi で正規化するため一時的に差し替える
    const double saveLo = m_lo, saveHi = m_hi;
    m_lo = lo;
    m_hi = (hi > lo) ? hi : lo + 1.0;
    QImage img(cols, rows, QImage::Format_RGB32);
    for (int r = 0; r < rows; ++r) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(r));
        const double *src = d.constData() + qsizetype(r) * cols;
        for (int c = 0; c < cols; ++c)
            line[c] = mapColor(src[c]).rgb();
    }
    m_lo = saveLo;
    m_hi = saveHi;
    cellPx = qMax(1, cellPx);
    return img.scaled(cols * cellPx, rows * cellPx, Qt::IgnoreAspectRatio,
                      Qt::FastTransformation);
}

void FieldCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (m_img.isNull()) {
        // データ未読込 / 表示未対応 — 中央にメッセージ
        p.setPen(QColor("#9AA7B4"));
        QFont f = p.font();
        f.setPixelSize(12);
        p.setFont(f);
        const QString msg = m_msg.isEmpty() ? I18n::tr("h5_no_data") : m_msg;
        p.drawText(rect().adjusted(16, 16, -16, -16),
                   Qt::AlignCenter | Qt::TextWordWrap, msg);
        p.setPen(QPen(palette().mid().color(), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        return;
    }

    // 実データ行列を全面へ拡大描画 (row 0 が上端)
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(rect(), m_img);

    if (m_grid) {
        QPen pen(QColor(255, 255, 255, 38));   // rgba(255,255,255,0.15)
        pen.setWidthF(1.0);
        p.setPen(pen);
        for (int g = 1; g < 4; ++g) {          // 1/4 刻みの補助線
            const double x = width() * g / 4.0;
            const double y = height() * g / 4.0;
            p.drawLine(QPointF(x, 0), QPointF(x, height()));
            p.drawLine(QPointF(0, y), QPointF(width(), y));
        }
    }
    if (m_axes) {
        // 軸は行列 index (列 0..cols-1 を下辺、行 0..rows-1 を左辺) で表示
        p.setPen(QColor(255, 255, 255, 150));
        QFont f = p.font();
        f.setPixelSize(10);
        p.setFont(f);
        const QFontMetrics fm(p.font());
        const QString cmax = QString::number(m_cols - 1);
        const QString rmax = QString::number(m_rows - 1);
        p.drawText(QPointF(4, height() - 4), "0");
        p.drawText(QPointF(width() - fm.horizontalAdvance(cmax) - 4,
                           height() - 4), cmax);
        p.drawText(QPointF(4, fm.ascent() + 2), "0");
        p.drawText(QPointF(4, height() - fm.height() - 6), rmax);
    }

    // 左上オーバーレイ: データセット名
    if (!m_name.isEmpty()) {
        QFont f = p.font();
        f.setPixelSize(10);
        p.setFont(f);
        const QFontMetrics fm(p.font());
        const int tw = fm.horizontalAdvance(m_name);
        p.fillRect(QRectF(8, 8, tw + 12, fm.height() + 6), QColor(0, 0, 0, 102));
        p.setPen(Qt::white);
        p.drawText(QPointF(14, 8 + fm.ascent() + 3), m_name);
    }

    // 枠線
    p.setPen(QPen(palette().mid().color(), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

// ── ColorBar ────────────────────────────────────────────────────────────────
ColorBar::ColorBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(18);
    setMinimumHeight(120);
}

void ColorBar::paintEvent(QPaintEvent *)
{
    // mock の linear-gradient(to top, …) を転記
    static const char *kStops[4][6] = {
        { "#000080", "#0000ff", "#00ffff", "#ffff00", "#ff0000", "#800000" },
        { "#440154", "#3b528b", "#21918c", "#5ec962", "#fde725", nullptr },
        { "#0000ff", "#ffffff", "#ff0000", nullptr,   nullptr,   nullptr },
        { "#000000", "#ffffff", nullptr,   nullptr,   nullptr,   nullptr },
    };
    const int cmap = qBound(0, m_cmap, 3);
    int n = 0;
    while (n < 6 && kStops[cmap][n]) ++n;

    QPainter p(this);
    QLinearGradient grad(0, height(), 0, 0);   // 下→上
    for (int i = 0; i < n; ++i)
        grad.setColorAt(n <= 1 ? 0.0 : double(i) / (n - 1),
                        QColor(kStops[cmap][i]));
    p.fillRect(rect(), grad);
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

// ── H5ViewerTab ─────────────────────────────────────────────────────────────
H5ViewerTab::H5ViewerTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // HDF5 ファイル / File
    auto *sf = new SectionBox(I18n::tr("h5_file_section"), body);
    auto *frow = new QHBoxLayout();
    m_file = new QLineEdit(sf);
    m_file->setPlaceholderText(I18n::tr("h5_file_ph"));
    frow->addWidget(m_file, 1);
    m_browseBtn = new QPushButton(I18n::tr("h5_browse"), sf);
    connect(m_browseBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("h5_file_section"), {},
            "HDF5 (*.h5 *.hdf5);;All files (*)");
        if (path.isEmpty()) return;
        m_file->setText(path);
        loadFile();
    });
    frow->addWidget(m_browseBtn);
    m_reloadBtn = new QPushButton(I18n::tr("h5_reload"), sf);
    connect(m_reloadBtn, &QPushButton::clicked, this, [this] { loadFile(); });
    frow->addWidget(m_reloadBtn);
    sf->form()->addRow(I18n::tr("h5_file"), frow);
    connect(m_file, &QLineEdit::returnPressed, this, [this] { loadFile(); });
    auto *fmts = new QHBoxLayout();
    fmts->addWidget(makeBadge(".h5", sf));
    fmts->addWidget(makeBadge(".hdf5", sf));
    fmts->addStretch(1);
    sf->form()->addRow(I18n::tr("h5_formats"), fmts);
    auto *fhint = new QLabel(I18n::tr("h5_file_hint"), sf);
    fhint->setWordWrap(true);
    sf->vbox()->addWidget(fhint);
    v->addWidget(sf);

    // データセット / Dataset tree (実ファイルの列挙結果)
    auto *st = new SectionBox(I18n::tr("h5_tree_section"), body);
    m_tree = new QTreeWidget(st);
    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(14);
    m_tree->setMaximumHeight(240);
    m_tree->setMinimumHeight(240);
    st->vbox()->addWidget(m_tree);
    m_selected = new QLabel(I18n::tr("h5_selected") + " -", st);
    st->vbox()->addWidget(m_selected);
    v->addWidget(st);

    // 可視化 / Visualization
    auto *sv = new SectionBox(I18n::tr("h5_vis_section"), body);
    m_cmap = new QComboBox(sv);
    m_cmap->addItems({ "Jet", "Viridis", "Seismic", "Gray" });
    sv->form()->addRow(I18n::tr("h5_colormap"), m_cmap);
    auto *scaleRow = new QHBoxLayout();
    m_autoScale = new QCheckBox(I18n::tr("h5_auto"), sv);
    m_autoScale->setChecked(true);
    m_scaleMin = new QLineEdit("-1", sv);
    m_scaleMin->setMaximumWidth(80);
    m_scaleMin->setEnabled(false);
    m_scaleMax = new QLineEdit("1", sv);
    m_scaleMax->setMaximumWidth(80);
    m_scaleMax->setEnabled(false);
    scaleRow->addWidget(m_autoScale);
    scaleRow->addWidget(m_scaleMin);
    scaleRow->addWidget(new QLabel(QString::fromUtf8("〜"), sv));
    scaleRow->addWidget(m_scaleMax);
    scaleRow->addStretch(1);
    sv->form()->addRow(I18n::tr("h5_scale"), scaleRow);
    auto *checks = new QHBoxLayout();
    auto *ckGrid = new QCheckBox(I18n::tr("h5_show_grid"), sv);
    auto *ckAxes = new QCheckBox(I18n::tr("h5_show_axes"), sv);
    ckAxes->setChecked(true);
    checks->addWidget(ckGrid);
    checks->addWidget(ckAxes);
    // オーバーレイ 2 種はどこにも配線されていない → 未実装として無効化
    auto *ckOvGeom = new QCheckBox(I18n::tr("h5_overlay_geom"), sv);
    auto *ckOvMon  = new QCheckBox(I18n::tr("h5_overlay_mon"), sv);
    ofd::tabhelp::markNotImplemented(ckOvGeom, I18n::tr(tabhelp::notimpl::kPlot));
    ofd::tabhelp::markNotImplemented(ckOvMon, I18n::tr(tabhelp::notimpl::kPlot));
    checks->addWidget(ckOvGeom);
    checks->addWidget(ckOvMon);
    checks->addStretch(1);
    sv->form()->addRow(checks);
    v->addWidget(sv);

    // プレビュー / Preview
    m_previewBox = new SectionBox(I18n::tr("h5_preview"), body);
    auto *ph = new QHBoxLayout();
    m_canvas = new FieldCanvas(m_previewBox);
    ph->addWidget(m_canvas, 1);
    // 3 面ビュー用のキャンバス群 (XY / XZ / YZ を横並び)。既定は非表示で、
    // チェック ON のときだけ単一断面キャンバスと入れ替える
    m_multiWrap = new QWidget(m_previewBox);
    auto *mh = new QHBoxLayout(m_multiWrap);
    mh->setContentsMargins(0, 0, 0, 0);
    mh->setSpacing(8);
    for (int p = 0; p < 3; ++p) {
        auto *col = new QVBoxLayout();
        m_multiCaption[p] = new QLabel(QStringLiteral("-"), m_multiWrap);
        m_multiCaption[p]->setWordWrap(true);
        m_multiCaption[p]->setStyleSheet("font-size:11px;");
        m_multiCanvas[p] = new FieldCanvas(m_multiWrap);
        m_multiCanvas[p]->setMinimumSize(140, 140);
        col->addWidget(m_multiCaption[p]);
        col->addWidget(m_multiCanvas[p], 1);
        mh->addLayout(col, 1);
    }
    m_multiWrap->setVisible(false);
    ph->addWidget(m_multiWrap, 3);
    auto *barCol = new QVBoxLayout();
    m_barMax = new QLabel("-", m_previewBox);
    m_barMin = new QLabel("-", m_previewBox);
    m_bar = new ColorBar(m_previewBox);
    barCol->addWidget(m_barMax, 0, Qt::AlignHCenter);
    barCol->addWidget(m_bar, 1, Qt::AlignHCenter);
    barCol->addWidget(m_barMin, 0, Qt::AlignHCenter);
    ph->addLayout(barCol);
    m_previewBox->vbox()->addLayout(ph);
    v->addWidget(m_previewBox);

    // 再生コントロール / Playback (3D データセット選択時のみ有効)
    auto *sp = new SectionBox(I18n::tr("h5_playback"), body);
    auto *prow = new QHBoxLayout();
    m_playBtn = new QPushButton(I18n::tr("h5_play"), sp);
    m_playBtn->setMinimumWidth(64);
    m_firstBtn = new QPushButton(I18n::tr("h5_first"), sp);
    m_prevBtn  = new QPushButton(I18n::tr("h5_prev"), sp);
    m_nextBtn  = new QPushButton(I18n::tr("h5_next"), sp);
    m_lastBtn  = new QPushButton(I18n::tr("h5_last"), sp);
    prow->addWidget(m_playBtn);
    prow->addWidget(m_firstBtn);
    prow->addWidget(m_prevBtn);
    prow->addWidget(m_nextBtn);
    prow->addWidget(m_lastBtn);
    // ループ切替 (checkable)。既定 ON = 従来どおりのループ再生、
    // OFF は末尾フレーム到達で再生を停止する (タイマー timeout 参照)
    m_loopBtn = new QPushButton(I18n::tr("h5_loop"), sp);
    m_loopBtn->setCheckable(true);
    m_loopBtn->setChecked(true);
    m_loopBtn->setToolTip(I18n::tr("h5_loop_tip"));
    prow->addWidget(m_loopBtn);
    prow->addStretch(1);
    sp->vbox()->addLayout(prow);
    auto *fr = new QHBoxLayout();
    m_frameSlider = new QSlider(Qt::Horizontal, sp);
    m_frameSlider->setRange(0, 0);
    m_frameLabel = new QLabel("- / -", sp);
    m_frameLabel->setMinimumWidth(80);
    m_frameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    fr->addWidget(m_frameSlider, 1);
    fr->addWidget(m_frameLabel);
    sp->form()->addRow(I18n::tr("h5_frame"), fr);
    m_speed = new QComboBox(sp);
    m_speed->addItems({ "0.25x", "0.5x", "1x", "2x", "5x" });
    m_speed->setCurrentIndex(2);
    sp->form()->addRow(I18n::tr("h5_speed"), m_speed);
    auto *tr0 = new QHBoxLayout();
    m_rangeLo = new QLineEdit("0", sp);
    m_rangeLo->setMaximumWidth(60);
    m_rangeHi = new QLineEdit("1000", sp);
    m_rangeHi->setMaximumWidth(60);
    tr0->addWidget(m_rangeLo);
    tr0->addWidget(new QLabel(QString::fromUtf8("〜"), sp));
    tr0->addWidget(m_rangeHi);
    // 単位はドメイン別 (EM/光: ps、室内音響: ms、水中: s) — updateDomainVisibility
    m_timeUnit = new QLabel("ps", sp);
    tr0->addWidget(m_timeUnit);
    m_rangeOnly = new QCheckBox(I18n::tr("h5_range_only"), sp);
    tr0->addWidget(m_rangeOnly);
    tr0->addStretch(1);
    sp->form()->addRow(I18n::tr("h5_time_range"), tr0);
    // 絞り込みの結果 (対象フレーム / 使えない理由) を必ず出す
    m_rangeNote = new QLabel(sp);
    m_rangeNote->setWordWrap(true);
    m_rangeNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sp->form()->addRow(m_rangeNote);
    for (QLineEdit *e : { m_rangeLo, m_rangeHi })
        connect(e, &QLineEdit::editingFinished, this,
                &H5ViewerTab::applyTimeRange);
    connect(m_rangeOnly, &QCheckBox::toggled, this,
            &H5ViewerTab::applyTimeRange);
    v->addWidget(sp);

    // 時間断面 / Cross-sections (XY/XZ/YZ) — 伝搬時系列の表示断面を選ぶ
    auto *sx = new SectionBox(I18n::tr("h5_xsec_section"), body);
    auto *xrow = new QHBoxLayout();
    m_planeBox = new QComboBox(sx);
    m_planeBox->addItem(I18n::tr("h5_sec_xy"));
    m_planeBox->addItem(I18n::tr("h5_sec_xz"));
    m_planeBox->addItem(I18n::tr("h5_sec_yz"));
    xrow->addWidget(m_planeBox);
    xrow->addWidget(new QLabel(I18n::tr("h5_sec_pos"), sx));
    m_secSlider = new QSlider(Qt::Horizontal, sx);
    m_secSlider->setRange(0, 30);
    m_secSlider->setValue(15);
    xrow->addWidget(m_secSlider, 1);
    m_secValue = new QLabel("15", sx);
    xrow->addWidget(m_secValue);
    sx->vbox()->addLayout(xrow);
    // 3 面ビュー — 伝搬時系列 (3 次元) のときだけ有効 (updateSliceControls)
    m_multiChk = new QCheckBox(I18n::tr("h5_sec_multi"), sx);
    m_multiChk->setEnabled(false);
    m_multiChk->setToolTip(I18n::tr("h5_multi_tip_off"));
    sx->vbox()->addWidget(m_multiChk);
    // 3D シーンへの重ね描き — 断面の座標 (/metadata/Xn|Yn|Zn) が分かる
    // 伝搬時系列のときだけ有効 (updateSliceControls)
    m_sceneChk = new QCheckBox(I18n::tr("h5_scene_chk"), sx);
    m_sceneChk->setEnabled(false);
    m_sceneChk->setToolTip(I18n::tr("h5_scene_tip_off"));
    sx->vbox()->addWidget(m_sceneChk);
    connect(m_sceneChk, &QCheckBox::toggled, this, [this](bool on) {
        if (on) pushSceneSlice();
        else    emit sceneSliceCleared();
    });
    m_sceneNote = new QLabel(I18n::tr("h5_scene_note"), sx);
    m_sceneNote->setWordWrap(true);
    m_sceneNote->setStyleSheet("color:palette(mid); font-size:11px;");
    sx->vbox()->addWidget(m_sceneNote);
    m_secNote = new QLabel(I18n::tr("h5_sec_note_off"), sx);
    m_secNote->setWordWrap(true);
    m_secNote->setStyleSheet("color:palette(mid); font-size:11px;");
    sx->vbox()->addWidget(m_secNote);
    v->addWidget(sx);

    // エクスポート / Export — PNG/CSV は表示中データ、連番/動画は全フレーム。
    // 動画 (MP4/GIF) は外部 ffmpeg を PATH から起動する
    auto *se = new SectionBox(I18n::tr("h5_export"), body);
    auto *erow = new QHBoxLayout();
    m_expMp4 = new QPushButton(I18n::tr("h5_exp_mp4"), se);
    m_expGif = new QPushButton(I18n::tr("h5_exp_gif"), se);
    m_expPng = new QPushButton(I18n::tr("h5_exp_png"), se);
    m_expPngSeq = new QPushButton(I18n::tr("h5_exp_pngseq"), se);
    m_expCsv = new QPushButton(I18n::tr("h5_exp_csv"), se);
    for (QPushButton *b : { m_expMp4, m_expGif, m_expPng, m_expPngSeq,
                            m_expCsv })
        erow->addWidget(b);
    erow->addStretch(1);
    se->vbox()->addLayout(erow);
    // 3 面ビュー時に「何が書き出されるか」を明示する注記 (既定は非表示)
    m_expMultiNote = new QLabel(se);
    m_expMultiNote->setWordWrap(true);
    m_expMultiNote->setStyleSheet("font-size:11px; color:palette(mid);");
    m_expMultiNote->setVisible(false);
    se->vbox()->addWidget(m_expMultiNote);
    m_expStatus = new QLabel(se);
    m_expStatus->setWordWrap(true);
    se->vbox()->addWidget(m_expStatus);
    connect(m_expPng, &QPushButton::clicked, this,
            &H5ViewerTab::exportPngCurrent);
    connect(m_expCsv, &QPushButton::clicked, this,
            &H5ViewerTab::exportCsvCurrent);
    connect(m_expPngSeq, &QPushButton::clicked, this,
            [this] { exportFrames(false, QString()); });
    connect(m_expMp4, &QPushButton::clicked, this,
            [this] { exportFrames(true, QStringLiteral("mp4")); });
    connect(m_expGif, &QPushButton::clicked, this,
            [this] { exportFrames(true, QStringLiteral("gif")); });
    auto *mrow = new QHBoxLayout();
    mrow->addWidget(new QLabel("FPS", se));
    m_movieFps = new QLineEdit("30", se);
    m_movieFps->setMaximumWidth(60);
    mrow->addWidget(m_movieFps);
    mrow->addWidget(new QLabel(I18n::tr("h5_resolution"), se));
    m_movieRes = new QComboBox(se);
    // 先頭は「元の大きさのまま」(拡大でぼかさない) — 既定にする
    m_movieRes->addItem(I18n::tr("h5_res_native"));
    m_movieRes->addItems({ QString::fromUtf8("1920 × 1080"),
                           QString::fromUtf8("3840 × 2160 (4K)"),
                           QString::fromUtf8("1280 × 720") });
    mrow->addWidget(m_movieRes);
    mrow->addWidget(new QLabel(I18n::tr("h5_codec"), se));
    m_movieCodec = new QComboBox(se);
    m_movieCodec->addItems({ "H.264", "H.265", "VP9" });
    mrow->addWidget(m_movieCodec);
    mrow->addStretch(1);
    se->form()->addRow(I18n::tr("h5_movie"), mrow);
    auto *echecks = new QHBoxLayout();
    // 図中への凡例・形状の焼き込みはレンダリング側の未実装 (ffmpeg では
    // どうにもならない) — 明示して無効化する
    auto *ckEmbed = new QCheckBox(I18n::tr("h5_embed_bar"), se);
    auto *ckGeom = new QCheckBox(I18n::tr("h5_embed_geom"), se);
    ofd::tabhelp::markNotImplemented(ckEmbed, I18n::tr(tabhelp::notimpl::kPlot));
    ofd::tabhelp::markNotImplemented(ckGeom, I18n::tr(tabhelp::notimpl::kPlot));
    echecks->addWidget(ckEmbed);
    echecks->addWidget(ckGeom);
    echecks->addStretch(1);
    se->form()->addRow(echecks);
    auto *movieNote = new QLabel(I18n::tr("h5_movie_note"), se);
    movieNote->setWordWrap(true);
    movieNote->setStyleSheet("font-size:11px; color:palette(mid);");
    se->vbox()->addWidget(movieNote);
    v->addWidget(se);

    // 統計・派生量 / Statistics & derived — 表示中フレームの実計算値
    auto *ss = new SectionBox(I18n::tr("h5_stats_section"), body);
    auto *brow = new QHBoxLayout();
    m_statMin  = makeBadge("min: -", ss);
    m_statMax  = makeBadge("max: -", ss, "ok");
    m_statMean = makeBadge(I18n::tr("h5_stat_mean") + ": -", ss, "acc");
    brow->addWidget(m_statMin);
    brow->addWidget(m_statMax);
    brow->addWidget(m_statMean);
    brow->addStretch(1);
    ss->vbox()->addLayout(brow);
    auto *srow = new QHBoxLayout();
    for (const char *key : { "h5_stat_series", "h5_stat_schroeder",
                             "h5_stat_fft", "h5_stat_lineint" }) {
        auto *b = new QPushButton(I18n::tr(QLatin1String(key)), ss);
        ofd::tabhelp::markNotImplemented(b, I18n::tr(tabhelp::notimpl::kExternal));
        // Schroeder 減衰は室内音響の指標 — ドメイン別に表示を切り替える
        if (qstrcmp(key, "h5_stat_schroeder") == 0)
            m_schroederBtn = b;
        srow->addWidget(b);
    }
    srow->addStretch(1);
    ss->vbox()->addLayout(srow);
    v->addWidget(ss);

    // 連携 / Integration
    auto *sg = new SectionBox(I18n::tr("h5_integration"), body);
    auto *grow = new QHBoxLayout();
    // Python / Jupyter は開いている .h5 の実スキーマから h5py 読み込み
    // コードを生成して保存する (外部ツールの起動はしない)。
    // ParaView / Matlab 変換は未実装のまま → 無効化
    auto *pyBtn  = new QPushButton(I18n::tr("h5_int_python"), sg);
    auto *jupBtn = new QPushButton(I18n::tr("h5_int_jupyter"), sg);
    auto *pvBtn  = new QPushButton(I18n::tr("h5_int_paraview"), sg);
    auto *mlBtn  = new QPushButton(I18n::tr("h5_int_matlab"), sg);
    for (QPushButton *b : { pvBtn, mlBtn })
        ofd::tabhelp::markNotImplemented(b, I18n::tr(tabhelp::notimpl::kExternal));
    connect(pyBtn, &QPushButton::clicked, this,
            [this] { exportPythonScript(false); });
    connect(jupBtn, &QPushButton::clicked, this,
            [this] { exportPythonScript(true); });
    grow->addWidget(pyBtn);
    grow->addWidget(jupBtn);
    grow->addWidget(pvBtn);
    grow->addWidget(mlBtn);
    grow->addStretch(1);
    sg->vbox()->addLayout(grow);
    auto *ghint = new QLabel(I18n::tr("h5_int_hint"), sg);
    ghint->setWordWrap(true);
    ghint->setStyleSheet("font-size:11px; color:palette(mid);");
    sg->vbox()->addWidget(ghint);
    v->addWidget(sg);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // ── 接続 ──
    m_timer = new QTimer(this);
    m_timer->setInterval(int(1000.0 / (30.0 * kSpeeds[2])));
    connect(m_timer, &QTimer::timeout, this, [this] {
        if (m_nframes <= 1) return;
        const bool loop = m_loopBtn->isChecked();
        // 時間範囲で絞り込んでいるときはその区間だけを再生する
        const int first = qBound(0, m_playFirst, m_nframes - 1);
        const int last = (m_playLast < 0) ? m_nframes - 1
                                          : qBound(first, m_playLast,
                                                   m_nframes - 1);
        int next = m_frame + 1;
        if (m_frame < first) next = first;
        if (next > last) {
            if (!loop) {
                // 念のため (末尾到達時に停止済みのはずだが二重に守る)
                m_timer->stop();
                m_playBtn->setText(I18n::tr("h5_play"));
                return;
            }
            next = first;                             // ループ再生
        }
        setFrame(next);
        if (!loop && next == last) {
            // ループ OFF: 末尾フレーム到達で停止 (ボタン表示も再生へ戻す)
            m_timer->stop();
            m_playBtn->setText(I18n::tr("h5_play"));
        }
    });
    connect(m_playBtn, &QPushButton::clicked, this, [this] {
        if (m_timer->isActive()) {
            m_timer->stop();
            m_playBtn->setText(I18n::tr("h5_play"));
        } else if (m_nframes > 1) {
            const int first = qBound(0, m_playFirst, m_nframes - 1);
            const int last = (m_playLast < 0) ? m_nframes - 1
                                              : qBound(first, m_playLast,
                                                       m_nframes - 1);
            // ループ OFF で末尾に居るときは (範囲の) 先頭から再生し直す
            if (!m_loopBtn->isChecked() && m_frame >= last) setFrame(first);
            else if (m_frame < first) setFrame(first);
            m_timer->start();
            m_playBtn->setText(I18n::tr("h5_pause"));
        }
    });
    connect(m_firstBtn, &QPushButton::clicked, this, [this] { setFrame(0); });
    connect(m_prevBtn, &QPushButton::clicked, this, [this] {
        setFrame(m_frame - 1);
    });
    connect(m_nextBtn, &QPushButton::clicked, this, [this] {
        setFrame(m_frame + 1);
    });
    connect(m_lastBtn, &QPushButton::clicked, this, [this] {
        setFrame(m_nframes - 1);
    });
    connect(m_frameSlider, &QSlider::valueChanged, this, [this](int f) {
        setFrame(f);
    });
    connect(m_speed, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_timer->setInterval(int(1000.0 / (30.0 * kSpeeds[qBound(0, i, 4)])));
    });
    connect(m_cmap, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_canvas->setColormap(i);
        for (FieldCanvas *c : m_multiCanvas) c->setColormap(i);
        m_bar->setColormap(i);
    });
    connect(m_autoScale, &QCheckBox::toggled, this, [this](bool on) {
        m_scaleMin->setEnabled(!on);
        m_scaleMax->setEnabled(!on);
        if (multiActive()) {
            loadMultiFrames();       // 3 面共通スケールを取り直す
        } else if (on) {
            // 自動へ戻したら現在データの min/max へ再スケール
            if (!m_data.isEmpty())
                showData(m_data, m_rows, m_cols);
        } else {
            applyScale();
        }
    });
    connect(m_scaleMin, &QLineEdit::editingFinished, this, [this] { applyScale(); });
    connect(m_scaleMax, &QLineEdit::editingFinished, this, [this] { applyScale(); });
    connect(ckGrid, &QCheckBox::toggled, this, [this](bool on) {
        m_canvas->setShowGrid(on);
        for (FieldCanvas *c : m_multiCanvas) c->setShowGrid(on);
    });
    connect(ckAxes, &QCheckBox::toggled, this, [this](bool on) {
        m_canvas->setShowAxes(on);
        for (FieldCanvas *c : m_multiCanvas) c->setShowAxes(on);
    });
    connect(m_secSlider, &QSlider::valueChanged, this, [this](int val) {
        m_secValue->setText(QStringLiteral("%1 / %2")
                                .arg(val).arg(m_secSlider->maximum()));
        // 位置スライダは主断面 (planeBox の選択) の軸を編集する
        if (m_seriesMode) {
            m_secPos[sliceAxis()] = val;
            loadCurrentFrame();
        }
    });
    connect(m_planeBox, &QComboBox::currentIndexChanged, this, [this](int) {
        updateSliceControls();
        if (m_seriesMode) loadCurrentFrame();
    });
    connect(m_multiChk, &QCheckBox::toggled, this, [this](bool) {
        updateMultiVisibility();
        if (m_seriesMode) loadCurrentFrame();
    });
    connect(m_tree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem *it, int) {
        const QVariant idx = it->data(0, Qt::UserRole);
        if (!idx.isValid()) return;     // グループノードは選択対象外
        selectDataset(idx.toInt());
    });

    setPlaybackEnabled(false);

    // ドメイン別の出し分け (時間範囲の単位ラベル / Schroeder 減衰ボタン)
    connect(m_p, &Project::domainChanged, this,
            &H5ViewerTab::updateDomainVisibility);
    connect(m_p, &Project::loaded, this,
            &H5ViewerTab::updateDomainVisibility);
    updateDomainVisibility();

    // HDF5 無効ビルド: 注記をタブ先頭に出し、読込 UI を無効化する
    if (!H5Reader::available()) {
        auto *note = new QLabel(I18n::tr("h5_disabled"), body);
        note->setWordWrap(true);
        note->setStyleSheet("padding:6px 8px; border:1px solid #C9A227; "
                            "border-radius:3px; color:#C9A227;");
        v->insertWidget(0, note);
        m_file->setEnabled(false);
        m_browseBtn->setEnabled(false);
        m_reloadBtn->setEnabled(false);
        m_tree->setEnabled(false);
        m_canvas->setMessage(I18n::tr("h5_disabled"));
        for (FieldCanvas *c : m_multiCanvas)
            c->setMessage(I18n::tr("h5_disabled"));
    }
}

// ファイルを列挙してツリーを再構築し、最初の表示可能データセットを選択する
void H5ViewerTab::loadFile()
{
    if (!H5Reader::available()) return;
    const QString path = m_file->text().trimmed();
    if (path.isEmpty()) return;

    m_timer->stop();
    m_playBtn->setText(I18n::tr("h5_play"));
    m_dsets.clear();
    m_dataset.clear();
    m_data.clear();
    m_rows = m_cols = 0;
    m_nframes = 0;
    m_seriesMode = false;
    m_filePath = path;

    QString err;
    if (!H5Reader::listDatasets(path, m_dsets, &err)) {
        m_tree->clear();
        m_selected->setText(I18n::tr("h5_selected") + " -");
        m_canvas->setDatasetName({});
        m_canvas->setMessage(I18n::tr("h5_load_error") + " " + err);
        updateSliceControls();      // 3 面ビューを解除して単一表示へ戻す
        clearStats();
        setPlaybackEnabled(false);
        m_previewBox->setTitle(I18n::tr("h5_preview"));
        return;
    }
    rebuildTree();

    // 伝搬時系列 → 最初の 2D/3D の順で自動選択 (無ければ未読込表示のまま)。
    // 新レイアウトの /timeseries/E が最優先。旧レイアウト (/data%06d/E) は
    // 4D なので 2D/3D の候補に入らず、そのままだと同じグループの
    // /data%06d/P (3D) が選ばれて再生も 3 面ビューも無効のままになる。
    int first = -1;
    for (int i = 0; i < m_dsets.size(); ++i) {
        if (m_dsets[i].path == QLatin1String("/timeseries/E")) {
            first = i;
            break;
        }
    }
    if (first < 0) {
        // 旧レイアウトの先頭フレームの E (selectDataset が全グループを
        // フレーム列として扱う)
        static const QRegularExpression oldSeriesRe(
            QStringLiteral("^/data\\d+/E$"));
        for (int i = 0; i < m_dsets.size(); ++i)
            if (oldSeriesRe.match(m_dsets[i].path).hasMatch()) {
                first = i;
                break;
            }
    }
    for (int i = 0; first < 0 && i < m_dsets.size(); ++i) {
        const int nd = m_dsets[i].dims.size();
        if (nd == 2 || nd == 3) { first = i; break; }
    }
    if (first >= 0) {
        selectDataset(first);
    } else {
        m_selected->setText(I18n::tr("h5_selected") + " -");
        m_canvas->setDatasetName({});
        m_canvas->setMessage({});
        clearStats();
        setPlaybackEnabled(false);
        updateSliceControls();      // 3 面ビューを解除して単一表示へ戻す
        m_previewBox->setTitle(I18n::tr("h5_preview"));
    }
}

// 列挙結果からパス階層のツリーを作る。リーフの UserRole = m_dsets の index。
void H5ViewerTab::rebuildTree()
{
    m_tree->clear();
    const QFileInfo fi(m_filePath);
    auto *root = new QTreeWidgetItem(m_tree);
    root->setText(0, QString::fromUtf8("📁 ") + fi.fileName());
    // ファイル情報行 = 実ファイルサイズ (QFileInfo)
    root->setText(1, QLocale().formattedDataSize(fi.size())
                         + QString::fromUtf8(" · HDF5"));
    root->setForeground(1, QColor("#888888"));

    QHash<QString, QTreeWidgetItem *> groups;   // "/field" などの中間ノード
    for (int i = 0; i < m_dsets.size(); ++i) {
        const H5DatasetInfo &ds = m_dsets[i];
        const QStringList parts = ds.path.split(QLatin1Char('/'),
                                                Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        QTreeWidgetItem *parent = root;
        QString acc;
        for (int k = 0; k < parts.size() - 1; ++k) {
            acc += QLatin1Char('/') + parts[k];
            auto it = groups.find(acc);
            if (it == groups.end()) {
                auto *g = new QTreeWidgetItem(parent);
                g->setText(0, QString::fromUtf8("📁 ") + parts[k]);
                it = groups.insert(acc, g);
            }
            parent = it.value();
        }
        auto *leaf = new QTreeWidgetItem(parent);
        leaf->setText(0, QString::fromUtf8("🗂 ") + parts.last());
        leaf->setText(1, ds.typeName + QString::fromUtf8(" · ")
                             + dimsText(ds.dims));
        leaf->setForeground(1, QColor("#888888"));
        leaf->setData(0, Qt::UserRole, i);
    }
    if (m_dsets.isEmpty()) {
        auto *empty = new QTreeWidgetItem(root);
        empty->setText(0, I18n::tr("h5_no_datasets"));
        empty->setForeground(0, QColor("#888888"));
    }
    m_tree->expandAll();
    m_tree->resizeColumnToContents(0);
}

// ツリー選択 → 2D は即描画、3D はスライダを構成して先頭フレーム、他は未対応表示
void H5ViewerTab::selectDataset(int idx)
{
    if (idx < 0 || idx >= m_dsets.size()) return;
    const H5DatasetInfo &ds = m_dsets[idx];

    m_timer->stop();
    m_playBtn->setText(I18n::tr("h5_play"));
    m_dataset = ds.path;
    m_selected->setText(I18n::tr("h5_selected") + " " + ds.path
                        + "  " + ds.typeName + " " + dimsText(ds.dims));
    m_canvas->setDatasetName(ds.path.section(QLatin1Char('/'), -1));

    // ツリー側の選択表示も同期 (自動選択経由で呼ばれたとき)
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        // UserRole 一致のリーフを探す
        QTreeWidgetItem *found = nullptr;
        std::function<void(QTreeWidgetItem *)> walk =
            [&](QTreeWidgetItem *it) {
            if (found) return;
            if (it->data(0, Qt::UserRole).isValid()
                    && it->data(0, Qt::UserRole).toInt() == idx) {
                found = it;
                return;
            }
            for (int c = 0; c < it->childCount(); ++c) walk(it->child(c));
        };
        walk(m_tree->topLevelItem(i));
        if (found) { m_tree->setCurrentItem(found); break; }
    }

    // ofd の伝搬時系列 (新 /timeseries/E|H, 旧 /data%06d/E|H) は
    // z 中央断面のフレーム列として再生する (io/H5Reader が再構成)
    static const QRegularExpression seriesRe(
        QStringLiteral("^(?:/timeseries|/data\\d+)/(E|H)$"));
    const QRegularExpressionMatch sm = seriesRe.match(ds.path);
    if (sm.hasMatch()) {
        H5OfdSeriesInfo info;
        if (H5Reader::ofdSeriesInfo(m_filePath, sm.captured(1), info)
            && info.frames > 0) {
            m_seriesMode = true;
            m_seriesComp = sm.captured(1);
            m_seriesInfo = info;
            m_nframes = info.frames;
            // 時間範囲での絞り込みに使う時刻列 (旧形式には無い)
            m_frameTimes.clear();
            H5Reader::readOfdSeriesTimes(m_filePath, m_seriesComp,
                                         m_frameTimes);
            if (m_frameTimes.size() != m_nframes) m_frameTimes.clear();
            m_playFirst = 0;
            m_playLast = m_nframes - 1;
            // 断面位置は各軸とも未設定 (= 中央) から始める
            m_secPos[0] = m_secPos[1] = m_secPos[2] = -1;
            loadSliceCoords();
            m_frameSlider->blockSignals(true);
            m_frameSlider->setRange(0, std::max(0, m_nframes - 1));
            m_frameSlider->blockSignals(false);
            setPlaybackEnabled(true);
            updateSliceControls();
            m_frame = 0;
            setFrame(0);
            applyTimeRange();
            return;
        }
    }
    m_seriesMode = false;
    m_frameTimes.clear();
    m_playFirst = 0;
    m_playLast = -1;
    updateSliceControls();
    applyTimeRange();

    const int nd = ds.dims.size();
    if (nd == 2) {
        // 2D → read2D して描画、時間スライダ無効
        m_nframes = 0;
        setPlaybackEnabled(false);
        m_frameLabel->setText("- / -");
        m_previewBox->setTitle(I18n::tr("h5_preview"));
        QVector<double> d;
        int rows = 0, cols = 0;
        QString err;
        if (!H5Reader::read2D(m_filePath, m_dataset, d, rows, cols, &err)) {
            m_canvas->setMessage(I18n::tr("h5_load_error") + " " + err);
            clearStats();
            return;
        }
        showData(d, rows, cols);
    } else if (nd == 3) {
        // 3D → スライダ範囲 0..nframes-1、先頭フレームを表示
        m_nframes = int(ds.dims[0]);
        m_frameSlider->blockSignals(true);
        m_frameSlider->setRange(0, std::max(0, m_nframes - 1));
        m_frameSlider->blockSignals(false);
        setPlaybackEnabled(true);
        m_frame = 0;
        setFrame(0);
    } else {
        // 1D / 4D+ → 表示未対応 (ofd の /data*/E は空間展開不能)
        m_nframes = 0;
        setPlaybackEnabled(false);
        m_frameLabel->setText("- / -");
        m_previewBox->setTitle(I18n::tr("h5_preview"));
        m_canvas->setMessage(I18n::tr("h5_unsupported"));
        clearStats();
    }
}

// 3D データセット / 伝搬時系列の現在フレームを読み込んで表示する
void H5ViewerTab::loadCurrentFrame()
{
    if (multiActive()) { loadMultiFrames(); return; }

    QVector<double> d;
    int rows = 0, cols = 0;
    QString err;
    if (m_seriesMode) {
        QString label;
        if (!H5Reader::readOfdSeriesFrame(m_filePath, m_seriesComp, m_frame,
                                          sliceAxis(), m_secPos[sliceAxis()],
                                          d, rows, cols, &label, &err)) {
            m_canvas->setMessage(I18n::tr("h5_load_error") + " " + err);
            clearStats();
            return;
        }
        m_seriesFrameLabel = label;
        m_previewBox->setTitle(I18n::tr("h5_series_title")
            .arg(m_seriesComp, m_planeBox->currentText(), label));
        showData(d, rows, cols);
        pushSceneSlice();       // 3D シーンへ (重ねる設定のときだけ流れる)
        return;
    }
    if (!H5Reader::readFrame(m_filePath, m_dataset, m_frame, d, rows, cols,
                             &err)) {
        m_canvas->setMessage(I18n::tr("h5_load_error") + " " + err);
        clearStats();
        return;
    }
    showData(d, rows, cols);
}

// 実行完了時などに外部からファイルを渡して読み込む (MainWindow から)
void H5ViewerTab::openFile(const QString &path)
{
    m_file->setText(path);
    loadFile();
}

// planeBox (XY/XZ/YZ) → 固定軸 (2=Z, 1=Y, 0=X)
int H5ViewerTab::sliceAxis() const
{
    switch (m_planeBox->currentIndex()) {
    case 1: return 1;      // XZ 面 = Y 固定
    case 2: return 0;      // YZ 面 = X 固定
    default: return 2;     // XY 面 = Z 固定
    }
}

// 断面 UI の範囲と有効状態 (伝搬時系列のときだけ効く)。
// 位置スライダは planeBox で選んだ面 (主断面) の軸の位置を編集する
void H5ViewerTab::updateSliceControls()
{
    const bool on = m_seriesMode;
    m_planeBox->setEnabled(on);
    m_secSlider->setEnabled(on);
    // 3 面ビューは直交 3 断面が定義できる伝搬時系列でのみ有効
    m_multiChk->setEnabled(on);
    m_multiChk->setToolTip(I18n::tr(on ? "h5_multi_tip_on"
                                       : "h5_multi_tip_off"));
    // 3D への重ね描きは、断面の座標が読めるときだけ (位置を推測しない)。
    // 座標は loadSliceCoords が /metadata/Xn|Yn|Zn から読む
    const bool hasCoords = on && !m_coord[0].isEmpty()
                        && !m_coord[1].isEmpty() && !m_coord[2].isEmpty();
    if (m_sceneChk) {
        m_sceneChk->setEnabled(hasCoords);
        m_sceneChk->setToolTip(I18n::tr(hasCoords ? "h5_scene_tip_on"
                                                  : "h5_scene_tip_off"));
        if (!hasCoords && m_sceneChk->isChecked()) {
            m_sceneChk->setChecked(false);   // toggled で 3D 側も消える
        }
    }
    if (m_sceneNote) {
        QString note = I18n::tr("h5_scene_note");
        // 自動スケールだとフレームごとに正規化されることを明示する
        if (hasCoords && m_autoScale && m_autoScale->isChecked())
            note += I18n::tr("h5_scene_auto");
        m_sceneNote->setText(note);
    }
    if (!on) {
        if (m_multiChk->isChecked()) m_multiChk->setChecked(false);
        m_secNote->setText(I18n::tr("h5_sec_note_off"));
        updateMultiVisibility();
        return;
    }
    const int n[3] = { m_seriesInfo.nx1, m_seriesInfo.ny1, m_seriesInfo.nz1 };
    const int axis = sliceAxis();
    const int maxIdx = std::max(0, n[axis] - 1);
    if (m_secPos[axis] < 0) m_secPos[axis] = maxIdx / 2;   // 既定は中央断面
    m_secPos[axis] = qBound(0, m_secPos[axis], maxIdx);
    m_secSlider->blockSignals(true);
    m_secSlider->setRange(0, maxIdx);
    m_secSlider->setValue(m_secPos[axis]);
    m_secSlider->blockSignals(false);
    m_secValue->setText(QStringLiteral("%1 / %2")
                            .arg(m_secPos[axis]).arg(maxIdx));
    updateMultiVisibility();
}

// 3 面ビュー表示中か (チェック ON かつ伝搬時系列)
bool H5ViewerTab::multiActive() const
{
    return m_seriesMode && m_multiChk && m_multiChk->isChecked();
}

// 単一断面キャンバス ⇄ 3 面ビューの表示切替と、注記類の更新
void H5ViewerTab::updateMultiVisibility()
{
    const bool multi = multiActive();
    m_canvas->setVisible(!multi);
    m_multiWrap->setVisible(multi);
    if (m_seriesMode)
        m_secNote->setText(I18n::tr(multi ? "h5_sec_note_multi"
                                          : "h5_sec_note_on"));
    // 主断面 (位置スライダの対象) の見出しを太字にする
    const int prim = qBound(0, m_planeBox->currentIndex(), 2);
    for (int p = 0; p < 3; ++p) {
        QFont f = m_multiCaption[p]->font();
        f.setBold(p == prim);
        m_multiCaption[p]->setFont(f);
    }
    updateExportNote();
}

// 3 面ビュー時に「何が書き出されるか」を明示する
void H5ViewerTab::updateExportNote()
{
    const bool multi = multiActive();
    m_expMultiNote->setVisible(multi);
    if (multi)
        m_expMultiNote->setText(I18n::tr("h5_multi_exp_note")
                                    .arg(m_planeBox->currentText()));
}

// /metadata/Xn|Yn|Zn (ノード座標 [m]) を読む。無いファイルでは空のまま
// (キャプションはノード番号だけになる — 座標を捏造しない)
void H5ViewerTab::loadSliceCoords()
{
    static const char *const kName[3] = { "Xn", "Yn", "Zn" };
    const int n[3] = { m_seriesInfo.nx1, m_seriesInfo.ny1, m_seriesInfo.nz1 };
    for (int a = 0; a < 3; ++a) {
        m_coord[a].clear();
        const QString p = QStringLiteral("/metadata/%1")
                              .arg(QLatin1String(kName[a]));
        bool exists = false;
        for (const H5DatasetInfo &ds : m_dsets)
            if (ds.path == p) { exists = true; break; }
        if (!exists) continue;
        QVector<double> v;
        QVector<qlonglong> dims;
        if (H5Reader::readAll(m_filePath, p, v, dims) && v.size() == n[a])
            m_coord[a] = v;
    }
}

// 表示中のフレームを 3D シーンへ流す。
//
// **座標が分かるときだけ流す。** /metadata/Xn|Yn|Zn が無いファイル
// (obpm の /field/Ixz など) は断面をどこへ置けばよいか決まらないので、
// 位置を推測せず何も送らない (CenterPane::applyResultSliceTo3D と同じ規則)。
// 行 0 = 面内 第2軸の + 側、列 = 第1軸という並びは H5Reader が返すものと
// Viewport3D が期待するものが一致しているので、そのまま渡す。
void H5ViewerTab::pushSceneSlice()
{
    if (!m_sceneChk || !m_sceneChk->isChecked()) return;
    // 伝搬時系列 (断面の軸と位置が決まる) 以外は対象にしない
    if (!m_seriesMode || m_data.isEmpty() || m_rows <= 0 || m_cols <= 0) {
        emit sceneSliceCleared();
        return;
    }
    const int axis = sliceAxis();
    // 面内 2 軸の対応は H5Reader が返す行列の規約そのもの。定義を borrow して
    // 二重管理にしない (片方だけ直すと 3D の断面が黙って転置する)
    int uAxis = 0, vAxis = 2;
    H5Reader::seriesSliceAxes(axis, &uAxis, &vAxis);
    // 3 軸ぶんの座標が要る (固定軸の位置と、面内 2 軸の範囲)
    if (m_coord[axis].isEmpty() || m_coord[uAxis].size() < 2
        || m_coord[vAxis].size() < 2) {
        emit sceneSliceCleared();
        return;
    }
    const int maxIdx = std::max(0, int(m_coord[axis].size()) - 1);
    const int idx = qBound(0, (m_secPos[axis] < 0) ? maxIdx / 2
                                                   : m_secPos[axis], maxIdx);

    H5SliceForScene sl;
    sl.cells = m_data;
    sl.rows = m_rows;
    sl.cols = m_cols;
    sl.axis = axis;
    sl.pos_m = m_coord[axis][idx];
    sl.u0 = m_coord[uAxis].first();
    sl.u1 = m_coord[uAxis].last();
    sl.v0 = m_coord[vAxis].first();
    sl.v1 = m_coord[vAxis].last();
    // 手動スケールならその上限を渡してフレーム間で明るさを揃える。
    // 自動スケールのときは 0 を渡し、2D と同じ「フレームごとの正規化」にする
    // (2 つの画面で別々の正規化をすると同じデータが違う強さに見える)。
    if (!m_autoScale->isChecked()) {
        bool ok = false;
        const double hi = m_scaleMax->text().trimmed().toDouble(&ok);
        if (ok && hi > 0.0) sl.scaleMax = hi;
    }
    // 凡例には「何の・いつの・どこの断面か」を出す (3D 側だけ見ても分かる)
    sl.label = I18n::tr("h5_scene_label")
                   .arg(m_seriesComp, m_planeBox->currentText(),
                        m_seriesFrameLabel,
                        QString::number(sl.pos_m, 'g', 4));
    emit sceneSliceReady(sl);
}

// 断面のキャプション: 面名 + 固定軸のノード番号 (座標があれば [m] も)
QString H5ViewerTab::sliceCaption(int axis) const
{
    axis = qBound(0, axis, 2);
    const int n[3] = { m_seriesInfo.nx1, m_seriesInfo.ny1, m_seriesInfo.nz1 };
    const int maxIdx = std::max(0, n[axis] - 1);
    const int idx = qBound(0, (m_secPos[axis] < 0) ? maxIdx / 2
                                                   : m_secPos[axis], maxIdx);
    const QString ax = QString::fromLatin1(kAxisName[axis]);
    QString s = QStringLiteral("%1  %2 = %3 / %4")
                    .arg(I18n::tr(QLatin1String(kPlaneKey[axis])), ax)
                    .arg(idx).arg(maxIdx);
    if (idx < m_coord[axis].size())
        s += QStringLiteral("  (%1 = %2 m)")
                 .arg(ax, QString::number(m_coord[axis][idx], 'g', 4));
    return s;
}

// XY / XZ / YZ の 3 断面を読み、3 面共通のカラースケールで同時に描画する。
// 統計 (min/max/平均) も 3 面の合成。CSV 出力用の m_data は主断面を保持する
void H5ViewerTab::loadMultiFrames()
{
    QVector<double> d[3];
    int rows[3] = { 0, 0, 0 }, cols[3] = { 0, 0, 0 };
    QString label, err;
    for (int p = 0; p < 3; ++p) {
        const int axis = kPlaneAxis[p];
        if (!H5Reader::readOfdSeriesFrame(m_filePath, m_seriesComp, m_frame,
                                          axis, m_secPos[axis], d[p], rows[p],
                                          cols[p], p == 0 ? &label : nullptr,
                                          &err)) {
            for (FieldCanvas *c : m_multiCanvas)
                c->setMessage(I18n::tr("h5_load_error") + " " + err);
            clearStats();
            return;
        }
    }

    // 3 面をまとめた min / max / 平均 (面ごとに正規化すると強度が比較できない)
    double lo = 0.0, hi = 0.0, sum = 0.0;
    qsizetype cnt = 0;
    bool first = true;
    for (int p = 0; p < 3; ++p) {
        for (const double v : d[p]) {
            if (first) { lo = hi = v; first = false; }
            lo = std::min(lo, v);
            hi = std::max(hi, v);
            sum += v;
            ++cnt;
        }
    }
    const double mean = cnt ? sum / double(cnt) : 0.0;
    m_statMin->setText(QString("min: %1").arg(lo, 0, 'g', 4));
    m_statMax->setText(QString("max: %1").arg(hi, 0, 'g', 4));
    m_statMean->setText(QString("%1: %2").arg(I18n::tr("h5_stat_mean"))
                            .arg(mean, 0, 'g', 4));

    // 表示スケール: 自動 = 合成 min/max、手動 = 入力値 (単一断面と同じ規則)
    double slo = lo, shi = hi;
    if (!m_autoScale->isChecked()) {
        const double mlo = m_scaleMin->text().toDouble();
        const double mhi = m_scaleMax->text().toDouble();
        if (mlo < mhi) { slo = mlo; shi = mhi; }
    }
    setScaleLabels(slo, shi);

    for (int p = 0; p < 3; ++p) {
        m_multiCanvas[p]->setDatasetName(
            QString::fromLatin1(kPlaneShort[p]));
        m_multiCanvas[p]->setScale(slo, shi);
        m_multiCanvas[p]->setData(d[p], rows[p], cols[p]);
        m_multiCaption[p]->setText(sliceCaption(kPlaneAxis[p]));
    }

    // CSV 出力・自動/手動スケール切替の対象は主断面 (planeBox の選択)
    const int prim = qBound(0, m_planeBox->currentIndex(), 2);
    m_data = d[prim];
    m_rows = rows[prim];
    m_cols = cols[prim];

    m_previewBox->setTitle(I18n::tr("h5_multi_title").arg(m_seriesComp, label));
}

// 現在フレームの表示画像を PNG 保存 (軸・見出しごと見た目のまま)。
// 3 面ビュー時は 3 面を並べたまま (キャプション付き) 保存する
void H5ViewerTab::exportPngCurrent()
{
    const bool multi = multiActive();
    QWidget *src = multi ? static_cast<QWidget *>(m_multiWrap) : m_canvas;
    if (multi ? !m_multiCanvas[0]->hasData() : !m_canvas->hasData()) return;
    const QString path = QFileDialog::getSaveFileName(
        this, I18n::tr("h5_exp_png"), QStringLiteral("h5_frame.png"),
        QStringLiteral("PNG (*.png)"));
    if (path.isEmpty()) return;
    m_expStatus->setText(src->grab().save(path)
        ? I18n::tr("h5_exp_done").arg(QFileInfo(path).fileName())
        : I18n::tr("h5_exp_fail").arg(path));
}

// 現在フレームの行列を CSV 保存 (実データ)
void H5ViewerTab::exportCsvCurrent()
{
    if (m_data.isEmpty() || m_rows <= 0 || m_cols <= 0) return;
    const QString path = QFileDialog::getSaveFileName(
        this, I18n::tr("h5_exp_csv"), QStringLiteral("h5_frame.csv"),
        QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_expStatus->setText(I18n::tr("h5_exp_fail").arg(path));
        return;
    }
    QTextStream out(&f);
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            if (c) out << ',';
            out << m_data[r * m_cols + c];
        }
        out << '\n';
    }
    m_expStatus->setText(
        I18n::tr("h5_exp_done").arg(QFileInfo(path).fileName()));
}

// 開いている .h5 の実スキーマ (列挙結果) から h5py 読み込みコードを生成し、
// .py スクリプト / .ipynb ノートブック (JSON 直書き) として保存する。
// 外部ツール (python / jupyter) の起動は行わない — 生成のみ
void H5ViewerTab::exportPythonScript(bool notebook)
{
    if (m_filePath.isEmpty() || m_dsets.isEmpty()) {
        QMessageBox::information(this, I18n::tr("h5_integration"),
                                 I18n::tr("h5_int_need_file"));
        return;
    }
    const QStringList code = buildH5PyCode(m_filePath, m_dsets);
    const QStringList schema = schemaLines(m_dsets);
    const QFileInfo fi(m_filePath);

    if (!notebook) {
        // .py: ヘッダコメントに実スキーマの一覧を添える
        QStringList lines;
        lines << QStringLiteral("#!/usr/bin/env python3")
              << QStringLiteral("# -*- coding: utf-8 -*-")
              << QStringLiteral("# OpenFDTD-X (H5 アニメタブ) が生成した "
                                "h5py 読み込みスクリプト")
              << QStringLiteral("# 対象: %1").arg(m_filePath)
              << QStringLiteral("# 生成時点のデータセット (実スキーマ):");
        for (const QString &l : schema)
            lines << QLatin1Char('#') + l;
        lines << QString();
        lines += code;
        tabhelp::saveTextFile(this, I18n::tr("h5_int_python"),
                              fi.completeBaseName()
                                  + QStringLiteral("_h5py.py"),
                              QStringLiteral("Python (*.py)"),
                              lines.join(QLatin1Char('\n'))
                                  + QLatin1Char('\n'));
        return;
    }

    // .ipynb: nbformat 4 の JSON を直接組み立てる (markdown + code の 2 セル)
    QStringList md;
    md << QStringLiteral("# %1 — h5py 読み込み").arg(fi.fileName())
       << QString()
       << QStringLiteral("OpenFDTD-X (H5 アニメタブ) が生成した"
                         "ノートブックです。")
       << QString()
       << QStringLiteral("対象ファイル: `%1`").arg(m_filePath)
       << QString()
       << QStringLiteral("生成時点のデータセット (実スキーマ):")
       << QStringLiteral("```");
    md += schema;
    md << QStringLiteral("```");

    const QJsonObject mdCell{
        { QStringLiteral("cell_type"), QStringLiteral("markdown") },
        { QStringLiteral("metadata"), QJsonObject() },
        { QStringLiteral("source"), ipynbSource(md) },
    };
    const QJsonObject codeCell{
        { QStringLiteral("cell_type"), QStringLiteral("code") },
        { QStringLiteral("execution_count"), QJsonValue() },
        { QStringLiteral("metadata"), QJsonObject() },
        { QStringLiteral("outputs"), QJsonArray() },
        { QStringLiteral("source"), ipynbSource(code) },
    };
    const QJsonObject root{
        { QStringLiteral("cells"), QJsonArray{ mdCell, codeCell } },
        { QStringLiteral("metadata"), QJsonObject{
            { QStringLiteral("kernelspec"), QJsonObject{
                { QStringLiteral("display_name"),
                  QStringLiteral("Python 3") },
                { QStringLiteral("language"), QStringLiteral("python") },
                { QStringLiteral("name"), QStringLiteral("python3") } } },
            { QStringLiteral("language_info"), QJsonObject{
                { QStringLiteral("name"), QStringLiteral("python") } } } } },
        { QStringLiteral("nbformat"), 4 },
        { QStringLiteral("nbformat_minor"), 5 },
    };
    tabhelp::saveTextFile(this, I18n::tr("h5_int_jupyter"),
                          fi.completeBaseName()
                              + QStringLiteral("_h5py.ipynb"),
                          QStringLiteral("Jupyter Notebook (*.ipynb)"),
                          QString::fromUtf8(QJsonDocument(root)
                              .toJson(QJsonDocument::Indented)));
}

// 全フレーム走査 (自動スケール決定) 用に frame の値を集める。
// 3 面ビュー時は 3 面を連結して返す (スケールを 3 面共通にするため)
bool H5ViewerTab::scanFrameValues(int frame, QVector<double> &out)
{
    out.clear();
    int rows = 0, cols = 0;
    if (multiActive()) {
        for (int p = 0; p < 3; ++p) {
            const int axis = kPlaneAxis[p];
            QVector<double> d;
            if (!H5Reader::readOfdSeriesFrame(m_filePath, m_seriesComp, frame,
                                              axis, m_secPos[axis], d, rows,
                                              cols))
                return false;
            out += d;
        }
        return true;
    }
    if (m_seriesMode)
        return H5Reader::readOfdSeriesFrame(m_filePath, m_seriesComp, frame,
                                            sliceAxis(), m_secPos[sliceAxis()],
                                            out, rows, cols);
    return H5Reader::readFrame(m_filePath, m_dataset, frame, out, rows, cols);
}

// 3 面 (XY / XZ / YZ) を横に並べ、上にキャプションを載せた 1 枚の画像を作る。
// カラースケール (lo, hi) は 3 面共通 — 面どうしの強度が比較できる
QImage H5ViewerTab::multiImage(int frame, double lo, double hi, bool *ok)
{
    QImage panes[3];
    QString caps[3];
    int totW = 0, maxH = 0;
    for (int p = 0; p < 3; ++p) {
        const int axis = kPlaneAxis[p];
        QVector<double> d;
        int rows = 0, cols = 0;
        if (!H5Reader::readOfdSeriesFrame(m_filePath, m_seriesComp, frame,
                                          axis, m_secPos[axis], d, rows,
                                          cols)) {
            if (ok) *ok = false;
            return QImage();
        }
        const int cellPx = qMax(1, 360 / qMax(rows, cols));
        panes[p] = m_canvas->renderImage(d, rows, cols, cellPx, lo, hi);
        caps[p] = sliceCaption(axis);
        if (panes[p].isNull()) {
            if (ok) *ok = false;
            return QImage();
        }
        totW += panes[p].width();
        maxH = qMax(maxH, panes[p].height());
    }
    const int margin = 10, gap = 10, capH = 20;
    QImage out(totW + 2 * margin + 2 * gap, maxH + 2 * margin + capH,
               QImage::Format_RGB32);
    out.fill(Qt::black);
    QPainter p(&out);
    QFont f = p.font();
    f.setPixelSize(12);
    p.setFont(f);
    const QFontMetrics fm(f);
    int x = margin;
    for (int i = 0; i < 3; ++i) {
        p.setPen(Qt::white);
        p.drawText(QRect(x, margin, panes[i].width(), capH),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   fm.elidedText(caps[i], Qt::ElideRight, panes[i].width()));
        p.drawImage(QPoint(x, margin + capH), panes[i]);
        x += panes[i].width() + gap;
    }
    if (ok) *ok = true;
    return out;
}

// frame 番目のフレームを読み込んで指定スケールで画像化する
QImage H5ViewerTab::frameImage(int frame, double lo, double hi, bool *ok)
{
    if (multiActive()) return multiImage(frame, lo, hi, ok);

    QVector<double> d;
    int rows = 0, cols = 0;
    bool loaded = false;
    if (m_seriesMode)
        loaded = H5Reader::readOfdSeriesFrame(
            m_filePath, m_seriesComp, frame, sliceAxis(),
            m_secPos[sliceAxis()], d, rows, cols);
    else
        loaded = H5Reader::readFrame(m_filePath, m_dataset, frame, d, rows,
                                     cols);
    if (ok) *ok = loaded;
    if (!loaded) return QImage();
    // 512px 程度になるようセルを拡大 (ニアレスト — 物理格子をぼかさない)
    const int cellPx = qMax(1, 512 / qMax(rows, cols));
    return m_canvas->renderImage(d, rows, cols, cellPx, lo, hi);
}

// ── 時間範囲 → 再生対象フレーム ────────────────────────────────────────────
// 時刻は /timeseries/time (H は time_H) から読む。旧 /data%06d 形式には
// 時刻が無いので絞り込みはできない — チェックを無効化して理由を出す
// (フレーム番号を時刻とみなす、といった推測はしない)。
void H5ViewerTab::applyTimeRange()
{
    if (!m_rangeOnly) return;
    m_playFirst = 0;
    m_playLast = m_nframes - 1;

    const bool haveTimes = !m_frameTimes.isEmpty()
                           && m_frameTimes.size() == m_nframes;
    m_rangeOnly->setEnabled(haveTimes);
    m_rangeLo->setEnabled(haveTimes);
    m_rangeHi->setEnabled(haveTimes);
    if (!haveTimes) {
        m_rangeOnly->setChecked(false);
        m_rangeNote->setText(m_seriesMode ? I18n::tr("h5_range_notime")
                                          : I18n::tr("h5_range_noseries"));
        return;
    }
    // 表示単位 → 秒 (ドメイン別: ps / ms / s)
    const double scale = timeUnitToSeconds();
    if (!m_rangeOnly->isChecked()) {
        m_rangeNote->setText(I18n::tr("h5_range_all")
            .arg(QString::number(m_frameTimes.first() / scale, 'g', 4),
                 QString::number(m_frameTimes.last() / scale, 'g', 4),
                 m_timeUnit->text()));
        return;
    }
    bool a = false, b = false;
    const double lo = m_rangeLo->text().trimmed().toDouble(&a) * scale;
    const double hi = m_rangeHi->text().trimmed().toDouble(&b) * scale;
    int f0 = 0, f1 = m_nframes - 1;
    if (!a || !b || !movie::frameRangeForTimes(m_frameTimes, lo, hi, f0, f1)) {
        // 1 フレームも入らない範囲を黙って適用しない (全フレームのまま)
        m_rangeNote->setText(I18n::tr("h5_range_empty"));
        m_rangeOnly->setChecked(false);
        return;
    }
    m_playFirst = f0;
    m_playLast = f1;
    m_rangeNote->setText(I18n::tr("h5_range_applied")
        .arg(f1 - f0 + 1)
        .arg(f0).arg(f1)
        .arg(QString::number(m_frameTimes[f0] / scale, 'g', 4),
             QString::number(m_frameTimes[f1] / scale, 'g', 4),
             m_timeUnit->text()));
    if (m_frame < m_playFirst || m_frame > m_playLast) setFrame(m_playFirst);
}

// 表示単位 (ps / ms / s) → 秒への換算係数
double H5ViewerTab::timeUnitToSeconds() const
{
    const QString u = m_timeUnit ? m_timeUnit->text() : QStringLiteral("s");
    if (u == QLatin1String("ps")) return 1e-12;
    if (u == QLatin1String("ms")) return 1e-3;
    return 1.0;
}

// 動画設定 (FPS / 解像度 / コーデック) → ffmpeg のオプション
movie::MovieOptions H5ViewerTab::movieOptions(bool gif) const
{
    movie::MovieOptions o;
    o.gif = gif;
    bool ok = false;
    const int fps = m_movieFps ? m_movieFps->text().trimmed().toInt(&ok) : 0;
    // 未入力・不正値のときは再生速度コンボから決める (従来の挙動)
    static const int kFps[] = { 3, 5, 10, 20, 30 };
    o.fps = (ok && fps > 0) ? fps
                            : kFps[qBound(0, m_speed->currentIndex(), 4)];
    switch (m_movieRes ? m_movieRes->currentIndex() : 0) {
        case 1: o.width = 1920; o.height = 1080; break;
        case 2: o.width = 3840; o.height = 2160; break;
        case 3: o.width = 1280; o.height = 720;  break;
        default: break;                          // 0 = 元の大きさのまま
    }
    switch (m_movieCodec ? m_movieCodec->currentIndex() : 0) {
        case 1:  o.codec = movie::Codec::H265; break;
        case 2:  o.codec = movie::Codec::VP9;  break;
        default: o.codec = movie::Codec::H264; break;
    }
    return o;
}

// 全フレームを PNG 連番へ描き出す。video=true なら ffmpeg で動画化
void H5ViewerTab::exportFrames(bool video, const QString &videoExt)
{
    if (m_exporting || m_nframes <= 0) return;

    QString ffmpeg, outPath, framesDir;
    if (video) {
        // 動画化は外部 ffmpeg (PATH) — 無ければ導入方法を案内して中止
        ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
        if (ffmpeg.isEmpty()) {
            QMessageBox::information(this, I18n::tr("h5_export"),
                                     I18n::tr("h5_exp_noffmpeg"));
            return;
        }
        outPath = QFileDialog::getSaveFileName(
            this, I18n::tr("h5_export"),
            QStringLiteral("propagation.%1").arg(videoExt),
            QStringLiteral("%1 (*.%2)").arg(videoExt.toUpper(), videoExt));
        if (outPath.isEmpty()) return;
    } else {
        framesDir = QFileDialog::getExistingDirectory(
            this, I18n::tr("h5_exp_pngseq"));
        if (framesDir.isEmpty()) return;
    }

    m_exporting = true;

    // 時間範囲で絞り込んでいるときは、その区間だけを書き出す
    // (画面で見ている範囲と書き出しがずれないように)
    const int expFirst = qBound(0, m_playFirst, m_nframes - 1);
    const int expLast = (m_playLast < 0)
                            ? m_nframes - 1
                            : qBound(expFirst, m_playLast, m_nframes - 1);
    const int expCount = expLast - expFirst + 1;

    // スケールは全フレーム共通 (自動: 全フレーム走査 / 手動: 入力値)。
    // フレームごとの自動スケールはアニメがちらつくため使わない
    double lo = 0.0, hi = 1.0;
    QProgressDialog prog(I18n::tr("h5_export"), I18n::tr("gal_cancel"),
                         0, expCount * (m_autoScale->isChecked() ? 2 : 1),
                         this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(200);
    int step = 0;
    if (m_autoScale->isChecked()) {
        bool first = true;
        for (int f = expFirst; f <= expLast; ++f) {
            // 3 面ビュー時は 3 面すべてを走査する (共通スケールのため)
            QVector<double> d;
            const bool okF = scanFrameValues(f, d);
            if (okF) {
                for (const double v : d) {
                    if (first) { lo = hi = v; first = false; }
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
            }
            prog.setValue(++step);
            if (prog.wasCanceled()) {
                m_expStatus->setText(I18n::tr("h5_exp_cancelled"));
                m_exporting = false;
                return;
            }
        }
    } else {
        lo = m_scaleMin->text().toDouble();
        hi = m_scaleMax->text().toDouble();
    }
    if (hi <= lo) hi = lo + 1.0;

    // PNG 連番の書き出し先 (動画時は一時ディレクトリ)
    auto tmp = std::make_shared<QTemporaryDir>();
    const QString dir = video ? tmp->path() : framesDir;
    int written = 0;
    for (int f = expFirst; f <= expLast; ++f) {
        bool okF = false;
        const QImage img = frameImage(f, lo, hi, &okF);
        if (okF && !img.isNull()) {
            // 動画化する場合は 0 から連番にする (ffmpeg の %05d は既定で
            // 0 始まり)。PNG 連番として出す場合は元のフレーム番号を残す
            // (どのフレームか分かるほうが役に立つ)。
            const int idx = video ? written : f;
            img.save(QStringLiteral("%1/frame%2.png").arg(dir)
                         .arg(idx, 5, 10, QLatin1Char('0')));
            ++written;
        }
        prog.setValue(++step);
        if (prog.wasCanceled()) {
            m_expStatus->setText(I18n::tr("h5_exp_cancelled"));
            m_exporting = false;
            return;
        }
    }
    if (!video) {
        m_expStatus->setText(I18n::tr("h5_exp_done")
            .arg(QStringLiteral("%1 (%2 PNG)").arg(framesDir).arg(written)));
        m_exporting = false;
        return;
    }

    // ffmpeg でエンコード (非同期 — 終了はシグナルで受ける)
    m_expStatus->setText(I18n::tr("h5_exp_encoding"));
    // 引数の組み立ては io/MovieExport の純関数 (selftest から検証している)
    const QStringList args = movie::buildFfmpegArgs(
        QStringLiteral("%1/frame%05d.png").arg(dir), outPath,
        movieOptions(videoExt == QLatin1String("gif")));
    auto *proc = new QProcess(this);
    connect(proc,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, proc, tmp, outPath](int code,
                                             QProcess::ExitStatus st) {
        m_expStatus->setText(
            (st == QProcess::NormalExit && code == 0)
                ? I18n::tr("h5_exp_done").arg(QFileInfo(outPath).fileName())
                : I18n::tr("h5_exp_fail")
                      .arg(QString::fromUtf8(
                          proc->readAllStandardError().right(300))));
        m_exporting = false;
        proc->deleteLater();
        // tmp (連番の一時ディレクトリ) はここで破棄される
    });
    proc->start(ffmpeg, args);
}

// 行列をキャンバスへ渡し、min / max / 平均 を実計算して統計・スケールを更新
void H5ViewerTab::showData(const QVector<double> &d, int rows, int cols)
{
    m_data = d;
    m_rows = rows;
    m_cols = cols;

    double lo = 0.0, hi = 0.0, sum = 0.0;
    if (!d.isEmpty()) {
        lo = hi = d[0];
        for (const double v : d) {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
            sum += v;
        }
    }
    const double mean = d.isEmpty() ? 0.0 : sum / double(d.size());

    m_statMin->setText(QString("min: %1").arg(lo, 0, 'g', 4));
    m_statMax->setText(QString("max: %1").arg(hi, 0, 'g', 4));
    m_statMean->setText(QString("%1: %2").arg(I18n::tr("h5_stat_mean"))
                            .arg(mean, 0, 'g', 4));

    if (m_autoScale->isChecked()) {
        m_canvas->setScale(lo, hi);
        setScaleLabels(lo, hi);
    } else {
        // 手動スケールはそのまま (キャンバス側の設定を維持)
        applyScale();
    }
    m_canvas->setData(d, rows, cols);
}

void H5ViewerTab::setFrame(int f)
{
    if (m_nframes <= 0) return;
    f = qBound(0, f, m_nframes - 1);
    m_frame = f;
    m_frameSlider->blockSignals(true);
    m_frameSlider->setValue(f);
    m_frameSlider->blockSignals(false);
    m_frameLabel->setText(QString("%1 / %2").arg(f).arg(m_nframes - 1));
    m_previewBox->setTitle(I18n::tr("h5_preview")
        + QString(" (frame %1 / %2)").arg(f).arg(m_nframes));
    loadCurrentFrame();
}

void H5ViewerTab::applyScale()
{
    // 3 面ビューは 3 面へまとめて反映する (loadMultiFrames が共通スケール)
    if (multiActive()) { loadMultiFrames(); return; }
    const double lo = m_scaleMin->text().toDouble();
    const double hi = m_scaleMax->text().toDouble();
    if (lo >= hi) return;
    m_canvas->setScale(lo, hi);
    setScaleLabels(lo, hi);
}

void H5ViewerTab::setScaleLabels(double lo, double hi)
{
    m_barMax->setText(QString::number(hi, 'g', 4));
    m_barMin->setText(QString::number(lo, 'g', 4));
    // 手動欄も現在の実レンジを反映 (自動→手動の切替時に出発点になる)
    if (m_autoScale->isChecked()) {
        m_scaleMin->setText(QString::number(lo, 'g', 6));
        m_scaleMax->setText(QString::number(hi, 'g', 6));
    }
}

void H5ViewerTab::setPlaybackEnabled(bool on)
{
    m_playBtn->setEnabled(on);
    m_firstBtn->setEnabled(on);
    m_prevBtn->setEnabled(on);
    m_nextBtn->setEnabled(on);
    m_lastBtn->setEnabled(on);
    m_loopBtn->setEnabled(on);
    m_frameSlider->setEnabled(on);
    if (!on && m_timer->isActive()) {
        m_timer->stop();
        m_playBtn->setText(I18n::tr("h5_play"));
    }
}

void H5ViewerTab::clearStats()
{
    m_data.clear();
    m_rows = m_cols = 0;
    m_statMin->setText("min: -");
    m_statMax->setText("max: -");
    m_statMean->setText(I18n::tr("h5_stat_mean") + ": -");
    m_barMax->setText("-");
    m_barMin->setText("-");
}

// ドメインに応じた出し分け:
//   - 時間範囲の単位ラベル (EM/光: ps、室内音響: ms、水中: s)。値の換算は
//     しない — 時間範囲入力は再生に反映されない飾り (unwiredNote 済み)。
//   - Schroeder 減衰ボタンは室内音響でのみ意味を持つため他ドメインでは隠す。
void H5ViewerTab::updateDomainVisibility()
{
    const Domain d = m_p->activeDomain();
    const char *unit = "ps";                       // EM / 光
    if (d == Domain::Acoustic)        unit = "ms"; // 室内音響
    else if (d == Domain::Underwater) unit = "s";  // 水中音響
    m_timeUnit->setText(QLatin1String(unit));
    m_schroederBtn->setVisible(d == Domain::Acoustic);
}
