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

option(USE_HDF5   "Enable HDF5 time-series export"            OFF)
option(USE_LIBIGL "Use libigl for STL import + voxelization"  OFF)
option(BUILD_TESTS "Build the .ofd round-trip self test"      ON)

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
    src/core/Material.h
    src/core/Geometry.h
    src/core/Source.h
    src/core/MeshAxis.h
    src/core/PostOpts.h
    src/core/GlassCatalog.cpp
    src/core/GlassCatalog.h
    src/core/RoomAcoustics.cpp
    src/core/RoomAcoustics.h
    src/core/OperaHalls.h

    src/io/OfdIO.cpp
    src/io/OfdIO.h
    src/io/ActivationCurve.cpp
    src/io/ActivationCurve.h
    src/io/Touchstone.cpp
    src/io/Touchstone.h
    src/io/Tidy3dExporter.cpp
    src/io/Tidy3dExporter.h
    src/io/H5Writer.cpp
    src/io/H5Writer.h
    src/io/EvReader.h
    src/io/StlImporter.cpp
    src/io/StlImporter.h
    src/io/Voxelizer.cpp
    src/io/Voxelizer.h

    src/kernel/Runner.cpp
    src/kernel/Runner.h
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
    src/widgets/EvViewer.cpp
    src/widgets/EvViewer.h

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
    src/dialogs/GettingStartedDialog.cpp
    src/dialogs/GettingStartedDialog.h

    # オペラ音響分析 (実測RIR分析) — Qt アダプターと分析タブ
    src/acoustics/qt/QtAcousticAdapter.cpp
    src/acoustics/qt/QtAcousticAdapter.h
    src/acoustics/qt/AcousticResultModel.cpp
    src/acoustics/qt/AcousticResultModel.h
    src/tabs/RirAnalysisTab.cpp
    src/tabs/RirAnalysisTab.h

    # フェーズ3〜5 — 歌唱分析 / 可聴化タブと外部音響ソルバーランナー
    src/tabs/VocalAnalysisTab.cpp
    src/tabs/VocalAnalysisTab.h
    src/tabs/AuralizationTab.cpp
    src/tabs/AuralizationTab.h
    src/kernel/AcousticRunner.cpp
    src/kernel/AcousticRunner.h

    resources/openfdtd.qrc
)

# ── 共有 GUI ライブラリ ─────────────────────────────────────────────────────
# 本体 (openfdtd_x) と水中音響の分離アプリ (openuwa) が同じタブ実装を使うため、
# main.cpp を除く全 GUI/コアを一度だけコンパイルして両方からリンクする。
qt_add_library(ofdx_gui STATIC ${CORE_SOURCES} ${GUI_SOURCES})
target_include_directories(ofdx_gui PUBLIC src)
target_link_libraries(ofdx_gui PUBLIC Qt6::Widgets)

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
    src/acoustics/core/Fft.cpp
    src/acoustics/core/VocalAnalyzer.cpp
    src/acoustics/core/DirectSoundDetector.cpp
    src/acoustics/core/NoiseFloorEstimator.cpp
    src/acoustics/core/ReflectionDetector.cpp
    src/acoustics/core/RirAnalyzer.cpp
    src/acoustics/core/SchroederDecay.cpp
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
        tests/selftest.cpp)
    target_include_directories(ofdx_selftest PRIVATE src)
    target_link_libraries(ofdx_selftest PRIVATE Qt6::Widgets ofdx_acoustic_core)
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
