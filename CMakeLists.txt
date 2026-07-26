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
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/DomainBar.cpp
    src/DomainBar.h
    src/RightDock.cpp
    src/RightDock.h
    src/TabNavigator.cpp
    src/TabNavigator.h
    src/I18n.cpp
    src/I18n.h

    src/widgets/SectionBox.cpp
    src/widgets/SectionBox.h
    src/widgets/MiniPlot.cpp
    src/widgets/MiniPlot.h
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

    resources/openfdtd.qrc
)

qt_add_executable(openfdtd_x ${CORE_SOURCES} ${GUI_SOURCES})

target_include_directories(openfdtd_x PRIVATE src)
target_link_libraries(openfdtd_x PRIVATE Qt6::Widgets)

if(USE_HDF5)
    target_link_libraries(openfdtd_x PRIVATE ${HDF5_C_LIBRARIES})
    target_include_directories(openfdtd_x PRIVATE ${HDF5_INCLUDE_DIRS})
    target_compile_definitions(openfdtd_x PRIVATE OFD_USE_HDF5)
endif()

if(USE_LIBIGL)
    target_link_libraries(openfdtd_x PRIVATE igl::core)
    target_compile_definitions(openfdtd_x PRIVATE OFD_USE_LIBIGL)
endif()

if(MSVC)
    target_compile_options(openfdtd_x PRIVATE /utf-8 /W3)
else()
    target_compile_options(openfdtd_x PRIVATE -Wall -Wextra)
endif()

set_target_properties(openfdtd_x PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE    ON
)

# ── Self test: .ofd round-trip against ../data/sample ───────────────────────
if(BUILD_TESTS)
    qt_add_executable(ofdx_selftest ${CORE_SOURCES} tests/selftest.cpp)
    target_include_directories(ofdx_selftest PRIVATE src)
    target_link_libraries(ofdx_selftest PRIVATE Qt6::Widgets)
    set_target_properties(ofdx_selftest PROPERTIES
        WIN32_EXECUTABLE OFF
        MACOSX_BUNDLE    OFF
    )
endif()
