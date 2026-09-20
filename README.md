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
├── run/                      运行目录（exe + DLL + robotType，构建脚本生成）
└── port_assets.py, port_code.py, capture_demo.ps1   移植与验证脚本（可复用/可参考）
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
| **模型渲染** | **六轴机械臂 6 个部件 + RGB 坐标系正确渲染**（截图 `run/demo_frame.png`） |
| 机型配置 | `config.xml` 中 6 段 DH 参数解析成功（alog：`载入机型 "G6L-3-20" DH 段数: 6`） |
| **关节联动** | 推进 90 步模拟后，大臂前倾、小臂弯曲，姿态随关节角正确变化（`run/frame_tcp.png`） |
| TCP 点 | 缩放修正后落在末端法兰，与正解位姿一致 |
| 着色器 | 8 个 GLSL 全部编译链接成功（`demo_debug.log` 无 shader 报错） |

复现命令：

```bat
cd demo\run
set DEMO_DUMP_START=frame_start.png
set DEMO_DUMP_FRAME=frame_sim.png
set DEMO_DUMP_SIMSTEPS=90
robot3ddemo.exe
```

（三个环境变量用于无人值守验证：先存初始帧 → 推进 n 步模拟 → 存结果帧 → 自动退出。
`demo/capture_demo.ps1` 可另行抓取窗口 GDI 截图。）

---

## 八、移植到其它项目的操作步骤

1. **拷贝文件**：`3dmodel/`（含 `demo_support.*`）、`shaders/`、`resource/image/` 中的 17 个图标、
   `assimp/`（头文件 + 对应平台的导入库）、目标机型目录 `robotType/<机型>/`。
2. **改 include**：把 `3dmodel/` 里 5 个源文件头部对原项目路径的 include 换成 `#include "demo_support.h"`
   （可直接复用 `port_code.py` 的 `REPLACE_MAP`）。
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

## 十、移植脚本说明（可复用）

| 脚本 | 作用 |
|---|---|
| `port_assets.py` | 从原项目拷贝源码/shader/图标/机型模型/assimp 到 demo（本机有透明加密，Python 在白名单内，读写为明文） |
| `port_code.py` | 批量改写 include 指向 `demo_support.h`（`REPLACE_MAP` 可直接复用到其它项目） |
| `capture_demo.ps1` | 启动 demo 并抓取窗口截图（`PrintWindow` 抓不到 OpenGL 内容，验证 3D 请用 `DEMO_DUMP_FRAME` 帧缓冲导出） |

> 编译期暴露的类型缺口（`UserPushButton`、`RamMonotorThread`、`EXJointPose`、`CoordSysType`、
> `CR6_SAFETY_LIMITS_BOUNDARY_PLANE_NUM` 等）已全部合入 `demo_support.h`，无需再跑额外补丁脚本。

> **注意**：本机装有透明加密系统，`.cpp/.h` 等扩展名在非白名单进程写入后会被重新加密，
> 因此以上补丁脚本统一用 `open(p,'rb')` + `open(p,'wb')` 的二进制方式读写，
> 避免 Python 文本模式把 CRLF 静默转成 LF。
