// I18n.cpp
#include "I18n.h"

using namespace ofd;

I18n &I18n::instance() { static I18n inst; return inst; }

I18n::I18n() { loadTables(); }

void I18n::setLanguage(const QString &lang) { m_lang = lang; }

void I18n::reg(const char *key, const char *ja, const char *en) {
    auto &I = instance();
    if (I.m_ja.contains(key)) return;   // loadTables() entries win
    I.m_ja[key] = QString::fromUtf8(ja);
    I.m_en[key] = QString::fromUtf8(en);
}

QString I18n::tr(const QString &key) {
    auto &I = instance();
    const QString ja = I.m_ja.value(key, key);
    const QString en = I.m_en.value(key, key);
    if (I.m_lang == "en")   return en;
    if (I.m_lang == "both") return (ja == en) ? ja : (ja + " / " + en);
    return ja;
}

// All strings declared once here. Add a key → it shows up everywhere.
void I18n::loadTables() {
    auto add = [this](const char *key, const char *ja, const char *en) {
        m_ja[key] = QString::fromUtf8(ja);
        m_en[key] = QString::fromUtf8(en);
    };

    // App
    add("app_name", "OpenFDTD-X", "OpenFDTD-X");
    add("app_subtitle", "マルチドメインFDTDシミュレーター", "Multi-Domain FDTD Simulator");
    add("untitled", "無題", "Untitled");

    // Menus & toolbar
    add("m_file", "ファイル(&F)", "&File");
    add("m_edit", "編集(&E)", "&Edit");
    add("m_view", "表示(&V)", "&View");
    add("m_tools", "ツール(&T)", "&Tools");
    add("m_run", "実行(&R)", "&Run");
    add("m_post", "ポスト処理(&P)", "&Post");
    add("m_help", "ヘルプ(&H)", "&Help");

    add("tb_new", "新規", "New");
    add("tb_open", "開く", "Open");
    add("tb_save", "保存", "Save");
    add("tb_saveas", "名前を付けて保存", "Save As");
    add("tb_calc", "計算", "Compute");
    add("tb_post", "ポスト処理", "Post");
    add("tb_plot2d", "図形表示2D", "Plot 2D");
    add("tb_plot3d", "図形表示3D", "Plot 3D");
    add("tb_plot2d_tip",
        "結果プロットタブを開く — 実行後は給電点の周波数特性 (<kernel>.log) "
        "と遠方界パターン (far1d.log) を表示。実行前は波源波形プレビュー。"
        "カーネル作図 (ev.ev2) は下部の「ev2d を開く」から",
        "Open the result-plot tab — after a run it shows the feed-point "
        "frequency response (<kernel>.log) and far-field pattern (far1d.log); "
        "before a run, the source-waveform preview. Kernel-drawn figures "
        "(ev.ev2) open via the ev2d button at the bottom");
    add("tb_plot3d_tip",
        "3D シーン (形状・メッシュのビューポート) へ切替",
        "Switch to the 3D scene (geometry / mesh viewport)");
    add("tb_stop", "停止", "Stop");
    add("tb_cloud", "クラウド送信", "Cloud");
    add("tb_cloud_optical_only", "クラウド送信 (光のみ)", "Cloud (optical only)");
    add("tb_export", "エクスポート", "Export");
    add("m_acoustic_report", "音響レポート出力…", "Export acoustic report…");
    add("m_kernel_paths", "カーネルパスの設定…", "Kernel paths…");
    add("sb_kernel_missing", "⚠ カーネル未検出: %1", "⚠ Kernel not found: %1");
    add("sb_kernel_missing_tip",
        "このドメインの計算に必要なソルバーカーネルが見つかりません。"
        "クリックしてカーネルパスを設定してください "
        "(OpenFDTD は基幹カーネル、光/水中はそのドメインを使う場合のみ必要)",
        "The solver kernel required by this domain was not found. Click to "
        "configure the kernel paths (OpenFDTD is the core kernel; the "
        "optical / underwater kernels are only needed for those domains)");
    add("m_exit", "終了", "Exit");
    add("m_lang", "言語", "Language");
    add("m_lang_restart", "言語設定を保存しました。再起動後に反映されます。",
        "Language saved. It takes effect after restart.");

    // Domains
    add("d_em", "電磁波", "Electromagnetic");
    add("d_optical", "光", "Optical");
    add("d_acoustic", "室内音響", "Room Acoustic");
    add("d_underwater", "水中音響", "Underwater");

    // Left tabs
    add("t_general", "全般", "General");
    add("t_mesh", "メッシュ", "Mesh");
    add("t_material", "物性値・集中定数", "Materials && Lumped");
    add("t_geometry", "物体形状", "Geometry");
    add("t_source", "波源・観測点", "Source && Probe");
    add("t_post1", "ポスト処理(1)", "Post-Proc (1)");
    add("t_post2", "ポスト処理(2)", "Post-Proc (2)");
    add("t_optical", "光解析", "Optical");
    add("t_acoustic", "音響解析", "Acoustic");
    add("t_underwater", "水中音響", "Underwater");
    add("t_tidy3d", "tidy3d連携", "tidy3d");
    add("t_glass", "🔷 ガラスカタログ", "🔷 Glass Catalog");
    add("t_roomac", "🏛 ホール解析", "🏛 Hall Analysis");

    // General tab
    add("g_title", "タイトル", "Title");
    add("g_solver", "計算条件", "Solver");
    add("g_max_iter", "最大反復回数", "Max iterations");
    add("g_nout", "出力間隔", "Output interval");
    add("g_conv_tol", "収束判定値", "Tolerance");
    add("g_abc", "吸収境界条件", "Absorbing BC");
    add("g_mur1", "Mur 1次", "Mur 1st");
    add("g_pml", "PML", "PML");
    add("g_pml_layers", "PML層数", "PML layers");
    add("g_pml_order", "次数 m", "Order m");
    add("g_pml_r0", "反射係数 R0", "Reflection R0");
    add("g_periodic", "周期境界条件 (PBC)", "Periodic BC");
    add("g_freq1", "給電点・観測点周波数 [Hz]", "Feed/Probe frequency [Hz]");
    add("g_freq2", "遠方界・近傍界周波数 [Hz]", "Far/Near field frequency [Hz]");
    add("g_freq_min", "最小", "min");
    add("g_freq_max", "最大", "max");
    add("g_freq_div", "分割数", "divisions");
    add("g_advanced", "詳細設定", "Advanced");
    add("g_timestep", "タイムステップ Δt [s] (0=自動)", "Timestep Δt [s] (0=auto)");
    add("g_pulsewidth", "パルス幅 Tw [s] (0=自動)", "Pulse width Tw [s] (0=auto)");
    add("g_rfeed", "給電点抵抗 rfeed [Ω]", "Feed resistance rfeed [Ω]");
    add("g_plot3dgeom", "形状確認モード (plot3dgeom)", "Geometry check (plot3dgeom)");

    // Mesh tab
    add("me_axis_x", "Xメッシュ", "X mesh");
    add("me_axis_y", "Yメッシュ", "Y mesh");
    add("me_axis_z", "Zメッシュ", "Z mesh");
    add("me_coord", "座標 [m]", "Coord [m]");
    add("me_div", "分割数", "Divisions");
    add("me_add", "行を追加", "Add row");
    add("me_del", "行を削除", "Delete row");
    add("me_cells", "セル数", "cells");
    add("me_dmin", "最小間隔", "min spacing");
    add("me_total", "総セル数", "Total cells");

    // Material tab
    add("ma_section", "物性値 (材質番号は2から)", "Materials (IDs start at 2)");
    add("ma_builtin", "0=真空, 1=PEC (組込み)", "0=vacuum, 1=PEC (built-in)");
    add("ma_id", "番号", "ID");
    add("ma_type", "種別", "Type");
    add("ma_normal", "通常", "Normal");
    add("ma_dispersive", "分散性", "Dispersive");
    add("ma_name", "名前", "Name");
    add("ma_add", "材質を追加", "Add material");
    add("ma_del", "材質を削除", "Delete material");
    add("ma_lumped", "集中定数 (load)", "Lumped elements (load)");
    add("ma_kind", "種類", "Kind");
    add("ma_value", "値 [Ω/H/F]", "Value [Ω/H/F]");
    add("ma_dir", "方向", "Dir");
    add("ma_add_load", "素子を追加", "Add element");
    add("ma_del_load", "素子を削除", "Delete element");

    // Geometry tab
    add("ge_section", "物体形状 (ユニット番号順, 後優先)", "Geometry units (later wins)");
    add("ge_mat", "材質", "Material");
    add("ge_shape", "形状", "Shape");
    add("ge_add", "ユニットを追加", "Add unit");
    add("ge_del", "ユニットを削除", "Delete unit");
    add("ge_unit", "ユニット", "Unit");
    add("ge_params", "形状パラメータ", "Shape parameters");
    add("ge_shape_1", "直方体", "Brick");
    add("ge_shape_2", "楕円体/球", "Ellipsoid/Sphere");
    add("ge_shape_11", "円柱 (X軸)", "Cylinder (X)");
    add("ge_shape_12", "円柱 (Y軸)", "Cylinder (Y)");
    add("ge_shape_13", "円柱 (Z軸)", "Cylinder (Z)");
    add("ge_shape_31", "三角柱 (X軸)", "Triangle pillar (X)");
    add("ge_shape_32", "三角柱 (Y軸)", "Triangle pillar (Y)");
    add("ge_shape_33", "三角柱 (Z軸)", "Triangle pillar (Z)");
    add("ge_shape_41", "角錐台 (X軸)", "Pyramid (X)");
    add("ge_shape_42", "角錐台 (Y軸)", "Pyramid (Y)");
    add("ge_shape_43", "角錐台 (Z軸)", "Pyramid (Z)");
    add("ge_shape_51", "円錐台 (X軸)", "Cone (X)");
    add("ge_shape_52", "円錐台 (Y軸)", "Cone (Y)");
    add("ge_shape_53", "円錐台 (Z軸)", "Cone (Z)");
    add("ge_import", "3Dモデル取込 (STL)", "3D model import (STL)");
    add("ge_import_btn", "STLを取り込む…", "Import STL…");
    add("ge_import_hint",
        "STLを取り込み後「ボクセル化」で現在のメッシュへ階段近似変換します。",
        "After import, press Voxelize to staircase-map the mesh onto the grid.");
    add("ge_voxelize_btn", "ボクセル化 (Yee格子へ)", "Voxelize (to Yee grid)");
    add("ge_voxel_mat", "材質番号", "Material id");
    add("ge_voxelize_hint",
        "「ボクセル化」で各セル中心の内外判定 (レイキャスト) を行い、占有セルを"
        "直方体ユニットとして追加します。より高精度な共形/winding number 判定は"
        "-DUSE_LIBIGL=ON で拡張できます。",
        "Voxelize runs an inside/outside ray-cast per cell centre and adds the "
        "occupied cells as brick units. Build with -DUSE_LIBIGL=ON for "
        "conformal / winding-number accuracy.");

    // Source tab
    add("so_feeds", "給電点 (feed)", "Feeds");
    add("so_volt", "電圧 [V]", "Volt [V]");
    add("so_delay", "遅延 [deg]", "Delay [deg]");
    add("so_z0", "内部抵抗 Z0 [Ω]", "Z0 [Ω]");
    add("so_add_feed", "給電点を追加", "Add feed");
    add("so_del_feed", "給電点を削除", "Delete feed");
    add("so_planewave", "平面波入射 (planewave)", "Plane wave");
    add("so_pw_enable", "平面波を使用", "Enable plane wave");
    add("so_theta", "θ [deg]", "θ [deg]");
    add("so_phi", "φ [deg]", "φ [deg]");
    add("so_pol", "偏波", "Polarization");
    add("so_pol_v", "1: 垂直 (V)", "1: Vertical");
    add("so_pol_h", "2: 水平 (H)", "2: Horizontal");
    add("so_points", "観測点 (point)", "Observation points");
    add("so_prop", "伝搬方向 (#1のみ)", "Propagation (#1 only)");
    add("so_add_point", "観測点を追加", "Add point");
    add("so_del_point", "観測点を削除", "Delete point");
    add("so_exclusive", "給電点と平面波は同時に指定できません (本家仕様)",
        "Feed and plane wave are mutually exclusive");

    // Post1 tab
    add("p1_freq_section", "周波数特性プロット", "Frequency-domain plots");
    add("p1_iter", "反復回数特性 (plotiter)", "Iteration history (plotiter)");
    add("p1_feed", "給電点波形 (plotfeed)", "Feed waveform (plotfeed)");
    add("p1_point", "観測点波形 (plotpoint)", "Point waveform (plotpoint)");
    add("p1_smith", "スミスチャート (plotsmith)", "Smith chart (plotsmith)");
    add("p1_zin", "入力インピーダンス (plotzin)", "Input impedance (plotzin)");
    add("p1_yin", "入力アドミタンス (plotyin)", "Input admittance (plotyin)");
    add("p1_ref", "反射係数 (plotref)", "Reflection (plotref)");
    add("p1_spara", "Sパラメータ (plotspara)", "S-parameters (plotspara)");
    add("p1_coupling", "結合係数 (plotcoupling)", "Coupling (plotcoupling)");
    add("p1_matching", "整合損を含む (matchingloss)", "Include matching loss");
    add("p1_freqdiv", "周波数目盛分割 (freqdiv)", "Frequency divisions (freqdiv)");
    add("p1_user_scale", "スケール指定", "User scale");
    add("p1_min", "最小", "min");
    add("p1_max", "最大", "max");
    add("p1_div", "分割", "div");

    // Post2 tab
    add("p2_far0d", "遠方界周波数特性 (plotfar0d)", "Far field vs freq (plotfar0d)");
    add("p2_far1d", "遠方界指向性 (plotfar1d)", "Far-field pattern (plotfar1d)");
    add("p2_far2d", "遠方界全方向 (plotfar2d)", "Far-field 3D (plotfar2d)");
    add("p2_near1d", "近傍界1D (plotnear1d)", "Near field 1D (plotnear1d)");
    add("p2_near2d", "近傍界2D (plotnear2d)", "Near field 2D (plotnear2d)");
    add("p2_dir", "方向/面", "Dir/Plane");
    add("p2_division", "分割数", "Divisions");
    add("p2_angle", "角度 [deg]", "Angle [deg]");
    add("p2_style", "スタイル (far1dstyle)", "Style (far1dstyle)");
    add("p2_db", "dB表示", "dB scale");
    add("p2_norm", "正規化 (far1dnorm)", "Normalize (far1dnorm)");
    add("p2_component", "成分", "Components");
    add("p2_obj", "物体表示 (far2dobj)", "Object overlay (far2dobj)");
    add("p2_cmp", "成分", "Component");
    add("p2_pos", "位置", "Position");
    add("p2_add", "追加", "Add");
    add("p2_del", "削除", "Delete");
    add("p2_noinc", "入射波を除く (noinc)", "Exclude incident (noinc)");
    add("p2_contour", "等高線 (contour)", "Contour");
    add("p2_theta_div", "θ分割", "θ div");
    add("p2_phi_div", "φ分割", "φ div");

    // Optical tab
    add("opt_solver", "光ソルバー選択", "Optical solver");
    add("opt_solver_fdtd", "FDTD — 広帯域・任意形状・分散材料",
        "FDTD — broadband, arbitrary shapes, dispersive media");
    add("opt_solver_rcwa", "RCWA — 周期格子・薄膜・回折次数 (OpenRCWA)",
        "RCWA — periodic gratings, thin films (OpenRCWA)");
    add("opt_solver_bpm", "BPM — 導波路の高速伝搬解析 (OpenBPM)",
        "BPM — fast waveguide propagation (OpenBPM)");
    add("opt_solver_fmm", "FMM — 多層周期構造のS行列カスケード",
        "FMM — layered periodic structures, S-matrix cascade");
    add("opt_mode", "光解析モード", "Optical mode");
    add("opt_bpf", "バンドパスフィルタ (BPF)", "Band-pass filter");
    add("opt_wg", "導波路モード解析", "Waveguide mode");
    add("opt_ring", "リング共振器", "Ring resonator");
    add("opt_mzi", "MZI干渉計", "MZI interferometer");
    add("opt_meta", "メタサーフェス", "Metasurface");
    add("opt_phc", "フォトニック結晶", "Photonic crystal");
    add("opt_nf2ff", "近傍界→遠方界変換", "NF→FF transform");
    add("opt_spara", "Sパラメータ抽出", "S-parameter extraction");
    add("opt_wavelength", "波長範囲", "Wavelength range");
    add("opt_lambda_min", "λmin [nm]", "λmin [nm]");
    add("opt_lambda_max", "λmax [nm]", "λmax [nm]");
    add("opt_lambda_div", "分割数", "divisions");
    add("opt_rcwa_section", "RCWAパラメータ", "RCWA parameters");
    add("opt_rcwa_orders", "Fourier次数 (Nx × Ny)", "Fourier orders (Nx × Ny)");
    add("opt_rcwa_period", "格子周期 Λ [nm]", "Grating period Λ [nm]");
    add("opt_rcwa_layers", "層分割数", "Layer slices");
    add("opt_rcwa_stack", "層構成 (入射側 → 透過側)",
        "Layer stack (incidence → exit)");
    add("opt_rcwa_eps1", "ε1", "ε1");
    add("opt_rcwa_eps2", "ε2", "ε2");
    add("opt_rcwa_fill", "フィル率 f", "Fill factor f");
    add("opt_rcwa_thick", "厚み [nm]", "Thickness [nm]");
    add("opt_rcwa_add", "層を追加", "Add layer");
    add("opt_rcwa_del", "選択層を削除", "Remove selected layer");
    add("opt_rcwa_stack_hint",
        "先頭と末尾は半無限層として扱われ、厚みは無視されます (0 で可)。"
        "orcwa は 1D 格子ソルバーのため Nx と Λx のみを使い、Ny / Λy は "
        "無視されます。周波数は全般タブの frequency1 [Hz] を使います "
        "(λ [nm] → f = c/λ)。カーネルは 0 < f < 1 と中間層の厚み > 0 を"
        "要求します。",
        "The first and last layers are semi-infinite; their thickness is "
        "ignored (0 is fine). orcwa is a 1-D grating solver, so only Nx and "
        "Λx are used — Ny / Λy are ignored. Frequencies come from "
        "frequency1 [Hz] on the General tab (λ [nm] → f = c/λ). The kernel "
        "requires 0 < f < 1 and a positive thickness for interior layers.");
    add("opt_rcwa_warn_layer",
        "層 %1: ε1 > 0, ε2 > 0, 0 ≤ f ≤ 1, 厚み ≥ 0 を満たしていません。",
        "Layer %1: needs ε1 > 0, ε2 > 0, 0 ≤ f ≤ 1, thickness ≥ 0.");
    add("opt_rcwa_warn_skip",
        "不正な層があるため、RCWA 設定 (rcwa / rcwalayer) は .ofd に"
        "書き出されません。",
        "The RCWA settings (rcwa / rcwalayer) will NOT be written to the "
        ".ofd file while a layer is invalid.");
    add("opt_rcwa_warn_empty",
        "層が 1 つも定義されていないため、RCWA 設定は .ofd に書き出されません "
        "(カーネルは 2 層以上を要求します)。",
        "No layers defined, so no RCWA settings are written to the .ofd file "
        "(the kernel requires at least two layers).");
    add("opt_bpm_section", "BPMパラメータ", "BPM parameters");
    add("opt_bpm_algo", "アルゴリズム", "Algorithm");
    add("opt_bpm_dz", "Δz ステップ [nm]", "Δz step [nm]");
    add("opt_bpm_n0", "参照屈折率 n0", "Reference index n0");
    add("opt_bpm_input", "入射モード", "Input mode");
    add("opt_fmm_section", "FMMパラメータ", "FMM parameters");
    add("opt_fmm_harmonics", "ハーモニクス数", "Harmonics");
    add("opt_fmm_li", "Liの因数化規則", "Li's factorization rules");
    add("opt_fmm_kernel_hint",
        "FMM (フーリエモード法) は RCWA と同一手法の別名で、OpenRCWA "
        "(orcwa) カーネルで実行されます。周期・層構成は RCWA の設定を"
        "共用し、調和次数のみ本ページの値を使用します。",
        "FMM (Fourier Modal Method) is another name for RCWA and runs on "
        "the OpenRCWA (orcwa) kernel. Period and layer stack are shared "
        "with the RCWA settings; only the harmonics count comes from this "
        "page.");
    add("opt_fmm_li_tip",
        "orcwa カーネルに対応する入力キーが無いため、この切替は現在"
        "効きません (カーネルの実装に従います)。",
        "Currently has no effect: the orcwa kernel has no matching input "
        "key (the kernel's own implementation applies).");
    add("opt_bpf_section", "BPF目標仕様", "BPF target spec");
    add("opt_band", "通過帯域 [nm]", "Passband [nm]");
    add("opt_q", "Q値", "Q factor");
    add("opt_ring_section", "リング共振器", "Ring resonator");
    add("opt_radius", "半径 [μm]", "Radius [μm]");
    add("opt_gap", "ギャップ [nm]", "Gap [nm]");
    add("opt_kernel_hint",
        "RCWA は OpenRCWA (orcwa)、BPM は OpenBPM (obpm) のカーネルを実行します。",
        "RCWA runs the OpenRCWA kernel (orcwa); BPM runs OpenBPM (obpm).");

    // 非線形 (TPA) / ONN 光活性化関数 (Opt. Lett. 49, 5811 (2024))
    add("opt_tpa_section", "非線形 (TPA) / ONN 活性化",
        "Nonlinear (TPA) / ONN activation");
    add("opt_tpa_enable", "TPA (二光子吸収) を有効化",
        "Enable TPA (two-photon absorption)");
    add("opt_tpa_tip",
        "メタマテリアル装荷 Si 導波路の二光子吸収による飽和型 (ReLU 相当) "
        "光活性化関数。出典: Honda, Shoji, Amemiya, Opt. Lett. 49, 5811 (2024).",
        "Saturating (ReLU-like) optical activation function via two-photon "
        "absorption in a metamaterial-loaded Si waveguide. Ref: Honda, Shoji, "
        "Amemiya, Opt. Lett. 49, 5811 (2024).");
    add("opt_tpa_mat", "TPA 材料 ID", "TPA material ID");
    add("opt_tpa_mat_tip",
        "TPA を適用する材料の ID (material 行の並び順)。",
        "ID of the material the TPA coefficient applies to.");
    add("opt_tpa_beta", "β [cm/GW]", "β [cm/GW]");
    add("opt_tpa_beta_tip",
        "TPA 係数 β。既定値 424 cm/GW はメタマテリアル装荷 Si 導波路の実測値 "
        "(Honda, Shoji, Amemiya, Opt. Lett. 49, 5811 (2024))。",
        "TPA coefficient β. The default 424 cm/GW is the measured value for "
        "the metamaterial-loaded Si waveguide (Honda, Shoji, Amemiya, "
        "Opt. Lett. 49, 5811 (2024)).");
    add("opt_ps_enable", "パワースイープ (活性化カーブ出力)",
        "Power sweep (activation curve output)");
    add("opt_ps_tip",
        "入力パワー P_in を掃引し activation_curve.csv "
        "(P_in, P_out, 透過率) を出力します。",
        "Sweeps the input power P_in and writes activation_curve.csv "
        "(P_in, P_out, transmission).");
    add("opt_ps_pmin", "P_min [W]", "P_min [W]");
    add("opt_ps_pmax", "P_max [W]", "P_max [W]");
    add("opt_ps_points", "掃引点数", "Sweep points");
    add("opt_ps_scale", "間隔", "Spacing");
    add("opt_ps_log", "対数 (log)", "Logarithmic (log)");
    add("opt_ps_lin", "線形 (lin)", "Linear (lin)");
    add("opt_tpa_warn_beta", "β は正の値を入力してください (例: 424)。",
        "β must be positive (e.g. 424).");
    add("opt_ps_warn_range",
        "パワー範囲は 0 < P_min ≤ P_max を満たしてください。",
        "Power range must satisfy 0 < P_min ≤ P_max.");
    add("opt_tpa_warn_disabled",
        "不正な値のため TPA を無効にしました (この設定はカーネルに渡しません)。",
        "TPA has been disabled because of the invalid value "
        "(this setting is not passed to the kernel).");
    add("opt_ps_warn_disabled",
        "不正な範囲のためパワースイープを無効にしました "
        "(この設定はカーネルに渡しません)。",
        "The power sweep has been disabled because of the invalid range "
        "(this setting is not passed to the kernel).");
    add("opt_onn_section", "ONN 活性化カーブ (結果)",
        "ONN activation curve (results)");
    add("opt_onn_no_data",
        "BPM ソルバーでパワースイープを実行すると activation_curve.csv "
        "がここに表示されます。",
        "Run the BPM solver with a power sweep to display "
        "activation_curve.csv here.");
    add("opt_onn_loaded", "activation_curve.csv を読み込みました (%1 点)。",
        "Loaded activation_curve.csv (%1 points).");
    add("opt_onn_parse_err", "activation_curve.csv の読み込みに失敗: %1",
        "Failed to read activation_curve.csv: %1");
    add("opt_onn_pin", "P_in [W]", "P_in [W]");
    add("opt_onn_pout", "P_out [W]", "P_out [W]");
    add("opt_onn_trans", "透過率 T", "Transmission T");
    add("opt_onn_measured", "BPM 実測", "BPM computed");
    add("opt_onn_analytic", "解析解 1/(1+βPL/A_eff)",
        "Analytic 1/(1+βPL/A_eff)");
    add("opt_onn_aeff", "A_eff = %1 m²", "A_eff = %1 m²");

    // Acoustic tab
    add("ac_metrics", "室内音響指標", "Room-acoustic metrics");
    add("ac_rt60", "残響時間 RT60", "Reverb time RT60");
    add("ac_c80", "クラリティ C80", "Clarity C80");
    add("ac_d50", "明瞭度 D50", "Definition D50");
    add("ac_sti", "STI (音声明瞭度)", "STI");
    add("ac_edt", "初期減衰時間 EDT", "Early decay time EDT");
    add("ac_irf", "インパルス応答 (IRF)", "Impulse response");
    add("ac_aurora", "可聴化 (オーラリゼーション)", "Auralization");
    add("ac_sample_rate", "サンプリング周波数 [Hz]", "Sample rate [Hz]");
    add("ac_source", "音源", "Source");
    add("ac_directivity", "指向性", "Directivity");
    add("ac_omni", "無指向", "Omni");
    add("ac_cardioid", "カーディオイド", "Cardioid");
    add("ac_speaker", "スピーカー", "Speaker");
    add("ac_spl", "音圧レベル [dB SPL]", "SPL [dB]");
    add("ac_mics", "受音点 / マイクアレイ", "Mic array");
    add("ac_mic_count", "マイク数", "Mic count");
    add("ac_mapping_hint",
        "音響カーネルは .ofd の εr↔(ρ,c) マッピングで実行されます (.ofdx 参照)。",
        "Acoustic kernels map εr↔(ρ,c) on the same .ofd schema (see .ofdx).");

    // Underwater tab
    add("uw_env", "海洋環境", "Ocean environment");
    add("uw_temp", "水温 [°C]", "Temperature [°C]");
    add("uw_salinity", "塩分 [psu]", "Salinity [psu]");
    add("uw_ssp", "音速プロファイル (SSP)", "Sound speed profile");
    add("uw_depth", "深度 [m]", "Depth [m]");
    add("uw_speed", "音速 [m/s]", "c [m/s]");
    add("uw_sofar", "SOFARチャネル", "SOFAR channel");
    add("uw_bottom", "海底特性", "Seabed");
    add("uw_bottom_type", "底質", "Bottom type");
    add("uw_bottom_c", "音速 [m/s]", "c [m/s]");
    add("uw_bottom_rho", "密度 [kg/m³]", "ρ [kg/m³]");
    add("uw_sonar", "ソナー", "Sonar");
    add("uw_freq", "周波数 [kHz]", "Frequency [kHz]");
    add("uw_sl", "音源レベル SL [dB]", "Source level [dB]");
    add("uw_range", "最大距離 [km]", "Max range [km]");

    // tidy3d tab
    add("t3_section", "tidy3d クラウド計算 (光FDTD専用)", "tidy3d Cloud (optical FDTD only)");
    add("t3_hint",
        "tidy3d は Flexcompute 社の光FDTD専用クラウドです。光ドメインのプロジェクトを "
        "Python スクリプトへ変換して送信します。",
        "tidy3d is Flexcompute's photonic-FDTD cloud. The optical project is "
        "exported as a Python script for submission.");
    add("t3_apikey", "APIキー", "API key");
    add("t3_project", "プロジェクト名", "Project name");
    add("t3_resolution", "解像度", "Resolution");
    add("t3_auto_pml", "PML自動設定", "Auto PML");
    add("t3_export", "Pythonスクリプト生成…", "Generate Python script…");
    add("t3_mapping", "自動変換マッピング", "Conversion mapping");

    // Run / status
    add("run_engine", "実行エンジン", "Engine");
    add("run_cpu", "CPU+OpenMP", "CPU+OpenMP");
    add("run_cpu_mpi", "CPU+MPI", "CPU+MPI");
    add("run_gpu", "GPU (CUDA)", "GPU (CUDA)");
    add("run_gpu_mpi", "GPU+MPI", "GPU+MPI");
    add("run_threads", "スレッド数", "Threads");
    add("run_solver_only", "計算のみ (-solver相当)", "Solver only");
    add("run_post_only", "ポストのみ", "Post only");
    add("run_both", "一括 (計算+ポスト)", "Solver + post");

    add("uw_shd_ok", "%1 を生成しました (%2) — TL 音場 (SHDFIL 形式)",
        "Generated %1 (%2) — TL field (SHDFIL format)");
    add("uw_shd_missing",
        "警告: .shd が生成されませんでした (.prt のログを確認してください)",
        "Warning: no .shd was generated (check the .prt log)");
    add("uw_shd_viewer_note",
        "※ .shd の可視化 (TL 図) はアプリ内では未実装です。"
        "bellhopcuda 同梱の MATLAB/Python ツール等で表示してください。",
        "Note: in-app visualization of .shd (TL plots) is not implemented. "
        "Use the MATLAB/Python tools bundled with bellhopcuda.");
    add("run_post_cloud_na",
        "エンジンに「tidy3d Cloud」を選択中はポスト処理を実行できません。\n"
        "ローカルのポスト処理を行うには、エンジンで CPU / MPI / GPU の"
        "いずれかを選択してください。",
        "Post-processing is not available while the engine is set to "
        "\"tidy3d Cloud\".\nSelect CPU / MPI / GPU to run local "
        "post-processing.");
    add("run_rcwa_stack_err",
        "RCWA / FMM の層スタックが空または不正なため実行できません。\n"
        "光タブの「層構成 (入射側 → 透過側)」で 2 層以上の有効な層を"
        "定義してください (orcwa カーネルへの入力が生成できません)。",
        "Cannot run: the RCWA / FMM layer stack is empty or invalid.\n"
        "Define at least two valid layers under \"Layer stack\" on the "
        "Optical tab (no orcwa kernel input can be generated).");
    add("sb_ready", "準備完了", "Ready");
    add("sb_running", "実行中…", "Running…");
    add("sb_failed", "失敗", "Failed");
    add("sb_done", "完了", "Done");

    // Right dock
    add("rd_project", "プロジェクト", "Project");
    add("rd_log", "ログ", "Log");
    add("rd_tree_mesh", "メッシュ", "Mesh");
    add("rd_tree_materials", "物性値", "Materials");
    add("rd_tree_geometry", "物体形状", "Geometry");
    add("rd_tree_sources", "波源", "Sources");
    add("rd_tree_points", "観測点", "Points");
    add("rd_tree_loads", "集中定数", "Lumped");

    // Plot panel / viewport
    add("vp_wire", "ワイヤ", "Wire");
    add("vp_solid", "ソリッド", "Solid");
    add("vp_fit", "フィット", "Fit");
    add("pp_waveform", "波源波形プレビュー", "Source waveform preview");
    add("pp_convergence", "収束特性 (実行ログ)", "Convergence (run log)");
    add("pp_export_csv", "CSV出力", "Export CSV");
    add("pp_export_h5", "HDF5出力", "Export HDF5");
    add("pp_export_s2p", "Touchstone出力 (.s2p)", "Export Touchstone (.s2p)");
    add("s2p_run_first",
        "Touchstone (.snp) が見つかりません。先に plotspara を有効にして"
        "計算・ポスト処理を実行してください。",
        "No Touchstone (.snp) found. Run the solver/post with plotspara "
        "enabled first.");
    add("s2p_copy_failed", "ファイルのコピーに失敗しました。",
        "Failed to copy the Touchstone file.");

    // Glass catalog tab
    add("gc_section", "光学ガラスカタログ", "Optical Glass Catalog");
    add("gc_hint",
        "Schott / Ohara / Hoya / CDGM 等のガラスカタログ。各銘柄の Sellmeier 係数"
        "から任意波長の屈折率を計算します。「物性値リストへ」で光解析の中心波長の "
        "n から εr=n² の材質を追加します。",
        "Glass catalogs (Schott / Ohara / Hoya / CDGM …). Refractive index at any "
        "wavelength is computed from each glass's Sellmeier coefficients. "
        "'Add to materials' converts n at the optical centre wavelength to εr=n².");
    add("gc_search_ph", "🔎 ガラス銘柄を検索 (N-BK7, S-LAH64...)",
        "🔎 Search glass (N-BK7, S-LAH64...)");
    add("gc_all_makers", "全社", "All makers");
    add("gc_import_csv", "📁 CSVカタログ取込", "📁 Import CSV catalog");
    add("gc_import_agf", "📁 Zemax AGF取込 (.agf)", "📁 Import Zemax AGF");
    add("gc_list", "ガラス一覧", "Glass list");
    add("gc_maker", "メーカー", "Maker");
    add("gc_name", "銘柄", "Glass");
    add("gc_price", "価格", "Price");
    add("gc_note", "特徴", "Note");
    add("gc_selected", "選択銘柄", "Selected glass");
    add("gc_dispersion", "分散曲線 n(λ)", "Dispersion n(λ)");
    add("gc_no_sellmeier", "(係数なし — nd/vd 近似)", "(no coeffs — nd/vd approx.)");
    add("gc_add_material", "この銘柄を物性値リストへ", "Add to materials");
    add("gc_add_hint",
        "光解析タブの波長範囲の中心で n を評価し εr = n² の通常媒質として追加します"
        " (分散カーブが必要な場合は波長を変えて再追加)。",
        "Evaluates n at the centre of the optical wavelength range and adds a "
        "normal medium with εr = n².");
    add("gc_abbe", "ガラスマップ / Abbe Diagram (nd vs vd)",
        "Glass map / Abbe diagram (nd vs vd)");
    add("gc_abbe_hint",
        "横軸=アッベ数 vd (右ほど低分散)、縦軸=屈折率 nd。色消しレンズ設計の選定図。"
        "点をクリックで銘柄選択。",
        "x = Abbe number vd (right = lower dispersion), y = index nd. Click a "
        "point to select the glass.");
    add("gc_fmt", "Excel/AGF 取込フォーマット", "Excel/AGF import format");
    add("gc_fmt_body",
        "CSV: ヘッダ行に name(銘柄)/nd [必須], vd, B1..B3, C1..C3, maker。"
        "メーカー Excel カタログは CSV 書き出しで取込。Sellmeier 係数が無い行は "
        "nd/vd の1次近似分散で登録します。\n"
        "AGF: Zemax カタログ (NM/CD レコード)。分散式 2 (Sellmeier1) を係数付きで"
        "取込、他の式は nd/vd のみ登録。",
        "CSV: header with name/nd [required], vd, B1..B3, C1..C3, maker. Export "
        "maker Excel catalogs as CSV. Rows without Sellmeier coefficients fall "
        "back to an nd/vd first-order dispersion.\n"
        "AGF: Zemax catalog (NM/CD records). Formula 2 (Sellmeier1) imports with "
        "coefficients; other formulas import nd/vd only.");
    add("gc_imported", "取込完了: %1 銘柄 (係数なし %2)",
        "Imported %1 glasses (%2 without coefficients)");
    add("gc_added", "%1 を物性値 #%2 として追加しました",
        "Added %1 as material #%2");

    // Room acoustics tab
    add("ra_model_hint",
        "統計モデル (Sabine/Eyring + Barron) と1次鏡像法による設計段階の推定です。"
        "詳細解析は FDTD / 幾何音響カーネルを実行してください。",
        "Design-stage estimates from statistical models (Sabine/Eyring + Barron) "
        "and first-order image sources. Run the FDTD / geometric kernels for "
        "detailed analysis.");
    add("ra_tab_coverage", "客席カバレッジ", "Coverage");
    add("ra_tab_echogram", "エコーグラム", "Echogram");
    add("ra_tab_reverb", "残響計算 (Sabine)", "Reverb (Sabine)");
    add("ra_tab_noise", "暗騒音 NC/NR", "Noise NC/NR");
    add("ra_tab_defects", "音響障害診断", "Defects");
    add("ra_coverage_section", "客席カバレッジマップ", "Audience coverage");
    add("ra_coverage_hint",
        "客席エリア全体に評価指標を分布表示 (EASE/Odeon の mapping 相当)。"
        "席による聞こえ方のばらつきを一目で把握します。",
        "Metric distribution over the audience area (like EASE/Odeon mapping) — "
        "seat-to-seat variation at a glance.");
    add("ra_metric", "表示指標", "Metric");
    add("ra_band", "周波数帯", "Band");
    add("ra_band_avg", "平均", "Average");
    add("ra_map_section", "分布マップ", "Distribution");
    add("ra_seat_section", "席別評価", "Per-seat detail");
    add("ra_receiver", "受音点", "Receiver");
    add("ra_verdict", "判定", "Verdict");
    add("ra_p1", "P1 中央前列", "P1 front centre");
    add("ra_p2", "P2 左サイド", "P2 left side");
    add("ra_p3", "P3 右サイド", "P3 right side");
    add("ra_p4", "P4 後方中央", "P4 rear centre");
    add("ra_mean", "平均", "mean");
    add("ra_uniformity", "均一性", "uniformity");
    add("ra_good", "良好", "good");
    add("ra_excellent", "優", "excellent");
    add("ra_fair", "可", "fair");
    add("ra_check", "要確認", "check");
    add("ra_echo_section", "エコーグラム (反射音列)", "Echogram");
    add("ra_echo_hint",
        "受音点に届く直接音と1次反射音を時系列で表示。初期反射の到来パターンが"
        "音場の質を決めます (シューボックス鏡像法)。",
        "Direct sound and first-order reflections at the receiver (shoebox "
        "image-source method). The early-reflection pattern shapes the sound field.");
    add("ra_reflectogram", "反射音列 / Reflectogram", "Reflectogram");
    add("ra_time_ms", "時間 [ms]", "time [ms]");
    add("ra_level_db", "レベル [dB]", "level [dB]");
    add("ra_refl_section", "反射音解析", "Reflection analysis");
    add("ra_reflection", "反射", "Reflection");
    add("ra_delay_ms", "遅延 [ms]", "Delay [ms]");
    add("ra_refl_surface", "到来面", "Surface");
    add("ra_direct", "直接音", "Direct");
    add("ra_early", "初期反射", "Early");
    add("ra_late", "後期", "Late");
    add("ra_beneficial", "有益", "Beneficial");
    add("ra_echo_risk", "エコー注意", "Echo risk");
    add("ra_itdg_good", "親密感 良好, <25ms 目安", "intimate, target <25ms");
    add("ra_itdg_far", ">25ms — 初期反射が遠い", ">25ms — first reflection late");
    add("ra_reverb_section", "残響時間計算 (Sabine / Eyring)",
        "Reverberation (Sabine / Eyring)");
    add("ra_reverb_hint",
        "室の体積と吸音面積から残響時間を計算。RT = 0.161·V / A (Sabine)。"
        "α は 125/500/1k/4k を編集 (250/2k は隣接帯域から補間)。",
        "Reverberation from volume and absorption: RT = 0.161·V / A (Sabine). "
        "Edit α at 125/500/1k/4k (250/2k interpolated).");
    add("ra_room_dims", "室寸法 L×W×H", "Room L×W×H");
    add("ra_from_dims", "寸法から V,S 再計算", "Recompute V,S from dims");
    add("ra_volume", "室容積 V", "Volume V");
    add("ra_surface", "総表面積 S", "Surface S");
    add("ra_occupancy", "満席率", "Occupancy");
    add("ra_occ_empty", "空席", "Empty");
    add("ra_occ_half", "半分", "Half");
    add("ra_occ_full", "満席", "Full");
    add("ra_formula", "計算式", "Formula");
    add("ra_eyring", "Eyring (高吸音向)", "Eyring (high absorption)");
    add("ra_budget_section", "吸音バジェット", "Absorption budget");
    add("ra_element", "面・要素", "Element");
    add("ra_area", "面積[m²]", "Area [m²]");
    add("ra_rt_section", "残響時間結果 (帯域別)", "RT60 by band");
    add("ra_rt_targets",
        "用途目安: オペラ 1.4-1.6s / 交響楽 1.8-2.1s / 室内楽 1.4-1.7s / 講演 0.8-1.1s",
        "Targets: opera 1.4-1.6s / symphony 1.8-2.1s / chamber 1.4-1.7s / "
        "speech 0.8-1.1s");
    add("ra_noise_section", "暗騒音評価 (NC)", "Background noise (NC)");
    add("ra_noise_hint",
        "空調・設備騒音のオクターブ帯域レベルを入力し NC 曲線で判定。"
        "ホールは NC-15〜25, オフィスは NC-35〜40 が目安。",
        "Enter octave-band HVAC/equipment noise; rated against NC curves. "
        "Halls target NC-15–25, offices NC-35–40.");
    add("ra_noise_row", "騒音 [dB]", "Noise [dB]");
    add("ra_nc_section", "オクターブ帯域騒音レベル", "Octave-band noise");
    add("ra_octave_hz", "オクターブ中心周波数 [Hz] (log)", "Octave centre [Hz] (log)");
    add("ra_measured", "測定値", "Measured");
    add("ra_nc_hall_ok", "ホール基準域", "within hall range");
    add("ra_nc_high", "ホール基準超過", "above hall target");
    add("ra_nc_guide",
        "評価はタンジェント法 (全帯域で NC 曲線を超えない最小の値)。",
        "Tangency method: the lowest NC curve not exceeded in any band.");
    add("ra_defect_section", "音響障害診断", "Acoustic defect detection");
    add("ra_defect_hint",
        "フラッターエコー (低吸音の平行面) とロングディレイエコー (>50ms の強反射) を"
        "室モデルから自動検出します。",
        "Detects flutter echo (hard parallel faces) and long-delay echo "
        "(strong reflection >50ms) from the room model.");
    add("ra_defect", "障害", "Defect");
    add("ra_place", "場所", "Location");
    add("ra_cause", "原因", "Cause");
    add("ra_severity", "深刻度", "Severity");
    add("ra_recommend_section", "改善提案", "Recommendations");
    add("ra_rec_flutter",
        "平行壁面に拡散体(QRD)または吸音材を配置 → フラッター解消",
        "Add diffusers (QRD) or absorption on parallel walls → removes flutter");
    add("ra_rec_echo",
        "後壁を吸音化または傾斜 → ロングディレイエコー除去",
        "Absorb or tilt the rear wall → removes long-delay echo");
    add("ra_rec_none", "検出された障害はありません。", "No defects detected.");
    add("ra_defect_note",
        "音の焦点 (凹面) やバルコニー下の死角は曲面・遮蔽形状の解析が必要なため、"
        "本タブのシューボックスモデルでは対象外です。",
        "Sound focusing (concave surfaces) and balcony shadowing need curved/"
        "occluding geometry and are outside this shoebox model.");
    add("ra_export", "出力", "Export");
    add("ra_export_report", "📄 音響設計レポート (Markdown)", "📄 Report (Markdown)");
    add("ra_export_png", "📊 分布マップ (PNG)", "📊 Coverage map (PNG)");
    add("ra_report_title", "音響設計レポート (OpenFDTD-X ホール解析)",
        "Room-acoustics design report (OpenFDTD-X)");

    // RIR analysis tab (measured impulse response)
    add("t_riranalysis", "🎤 実測RIR分析", "🎤 Measured RIR");
    add("rir_model_hint",
        "実測した室内インパルス応答 (RIR) の WAV ファイルを ISO 3382-1 に基づき"
        "分析します。「🏛 ホール解析」タブは統計モデルによる設計段階の推定、"
        "本タブは実測RIRの分析です (FDTD 実行によるシミュレーションRIRの分析は"
        "別途カーネル出力に対して行います)。",
        "Analyzes a measured room impulse response (RIR) WAV per ISO 3382-1. "
        "The Hall Analysis tab gives design-stage statistical estimates; this "
        "tab analyzes measured RIRs (simulated RIRs from FDTD runs are "
        "analyzed separately on kernel output).");
    add("rir_input_section", "入力 (実測RIR)", "Input (measured RIR)");
    add("rir_file", "RIR WAVファイル", "RIR WAV file");
    add("rir_file_placeholder", "WAV ファイルを選択…", "Choose a WAV file…");
    add("rir_browse", "参照…", "Browse…");
    add("rir_wav_filter", "WAV ファイル (*.wav);;すべてのファイル (*)",
        "WAV files (*.wav);;All files (*)");
    add("rir_channel", "チャンネル", "Channel");
    add("rir_ch_left", "L", "L");
    add("rir_ch_right", "R", "R");
    add("rir_ch_mono", "平均モノ", "Averaged mono");
    add("rir_calibration", "校正状態", "Calibration");
    add("rir_calib_absolute", "絶対校正済 (SPL 換算可)", "Absolute (SPL known)");
    add("rir_calib_relative", "相対校正", "Relative");
    add("rir_calib_uncalibrated", "未校正", "Uncalibrated");
    add("rir_calib_offset", "校正オフセット", "Calibration offset");
    add("rir_calib_offset_tip",
        "基準音源のレベルが既知のときに、dBFS を dB SPL へ換算する"
        "オフセット (dB SPL = dBFS + オフセット)。"
        "校正状態が「絶対校正済」のときのみ有効です。",
        "Offset that converts dBFS to dB SPL when the level of a reference "
        "source is known (dB SPL = dBFS + offset). "
        "Only applied when the calibration state is Absolute.");
    add("rir_direct_method", "直接音検出", "Direct-sound detection");
    add("rir_dm_peak", "ピーク", "Peak");
    add("rir_dm_envelope", "包絡線閾値", "Envelope threshold");
    add("rir_dm_movingrms", "移動RMS閾値", "Moving-RMS threshold");
    add("rir_band_mode", "帯域モード", "Band mode");
    add("rir_bm_compat6", "互換6帯域 (125Hz-4kHz)", "Compat 6 bands (125Hz-4kHz)");
    add("rir_bm_oct", "1オクターブ (63Hz-8kHz)", "1 octave (63Hz-8kHz)");
    add("rir_bm_thirdoct", "1/3オクターブ (100Hz-5kHz)", "1/3 octave (100Hz-5kHz)");
    add("rir_bm_formant", "歌手フォルマント帯域 (2-4kHz)",
        "Singer-formant bands (2-4kHz)");
    add("rir_noise_correction", "ノイズ補正 (Chu 1978)",
        "Noise correction (Chu 1978)");
    add("rir_min_dr", "最小動的範囲", "Minimum dynamic range");
    add("rir_run_section", "実行", "Run");
    add("rir_run", "▶ RIR を分析", "▶ Analyze RIR");
    add("rir_status_idle", "WAV ファイルを選択して実行してください。",
        "Choose a WAV file and run the analysis.");
    add("rir_status_nofile", "RIR WAV ファイルが選択されていません。",
        "No RIR WAV file selected.");
    add("rir_status_error", "分析失敗: %1 — %2", "Analysis failed: %1 — %2");
    add("rir_status_ok",
        "分析完了 — 総合品質: %1 / 動的範囲 %2 dB / 直接音 %3 ms",
        "Done — overall quality: %1 / dynamic range %2 dB / direct sound %3 ms");
    add("rir_result_section", "結果 (ISO 3382-1 指標)", "Results (ISO 3382-1)");
    add("rir_metric", "指標", "Metric");
    add("rir_band", "帯域", "Band");
    add("rir_value", "値", "Value");
    add("rir_unit", "単位", "Unit");
    add("rir_quality", "品質", "Quality");
    add("rir_note", "備考", "Note");
    add("rir_not_computable", "算出不可", "Not computable");
    add("rir_q_valid", "有効", "Valid");
    add("rir_q_warning", "参考", "Warning");
    add("rir_q_invalid", "無効", "Invalid");
    add("rir_plot_section", "プロット", "Plots");
    add("rir_waveform", "波形 (分析チャンネル)", "Waveform (analyzed channel)");
    add("rir_decay", "エネルギー減衰曲線 (Schroeder)",
        "Energy decay curve (Schroeder)");
    add("rir_decay_label", "減衰", "decay");
    add("rir_noise_floor", "ノイズフロア", "noise floor");
    add("rir_time_ms", "時間 [ms]", "time [ms]");
    add("rir_amplitude", "振幅", "amplitude");
    add("rir_level_db", "レベル [dB]", "level [dB]");
    add("rir_refl_section", "初期反射一覧 (0-20 / 20-80 / 80-200 / 200+ ms)",
        "Early reflections (0-20 / 20-80 / 80-200 / 200+ ms)");
    add("rir_refl_no", "#", "#");
    add("rir_refl_time", "時刻 [ms]", "Arrival [ms]");
    add("rir_refl_delay", "遅延 [ms]", "Delay [ms]");
    add("rir_refl_level", "相対レベル [dB]", "Rel. level [dB]");
    add("rir_refl_bin", "区分", "Bin");
    add("rir_export_section", "出力", "Export");
    add("rir_export_csv", "📄 CSV 保存", "📄 Save CSV");
    add("rir_export_json", "📄 JSON 保存", "📄 Save JSON");

    // Vocal analysis tab (single unaccompanied monophonic singing)
    add("t_vocalanalysis", "🎶 歌声分析", "🎶 Vocal Analysis");
    add("vocal_scope_note",
        "単一・無伴奏・モノフォニック歌唱の WAV が分析対象です (合唱・伴奏付き・"
        "ポリフォニック音源は対象外)。本タブは物理量の測定のみを行い、声種は "
        "F0 探索範囲プリセットの選択にのみ使用します。",
        "Analyzes a WAV of a single unaccompanied monophonic voice (choirs, "
        "accompanied or polyphonic sources are out of scope). This tab only "
        "measures physical quantities; the voice type merely selects the F0 "
        "search-range preset.");
    add("vocal_input_section", "入力 (歌唱 WAV)", "Input (singing WAV)");
    add("vocal_file", "歌唱 WAVファイル", "Singing WAV file");
    add("vocal_voice_type", "声種 (F0探索範囲)", "Voice type (F0 search range)");
    add("vocal_vt_soprano", "ソプラノ (220-1400 Hz)", "Soprano (220-1400 Hz)");
    add("vocal_vt_mezzo", "メゾソプラノ (180-1000 Hz)",
        "Mezzo-soprano (180-1000 Hz)");
    add("vocal_vt_contralto", "アルト (150-800 Hz)", "Contralto (150-800 Hz)");
    add("vocal_vt_tenor", "テノール (120-700 Hz)", "Tenor (120-700 Hz)");
    add("vocal_vt_baritone", "バリトン (90-500 Hz)", "Baritone (90-500 Hz)");
    add("vocal_vt_bass", "バス (70-400 Hz)", "Bass (70-400 Hz)");
    add("vocal_vt_unknown", "不明 (60-1500 Hz)", "Unknown (60-1500 Hz)");
    add("vocal_f0_min", "F0 探索下限の上書き", "F0 search min override");
    add("vocal_f0_max", "F0 探索上限の上書き", "F0 search max override");
    add("vocal_f0_auto", "自動 (声種プリセット)", "Auto (voice-type preset)");
    add("vocal_calibration", "校正状態 (実測RIR分析タブで設定)",
        "Calibration (set in Measured RIR tab)");
    add("vocal_calib_spl_ok", "SPL 系指標を算出します",
        "SPL metrics will be computed");
    add("vocal_calib_spl_na", "SPL 系指標は算出不可 (dBFS 相対値のみ)",
        "SPL metrics not computable (dBFS only)");
    add("vocal_run_section", "実行", "Run");
    add("vocal_run", "▶ 歌声を分析", "▶ Analyze voice");
    add("vocal_status_idle", "歌唱 WAV ファイルを選択して実行してください。",
        "Choose a singing WAV file and run the analysis.");
    add("vocal_status_nofile", "歌唱 WAV ファイルが選択されていません。",
        "No singing WAV file selected.");
    add("vocal_status_error", "分析失敗: %1 — %2", "Analysis failed: %1 — %2");
    add("vocal_status_ok",
        "分析完了 — 総合品質: %1 / 有声率 %2% / F0中央値 %3",
        "Done — overall quality: %1 / voiced %2% / F0 median %3");
    add("vocal_result_section", "結果 (歌声指標)", "Results (vocal metrics)");
    add("vocal_plot_section", "プロット", "Plots");
    add("vocal_f0_plot", "F0 軌跡 (無声区間は欠落)",
        "F0 contour (gaps = unvoiced)");
    add("vocal_formant_plot",
        "フォルマント軌跡 F1/F2/F3 (LPC、有声区間のみ)",
        "Formant tracks F1/F2/F3 (LPC, voiced only)");
    add("vocal_ltas_plot", "LTAS (長時間平均スペクトル)",
        "LTAS (long-term average spectrum)");
    add("vocal_time_s", "時間 [s]", "time [s]");
    add("vocal_f0_hz", "F0 [Hz]", "F0 [Hz]");
    add("vocal_formant_hz", "フォルマント [Hz]", "formant [Hz]");
    add("vocal_freq_hz", "周波数 [Hz]", "frequency [Hz]");

    // Auralization tab (dry voice × RIR convolution)
    add("t_auralization", "🔊 可聴化", "🔊 Auralization");
    add("aur_model_hint",
        "ドライ (無響/近接収録) 歌唱 WAV と実測 RIR WAV を畳み込み、ホールで"
        "歌ったときの音をウェット WAV (float32) として書き出します。自動正規化"
        "とリサンプリングは行いません (fs 不一致はエラーになります)。",
        "Convolves a dry (anechoic/close-mic) singing WAV with a measured RIR "
        "WAV and writes the wet result as a float32 WAV. No automatic "
        "normalization or resampling is performed (a sample-rate mismatch is "
        "an error).");
    add("aur_input_section", "入力", "Input");
    add("aur_dry_file", "ドライ歌唱 WAV", "Dry singing WAV");
    add("aur_rir_file", "RIR WAV (実測RIR分析タブと共用)",
        "RIR WAV (shared with Measured RIR tab)");
    add("aur_output_file", "出力 WAV (ウェット)", "Output WAV (wet)");
    add("aur_output_placeholder", "保存先を選択…", "Choose an output file…");
    add("aur_gain_mode", "ゲインモード", "Gain mode");
    add("aur_gain_asis", "そのまま (ゲイン適用なし)", "As-is (no gain)");
    add("aur_gain_suggested", "推奨ゲイン適用 (ピーク→フルスケール)",
        "Apply suggested gain (peak → full scale)");
    add("aur_run_section", "実行", "Run");
    add("aur_run", "▶ 畳み込みを実行", "▶ Convolve");
    add("aur_status_idle",
        "ドライ WAV と RIR WAV を選択して実行してください。",
        "Choose a dry WAV and an RIR WAV, then run.");
    add("aur_status_nofile",
        "ドライ WAV または RIR WAV が選択されていません。",
        "Dry WAV or RIR WAV not selected.");
    add("aur_status_nooutput", "出力 WAV の保存先が指定されていません。",
        "No output WAV file chosen.");
    add("aur_status_error", "畳み込み失敗: %1 — %2",
        "Convolution failed: %1 — %2");
    add("aur_no_resample_note",
        "サンプルレートが一致していません。本機能はリサンプリングを行わない"
        "ため、外部ツールで fs を揃えてから再実行してください。",
        "Sample rates do not match. This feature does not resample; convert "
        "the files to a common rate externally and retry.");
    add("aur_status_ok", "完了 — 出力: %1", "Done — output: %1");
    add("aur_result_section", "結果", "Results");
    add("aur_output_peak", "出力ピーク", "Output peak");
    add("aur_suggested_gain", "推奨ゲイン", "Suggested gain");
    add("aur_clipped_samples", "クリップ", "Clipping");
    add("aur_clipped_yes", "あり (%1 サンプル)", "Yes (%1 samples)");
    add("aur_clipped_no", "なし", "None");
    add("aur_gain_applied", "(適用済み)", "(applied)");
    add("aur_post_gain_note",
        "出力ピークとクリップ数は、ゲイン適用後に書き出した WAV の"
        "サンプルで測った値です。",
        "Output peak and clipping count are measured on the written WAV "
        "samples, after the gain was applied.");
    add("aur_ab_section", "A/B 比較 (波形)", "A/B comparison (waveforms)");
    add("aur_dry_wave", "ドライ (入力)", "Dry (input)");
    add("aur_wet_wave", "ウェット (出力)", "Wet (output)");
    add("aur_playback_note",
        "アプリ内での再生は未対応です。書き出した WAV を外部プレイヤーで"
        "ドライ音源と聴き比べてください。",
        "In-app playback is not supported. Compare the exported WAV with the "
        "dry source in an external audio player.");

    // ev viewer
    add("ev_backend", "表示バックエンド", "Viewer backend");
    add("ev_html", "HTML出力 (-html) をブラウザで開く", "Open -html output in browser");
    add("ev_process", "外部ビューワー (ev2d/ev3d) を起動", "Launch external viewer");
    add("ev_native", "ネイティブ描画 (未実装)", "Native rendering (TODO)");
    add("ev_open2d", "ev2d を開く", "Open ev2d");
    add("ev_open3d", "ev3d を開く", "Open ev3d");
    add("ev_nofile", "出力ファイルが見つかりません。先にポスト処理を実行してください。",
        "No output file found. Run post-processing first.");

    // ── カテゴリ付き左ナビ (app.jsx LeftDock 相当) ──────────────────────────
    add("cat_setup",   "セットアップ / Setup",  "Setup");
    add("cat_library", "ライブラリ / Library",  "Library");
    add("cat_solve",   "解析 / Solve",          "Solve");
    add("cat_post",    "ポスト / Post",         "Post");
    add("cat_dom_em",         "電磁ドメイン", "EM domain");
    add("cat_dom_optical",    "光ドメイン",   "Optical domain");
    add("cat_dom_acoustic",   "音響ドメイン", "Acoustic domain");
    add("cat_dom_underwater", "水中ドメイン", "Underwater domain");

    // nav ラベル (モックのタブ名そのまま)
    add("nav_geometry",     "① 形状",        "① Geometry");
    add("nav_material",     "② 物性値",      "② Materials");
    add("nav_solverregion", "③ ソルバ領域",  "③ Solver region");
    add("nav_source",       "④ 波源",        "④ Sources");
    add("nav_monitors",     "⑤ モニター",    "⑤ Monitors");
    add("nav_general",      "全般",           "General");
    add("nav_mesh",         "メッシュ詳細",   "Mesh detail");
    add("nav_perface",      "境界面詳細",     "Per-face BC");
    add("nav_components",   "コンポーネント", "Components");
    add("nav_matexplorer",  "🔬 材料Explorer", "🔬 Material Explorer");
    add("nav_glasscatalog", "🔷 ガラスカタログ", "🔷 Glass Catalog");
    add("nav_lens",         "Lens",           "Lens");
    add("nav_layoutgds",    "GDS",            "GDS");
    add("nav_schematic",    "Schematic",      "Schematic");
    add("nav_photonics",    "FDTD/RCWA/BPM/FMM", "FDTD/RCWA/BPM/FMM");
    add("nav_modesolver",   "〓 モードソルバ FDE", "〓 Mode Solver FDE");
    add("nav_acsource",     "🎤 音源/WAV/指向性", "🎤 Source/WAV/Directivity");
    add("nav_audioedit",    "🎚 音響編集・解析", "🎚 Audio Editor");
    add("nav_oceanenv",     "🌏 海洋環境",    "🌏 Ocean Environment");
    add("nav_roomac",       "🏛 ホール解析",  "🏛 Hall Analysis");
    add("nav_acsolver",     "🔌 音響ソルバ連携", "🔌 Acoustic Solver");
    add("nav_soundproof",   "🔇 防音設計",    "🔇 Soundproofing");
    add("nav_outdoor",      "🌳 屋外騒音",    "🌳 Outdoor Noise");
    add("nav_cabin",        "🚗 車内NVH",     "🚗 Cabin NVH");
    add("nav_ultrasound",   "🩺 超音波",      "🩺 Ultrasound");
    add("nav_family",       "🌳 姉妹ソルバ",  "🌳 Solver Family");
    add("nav_solver",       "ソルバ詳細",     "Solver detail");
    add("nav_verification", "🔍 検証",        "🔍 Verification");
    add("nav_optimize",     "最適化・スイープ", "Optimization && sweep");
    add("nav_tolerance",    "ばらつき",       "Tolerance");
    add("nav_scripts",      "スクリプト",     "Scripts");
    add("nav_multiphysics", "マルチフィジクス", "Multiphysics");
    // 応用画面 (mock: em-applications.jsx / optical-applications.jsx)
    add("nav_emc",        "📜 EMC/EMI",        "📜 EMC/EMI");
    add("nav_sar",        "🧬 SAR/生体",       "🧬 SAR / bio");
    add("nav_channel",    "📶 電波伝搬",       "📶 Propagation");
    add("nav_thinfilm",   "🌈 多層膜コート",   "🌈 Thin-film coating");
    add("nav_illum",      "💡 照明/測色",      "💡 Illumination / color");
    // モックのラベルは「ディスプレAR/VR」で「イ」が欠落しているため補正
    add("nav_displayopt", "👓 ディスプレイAR/VR", "👓 Display AR/VR");
    add("nav_tidy3d",       "☁ tidy3d",       "☁ tidy3d");
    add("nav_analysisgroups", "解析グループ", "Analysis groups");
    add("nav_datasets",     "Datasets",       "Datasets");
    add("nav_h5viewer",     "🎬 H5アニメ",    "🎬 H5 Animation");
    add("nav_interop",      "🔗 ツール連携",  "🔗 Interop");
    add("nav_antennachar",  "📡 アンテナ特性", "📡 Antenna Char.");
    add("nav_txline",       "🔌 伝送線路",    "🔌 Transmission Line");
    add("nav_scattering",   "🎯 散乱/RCS",    "🎯 Scattering/RCS");
    add("nav_circuit",      "⚡ 回路 PEEC/FEM", "⚡ Circuit PEEC/FEM");
    add("nav_post1",        "ポスト(1)",      "Post (1)");
    add("nav_post2",        "ポスト(2)",      "Post (2)");
    add("nav_optical",      "光解析",         "Optical");
    add("nav_acoustic",     "音響解析",       "Acoustic");
    add("nav_underwater",   "水中音響",       "Underwater");

    // 表示モード (標準 / エキスパート)
    add("m_uilevel",        "表示モード",     "UI level");
    add("uilevel_standard", "標準",           "Standard");
    add("uilevel_expert",   "エキスパート (全機能)", "Expert (all features)");

    // ツールバー追加分 (Resources / はじめに / ギャラリー)
    add("tb_resources",  "Resources", "Resources");
    add("tb_gettingstarted", "🎓 はじめに", "🎓 Getting Started");
    add("tb_gallery_tip", "新規 — アプリケーションギャラリー",
        "New — Application Gallery");
    add("run_console",   "計算コンソール", "Run console");
    add("run_engine_tidy3d", "☁ tidy3d Cloud", "☁ tidy3d Cloud");
    add("run_device", "デバイス番号", "Device no.");
    add("log_starting", "計算開始", "Starting computation");
    add("gsd_expert_switched",
        "検証タブ表示のためエキスパート表示に切替えました",
        "Switched to expert mode to show the verification tab");
    add("log_h5_slice",
        "HDF5 結果 (%1) を 2D 断面ビューに反映しました",
        "Loaded the HDF5 result (%1) into the 2D slice view");
    add("log_freqchar",
        "%1 の給電点特性 (%2 点) を結果プロットに反映しました",
        "Loaded the feed-point response from %1 (%2 points) into the result "
        "plot");
    add("log_farpattern",
        "far1d.log の遠方界パターン (%1 面) を結果プロットに反映しました",
        "Loaded the far-field pattern from far1d.log (%1 cuts) into the "
        "result plot");
    add("log_h5_found",
        "既存の HDF5 結果を検出し H5アニメタブへセットしました: %1",
        "Found an existing HDF5 result and set it on the H5 Animation "
        "tab: %1");
    add("log_h5_anime",
        "伝搬の時系列は「H5アニメ」タブで再生できます (実行結果を読込済み)",
        "Play the propagation time series in the H5 Animation tab (this "
        "run's output is already loaded)");
    add("log_ev_ready",
        "作図出力 %1 を確認 — 下部の「ev2d を開く / ev3d を開く」で表示できます",
        "Plot output %1 is ready — open it with the ev2d / ev3d buttons at "
        "the bottom");
    add("log_validate", "入力データを検証中…", "Validating input data…");

    // 編集メニュー
    add("me_undo", "元に戻す", "Undo");
    add("me_redo", "やり直す", "Redo");
    add("me_undo_na", "このビルドでは元に戻す操作に未対応です。",
        "Undo is not supported in this build.");
    add("me_select_tab", "タブ一覧へフォーカス", "Focus tab list");

    // テーマ / 密度 / UIスタイル (モック TweaksPanel)
    add("m_theme",   "テーマ",       "Theme");
    add("m_density", "密度",         "Density");
    add("m_uistyle", "UI スタイル",  "UI style");
    add("theme_light", "ライト", "Light");
    add("theme_dark",  "ダーク", "Dark");
    add("density_compact",     "Compact",     "Compact");
    add("density_normal",      "Normal",      "Normal");
    add("density_comfortable", "Comfortable", "Comfortable");
    add("uistyle_classic",    "Qt Fusion (Classic)", "Qt Fusion (Classic)");
    add("uistyle_modern",     "Qt Modern",           "Qt Modern");
    add("uistyle_scientific", "Scientific (Dark)",   "Scientific (Dark)");

    // ステータスバー
    add("sb_dt", "Δt", "Δt");
    add("sb_cfl", "CFL", "CFL");
}
