#-------------------------------------------------------------------------------
# 机械臂 3D 显示模块 —— 独立移植 Demo
#
# 构建（MinGW 32 位 + Qt 5.9.9）：
#   cd demo && build.bat
#
# 目录约定：
#   demo/3dmodel/     从原项目 move3D/model3D 移植的源码 + demo_support 适配层
#   demo/shaders/     GLSL 着色器（编入 qrc）
#   demo/resource/    3D 控件图标（编入 qrc）
#   demo/robotType/   机型模型与 config.xml（运行时按相对路径读取）
#   demo/assimp/      assimp 头文件与 MinGW 导入库
#   demo/bin/         libassimp.dll 等随程序分发的动态库
#-------------------------------------------------------------------------------

QT += core gui widgets xml opengl

TARGET   = robot3ddemo
TEMPLATE = app
CONFIG  += c++11

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD \
               $$PWD/assimp/include

# ------------------------------- 移植的 3D 模块 -------------------------------
HEADERS += \
    3dmodel/basegeometry.h \
    3dmodel/linegeometry.h \
    3dmodel/planegeometry.h \
    3dmodel/spheregeometry.h \
    3dmodel/substancegeometry.h \
    3dmodel/robotgeometry.h \
    3dmodel/robot3dglwidget.h \
    3dmodel/robot3dcontrolwidget.h \
    3dmodel/demo_support.h

SOURCES += \
    3dmodel/basegeometry.cpp \
    3dmodel/linegeometry.cpp \
    3dmodel/planegeometry.cpp \
    3dmodel/spheregeometry.cpp \
    3dmodel/substancegeometry.cpp \
    3dmodel/robotgeometry.cpp \
    3dmodel/robot3dglwidget.cpp \
    3dmodel/robot3dcontrolwidget.cpp \
    3dmodel/demo_support.cpp

# --------------------------------- Demo 外壳 ---------------------------------
HEADERS += demoui.h
SOURCES += main.cpp demoui.cpp

RESOURCES += demo.qrc

# ---------------------------------- assimp -----------------------------------
LIBS += -L$$PWD/assimp/lib -lassimp

# 输出到 build/ 目录，保证 robotType 等相对路径资源能被找到
DESTDIR = $$OUT_PWD/../run
MOC_DIR = $$OUT_PWD/.moc
OBJECTS_DIR = $$OUT_PWD/.obj
RCC_DIR = $$OUT_PWD/.rcc
