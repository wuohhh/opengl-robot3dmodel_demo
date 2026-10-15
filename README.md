# 机械臂 3D 显示模块 —— 独立移植 Demo

> 从示教器项目 `CGX-TP` 中把「机械臂 3D 模型显示」相关代码完整剥离出来，做成一个
> **可独立编译、可运行、自包含** 的 Qt 工程。
> 本目录既是移植成果，也是后续往其它项目移植的**参考实现与操作说明**。

---

> 📖 **想理解 `3dmodel/` 内部的代码结构**：见
> [`docs/3dmodel-功能层级说明.md`](docs/3dmodel-功能层级说明.md) ——
> 按功能层级（L1 渲染底座 → L2 几何图元 → L3 机械臂模型 → L4 渲染控制总线 →
> L5 控制面板 → L6 对外接口 → L7 移植适配层 → L8 驱动外壳）逐层拆解，
> 含一帧数据流、单位约定速查表与改动索引。

---

## 一、可行性结论

**结论：可以完整移植**，但原代码有 6 处对外部模块的硬依赖，必须替换（已在 `3dmodel/demo_support.h` 中全部解决）：

| 原依赖 | 原位置 | 性质 | 移植方案 |
|---|---|---|---|
| `CommDataController` | `commu/commdatacontroller.h` | 全局数据中枢单例（机器人类型/DH/关节角/位姿/坐标系/Jog 参数/信号） | 用 `DemoDataHub` 单例等价实现；通过 `#define CommDataController DemoDataHub` 让业务代码零改动 |
| `RobotConfig` | `robotconfig/robotconfig.h` | 坐标系/TCP/安装姿态配置（依赖链极大：instruct、varconfig、safetyconfig…） | 只重建 3D 模块真正用到的 11 个接口 |
| `SoftwareUI_Controller` | `base/softwareui_controller.h` | 机型切换通知信号 | 最小单例 + `ChangeRobotType_signal()` |
| `RamMonotorThread` | `commu/rammonitorthread.h` | 内存保护中断 | 桩实现，`MemoryCheck()` 恒返回 false |
| `tpAlgApp` 算法库 | `robotAlgorithm/tpAlgApp/libs/win32/tpAlgApp.lib` | **纯 MSVC 预编译库**，MinGW 无法链接 | 按相同接口与语义用标准 C++ 重写（见 `demo_support.cpp`） |
| `UserPushButton` / `EXJointPose` | `base/userpushbutton.h`、`base/basedefine.h` | 自定义按钮与扩展轴位姿 | 按 3D 模块用到的接口重建 |

其中**算法库是最关键的一道坎**：`cr_PoseTrans` / `cr_PoseInv` / `cr_ForwardKineFull` 在原项目里
只是对 `tp_*` 函数的封装，而 `tpAlgApp` 只提供 `.dll/.lib`（MSVC ABI），跨编译器不可复用。
本 Demo 用标准 DH 参数模型重新实现了运动学正解与位姿变换，**不依赖任何预编译库**。

---

## 二、目录结构

```
demo/
├── 3dmodel/                  ← 从原项目 move3D/model3D 移植的源码（业务逻辑基本未改）
│   ├── robot3dglwidget.h/.cpp        3D 渲染主控件（QOpenGLWidget，1322 行）
│   ├── robot3dcontrolwidget.h/.cpp   单例入口 + 长按 jog 过滤器
│   ├── robotgeometry.h/.cpp          机械臂模型：assimp 加载 obj、关节链变换、绘制
│   ├── basegeometry.h/.cpp           几何基类与数据结构（G_SCALE=0.04 等）
│   ├── linegeometry.h/.cpp           坐标系轴、路点轨迹线
│   ├── spheregeometry.h/.cpp         TCP 球、路点球
│   ├── planegeometry.h/.cpp          安全限制平面
│   ├── substancegeometry.h/.cpp      末端工具（球/锥/柱/方体）
│   ├── demo_support.h                ★ 移植适配层（替代上述 6 项外部依赖）
│   └── demo_support.cpp              ★ 运动学算法 + 数据中枢 + 配置单例实现
├── shaders/                  8 个 GLSL（经 demo.qrc 编入资源）
├── resource/image/           3D 控件用到的 17 个图标（经 demo.qrc 编入资源）
├── robotType/                机型模型：G6L-3-20 / G6-1-1 / X20-1-3
│   └── <机型>/model3d/*.obj + *.mtl，以及 config.xml（DH 参数来源）
├── assimp/include/           assimp 头文件
├── assimp/lib/libassimp.dll.a  assimp MinGW 导入库
├── bin/libassimp.dll         assimp 运行库
├── demoui.h/.cpp             Demo 外壳界面（机型切换/关节滑条/运动模拟/显示项开关）
├── main.cpp                  入口（含 demo_debug.log 日志）
├── demo.qrc / demo.pro       资源与工程文件
├── build.bat                 ★ 一键构建（qmake + make + 部署运行库）
├── run_demo.bat              ★ 启动脚本（自动切到 run/ 目录）
└── run/                      运行目录（exe + DLL + robotType，构建脚本生成）
```

---

## 三、快速开始

```bat
cd demo
build.bat            :: 构建 + 部署到 run/
run_demo.bat         :: 运行
```

构建环境（本机已验证）：**Qt 5.9.9 MinGW 32 位** + **MinGW 5.3.0 32 位** + **assimp 3.2(MinGW)**。
路径写在 `build.bat` 顶部 `QT_DIR` / `MINGW_DIR`，换机器时改这两行即可。

---

## 四、实际改动清单（相对原代码）

移植遵循「**尽量不动业务代码**」原则：只改写 include，其余改动集中在适配层。
下面是**对原 `3dmodel/` 源文件**的全部改动（共 4 类）：

| # | 文件 | 位置 | 改动 | 原因 |
|---|---|---|---|---|
| 1 | `robot3dglwidget.cpp` `robot3dglwidget.h` `robotgeometry.cpp` `robotgeometry.h` `robot3dcontrolwidget.cpp` | 头部 | 10 行 `#include` 指向 `demo_support.h` | 依赖剥离 |
| 2 | `robot3dcontrolwidget.cpp` | `touchEndEvent()` | `WarningLevel::WARNING` → `WarningLevel::LV_WARNING` | 避免与 `windows.h` 的 `ERROR` 宏冲突（该头经 `qopengl.h` 间接引入） |
| 3 | `robotgeometry.cpp` | 头部 | 补 `#include <QFileInfo>` | 原项目由 `basedefine.h` 间接引入，`demo_support.h` 不再连带 |
| 4 | `robot3dglwidget.cpp` | `initializeGL()` / `SetRobotToolPosition_slot()` | TCP 球心坐标 `* G_SCALE` | **修 bug**：见下文第六节 |

其余全部逻辑（渲染管线、关节链变换、光照、交互、坐标系切换、路点/轨迹/安全平面/工具显示）**保持原样**。

---

## 五、适配层设计要点

`demo_support.h/.cpp` 用一个文件提供原项目 5 个头文件的全部内容：

```cpp
// 3dmodel/ 下的源文件只需把
#include"commu/commdatacontroller.h"      // 改为
#include "demo_support.h"
```

* **类名兼容**：`#define CommDataController DemoDataHub`，业务代码继续书写
  `CommDataController::GetInstance()->currentJointPose`，无需改名。
* **数据成员同名**：`robotType.robotTypeParaStr`、`stdRobotWholeDH[6]`、`currentJointPose`、
  `currentPose`、`baseInWorldPose`、`curJogPara.{coordSysID,tcpID,tcpOffset}`、
  `curTcpInCoordSysMsg.coordSysTcpPara.{coordSysType,coordinateId}`、`updateRealtimeRobot3dMsgSignal`
  等，与原项目字段名逐一对应。
* **运动学重写**（`demo_support.cpp`）：
  * `cr_ForwardKineFull()`：标准 DH 变换 `T = RotZ(θ)·TransZ(d)·TransX(a)·RotX(α)` 连乘，关节角为**度**；
  * `cr_PoseTrans()`：`result = Pose2⁻¹ · Pose1`；
  * `cr_PoseInv()`：位姿求逆；
  * 旋转约定与控件内 `Euler2RotateMatrix()` 一致：`R = Rz·Ry·Rx`。

---

## 六、移植中发现的原项目问题（建议回查）

### 1. TCP 球坐标单位不一致（已在 Demo 中修正）

`spheregeometry.cpp::GenerateSphereData()` 内部对 `center` 与 `radius` 都乘了 `G_SCALE`：

```cpp
temp0.position = QVector3D(x0,y0,z0) * G_SCALE;   // center 和 radius 都被缩放
```

而 `robot3dglwidget::initializeGL()` / `SetRobotToolPosition_slot()` 传入的是**毫米单位**的位姿
（`tmpPose` 来自正解，单位 mm），并没有乘 `G_SCALE`。模型顶点在 `LoadMesh()` 里已经缩放过，
于是 TCP 球会跑到偏移约 `1/0.04 = 25` 倍的位置——本 Demo 首次运行时该球确实出现在画面外。

Demo 的处理：在**传参侧**同步乘 `G_SCALE`（`robot3dglwidget.cpp` 两处，含 `[demo 适配]` 注释）。
修正后 TCP 球精确落在末端法兰处（已用像素分析验证）。

> 建议：在原示教器中确认该现象是否存在。若存在，同样的两行改动即可修复。

### 2. shader 文件不能带 UTF-8 BOM

GLSL 编译器会报 `illegal non-ASCII character (0xef/0xbb/0xbf)`，着色器全部编译失败 → 3D 全黑。
原项目 `shaders/*.glsl` 为无 BOM，移植时若用带 BOM 的方式重写就会踩坑（本 Demo 踩过并已修正）。

### 3. 初始化顺序有强约束

`RobotGeometry` 的**构造函数里**就会调用 `ChangeRobot3D()` 去读
`robotType/<机型>/model3d/*.obj` 和 `stdRobotWholeDH`。因此必须：

```
① DemoDataHub::LoadRobotType()  →  ② Robot3dControlWidget::InitInstance()
→ ③ GetRobot3dGLWidget() 并加入布局  →  ④ 首次显示触发 initializeGL()  →  ⑤ 才可调用 Update*
```

在 `initializeGL()` 之前调用 `UpdateRobot3dJointAngle()` / `UpdateRobot3dToolPosition()`
会因为 `robotGeometry` / `TCP_Geometry` 仍为空指针而崩溃（Demo 用 `QTimer::singleShot` 延迟到窗口显示后触发）。

### 4. 模型路径依赖工作目录

`ChangeRobot3D()` 使用 `QString appPath = "./"`，即模型按**相对可执行文件工作目录**查找。
因此运行目录下必须有 `robotType/`（`build.bat` 会自动复制）。

---

## 七、验证结果（本机实测）

| 项目 | 结果 |
|---|---|
| 编译 | `mingw32-make` 成功，`robot3ddemo.exe` 291 KB，无 error |
| 依赖链接 | assimp（MinGW 导入库）链接成功，`libassimp.dll` 正常加载 |
| 启动 | 进程稳定运行（约 130 MB 工作集），窗口与全部 Qt 控件正常 |
| **模型渲染** | **六轴机械臂 6 个部件 + RGB 坐标系正确渲染** |
| 机型配置 | `config.xml` 中 6 段 DH 参数解析成功（alog：`载入机型 "G6L-3-20" DH 段数: 6`） |
| **关节联动** | 推进 90 步模拟后，大臂前倾、小臂弯曲，姿态随关节角正确变化 |
| TCP 点 | 缩放修正后落在末端法兰，与正解位姿一致 |
| 着色器 | 8 个 GLSL 全部编译链接成功（`demo_debug.log` 无 shader 报错） |

验证日志与截图为临时输出，可用下方命令重新生成。

复现命令：

```bat
cd demo\run
set DEMO_DUMP_START=frame_start.png
set DEMO_DUMP_FRAME=frame_sim.png
set DEMO_DUMP_SIMSTEPS=90
robot3ddemo.exe
```

（三个环境变量用于无人值守验证：先存初始帧 → 推进 n 步模拟 → 存结果帧 → 自动退出。）

---

## 八、移植到其它项目的操作步骤

1. **拷贝文件**：`3dmodel/`（含 `demo_support.*`）、`shaders/`、`resource/image/` 中的 17 个图标、
   `assimp/`（头文件 + 对应平台的导入库）、目标机型目录 `robotType/<机型>/`。
2. **改 include**：把 `3dmodel/` 里 5 个源文件头部对原项目路径的 include 换成 `#include "demo_support.h"`。
3. **接数据源**：把 `DemoDataHub` 换成目标项目自己的数据单例——二选一：
   * 改 `#define CommDataController <你的单例类名>`；
   * 或保留 `DemoDataHub` 名字，在其 setter 里桥接到目标项目的实时数据。
   需要提供的数据见第五节字段清单。
4. **配资源**：把 shader 与图标加入 `.qrc`；把 `robotType/` 放到运行目录。
5. **配工程**：`.pro` 中加 `QT += opengl widgets`、`INCLUDEPATH += <assimp include>`、
   `LIBS += -L<assimp lib> -lassimp`。
6. **按约束初始化**：遵守第六节第 3 条的 ①→⑤ 顺序。

---

## 九、已知限制

* 运动学为**自实现**，只覆盖显示所需的正解与位姿变换；不含逆解、轨迹规划、标定等
  （原项目这些来自 `tpAlgApp`，如需可单独链接该库或另行移植）。
* 涉及业务逻辑的部分被合理降级为桩：`TipMsgDialog` 改为日志输出、`RamMonotorThread` 恒不拦截、
  长按 jog 的"未使能"警告按原样保留但不真正弹窗。
* Demo 只带了 3 个机型（`G6L-3-20` / `G6-1-1` / `X20-1-3`，共约 1.1 MB），
  需要更多机型时把对应目录拷进 `demo/robotType/` 即可（无需改代码，下拉框加一项即可）。
* `demo_support.h` 中的枚举（`CRData__RobotModes`、`CRData__LuaScriptStatus`、`CRService__MoveType`）
  取值取自原项目 protobuf 生成头，只包含本模块用到的项。

---

## 附：VS Code 无法 Ctrl+左键跳转的原因与修复

### 结论（不是加密系统的问题）

本机装有透明文件级加密系统，项目源码（`.h/.cpp`）在磁盘上是密文（文件头 `88 7D 1C`）。
但**加密不是跳转失败的原因**，已实测排除：用本工程同款 MinGW g++ 去 include
密文的 `3dmodel/basegeometry.h`，预处理器能读到明文内容，报错是
`fatal error: QOpenGLFunctions: No such file or directory`（只是在抱怨缺少 Qt 头文件路径），
说明编译器这类白名单进程读到的就是明文。VS Code 同理——你能正常看到并编辑明文即为证据。

### 真正的三层原因

| 层 | 问题 | 后果 |
|---|---|---|
| 1 | 工程是 **qmake**（`demo.pro`），而 VS Code 的 C/C++ 扩展（cpptools）**不解析 `.pro`**；Qt 官方扩展（qt-cpp）也只认 CMake 和 `compile_commands.json` | 工程里的 `INCLUDEPATH`、`DEFINES` 对 IDE 完全不可见 |
| 2 | 工程内既无 `compile_commands.json` 也无 `.vscode/`，cpptools 退化为"单文件默认模板" | `QtWidgets/*`、`assimp/*` 全部解析失败 → Qt 类型无定义 → 连带跳转失效 |
| 3 | 用户级 `settings.json` 里 `qt-core.additionalQtPaths` 指向 `msvc2015_64`，与本工程实际使用的 **MinGW 32 位** 不符 | 即使扩展试图推断工具链，拿到的也是另一套 Qt |

补充证据：修复前该项目的工作区缓存里**没有** cpptools 的符号库（`.browse.VC.db`），
整个目录仅约 288 KB，说明 IntelliSense 从未在此工程成功建立过索引。

### 修复方式（已落地）

不替代原有 qmake 流程，而是新增一条并存的 CMake 入口，用途是产出编译数据库：

```
CMakeLists.txt                       新增，与 demo.pro 逐项对齐
build-cmake/compile_commands.json    CMake 生成（已 gitignore）
expand_compile_commands.py           展开响应文件（必需，见下）
.vscode/c_cpp_properties.json        指向编译数据库 + Qt/assimp 头文件兜底
.vscode/settings.json                排除 build/run 目录，避免无谓索引
```

### 为什么需要 expand_compile_commands.py

CMake 的 MinGW Makefiles 生成器会把 include 路径塞进 `@CMakeFiles/.../includes_CXX.rsp`
响应文件（`CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES=OFF` 对该生成器无效，已实测）。
cpptools 不解析响应文件，若不展开则 Qt 头文件依然找不到。故配置后需展开一次。

### 日常使用

`build-cmake/` 是可删除的生成目录。清理该目录或改动源文件列表、依赖后，执行以下命令重新生成索引：

```bat
cmake -S . -B build-cmake -G "MinGW Makefiles" ^
      -DCMAKE_PREFIX_PATH=C:/Qt/Qt5.9.9/5.9.9/mingw53_32 ^
      -DCMAKE_BUILD_TYPE=Debug
python expand_compile_commands.py build-cmake
```

然后 VS Code 执行 `Ctrl+Shift+P` → `C/C++: Reset IntelliSense Database`，
（或 `Developer: Reload Window`）即可恢复跳转与补全。

用 CMake 构建（可选，替代 build.bat）：

```bat
cmake --build build-cmake -j 4
```

注意：程序用相对路径 `./robotType/<机型>/...` 加载模型，因此必须在 `run/`
目录下启动 `robot3ddemo.exe`（CMake 已把 exe 输出到 `run/`）。
两个构建入口共用同一个 `run/` 目录，切换时建议用 `windeployqt` 补全运行时 DLL。

### 如果只想手动配置（不引入 CMake）

删除 `.vscode/c_cpp_properties.json` 里的 `compileCommands` 一行即可——
该文件中的 `includePath` / `defines` / `compilerPath` 是独立可用的兜底配置
（但缺少每个文件的精确宏，遇到条件编译分支时不如编译数据库准确）。
