cmake_minimum_required(VERSION 3.20)
# Use CMake header scanning for the localized VS2015 compiler, including ABI probes.
set(CMAKE_DEPENDS_USE_COMPILER FALSE)
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_DEPENDS_USE_COMPILER)
project(
    robot3ddemo
    VERSION 1.0
    LANGUAGES CXX
)

if(NOT MSVC
   OR NOT MSVC_VERSION EQUAL 1900
   OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8
)
    message(FATAL_ERROR "This project requires MSVC 2015 (v140), x64. Use .vscode/qt-msvc.ps1.")
endif()
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
    message(FATAL_ERROR "Set CMAKE_BUILD_TYPE to Debug or Release.")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ROBOT3D_QT_ROOT
    "C:/Qt/Qt5.9.9/5.9.9/msvc2015_64"
    CACHE PATH "Qt 5.9.9 MSVC2015 x64 installation"
)
list(PREPEND CMAKE_PREFIX_PATH "${ROBOT3D_QT_ROOT}")
find_package(Qt5 5.9.9 EXACT REQUIRED COMPONENTS Core Gui Widgets Xml OpenGL)
option(ROBOT3D_WITH_VTK "Build the optional VTK comparison backend" OFF)
if(ROBOT3D_WITH_VTK)
    find_package(Qt5 5.9.9 EXACT REQUIRED COMPONENTS Concurrent)
    find_package(
        VTK 9.2.6 EXACT REQUIRED
        COMPONENTS CommonCore
                   CommonDataModel
                   CommonTransforms
                   FiltersCore
                   FiltersSources
                   RenderingCore
                   RenderingOpenGL2
                   InteractionStyle
                   GUISupportQt
    )
endif()

# Binaries from the original project's MSVC2015_64 package.
# The historical vc130 filename is retained; the PE linker is 14.0 and CRT is 140.
set(ROBOT3D_ASSIMP_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/assimp/MSVC2015_64"
    CACHE PATH "Assimp 3.2 MSVC2015 x64 package"
)
set(ROBOT3D_ASSIMP_IMPLIB "${ROBOT3D_ASSIMP_ROOT}/assimp-vc130-mt.lib")
set(ROBOT3D_ASSIMP_DLL "${ROBOT3D_ASSIMP_ROOT}/assimp-vc130-mt.dll")
foreach(required_file IN ITEMS "${ROBOT3D_ASSIMP_IMPLIB}" "${ROBOT3D_ASSIMP_DLL}")
    if(NOT EXISTS "${required_file}")
        message(
            FATAL_ERROR
                "Missing MSVC x64 dependency: ${required_file}. The MinGW .dll.a / libassimp.dll cannot be used."
        )
    endif()
endforeach()
add_library(robot3d_assimp SHARED IMPORTED)
set_target_properties(
    robot3d_assimp
    PROPERTIES IMPORTED_IMPLIB "${ROBOT3D_ASSIMP_IMPLIB}"
               IMPORTED_LOCATION "${ROBOT3D_ASSIMP_DLL}"
               INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/assimp/include"
               INTERFACE_COMPILE_DEFINITIONS ASSIMP_DLL
)

function(robot3d_configure_target name)
    target_compile_definitions(${name} PRIVATE UNICODE _UNICODE NOMINMAX _USE_MATH_DEFINES QT_DEPRECATED_WARNINGS)
    target_compile_options(${name} PRIVATE /utf-8 /W3 /FS)
    set_target_properties(${name} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endfunction()
add_library(
    robot3d_core STATIC
    src/robot3d/domain/Kinematics.cpp src/robot3d/domain/TrajectorySampler.cpp
    include/robot3d/domain/TrajectorySampler.h src/robot3d/application/Robot3dService.cpp
    src/robot3d/application/SimulationService.cpp
)
target_include_directories(robot3d_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_target_properties(robot3d_core PROPERTIES AUTOMOC OFF)
robot3d_configure_target(robot3d_core)
add_library(robot3d_scene INTERFACE)
target_link_libraries(robot3d_scene INTERFACE robot3d_core)
add_library(robot3d_infrastructure STATIC src/robot3d/infrastructure/XmlRobotConfigRepository.cpp)
target_link_libraries(
    robot3d_infrastructure
    PUBLIC robot3d_core
    PRIVATE Qt5::Core Qt5::Xml
)
robot3d_configure_target(robot3d_infrastructure)
add_library(robot3d_pointcloud STATIC src/robot3d/infrastructure/PointCloudFileLoader.cpp)
target_include_directories(robot3d_pointcloud PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(robot3d_pointcloud PUBLIC Qt5::Core)
robot3d_configure_target(robot3d_pointcloud)
add_library(
    robot3d_presentation STATIC
    src/robot3d/presentation/Robot3dPresenter.cpp include/robot3d/presentation/Robot3dPresenter.h
    include/robot3d/presentation/Robot3dDisplayModel.h
)
target_link_libraries(robot3d_presentation PUBLIC robot3d_core robot3d_scene Qt5::Core)
robot3d_configure_target(robot3d_presentation)
# Shared CPU mesh parsing has no window or GPU context dependency.
add_library(
    robot3d_rendering STATIC src/robot3d/rendering/RobotMeshLoader.cpp include/robot3d/rendering/RobotMeshLoader.h
)
target_include_directories(robot3d_rendering PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(
    robot3d_rendering
    PUBLIC Qt5::Gui
    PRIVATE robot3d_assimp
)
robot3d_configure_target(robot3d_rendering)

add_library(
    robot3d_rendering_opengl STATIC
    src/robot3d/rendering/opengl/OpenGlRobotSceneRenderer.cpp
    include/robot3d/rendering/opengl/OpenGlRobotSceneRenderer.h
    src/robot3d/rendering/opengl/geometry/OpenGlGeometryBase.cpp
    include/robot3d/rendering/opengl/geometry/OpenGlGeometryBase.h
    src/robot3d/rendering/opengl/geometry/OpenGlLineGeometry.cpp
    include/robot3d/rendering/opengl/geometry/OpenGlLineGeometry.h
    src/robot3d/rendering/opengl/geometry/OpenGlPlaneGeometry.cpp
    include/robot3d/rendering/opengl/geometry/OpenGlPlaneGeometry.h
    src/robot3d/rendering/opengl/geometry/OpenGlRobotGeometry.cpp
    include/robot3d/rendering/opengl/geometry/OpenGlRobotGeometry.h
    src/robot3d/rendering/opengl/geometry/OpenGlSphereGeometry.cpp
    include/robot3d/rendering/opengl/geometry/OpenGlSphereGeometry.h
    src/robot3d/rendering/opengl/geometry/OpenGlSolidGeometry.cpp
    include/robot3d/rendering/opengl/geometry/OpenGlSolidGeometry.h
    include/robot3d/rendering/opengl/geometry/OpenGlGeometryData.h
)
target_link_libraries(
    robot3d_rendering_opengl
    PUBLIC robot3d_scene robot3d_rendering Qt5::Gui Qt5::OpenGL
    PRIVATE opengl32
)
robot3d_configure_target(robot3d_rendering_opengl)

if(ROBOT3D_WITH_VTK)
    set(ROBOT3D_VTK_RENDERING_MODULES VTK::FiltersSources VTK::RenderingCore VTK::RenderingOpenGL2)
    set(ROBOT3D_VTK_VIEW_MODULES VTK::GUISupportQt VTK::InteractionStyle)
    add_library(
        robot3d_rendering_vtk STATIC
        src/robot3d/rendering/vtk/VtkRobotSceneRenderer.cpp include/robot3d/rendering/vtk/VtkRobotSceneRenderer.h
        include/robot3d/rendering/vtk/VtkSceneMath.h
    )
    target_link_libraries(
        robot3d_rendering_vtk
        PUBLIC robot3d_scene Qt5::Gui
        PRIVATE robot3d_assimp ${ROBOT3D_VTK_RENDERING_MODULES}
    )
    robot3d_configure_target(robot3d_rendering_vtk)
    vtk_module_autoinit(TARGETS robot3d_rendering_vtk MODULES ${ROBOT3D_VTK_RENDERING_MODULES})
endif()

add_library(
    robot3d_shell STATIC
    src/robot3d/shell/Robot3dDemoView.cpp
    include/robot3d/shell/Robot3dDemoView.h
    src/robot3d/shell/RobotViewport.cpp
    include/robot3d/shell/RobotViewport.h
    src/robot3d/shell/opengl/OpenGlRobotViewport.cpp
    include/robot3d/shell/opengl/OpenGlRobotViewport.h
    src/robot3d/shell/input/LongPressRepeatFilter.cpp
    include/robot3d/shell/input/LongPressRepeatFilter.h
    src/robot3d/composition/Robot3dComposition.cpp
    include/robot3d/composition/Robot3dComposition.h
)
target_link_libraries(
    robot3d_shell
    PUBLIC robot3d_presentation robot3d_rendering_opengl Qt5::Widgets
    PRIVATE robot3d_infrastructure
)
robot3d_configure_target(robot3d_shell)
if(ROBOT3D_WITH_VTK)
    target_compile_definitions(robot3d_shell PUBLIC ROBOT3D_WITH_VTK)
    target_sources(
        robot3d_shell
        PRIVATE src/robot3d/shell/vtk/VtkRobotViewport.cpp include/robot3d/shell/vtk/VtkRobotViewport.h
                src/robot3d/shell/vtk/VtkPointCloudViewport.cpp include/robot3d/shell/vtk/VtkPointCloudViewport.h
                src/robot3d/shell/PointCloudViewerDialog.cpp include/robot3d/shell/PointCloudViewerDialog.h
    )
    target_link_libraries(
        robot3d_shell
        PUBLIC robot3d_rendering_vtk
        PRIVATE robot3d_pointcloud Qt5::Concurrent ${ROBOT3D_VTK_VIEW_MODULES}
    )
    vtk_module_autoinit(TARGETS robot3d_shell MODULES ${ROBOT3D_VTK_RENDERING_MODULES} ${ROBOT3D_VTK_VIEW_MODULES})
endif()

# Legacy host adaptation depends on the concrete View layer, never the reverse.
add_library(
    robot3d_compatibility STATIC src/robot3d/compatibility/LegacyRobot3dViewAdapter.cpp
                                 include/robot3d/compatibility/LegacyRobot3dViewAdapter.h
)
target_link_libraries(robot3d_compatibility PUBLIC robot3d_shell)
robot3d_configure_target(robot3d_compatibility)

add_executable(robot3ddemo main.cpp demo.qrc)
target_link_libraries(robot3ddemo PRIVATE robot3d_shell)
robot3d_configure_target(robot3ddemo)
target_compile_options(robot3ddemo PRIVATE "$<$<CONFIG:Release>:/Zi>")
target_link_options(
    robot3ddemo PRIVATE "$<$<CONFIG:Release>:/DEBUG>" "$<$<CONFIG:Release>:/OPT:REF>" "$<$<CONFIG:Release>:/OPT:ICF>"
)
set_target_properties(
    robot3ddemo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" PDB_OUTPUT_DIRECTORY
                                                                              "${CMAKE_BINARY_DIR}/bin"
)
option(ROBOT3D_BUILD_TESTS "Build MVP boundary regression tests" ON)
if(ROBOT3D_BUILD_TESTS)
    enable_testing()
    add_executable(robot3d_pointcloud_tests tests/PointCloudFileLoaderTests.cpp)
    target_link_libraries(robot3d_pointcloud_tests PRIVATE robot3d_pointcloud)
    robot3d_configure_target(robot3d_pointcloud_tests)
    add_test(NAME robot3d_pointcloud COMMAND robot3d_pointcloud_tests)
    set_tests_properties(robot3d_pointcloud PROPERTIES ENVIRONMENT "PATH=${ROBOT3D_QT_ROOT}/bin\;$ENV{PATH}")
    add_executable(robot3d_mvp_tests tests/Robot3dMvpTests.cpp)
    target_link_libraries(robot3d_mvp_tests PRIVATE robot3d_presentation robot3d_infrastructure)
    robot3d_configure_target(robot3d_mvp_tests)
    add_test(NAME robot3d_mvp COMMAND robot3d_mvp_tests "${CMAKE_CURRENT_SOURCE_DIR}/robotType")
    add_executable(robot3d_gui_tests tests/Robot3dGuiTests.cpp demo.qrc)
    target_link_libraries(robot3d_gui_tests PRIVATE robot3d_compatibility)
    robot3d_configure_target(robot3d_gui_tests)
    set_target_properties(robot3d_gui_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME robot3d_gui COMMAND robot3d_gui_tests "${CMAKE_CURRENT_SOURCE_DIR}"
                                      "${CMAKE_BINARY_DIR}/mvp-gui-validation"
    )
    set_tests_properties(
        robot3d_gui
        PROPERTIES
            ENVIRONMENT
            "PATH=${ROBOT3D_QT_ROOT}/bin\;$ENV{PATH};QT_DISABLE_SHADER_DISK_CACHE=1;QT_PLUGIN_PATH=${ROBOT3D_QT_ROOT}/plugins;DEMO_RENDERER=opengl"
            TIMEOUT
            90
    )

    set_tests_properties(robot3d_mvp PROPERTIES ENVIRONMENT "PATH=${ROBOT3D_QT_ROOT}/bin\;$ENV{PATH}")
    if(ROBOT3D_WITH_VTK)
        add_executable(robot3d_vtk_tests tests/VtkRobotViewportTests.cpp demo.qrc)
        target_link_libraries(robot3d_vtk_tests PRIVATE robot3d_shell)
        robot3d_configure_target(robot3d_vtk_tests)
        set_target_properties(robot3d_vtk_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        add_test(NAME robot3d_vtk COMMAND robot3d_vtk_tests "${CMAKE_CURRENT_SOURCE_DIR}"
                                          "${CMAKE_BINARY_DIR}/vtk-validation"
        )
        set_tests_properties(
            robot3d_vtk
            PROPERTIES
                ENVIRONMENT
                "PATH=${ROBOT3D_QT_ROOT}/bin\;$ENV{PATH};QT_DISABLE_SHADER_DISK_CACHE=1;QT_PLUGIN_PATH=${ROBOT3D_QT_ROOT}/plugins;DEMO_RENDERER=opengl"
                TIMEOUT
                120
        )
    endif()
endif()

set(ROBOT3D_RUNTIME_DIR
    "${CMAKE_BINARY_DIR}/run"
    CACHE PATH "Working directory containing robotType"
)
add_custom_target(
    robot3d_assets
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/bin" "${ROBOT3D_RUNTIME_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${ROBOT3D_ASSIMP_DLL}" "${CMAKE_BINARY_DIR}/bin"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/robotType"
            "${ROBOT3D_RUNTIME_DIR}/robotType"
    COMMENT "Deploying Assimp x64 and robot models"
    VERBATIM
)
add_dependencies(robot3ddemo robot3d_assets)
if(TARGET robot3d_gui_tests)
    add_dependencies(robot3d_gui_tests robot3d_assets)
endif()
if(ROBOT3D_WITH_VTK)
    get_filename_component(ROBOT3D_VTK_PREFIX "${VTK_DIR}/../../.." ABSOLUTE)
    file(GLOB ROBOT3D_VTK_DLLS "${ROBOT3D_VTK_PREFIX}/bin/*.dll")
    if(NOT ROBOT3D_VTK_DLLS)
        message(FATAL_ERROR "No VTK runtime DLLs found in ${ROBOT3D_VTK_PREFIX}/bin")
    endif()
    add_custom_target(
        robot3d_vtk_runtime
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/bin"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${ROBOT3D_VTK_DLLS} "${CMAKE_BINARY_DIR}/bin"
        VERBATIM
    )
    add_dependencies(robot3ddemo robot3d_vtk_runtime)
    if(TARGET robot3d_gui_tests)
        add_dependencies(robot3d_gui_tests robot3d_vtk_runtime)
    endif()
    if(TARGET robot3d_vtk_tests)
        add_dependencies(robot3d_vtk_tests robot3d_vtk_runtime robot3d_assets)
    endif()
endif()
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT robot3ddemo)
set_property(TARGET robot3ddemo PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${ROBOT3D_RUNTIME_DIR}")
