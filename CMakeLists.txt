cmake_minimum_required(VERSION 3.20)
# Use CMake header scanning for the localized VS2015 compiler, including ABI probes.
set(CMAKE_DEPENDS_USE_COMPILER FALSE)
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_DEPENDS_USE_COMPILER)
project(robot3ddemo VERSION 1.0 LANGUAGES CXX)

if(NOT MSVC OR NOT MSVC_VERSION EQUAL 1900 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
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

set(ROBOT3D_QT_ROOT "C:/Qt/Qt5.9.9/5.9.9/msvc2015_64" CACHE PATH "Qt 5.9.9 MSVC2015 x64 installation")
list(PREPEND CMAKE_PREFIX_PATH "${ROBOT3D_QT_ROOT}")
find_package(Qt5 5.9.9 EXACT REQUIRED COMPONENTS Core Gui Widgets Xml OpenGL)

# Binaries from the original project's MSVC2015_64 package.
# The historical vc130 filename is retained; the PE linker is 14.0 and CRT is 140.
set(ROBOT3D_ASSIMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/assimp/MSVC2015_64" CACHE PATH "Assimp 3.2 MSVC2015 x64 package")
set(ROBOT3D_ASSIMP_IMPLIB "${ROBOT3D_ASSIMP_ROOT}/assimp-vc130-mt.lib")
set(ROBOT3D_ASSIMP_DLL "${ROBOT3D_ASSIMP_ROOT}/assimp-vc130-mt.dll")
foreach(required_file IN ITEMS "${ROBOT3D_ASSIMP_IMPLIB}" "${ROBOT3D_ASSIMP_DLL}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Missing MSVC x64 dependency: ${required_file}. The MinGW .dll.a / libassimp.dll cannot be used.")
    endif()
endforeach()
add_library(robot3d_assimp SHARED IMPORTED)
set_target_properties(robot3d_assimp PROPERTIES
    IMPORTED_IMPLIB "${ROBOT3D_ASSIMP_IMPLIB}"
    IMPORTED_LOCATION "${ROBOT3D_ASSIMP_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/assimp/include"
    INTERFACE_COMPILE_DEFINITIONS ASSIMP_DLL
)

set(model_sources
    3dmodel/basegeometry.cpp
    3dmodel/demo_support.cpp
    3dmodel/linegeometry.cpp
    3dmodel/planegeometry.cpp
    3dmodel/robot3dcontrolwidget.cpp
    3dmodel/robot3dglwidget.cpp
    3dmodel/robotgeometry.cpp
    3dmodel/spheregeometry.cpp
    3dmodel/substancegeometry.cpp
)
set(model_headers
    3dmodel/basegeometry.h
    3dmodel/demo_support.h
    3dmodel/linegeometry.h
    3dmodel/planegeometry.h
    3dmodel/robot3dcontrolwidget.h
    3dmodel/robot3dglwidget.h
    3dmodel/robotgeometry.h
    3dmodel/spheregeometry.h
    3dmodel/substancegeometry.h
)
add_executable(robot3ddemo main.cpp demoui.cpp demoui.h demo.qrc ${model_sources} ${model_headers})
target_include_directories(robot3ddemo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(robot3ddemo PRIVATE
    UNICODE _UNICODE NOMINMAX _USE_MATH_DEFINES QT_DEPRECATED_WARNINGS
)
target_compile_options(robot3ddemo PRIVATE /utf-8 /W3 /FS "$<$<CONFIG:Release>:/Zi>")
target_link_options(robot3ddemo PRIVATE "$<$<CONFIG:Release>:/DEBUG>" "$<$<CONFIG:Release>:/OPT:REF>" "$<$<CONFIG:Release>:/OPT:ICF>")
set_target_properties(robot3ddemo PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_link_libraries(robot3ddemo PRIVATE
    Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Xml Qt5::OpenGL robot3d_assimp opengl32
)

set(ROBOT3D_RUNTIME_DIR "${CMAKE_BINARY_DIR}/run" CACHE PATH "Working directory containing robotType")
add_custom_target(robot3d_assets
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/bin" "${ROBOT3D_RUNTIME_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${ROBOT3D_ASSIMP_DLL}" "${CMAKE_BINARY_DIR}/bin"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/robotType" "${ROBOT3D_RUNTIME_DIR}/robotType"
    COMMENT "Deploying Assimp x64 and robot models"
    VERBATIM
)
add_dependencies(robot3ddemo robot3d_assets)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT robot3ddemo)
set_property(TARGET robot3ddemo PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${ROBOT3D_RUNTIME_DIR}")
