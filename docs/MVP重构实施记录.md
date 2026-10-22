# MVP 重构实施记录

初次实施日期：2026-10-15；命名与目录更新：2026-10-22。迁移出发点为 `26e5fe9` 工作区；设计依据为 [MVP 架构指南](MVP分层架构设计与重构指南.md)，结构参考本机 CGXi_MasterWeld_new 的 View 接口、Presenter、服务和集中装配方式。

## 1. 已落地调用链

```text
Robot3dDemoView / 视口叠加面板
        ↓ 输入意图
Robot3dPresenter
        ↓
Robot3dService + SimulationService
        ↓
领域状态 / Kinematics / TrajectorySampler
        ↓ 一致状态快照
Presenter → Robot3dDisplayModel + RobotSceneSnapshot
        ↓
IRobot3dView → RobotViewport
                 ├─ OpenGlRobotViewport → OpenGlRobotSceneRenderer
                 └─ VtkRobotViewport → VtkRobotSceneRenderer
```

服务通过 `IRobotConfigRepository` 读取配置，具体 XML 实现由 `Robot3dComposition` 注入。Presenter 不包含具体 View 或 Renderer 头文件；Renderer 不访问全局机器人数据。composition 管理服务和 Presenter 的生命周期。

## 2. 实际代码位置

| 模块                   | 主要文件                                                               | 内容                          |
| -------------------- | ------------------------------------------------------------------ | --------------------------- |
| core/domain          | include/robot3d/core、domain；src/robot3d/domain                     | 结果、关节/位姿/DH、正解与位姿计算、轨迹采样    |
| application          | include/robot3d/application；src/robot3d/application                | 状态统一入口、机型准备与提交、模拟实例、坐标系与路点  |
| ports/infrastructure | include/robot3d/ports、infrastructure；src/robot3d/infrastructure    | 配置接口与 XML 加载、ID/轴数/数值/解析校验  |
| presentation         | include/robot3d/presentation；src/robot3d/presentation              | View 契约、展示模型、Presenter、定时调度 |
| scene                | include/robot3d/scene                                              | 无 GPU 对象的场景快照和显示选项          |
| rendering            | include/robot3d/rendering；src/robot3d/rendering | CPU 网格解析、GPU 管理、场景与相机绘制     |
| shell/composition    | include/src/robot3d/shell、src/robot3d/composition         | 具体 View、视口交互与调度、输入、集中装配      |
| compatibility        | include/src/robot3d/compatibility                      | 宿主门面和旧值类型/计算入口适配            |

当前自有代码按职责统一到 include/src/robot3d；具体 View 归 shell，后端 Renderer 和 OpenGL 图元归 rendering，旧宿主适配归 compatibility。展示模型、显示选项和轨迹采样器各有同名头文件。完整命名规则与旧名映射见 [MVP 命名与目录约定](MVP命名与目录约定.md)。

## 3. 状态与渲染变更

- 关节角和 TCP 经同一服务更新；无效输入不改变已有状态。
- 模拟时间是实例成员，固定 dt 可复现；TCP 速度是明确的模拟数据。
- 坐标系用 ID 传递，侧栏与叠加选择不再互相查找内部控件；显示选项统一回填，并阻止控件回填循环。
- XML 加载失败不覆盖当前机型。候选网格与着色器成功后才替换现有 GPU 模型。
- Renderer 采用显式配置；Assimp 解析不创建 GPU 对象，上传步骤由持有当前 GL 上下文的几何层完成。
- 普通场景提交只缓存数据并请求重绘。几何缓冲在 paintGL 处理；切换模型则显式 makeCurrent/doneCurrent。
- 每个几何对象记录自身上下文能力，不再读取全局 g_openglVersion；末端工件显隐由实例选项传入。
- 图元数量变化时按实际大小上传，避免固定构造容量的 map/memcpy 越界；模型析构释放数组并清理缓冲。
- 视口清理在上下文仍有效时完成。初始化和导帧通过就绪/呈现事件串联。

## 4. 兼容入口与限制

旧宿主入口已更名为 `LegacyRobot3dViewAdapter`；取得视口的方法由 `GetRobot3dGLWidget` 更名为 `openGlViewport`，返回 `OpenGlRobotViewport`。InitInstance、UnInitInstance、GetInstance、UpdateRobot3dJointAngle、UpdateRobot3dToolPosition、UpdateRobot3dSafetyToolData、AddRobotWaypointPath、ResizeRobot3dMode 保留原有宿主语义。宿主需同步更新 include、类名和视口取得方法。

它是持有独立 MVP 组合的过渡宿主门面，不再依赖全局 DemoDataHub。外部更新应在视口就绪后提交。`followRealtime` 参数仅保留旧签名；实时订阅需要宿主显式提供，新代码不再依赖全局回读标志。

- `UpdateRobot3dToolPosition` 作为明确的观测 TCP 输入进入服务；`LegacyEulerPose` 在此边界指代 mm/欧拉角度数。
- 旧公开 Geometry 成员、全局配置字段、UserPushButton 指针和按钮名称回调不再作为新接口。位姿确认信号传递关节角/TCP，取消单独通知。
- LongPressRepeatFilter 只识别输入并发出 stopRequested；具体运动停止由宿主应用服务接收，不再写原有的通信桩标志。
- 旧 XML 的产品连接字段和真实设备 SDK 不进入本次显示重构。

数学语义方面保持现有正解公式与显示旋转解释。`LegacyAxisAngleDeg` 将旧视口约定隔离出来，避免把它误当作新的标准旋转接口。工件坐标计算也保留既有组合顺序；后续数学修正应补充独立数值期望后单独实施。

## 5. 构建与验证入口

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .vscode/qt-msvc.ps1 -Configuration Debug -Action Build
powershell -NoProfile -ExecutionPolicy Bypass -File .vscode/qt-msvc.ps1 -Configuration Release -Action Build
& 'C:\CMake\bin\ctest.exe' --test-dir build/vscode-qt5.9.9-msvc2015_64-vtk/Debug --output-on-failure
& 'C:\CMake\bin\ctest.exe' --test-dir build/vscode-qt5.9.9-msvc2015_64-vtk/Release --output-on-failure
```

默认启用 `ROBOT3D_BUILD_TESTS`；关闭该选项可仅生成应用和分层库。

### 边界回归

`tests/Robot3dMvpTests.cpp` 使用三种机型的独立矩阵计算期望值，验证 90 步模拟后的 TCP 位置，并覆盖坏 XML、轴数不足、NaN、限位、坐标 ID、状态不污染、非零旋转转换、轨迹容量、模型准备失败和展示同步。

### OpenGL 集成回归

`tests/Robot3dGuiTests.cpp` 创建真实 Demo 与 OpenGL 上下文，验证关节输入、侧栏/叠加面板双向同步、三机型连续切换、失败候选保持原 GPU 帧、多个工具/路点/轨迹上传、模拟定时器、隐藏后显示、关闭重建及兼容门面清理。测试截图保存在对应构建目录的 `mvp-gui-validation`。

原导帧环境变量继续有效，新增 `DEMO_ROBOT_TYPE` 选择启动机型。成功导帧时另写 `.png.json` 记录机型、坐标系、关节角与 TCP，便于区分数值和显示问题。

## 6. 后续新功能的入口

- 扩展计算或规则：先修改 domain/application，再增加边界回归。
- 增加用户操作：Presenter 增加语义入口，View 只收集参数和显示结果。
- 增加文件/设备依赖：定义所需 ports 接口，由 composition 注入具体实现。
- 增加图形：扩展 scene 值契约与 Renderer，不将 GPU 类型带入服务。
- 增加后台数据源：先明确有效性、单位、顺序与旧来源隔离，再按架构指南实现排队回调。

真实设备接入、通用 DH 几何链、旋转语义修正和绘制性能改进均保持为后续独立需求，不以结构迁移代替它们的数值或设备验收。


## 7. 首轮重构实际验证结果（历史记录）

2026-10-15，在本机 Qt 5.9.9 / MSVC2015 x64 环境完成：

| 检查 | 结果 |
| --- | --- |
| Debug 构建 | 通过 |
| Release 构建 | 通过 |
| Debug CTest | 2/2，通过 52 项边界检查和 36 项 GUI 检查 |
| Release CTest | 2/2，通过 52 项边界检查和 36 项 GUI 检查 |
| Release 三种机型各 90 步模拟导帧 | 均正常退出并生成初始 PNG、最终 PNG、JSON |
| 实际导出 JSON 的 TCP 位置 | 与独立矩阵计算的结果误差小于 1e-7 mm |
| 核心编译依赖 | 编译命令不包含 Qt、Widgets、OpenGL 或 Assimp 依赖 |
| 文档本地链接、代码围栏、UTF-8/行尾、Git 空白检查 | 通过 |
| 源码及 CMakeLists 存储头 | 检查 47 个文件，47 个均为 88 7D 1C 密文头 |

图片与日志位于本机构建目录，未作为源码资源提交。对比基线检查了零位与运动后的场景；窗口布局/视口尺寸由 Qt 布局决定，未将截图字节相等作为运动学正确性的证明。

构建仍会提示既有 Assimp 头文件字符编码及部分旧图元 double/float 转换警告；本轮构建和上述回归均成功。真实设备与架构指南列出的后续数学语义核对不在这些验证结论内。

## 8. 2026-10-22 命名与目录整理验证

本轮按 [MVP 命名与目录约定](MVP命名与目录约定.md)迁移 33 个文件，拆出独立的展示模型、显示选项、轨迹采样器、图元数据和输入过滤器。具体 View 归 shell，两个场景渲染后端归 rendering，旧宿主入口归 compatibility；CMake 目标与这些职责对应。

| 构建配置 | 构建 | CTest |
| --- | --- | --- |
| VTK Debug | 通过 | 4/4：点云 35 项、MVP 52 项、GUI 44 项、VTK 79 项检查 |
| VTK Release | 通过 | 4/4：点云 35 项、MVP 52 项、GUI 44 项、VTK 79 项检查 |
| OpenGL-only Debug（ROBOT3D_WITH_VTK=OFF） | 通过 | 3/3：点云 35 项、MVP 52 项、GUI 46 项检查 |

GUI 回归增加了新宿主适配器的视口取得、旧显示标志转换、实际首帧，以及独立长按过滤器的重复点击、停止意图和状态清理。现有 VTK 回归继续验证双后端切换、相机迁移、连杆矩阵与点云查看器。

活动源码和 CMake 不再引用旧类名或旧源文件路径；项目头文件引用有效。四个领域/应用源文件的编译命令未包含 Qt、VTK 或 Assimp 依赖。文档工作区链接、代码围栏和 Git 空白检查通过。

所有源码通过白名单 Python 3.7 读写并逐字节读回，保留原文件 BOM 与行尾风格。68 个自有 C++ 头/源文件和 CMakeLists.txt 共 69 个文件的存储头均为 88 7D 1C。既有 Markdown 表格排版修改保留；历史材料保留原名与当时行号，并提供迁移说明。
