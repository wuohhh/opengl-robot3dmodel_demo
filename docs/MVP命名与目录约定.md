# MVP 命名与目录约定

2026-10-22 按现有功能统一项目自有 C++ 文件、类名和目录。文件移动与命名整理保持现有交互、计算和渲染约定；构建与回归结果以本次实际验证记录为准。

## 命名规则

- 类与对应的头文件、实现文件采用相同的 PascalCase 名称；一项职责有独立实现时，提供同名公共头文件。
- 具体 Qt 窗口、视口、对话框和输入处理放在 `shell`。`presentation` 保留 Presenter、View 接口和展示模型。
- `Viewport` 表示承载界面、相机和输入的视口；`SceneRenderer` 表示消费场景快照并管理绘制的组件。同层后端使用 `OpenGl` / `Vtk` 前缀。
- `Geometry` 表示渲染图元；OpenGL 图元以 `OpenGl` 开头，放在 `rendering/opengl/geometry`，不属于 MVP 的领域 Model。
- 旧宿主接口与值类型放在 `compatibility`，通过 `Legacy` 名称表明适配边界。新代码通过 Presenter、服务与 View 契约协作。
- 已准确表达职责的 `Robot3dPresenter`、`Robot3dService`、`SimulationService`、Repository、Snapshot 等名称保留。
- `main.cpp`、`CMakeLists.txt`、`demo.qrc` 使用常规入口名称；工具脚本、文档、着色器、图标与机器人模型按其用途命名。第三方 Assimp、生成文件与构建依赖沿用上游约定。

## 目录与层级

`include/robot3d` 和 `src/robot3d` 按模块对应：

| 模块 | 主要内容 |
| --- | --- |
| `core` | 公共结果类型 |
| `domain` | `RobotTypes`、`Kinematics`、`TrajectorySampler`、点云数据 |
| `ports` | 外部能力接口，如 `IRobotConfigRepository` |
| `application` | `Robot3dService`、`SimulationService` |
| `scene` | `RobotSceneSnapshot`、`RobotDisplayOptions` |
| `presentation` | `IRobot3dView`、`Robot3dPresenter`、`Robot3dDisplayModel` |
| `infrastructure` | XML 配置仓储、点云文件解析 |
| `rendering` | CPU 网格解析、`CameraState`、共享场景单位；`opengl` / `vtk` 场景渲染器 |
| `shell` | `Robot3dDemoView`、统一 `RobotViewport`、点云对话框、各后端视口和输入处理 |
| `compatibility` | `LegacyRobot3dViewAdapter`、`LegacyRobotTypes` |
| `composition` | 创建对象、注入依赖、连接 View 与 Presenter |

```text
Robot3dDemoView（实现 IRobot3dView）
└─ RobotViewport（统一入口、后端切换、相机迁移）
   ├─ shell/opengl/OpenGlRobotViewport
   │  └─ rendering/opengl/OpenGlRobotSceneRenderer
   │     └─ rendering/opengl/geometry/OpenGl*Geometry
   └─ shell/vtk/VtkRobotViewport
      └─ rendering/vtk/VtkRobotSceneRenderer

shell/PointCloudViewerDialog
└─ shell/vtk/VtkPointCloudViewport
```

`OpenGlRobotViewport` 与 `VtkRobotViewport` 同层，负责视口交互和渲染调度；两个 `SceneRenderer` 同层，负责具体绘制。`RobotViewport` 统一门面位于两种视口之上。领域与应用服务保持通过快照和显示模型交付数据。

CMake 以 `robot3d_rendering` 承载 CPU 网格加载，以 `robot3d_rendering_opengl` / `robot3d_rendering_vtk` 承载后端渲染，以 `robot3d_shell` 承载具体 View，以 `robot3d_compatibility` 承载旧宿主适配。图元与视口的目录和目标归属都反映其职责。

## 迁移映射

下表路径均相对于 `include/robot3d` 或 `src/robot3d`；原根目录与 `3dmodel` 文件已迁入对应模块。

| 原名称或位置 | 当前名称与位置 |
| --- | --- |
| `DemoUi`、根目录 `demoui.*` | `shell/Robot3dDemoView.*` |
| `rendering/RobotViewport.*` | `shell/RobotViewport.*` |
| `Robot3dGLWidget`、`3dmodel/robot3dglwidget.*` | `shell/opengl/OpenGlRobotViewport.*` |
| `rendering/vtk/VtkRobotViewport.*` | `shell/vtk/VtkRobotViewport.*` |
| `rendering/vtk/VtkPointCloudViewport.*` | `shell/vtk/VtkPointCloudViewport.*` |
| `PointCloudDialog`、`presentation/PointCloudDialog.*` | `shell/PointCloudViewerDialog.*` |
| `RobotSceneRenderer`、`rendering/RobotSceneRenderer.*` | `rendering/opengl/OpenGlRobotSceneRenderer.*` |
| `BaseGeometry`、`3dmodel/basegeometry.*` | `rendering/opengl/geometry/OpenGlGeometryBase.*` |
| `RobotGeometry`、`3dmodel/robotgeometry.*` | `rendering/opengl/geometry/OpenGlRobotGeometry.*` |
| `LineGeometry`、`3dmodel/linegeometry.*` | `rendering/opengl/geometry/OpenGlLineGeometry.*` |
| `PlaneGeometry`、`3dmodel/planegeometry.*` | `rendering/opengl/geometry/OpenGlPlaneGeometry.*` |
| `SphereGeometry`、`3dmodel/spheregeometry.*` | `rendering/opengl/geometry/OpenGlSphereGeometry.*` |
| `SubstanceGeometry`、`3dmodel/substancegeometry.*` | `rendering/opengl/geometry/OpenGlSolidGeometry.*` |
| 原图元头中的数据结构 | `rendering/opengl/geometry/OpenGlGeometryData.h` |
| `FeatureVertexData` / `VertexData` | `OpenGlPositionVertex` / `OpenGlMeshVertex` |
| `multiJointData` / `MeshEntry` | `OpenGlJointTransform` / `OpenGlMeshBuffer` |
| 渲染侧 `SubstanceMsg` | `OpenGlSolidData` |
| `SphereMsg` / `LineMsg` | `OpenGlSphereData` / `OpenGlLineData` |
| `PointMsg` / `PlaneMsg` | `OpenGlPointData` / `OpenGlPlaneData` |
| `JointPose` / `Pose` / `WholeDH` | `LegacyJointPose` / `LegacyEulerPose` / `LegacyDhParameter`，位于 `compatibility/LegacyRobotTypes.h` |
| `Robot3DShowType` / `Robot3DShowTypes` | `LegacyRobotDisplayFlag` / `LegacyRobotDisplayFlags` |
| 宿主边界的 `SubstanceMsg` | `LegacyToolShape`，由适配器转换为场景工具数据 |
| `Robot3dControlWidget`、`3dmodel/robot3dcontrolwidget.*` | `compatibility/LegacyRobot3dViewAdapter.*` |
| `GetRobot3dGLWidget()` | `LegacyRobot3dViewAdapter::openGlViewport()` |
| `demo_support.h` | `compatibility/LegacyRobotTypes.h`；原空实现 `demo_support.cpp` 删除 |
| `TouchLongPressFilter` | `shell/input/LongPressRepeatFilter.*` |
| `RobotTypes.h` 中的 `TrajectorySampler` | `domain/TrajectorySampler.h` |
| `IRobot3dView.h` 中的 `Robot3dDisplayModel` | `presentation/Robot3dDisplayModel.h` |
| `RobotSceneSnapshot.h` 中的显示选项 | `scene/RobotDisplayOptions.h` |
| 分散的场景比例常量 | `rendering/SceneUnits.h`，集中保存 0.04 的显示比例 |
| `tests/mvp_tests.cpp` | `tests/Robot3dMvpTests.cpp` |
| `tests/gui_tests.cpp` | `tests/Robot3dGuiTests.cpp` |
| `tests/vtk_tests.cpp` | `tests/VtkRobotViewportTests.cpp` |
| `tests/pointcloud_tests.cpp` | `tests/PointCloudFileLoaderTests.cpp` |

旧显示位掩码 `LegacyRobotDisplayFlags` 仅由 `LegacyRobot3dViewAdapter` 解释并转换为 `RobotDisplayOptions`。视口和渲染器使用统一的场景显示契约；宿主工具输入 `LegacyToolShape` 通过适配器转换为场景工具数据，`OpenGl*Data` 留在 OpenGL 图元实现内部。

旧宿主迁移需要更新 include、类型名及视口取得方法；`LegacyRobot3dViewAdapter` 的其余既有宿主方法继续承担原参数到 MVP 意图的转换。这里的旧类型名称仅用于识别兼容输入，不作为新模块的接口约定。

## 历史材料

[原始移植说明](原始移植说明.md)、[原代码功能层级说明](3dmodel-功能层级说明.md)、[BaseGeometry 详解](L1-渲染底座-BaseGeometry-代码详解.md)保留重构前的旧类名、旧文件名和当时行号；它们不表示当前目录或调用链。[MVP 架构指南](MVP分层架构设计与重构指南.md)的重构前分析、迁移映射及 [VTK 集成评估](VTK渲染层集成评估.md)的方案评估也属于历史依据。旧文件的本地源码链接会指向当前对应实现，具体定位应按类与函数名重新搜索。

当前入口、开发职责和目录以 [README](../README.md)、本文及 [MVP 实施记录](MVP重构实施记录.md)为准。
