# 机械臂 3D 显示 Demo：MVP 分层实现

本工程从示教器 `CGX-TP` 的三维显示模块移植而来，现已采用 MVP、应用服务、领域计算和独立渲染模块进行分层。窗口与资源继续使用 Qt Widgets、OpenGL 和 Assimp。

当前工具链：**Qt 5.9.9 + MSVC2015 v140 x64 + C++11 + CMake 3.20+**。

## 文档入口

- [MVP 命名与目录约定](docs/MVP命名与目录约定.md)：当前类名、文件位置、视口与渲染器层级，以及旧名迁移映射。

- [OpenGL / VTK 双渲染后端](docs/VTK双渲染后端使用说明.md)：依赖构建、界面切换与效果验证。

- [MVP 分层架构设计与重构指南](docs/MVP分层架构设计与重构指南.md)：职责、依赖、数据契约、迁移规划及新功能开发约定；第 17 节给出已落地实现。
- [MVP 重构实施记录](docs/MVP重构实施记录.md)：实际目录、调用链、兼容边界和验证方式。
- [构建与调试配置](.vscode/README.md)：VS Code 任务、Debug/Release、工作目录与本机依赖。
- [原始移植说明](docs/原始移植说明.md)：重构前的依赖分析与移植历史。
- [原代码功能层级说明](docs/3dmodel-功能层级说明.md)与 [BaseGeometry 详解](docs/L1-渲染底座-BaseGeometry-代码详解.md)：理解原几何实现时的历史参考；部分函数已迁移或替换。

## 已有功能

- [VTK 点云查看器](docs/VTK点云查看器.md)：右侧按钮打开独立弹窗，支持 PCD / PLY / XYZ / TXT、后台加载、点大小、颜色、显隐与视角复位（需启用 VTK 构建）。

- 加载 G6L-3-20、G6-1-1、X20-1-3 模型；切换前准备配置、网格和着色器，失败保留原状态与场景。
- 六轴关节滑条、归零、正运动学 TCP 数值与三维显示。
- 每 40 ms 推进一次正弦模拟；模拟时间属于服务实例，可以重复验证。
- 基座、工具、用户及工件坐标系选择；侧栏与视口叠加面板按 ID 同步。
- 视角旋转/平移/缩放、路点与轨迹显示开关、工具与末端工件分别控制。
- 导出初始帧、固定步数模拟后的帧及对应 JSON 状态快照。

路点、轨迹和工具数据可经应用入口或旧宿主适配入口提供；本次重构未接入真实机器人 SDK。已有旋转显示约定通过命名明确的适配类型保留，数学语义的后续核对见架构指南第 7 节。

## 目录与依赖

```text
include/robot3d/
  core/             Result
  domain/           RobotTypes、Kinematics、TrajectorySampler、PointCloud
  ports/            IRobotConfigRepository
  application/      Robot3dService、SimulationService
  scene/            RobotSceneSnapshot、RobotDisplayOptions
  presentation/     IRobot3dView、Robot3dPresenter、Robot3dDisplayModel
  infrastructure/   XmlRobotConfigRepository、PointCloudFileLoader
  rendering/        RobotMeshLoader、CameraState、SceneUnits
    opengl/         OpenGlRobotSceneRenderer、geometry/OpenGl*Geometry
    vtk/            VtkRobotSceneRenderer、VtkSceneMath
  shell/            Robot3dDemoView、RobotViewport、PointCloudViewerDialog
    opengl/         OpenGlRobotViewport
    vtk/            VtkRobotViewport、VtkPointCloudViewport
    input/          LongPressRepeatFilter
  compatibility/    LegacyRobot3dViewAdapter、LegacyRobotTypes
  composition/      Robot3dComposition
src/robot3d/         上述模块实现
main.cpp            QApplication 及日志入口
tests/              MVP、GUI、双后端与点云文件回归
robotType/          三种机型配置和 OBJ/MTL
shaders/、resource/ 由 demo.qrc 部署的着色器与图标
```

主要数据流：

```text
View 的操作意图 → Presenter → Robot3dService / SimulationService
                                    ↓
                           一致的机器人状态快照
                                    ↓
                      Presenter → 数值展示 + 场景快照
                                    ↓
                  View → RobotViewport → 后端视口 → SceneRenderer
```

CMake 分别构建核心、基础设施、展示、渲染、壳层和兼容模块。`robot3d_rendering` 仅加载 CPU 网格，`robot3d_rendering_opengl` / `robot3d_rendering_vtk` 负责各后端绘制，`robot3d_shell` 负责具体 View，`robot3d_compatibility` 负责旧宿主适配；通过 `robot3d_scene` 共享场景契约。领域/应用目标不依赖 Qt、Widgets、OpenGL 或 Assimp。

## 编译与运行

VS Code 中按 **Ctrl+Shift+B** 构建 Debug，按 **F5** 调试；Release 可从任务与调试配置中选择。

从工程根目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .vscode/qt-msvc.ps1 -Configuration Debug -Action Build
powershell -NoProfile -ExecutionPolicy Bypass -File .vscode/qt-msvc.ps1 -Configuration Release -Action Build
powershell -NoProfile -ExecutionPolicy Bypass -File .vscode/qt-msvc.ps1 -Configuration Debug -Action Run
```

脚本自动导入 VS2015 amd64 编译环境，部署模型、Assimp 和 VTK DLL。输出位置：

```text
build/vscode-qt5.9.9-msvc2015_64-vtk/Debug/bin/robot3ddemo.exe
build/vscode-qt5.9.9-msvc2015_64-vtk/Release/bin/robot3ddemo.exe
```

Run/F5 的工作目录为 `build/vscode-qt5.9.9-msvc2015_64-vtk`，包含部署的 `robotType/`。直接启动程序时也需保证该相对目录存在。运行日志为工作目录中的 `demo_debug.log`。

本机运行配置保留 `QT_DISABLE_SHADER_DISK_CACHE=1`。Assimp 使用项目随附的 `assimp/MSVC2015_64/assimp-vc130-mt.lib/.dll`；其文件名为历史名称，当前包为 MSVC2015 x64。网格加载使用 C 字符串入口，兼容 Release Assimp DLL 与 Debug 主程序的运行库模式。

## 回归验证

默认脚本启用 VTK 并构建四组测试，可用 CMake 选项 `ROBOT3D_BUILD_TESTS=OFF` 关闭。测试不依赖真实机器人。

```powershell
& 'C:\CMake\bin\ctest.exe' --test-dir build/vscode-qt5.9.9-msvc2015_64-vtk/Debug --output-on-failure
& 'C:\CMake\bin\ctest.exe' --test-dir build/vscode-qt5.9.9-msvc2015_64-vtk/Release --output-on-failure
```

- `robot3d_pointcloud`：点云文件格式解析、颜色、异常输入与容量边界。
- `robot3d_vtk`：真实双后端切换、场景/相机保持与点云弹窗。
- `robot3d_mvp`：三机型独立正解期望值、配置错误、无效输入、状态原子更新、坐标系、轨迹、模型准备失败及展示同步。
- `robot3d_gui`：实际 OpenGL 首帧、滑条与模拟、双向控件同步、连续切换、缺失网格时保留场景、动态工具/路点/轨迹、隐藏后显示、关闭重建和兼容门面销毁。

GUI 回归需要可用的 Windows 桌面 OpenGL 环境。帧和窗口截图位于各配置目录的 `mvp-gui-validation/`。

保留的导帧变量：

| 变量 | 用途 |
| --- | --- |
| `DEMO_DUMP_FRAME` | 最终 PNG 路径，成功后退出；另写同名 `.png.json` 状态快照 |
| `DEMO_DUMP_START` | 可选，初始 PNG 路径 |
| `DEMO_DUMP_SIMSTEPS` | 导出前推进的模拟步数，每步 0.04 秒，范围 0～10000 |
| `DEMO_ROBOT_TYPE` | 可选，启动机型 ID，默认 G6L-3-20 |

程序根据渲染就绪及已应用快照导帧，不再依赖固定的 120/900 ms 初始化延时。模拟速度字段为演示数据。

## 扩展功能

先在 domain/application 定义状态与用例，再由 Presenter 协调，View 仅处理输入与显示；文件/设备依赖通过 ports 接口接入，GPU 对象由 Renderer 独占。

`LegacyRobot3dViewAdapter`（原 `Robot3dControlWidget`）仅作为旧宿主 View 适配入口，取得 OpenGL 视口使用 `openGlViewport()`。新代码使用构造注入与 `IRobot3dView`；不再增加 `DemoDataHub::GetInstance()`、`CommDataController` 宏、对内部下拉框的跨组件访问或对 Geometry 数据成员的直接业务写入。
