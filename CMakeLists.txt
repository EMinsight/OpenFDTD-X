cmake_minimum_required(VERSION 3.21)
project(OpenFDTD_X
    VERSION 1.0.0
    DESCRIPTION "Multi-Domain FDTD GUI (EM / Optical / Acoustic / Underwater)"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt6 Widgets is the only hard requirement (classic C++ Qt, NOT QML/Quick).
find_package(Qt6 REQUIRED COMPONENTS Widgets)
# Qt6::Network は「あれば使う」— 海洋データセットの直接ダウンロードだけに
# 使う。qt6-base-dev / Homebrew qt / Windows の Qt 6.8 いずれにも同梱
# されているが、見つからない構成でもビルドは通り、その場合ダイアログは
# 従来どおり「配布ページを開く + 手動取込」だけになる。
find_package(Qt6 COMPONENTS Network QUIET)

option(USE_HDF5   "Enable HDF5 time-series export"            OFF)
option(USE_LIBIGL "Use libigl for STL import + voxelization"  OFF)
option(BUILD_TESTS "Build the .ofd round-trip self test"      ON)
option(BUILD_OPENUWA "Build the OpenUWA standalone underwater app" ON)

if(USE_HDF5)
    find_package(HDF5 REQUIRED COMPONENTS C)
endif()

if(USE_LIBIGL)
    include(FetchContent)
    set(LIBIGL_GLFW OFF CACHE BOOL "")
    set(LIBIGL_IMGUI OFF CACHE BOOL "")
    set(LIBIGL_OPENGL OFF CACHE BOOL "")
    set(LIBIGL_PNG OFF CACHE BOOL "")
    FetchContent_Declare(libigl
        GIT_REPOSITORY https://github.com/libigl/libigl.git
        GIT_TAG v2.5.0
    )
    FetchContent_MakeAvailable(libigl)
endif()

set(CORE_SOURCES
    src/core/Project.cpp
    src/core/Project.h
    src/core/Domain.h
    # コンポーネントライブラリの部品表とドメイン許可表 (ヘッダのみ)。
    # ComponentsTab / Viewport3D / selftest が共有する (二重管理の防止)
    src/core/ComponentCatalog.h
    # 外部音響ソルバー起動前の入力点検 (室外の音源・受音点など) —
    # AcousticSolverTab と selftest が共有する
    src/core/AcousticPreflight.cpp
    src/core/AcousticPreflight.h
    # 左ナビのタブキー → カテゴリ対応表 (ヘッダのみ)。
    # MainWindow::buildLeftNav / selftest が共有する (二重管理の防止)
    src/core/NavCatalog.h
    src/core/Material.h
    src/core/Geometry.h
    src/core/Source.h
    src/core/MeshAxis.h
    src/core/PostOpts.h
    src/core/GlassCatalog.cpp
    src/core/GlassCatalog.h
    src/core/RoomAcoustics.cpp
    src/core/RoomAcoustics.h
    # 受音点別 RIR WAV の自動割当 (可聴化タブ⑤の補助) — Qt Core のみ。
    # 対応規則 (完全一致 / rir 接頭接尾 / 唯一の rir.wav) を selftest から
    # テーブル駆動で検証する
    src/core/RirAutoAssign.cpp
    src/core/RirAutoAssign.h
    src/core/OperaHalls.h
    src/core/ProjectTemplates.cpp
    src/core/ProjectTemplates.h

    # FDTD 設定の検証計算 (メッシュ解像度計画 / 吸収境界の設計反射率 /
    # 収束履歴の解析 / 安定条件・分解能の判定) — Qt 非依存。
    # VerificationTab の計算実体で、selftest から解析解と直接突き合わせる
    src/core/FdtdVerification.cpp
    src/core/FdtdVerification.h

    # 製造ばらつきの入力分布 (正規/一様/レイリーの解析形) — Qt 非依存。
    # ToleranceTab の計算実体で、selftest からモーメント・被覆確率を検証する
    src/core/ToleranceStats.cpp
    src/core/ToleranceStats.h

    # 製造ばらつきのサンプリング (乱数 / ラテン超方格) と結果統計。
    # ToleranceStats が入力分布、こちらが標本と FoM 側 — Qt 非依存。
    src/core/MonteCarlo.cpp
    src/core/MonteCarlo.h

    # 音源の向き文字列 ("-Z 30°" 等) → 単位ベクトル。
    # 3D シーンの法線矢印と音源タブが共用する
    src/core/AimDirection.cpp
    src/core/AimDirection.h

    # 実材料の分散 (公刊 Sellmeier) — Qt 非依存。selftest からも直接検証する
    src/optics/MaterialDispersion.cpp
    src/optics/MaterialDispersion.h

    # 多層薄膜の特性行列法 (Abeles) — Qt 非依存。ThinFilmTab の計算実体で、
    # selftest から解析解 (フレネル / 単層無反射 / 四分の一波長積層) と照合する
    src/optics/ThinFilmStack.cpp
    src/optics/ThinFilmStack.h

    # 多層膜の周期記法 (Macleod 記法 `(H L)^12 H`) のパーサ — Qt 非依存
    src/optics/FilmNotation.cpp
    src/optics/FilmNotation.h

    # 自由キャリア (Drude / Soref-Bennett) の Δn・Δα — Qt 非依存。
    # MultiphysicsTab のプラズマ効果節の計算実体 (selftest から極限値と照合)
    src/optics/PlasmaDispersion.cpp
    src/optics/PlasmaDispersion.h

    # SAR の定義式と電波防護指針の指針値 (ICNIRP 2020 / IEEE C95.1 / FCC) —
    # Qt 非依存。SarTab の計算・規格値の実体 (selftest から手計算値と照合)
    src/em/SarMetrics.cpp
    src/em/SarMetrics.h

    # 見通し内リンクの電波伝搬モデル (Friis / 2 波 / 熱雑音 / Shannon) —
    # Qt 非依存。ChannelTab のチャネル特性表の計算実体で、selftest から
    # ITU-R P.525 の式・自由空間極限と照合する
    src/em/RadioPropagation.cpp
    src/em/RadioPropagation.h

    # EMC 規格の公表限度値 (CISPR 32 / FCC §15.109) とシールド遮蔽効果・
    # 挿入損失・ESD 電流の古典式 — Qt 非依存。EmcTab の数値の実体で、
    # selftest から規格値・定義式と照合する
    src/em/EmcStandards.cpp
    src/em/EmcStandards.h

    # 集中定数 RLC の |Z(f)| (直列/並列の解析式) — Qt 非依存。
    # CircuitSolversTab の結果ページの計算実体
    src/em/LumpedRlc.cpp
    src/em/LumpedRlc.h

    # ソルバ選定の目安 (電気サイズ / 分解能 / Schroeder 周波数 / Thorp 吸収) —
    # Qt 非依存。SolverSelectorTab の数値の計算実体
    src/core/SolverSelection.cpp
    src/core/SolverSelection.h

    src/io/OfdIO.cpp
    src/io/OfdIO.h
    src/io/ActivationCurve.cpp
    src/io/ActivationCurve.h
    src/io/BellhopIO.cpp
    src/io/BellhopIO.h
    # BELLHOP の到達ファイル (.arr) 読み取りと受信インパルス応答の合成。
    # 水中の可聴化の入口 — selftest から合成結果を直接検証する
    src/io/ArrReader.cpp
    src/io/ArrReader.h
    # 回路パラメータ抽出ソルバ (OpenPEEC / OpenFEM) の入力生成。
    # CircuitSolversTab と selftest が共有する
    src/io/CircuitIO.cpp
    src/io/CircuitIO.h
    src/io/BathymetryIO.cpp
    src/io/BathymetryIO.h
    # 配布ページ (HTML) からデータファイルへのリンクを抜き出す (Qt Core のみ)。
    # OeDownloadManager と selftest が共有する
    src/io/PageLinkScanner.cpp
    src/io/PageLinkScanner.h
    src/io/ShdReader.cpp
    src/io/ShdReader.h
    src/io/Touchstone.cpp
    src/io/Touchstone.h
    src/io/Tidy3dExporter.cpp
    src/io/Tidy3dExporter.h
    src/io/H5Writer.cpp
    src/io/H5Writer.h
    # 動画書き出しの純関数 (時間範囲→フレーム番号 / ffmpeg 引数の組み立て) —
    # ffmpeg を起動せずに selftest から検証するため GUI から切り離してある
    src/io/MovieExport.cpp
    src/io/MovieExport.h

    src/io/H5Reader.cpp
    src/io/H5Reader.h
    src/io/KernelResultReader.cpp
    src/io/KernelResultReader.h
    src/io/StlImporter.cpp
    src/io/StlImporter.h
    # 取込メッシュの位相・幾何検査 (重複頂点 / 縮退三角形 / 境界・非多様体辺 /
    # 法線の向き) — Qt Widgets 非依存。GeometryTab「ジオメトリ検査」の検出側の
    # 実体で、selftest から既知メッシュの厳密な期待値と突き合わせる
    src/io/MeshDiagnostics.cpp
    src/io/MeshDiagnostics.h
    src/io/Voxelizer.cpp
    src/io/Voxelizer.h

    # カーネルの作図出力 (.ev2) のパーサ + QPainter 描き直し。
    # 形式は post/ev2d.c の出力そのまま — selftest が parse を直接検証する
    src/io/EvReader.cpp
    src/io/EvReader.h

    # GDSII (Calma Stream) の最小限の読み書き。外部ライブラリを足さない
    # 方針のため自前実装 — selftest が REAL8 のビット列と往復を検証する
    src/io/GdsIO.cpp
    src/io/GdsIO.h

    src/kernel/Runner.cpp
    src/kernel/Runner.h

    # パラメータを振って同じカーネルを N 回まわす司会進行
    # (入射角スイープ)。カーネルは 1 実行 1 planewave なので
    # スイープは GUI 側の役目 — selftest が plan/collect を直接検証する
    src/kernel/SweepRunner.cpp
    src/kernel/SweepRunner.h

    # 光導波路の断面固有モード (FDE) ソルバ — Qt 非依存。
    # selftest から解析解 (スラブ導波路の厳密解) と直接突き合わせる
    src/optics/FdeModeSolver.cpp
    src/optics/FdeModeSolver.h

    # フォトニック集積回路の回路レベル解析 (S 行列で素子を接続) — Qt 非依存。
    # SchematicTab の計算実体で、selftest からリング共振器の閉形式・
    # FSR = λ²/(ng L) と直接突き合わせる
    src/optics/PhotonicCircuit.cpp
    src/optics/PhotonicCircuit.h

    # 曲がり導波路の共形変換と重なり積分 (Heiblum-Harris / Marcuse) —
    # Qt 非依存。ModeSolverTab の曲げ節の計算実体
    src/optics/BendWaveguide.cpp
    src/optics/BendWaveguide.h

    # 分散モデルの最小二乗フィットと診断 (残差 / 因果律 / 受動性) —
    # Qt 非依存。MaterialExplorerTab のフィット・診断表の計算実体
    src/optics/DispersionFit.cpp
    src/optics/DispersionFit.h

    # CIE 表色系の測色計算 (等色関数は Wyman et al. 2013 の解析近似) —
    # Qt 非依存。IlluminationTab の測色表の計算実体
    src/optics/Colorimetry.cpp
    src/optics/Colorimetry.h

    # 光源スペクトルモデル (IlluminationOpts → 分光分布 → 測色量)
    src/optics/SourceSpectrum.cpp
    src/optics/SourceSpectrum.h

    # ディスプレイ / AR-VR 光学の解析式 (導波路 FOV・射出円錐・環境光CR) —
    # Qt 非依存。DisplayOpticsTab の評価表の計算実体
    src/optics/DisplayMetrics.cpp
    src/optics/DisplayMetrics.h

    # 順次光学系の近軸光線追跡 (y-nu) — Qt 非依存。
    # LensEditorTab の近軸諸元 / EFFL・PIMH・ISFN オペランドの計算実体
    src/optics/ParaxialTrace.cpp
    src/optics/ParaxialTrace.h

    # 音響編集・解析エンジン (audio-editor.jsx) — Qt 非依存の GUI 層 DSP。
    # selftest からも直接検証するため CORE_SOURCES に置く
    src/audio/AudioEditEngine.cpp
    src/audio/AudioEditEngine.h
)

set(GUI_SOURCES
    src/MainWindow.cpp
    src/MainWindow.h
    src/DomainBar.cpp
    src/DomainBar.h
    src/RightDock.cpp
    src/RightDock.h
    src/TabNavigator.cpp
    src/TabNavigator.h
    src/CenterPane.cpp
    src/CenterPane.h
    src/I18n.cpp
    src/I18n.h
    src/Theme.cpp
    src/Theme.h

    src/widgets/SectionBox.cpp
    src/widgets/SectionBox.h
    src/widgets/MiniPlot.cpp
    src/widgets/MiniPlot.h
    src/widgets/FieldHeatmap.cpp
    src/widgets/FieldHeatmap.h
    src/widgets/MeshPreview.cpp
    src/widgets/MeshPreview.h
    src/widgets/UnitNav.cpp
    src/widgets/UnitNav.h
    src/widgets/Viewport3D.cpp
    src/widgets/Viewport3D.h
    src/widgets/PlotPanel.cpp
    src/widgets/PlotPanel.h
    src/widgets/LogConsole.cpp
    src/widgets/LogConsole.h
    src/widgets/EvCanvas.cpp
    src/widgets/EvCanvas.h
    src/widgets/EvViewer.cpp
    src/widgets/EvViewer.h
    src/widgets/AudioWaveformView.cpp
    src/widgets/AudioWaveformView.h

    # タブ間で共有する GUI ヘルパー (品質バッジ / 波形包絡線 / テキスト保存)
    src/tabs/TabHelpers.cpp
    src/tabs/TabHelpers.h

    src/tabs/GeneralTab.cpp
    src/tabs/GeneralTab.h
    src/tabs/MeshTab.cpp
    src/tabs/MeshTab.h
    src/tabs/MaterialTab.cpp
    src/tabs/MaterialTab.h
    src/tabs/GeometryTab.cpp
    src/tabs/GeometryTab.h
    src/tabs/SourceTab.cpp
    src/tabs/SourceTab.h
    src/tabs/Post1Tab.cpp
    src/tabs/Post1Tab.h
    src/tabs/Post2Tab.cpp
    src/tabs/Post2Tab.h
    src/tabs/OpticalTab.cpp
    src/tabs/OpticalTab.h
    src/tabs/AcousticTab.cpp
    src/tabs/AcousticTab.h
    src/tabs/UnderwaterTab.cpp
    src/tabs/UnderwaterTab.h
    src/tabs/Tidy3dTab.cpp
    src/tabs/Tidy3dTab.h
    src/tabs/GlassCatalogTab.cpp
    src/tabs/GlassCatalogTab.h
    src/tabs/RoomAcousticsTab.cpp
    src/tabs/RoomAcousticsTab.h

    # Workbench 拡張タブ (design mock 全カテゴリ)
    src/tabs/SolverRegionTab.cpp
    src/tabs/SolverRegionTab.h
    src/tabs/MonitorsTab.cpp
    src/tabs/MonitorsTab.h
    src/tabs/PerFaceBCTab.cpp
    src/tabs/PerFaceBCTab.h
    src/tabs/ComponentsTab.cpp
    src/tabs/ComponentsTab.h
    src/tabs/MaterialExplorerTab.cpp
    src/tabs/MaterialExplorerTab.h
    src/tabs/LensEditorTab.cpp
    src/tabs/LensEditorTab.h
    src/tabs/LayoutGDSTab.cpp
    src/tabs/LayoutGDSTab.h
    src/tabs/SchematicTab.cpp
    src/tabs/SchematicTab.h
    src/tabs/PhotonicsSolversTab.cpp
    src/tabs/PhotonicsSolversTab.h
    src/tabs/AcousticSourceTab.cpp
    src/tabs/AcousticSourceTab.h
    src/tabs/OceanEnvironmentTab.cpp
    src/tabs/OceanEnvironmentTab.h
    src/tabs/SoundproofTab.cpp
    src/tabs/SoundproofTab.h
    src/tabs/OutdoorNoiseTab.cpp
    src/tabs/OutdoorNoiseTab.h
    src/tabs/CabinAcousticsTab.cpp
    src/tabs/CabinAcousticsTab.h
    src/tabs/UltrasoundTab.cpp
    src/tabs/UltrasoundTab.h
    src/tabs/FamilySolverTab.cpp
    src/tabs/FamilySolverTab.h
    src/tabs/SolverSelectorTab.cpp
    src/tabs/SolverSelectorTab.h
    src/tabs/VerificationTab.cpp
    src/tabs/VerificationTab.h
    src/tabs/OptimizeTab.cpp
    src/tabs/OptimizeTab.h
    src/tabs/ToleranceTab.cpp
    src/tabs/ToleranceTab.h
    src/tabs/ScriptsTab.cpp
    src/tabs/ScriptsTab.h
    src/tabs/MultiphysicsTab.cpp
    src/tabs/MultiphysicsTab.h
    src/tabs/AnalysisGroupsTab.cpp
    src/tabs/AnalysisGroupsTab.h
    src/tabs/DatasetsTab.cpp
    src/tabs/DatasetsTab.h
    src/tabs/H5ViewerTab.cpp
    src/tabs/H5ViewerTab.h
    src/tabs/InteropTab.cpp
    src/tabs/InteropTab.h
    src/tabs/AntennaCharTab.cpp
    src/tabs/AntennaCharTab.h
    src/tabs/TransmissionLineTab.cpp
    src/tabs/TransmissionLineTab.h
    src/tabs/ScatteringTab.cpp
    src/tabs/ScatteringTab.h
    src/tabs/CircuitSolversTab.cpp
    src/tabs/CircuitSolversTab.h

    # 応用画面 (em-applications.jsx / optical-applications.jsx)
    src/tabs/EmcTab.cpp
    src/tabs/EmcTab.h
    src/tabs/SarTab.cpp
    src/tabs/SarTab.h
    src/tabs/ChannelTab.cpp
    src/tabs/ChannelTab.h
    src/tabs/ThinFilmTab.cpp
    src/tabs/ThinFilmTab.h
    src/tabs/IlluminationTab.cpp
    src/tabs/IlluminationTab.h
    src/tabs/DisplayOpticsTab.cpp
    src/tabs/DisplayOpticsTab.h

    # ダイアログ (app.jsx / ansys-workflow.jsx のモーダル)
    src/dialogs/RunDialog.cpp
    src/dialogs/RunDialog.h
    src/dialogs/CloudDialog.cpp
    src/dialogs/CloudDialog.h
    src/dialogs/AppGalleryDialog.cpp
    src/dialogs/AppGalleryDialog.h
    src/dialogs/ResourceDialog.cpp
    src/dialogs/ResourceDialog.h
    src/dialogs/KernelPathDialog.cpp
    src/dialogs/KernelPathDialog.h
    src/dialogs/GettingStartedDialog.cpp
    src/dialogs/GettingStartedDialog.h

    # オペラ音響分析 (実測RIR分析) — Qt アダプターと分析タブ
    src/acoustics/qt/QtAcousticAdapter.cpp
    src/acoustics/qt/QtAcousticAdapter.h
    src/acoustics/qt/AcousticResultModel.cpp
    src/acoustics/qt/AcousticResultModel.h
    src/acoustics/qt/AcousticReportBuilder.cpp
    src/acoustics/qt/AcousticReportBuilder.h
    src/tabs/RirAnalysisTab.cpp
    src/tabs/RirAnalysisTab.h

    # フェーズ3〜5 — 歌唱分析 / 可聴化タブと外部音響ソルバーランナー
    src/tabs/VocalAnalysisTab.cpp
    src/tabs/VocalAnalysisTab.h
    src/tabs/AuralizationTab.cpp
    src/tabs/AuralizationTab.h
    src/kernel/AcousticRunner.cpp
    src/kernel/AcousticRunner.h

    # handoff2 デザイン差分 (audio-editor.jsx / pic-tools.jsx / opera-analysis.jsx)
    src/tabs/AudioEditorTab.cpp
    src/tabs/AudioEditorTab.h
    src/tabs/ModeSolverTab.cpp
    src/tabs/ModeSolverTab.h
    src/tabs/AcousticSolverTab.cpp
    src/tabs/AcousticSolverTab.h

    resources/openfdtd.qrc
)

# ── 共有 GUI ライブラリ ─────────────────────────────────────────────────────
# 本体 (openfdtd_x) と水中音響の分離アプリ (openuwa) が同じタブ実装を使うため、
# main.cpp を除く全 GUI/コアを一度だけコンパイルして両方からリンクする。
qt_add_library(ofdx_gui STATIC ${CORE_SOURCES} ${GUI_SOURCES})
target_include_directories(ofdx_gui PUBLIC src)
target_link_libraries(ofdx_gui PUBLIC Qt6::Widgets)
if(Qt6Network_FOUND)
    target_link_libraries(ofdx_gui PUBLIC Qt6::Network)
    target_compile_definitions(ofdx_gui PUBLIC OFD_USE_NETWORK)
endif()

if(USE_HDF5)
    target_link_libraries(ofdx_gui PUBLIC ${HDF5_C_LIBRARIES})
    target_include_directories(ofdx_gui PUBLIC ${HDF5_INCLUDE_DIRS})
    target_compile_definitions(ofdx_gui PUBLIC OFD_USE_HDF5)
endif()

if(USE_LIBIGL)
    target_link_libraries(ofdx_gui PUBLIC igl::core)
    target_compile_definitions(ofdx_gui PUBLIC OFD_USE_LIBIGL)
endif()

if(MSVC)
    target_compile_options(ofdx_gui PRIVATE /utf-8 /W3)
else()
    target_compile_options(ofdx_gui PRIVATE -Wall -Wextra)
endif()

# ── 本体 GUI ────────────────────────────────────────────────────────────────
qt_add_executable(openfdtd_x src/main.cpp)
target_link_libraries(openfdtd_x PRIVATE ofdx_gui)

# ── OpenUWA: 水中音響 分離アプリ (OpenUWA-Underwater.html) ──────────────────
# ドメインタブを持たず水中固定。共有タブ (海洋環境/伝搬解析/音源/H5/連携) を
# そのまま再利用する派生シェル。
# BUILD_OPENUWA=OFF でこのターゲットだけを外せる (共有コード側には分岐を入れない)。
if(BUILD_OPENUWA)
    qt_add_executable(openuwa
        src/main_uwa.cpp
        src/UnderwaterWindow.cpp
        src/UnderwaterWindow.h
    )
    target_link_libraries(openuwa PRIVATE ofdx_gui)
    if(MSVC)
        target_compile_options(openuwa PRIVATE /utf-8 /W3)
    else()
        target_compile_options(openuwa PRIVATE -Wall -Wextra)
    endif()
    set_target_properties(openuwa PROPERTIES
        WIN32_EXECUTABLE ON
        MACOSX_BUNDLE    ON
    )
endif()

set_target_properties(openfdtd_x PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE    ON
)

# ── Acoustic analysis core (Qt-free, strict C++14) ──────────────────────────
# 音響コアは意図的に C++14 に固定する (プロジェクト全体の C++17 とは独立)。
# Qt には一切リンクしない。
add_library(ofdx_acoustic_core STATIC
    src/acoustics/core/AcousticFdtdEstimator.cpp
    src/acoustics/core/AcousticMetrics.cpp
    src/acoustics/core/BandFilter.cpp
    src/acoustics/core/ConvolutionEngine.cpp
    # 有理比ポリフェーズ Kaiser 窓 sinc リサンプラ (負債 #12) —
    # 可聴化の fs 不一致時に RIR をドライ側 fs へ変換する実体
    src/acoustics/core/Resampler.cpp
    # 低域 FDTD + 高域 幾何音響 のハイブリッド RIR 合成 (音源パルスの
    # 逆フィルタ + 相補な線形位相クロスオーバー)
    src/acoustics/core/HybridRir.cpp
    src/acoustics/core/Fft.cpp
    src/acoustics/core/FormantEstimator.cpp
    src/acoustics/core/VocalAnalyzer.cpp
    src/acoustics/core/DirectSoundDetector.cpp
    src/acoustics/core/NoiseFloorEstimator.cpp
    src/acoustics/core/ReflectionDetector.cpp
    src/acoustics/core/RirAnalyzer.cpp
    src/acoustics/core/SchroederDecay.cpp
    # 指数掃引正弦波 (ESS) の生成と逆畳み込み (Farina 2000) —
    # 実測 IR 取り込みの入口。線形応答と高調波を時間軸で分離する
    src/acoustics/core/SweepDeconvolution.cpp
    # 建築遮音 (質量則/コインシデンス, ISO 717, ASTM E413, ASHRAE, STI) —
    # SoundproofTab の計算実体。selftest から規格の不変量と直接突き合わせる
    src/acoustics/core/SoundInsulation.cpp
    # 直方体室の音響固有モード (Rayleigh の閉形式) — CabinAcousticsTab の実体
    src/acoustics/core/RoomModes.cpp
    # 屋外騒音伝搬 (ISO 9613-2 幾何拡散 / 前川チャート / 環境基準) —
    # OutdoorNoiseTab の計算実体
    src/acoustics/core/EnvironmentalNoise.cpp
    # 集束超音波の軸上音場 (O'Neil 1949 の閉形式) と MI / Gol'dberg 数 —
    # UltrasoundTab の焦点音圧計算の実体
    src/acoustics/core/FocusedField.cpp
    src/acoustics/io/WavReader.cpp
    src/acoustics/io/WavWriter.cpp
)
target_include_directories(ofdx_acoustic_core PUBLIC src)

# GUI (QtAcousticAdapter 経由) から C++14 コアを利用する
target_link_libraries(ofdx_gui PUBLIC ofdx_acoustic_core)
set_target_properties(ofdx_acoustic_core PROPERTIES
    AUTOMOC               OFF
    CXX_STANDARD          14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)
if(MSVC)
    target_compile_options(ofdx_acoustic_core PRIVATE /utf-8 /W3)
else()
    target_compile_options(ofdx_acoustic_core PRIVATE -Wall -Wextra)
endif()

# ── Acoustic C API (stable C ABI over the core, C++14) ──────────────────────
add_library(ofdx_acoustic_c_api STATIC
    src/acoustics/c_api/openfdtd_x_acoustics.cpp
    src/acoustics/c_api/openfdtd_x_acoustics.h
)
target_link_libraries(ofdx_acoustic_c_api PUBLIC ofdx_acoustic_core)
set_target_properties(ofdx_acoustic_c_api PROPERTIES
    AUTOMOC               OFF
    CXX_STANDARD          14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)
if(MSVC)
    target_compile_options(ofdx_acoustic_c_api PRIVATE /utf-8 /W3)
else()
    target_compile_options(ofdx_acoustic_c_api PRIVATE -Wall -Wextra)
endif()

# ── Self test: .ofd round-trip against ../data/sample ───────────────────────
if(BUILD_TESTS)
    # QtAcousticAdapter は GUI 層 (C++17 + Qt) だが Widget に依存しないため、
    # 校正オフセットのゲート規則などを selftest から直接検証できる。
    qt_add_executable(ofdx_selftest ${CORE_SOURCES}
        src/acoustics/qt/QtAcousticAdapter.cpp
        src/acoustics/qt/QtAcousticAdapter.h
        # 一括レポート (AcousticReportBuilder) は Widget 非依存なので、
        # 未実行の明示・決定性 (同一入力 → 同一バイト列) を selftest で検証する
        # (I18n はレポートの見出し文言に使う — GUI_SOURCES 側なのでここで足す)
        src/I18n.cpp
        src/I18n.h
        src/acoustics/qt/AcousticResultModel.cpp
        src/acoustics/qt/AcousticResultModel.h
        src/acoustics/qt/AcousticReportBuilder.cpp
        src/acoustics/qt/AcousticReportBuilder.h
        # 可聴化の RIR サンプルレート注記 (rirSampleRateNotes) は 3 タブが
        # 共有する文言生成なので、GUI を起こさず selftest から検証する
        src/tabs/TabHelpers.cpp
        src/tabs/TabHelpers.h
        tests/selftest.cpp)
    target_include_directories(ofdx_selftest PRIVATE src)
    target_link_libraries(ofdx_selftest PRIVATE Qt6::Widgets ofdx_acoustic_core)
    if(USE_HDF5)
        # selftest は CORE_SOURCES (H5Writer/H5Reader を含む) を自前で
        # コンパイルするため、HDF5 の定義とリンクをここにも与える
        target_link_libraries(ofdx_selftest PRIVATE ${HDF5_C_LIBRARIES})
        target_include_directories(ofdx_selftest PRIVATE ${HDF5_INCLUDE_DIRS})
        target_compile_definitions(ofdx_selftest PRIVATE OFD_USE_HDF5)
    endif()
    set_target_properties(ofdx_selftest PROPERTIES
        WIN32_EXECUTABLE OFF
        MACOSX_BUNDLE    OFF
    )
endif()

# ── Acoustics tests (CTest) ──────────────────────────────────────────────────
if(BUILD_TESTS)
    enable_language(C)  # test_c_api.c は純 C (C99) としてコンパイルする
    enable_testing()

    # C++ 単体テスト (各 tests/acoustics/test_*.cpp → 個別実行ファイル)
    set(OFDX_ACOUSTIC_TESTS
        bandfilter
        clarity
        clipping
        convolution
        direct_sound
        estimator
        fft
        formant
        inr
        sweep
        reflections
        reverberation
        vocal
        schroeder
        wav
    )
    foreach(_t IN LISTS OFDX_ACOUSTIC_TESTS)
        add_executable(ofdx_test_${_t} tests/acoustics/test_${_t}.cpp)
        target_link_libraries(ofdx_test_${_t} PRIVATE ofdx_acoustic_core)
        set_target_properties(ofdx_test_${_t} PROPERTIES
            AUTOMOC               OFF
            CXX_STANDARD          14
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS        OFF
        )
        add_test(NAME acoustics.${_t} COMMAND ofdx_test_${_t})
    endforeach()

    # C API テスト — 純 C (C99) としてコンパイルされることが検証項目
    add_executable(ofdx_test_c_api tests/acoustics/test_c_api.c)
    target_link_libraries(ofdx_test_c_api PRIVATE ofdx_acoustic_c_api)
    set_target_properties(ofdx_test_c_api PROPERTIES
        AUTOMOC             OFF
        C_STANDARD          99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS        OFF
    )
    add_test(NAME acoustics.c_api COMMAND ofdx_test_c_api)

    # ── 外部音響ソルバー疎結合 (フェーズ5) ────────────────────────────────
    # モックソルバー: 出力契約 (metadata.json / rir.wav / metrics.json /
    # solver.log) の実行可能な仕様を兼ねる純 C99 実行ファイル (ADR-0007)。
    add_executable(ofdx_mock_acoustic_solver
        tests/acoustics/mock_acoustic_solver.c)
    set_target_properties(ofdx_mock_acoustic_solver PROPERTIES
        AUTOMOC             OFF
        C_STANDARD          99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS        OFF
    )
    if(UNIX)
        # exp()/sqrt() のため (MSVC は libm 分離なし)
        target_link_libraries(ofdx_mock_acoustic_solver PRIVATE m)
    endif()

    # AcousticRunner 統合テスト: モック起動 → 進捗解析 → 契約検証 →
    # WavReader → RirAnalyzer (T30)。Qt 依存テストのため C++17 でよい
    # (音響コアの C++14 縛りの対象外)。AUTOMOC はグローバル設定 (ON) を
    # そのまま使う (AcousticRunner が Q_OBJECT を持つ)。
    qt_add_executable(ofdx_test_acoustic_runner
        tests/acoustics/test_acoustic_runner.cpp
        src/kernel/AcousticRunner.cpp
        src/kernel/AcousticRunner.h
    )
    target_include_directories(ofdx_test_acoustic_runner PRIVATE src)
    target_link_libraries(ofdx_test_acoustic_runner PRIVATE
        Qt6::Core ofdx_acoustic_core)
    set_target_properties(ofdx_test_acoustic_runner PROPERTIES
        WIN32_EXECUTABLE OFF
        MACOSX_BUNDLE    OFF
    )
    add_test(NAME acoustics.runner COMMAND ofdx_test_acoustic_runner
        $<TARGET_FILE:ofdx_mock_acoustic_solver>)

    # Fitzroy 残響式 (core/RoomAcoustics, rtFormula=2) の数値検証。
    # GUI コア (Qt 型を使う) のため ofdx_gui にリンクする (QApplication 不要)。
    qt_add_executable(ofdx_test_fitzroy tests/test_fitzroy.cpp)
    target_include_directories(ofdx_test_fitzroy PRIVATE src)
    target_link_libraries(ofdx_test_fitzroy PRIVATE ofdx_gui)
    set_target_properties(ofdx_test_fitzroy PROPERTIES
        WIN32_EXECUTABLE OFF
        MACOSX_BUNDLE    OFF
    )
    add_test(NAME roomac.fitzroy COMMAND ofdx_test_fitzroy)

    # 人工 RIR 生成 CLI (wav はコミットせず実行時に生成する)
    add_executable(ofdx_generate_synthetic_rir
        tests/generators/generate_synthetic_rir.cpp)
    target_link_libraries(ofdx_generate_synthetic_rir PRIVATE ofdx_acoustic_core)
    set_target_properties(ofdx_generate_synthetic_rir PROPERTIES
        AUTOMOC               OFF
        CXX_STANDARD          14
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS        OFF
    )
endif()
