# VTK 渲染层集成评估

后续实施状态：已按保留原工具链的路线新增独立 VTK 后端和运行时切换入口，Release 构建与运行回归均已通过。VTK 9.3.1 在本机 v140 的 vtkImplicitArray 模板实例化处失败，因此改用 9.2.6。构建及切换方法见 [VTK双渲染后端使用说明](VTK双渲染后端使用说明.md)。下文第 1～7 节保留方案评估，其中原架构及工作量描述对应改造前状态；第 8 节记录后续实施与本机实测。

评估日期：2026-10-15。范围：当前 robot3d_demo 源码、构建配置、模型资源与已有测试记录，以及 VTK / Qt 官方版本源码和文档。本文分别评估保留现有工具链与升级工具链两条路线。

**结论：可以集成，架构适配度较高，改造规模中等。建议将 VTK 作为独立渲染后端接入现有场景快照边界，并先完成小范围原型验证。后续已完成保留原工具链路线的接入和 Release 验证，详见第 8 节。**

## 1. 项目为什么适合接入

当前调用链为 View → Presenter → 应用服务 → 状态快照 → View → Renderer。业务与渲染已经有明确边界：

| 现有位置 | 检查结果 | 对集成的意义 |
| --- | --- | --- |
| [CMakeLists.txt](../CMakeLists.txt)，第 48～73 行 | core、infrastructure、presentation、rendering、shell 分目标构建 | 可以增加 VTK 目标而不改运动学和应用服务 |
| [RobotSceneSnapshot.h](../include/robot3d/scene/RobotSceneSnapshot.h)，第 14 行 | 快照包含关节、TCP、显示选项、工具、安全面和目标关节 | 可继续作为两个后端共享的场景输入 |
| [IRobot3dView.h](../include/robot3d/presentation/IRobot3dView.h)，第 13～15 行 | prepareModel / showState / submitScene 已形成接口 | Presenter 无需依赖 vtkActor 或 vtkRenderer |
| [RobotSceneRenderer.cpp](../src/robot3d/rendering/RobotSceneRenderer.cpp)，第 37、48、79 行 | 模型准备、快照应用、实际绘制集中 | 是建立后端适配的主要切入点 |
| [robot3dglwidget.cpp](../3dmodel/robot3dglwidget.cpp)，第 13～68 行 | Qt 上下文、输入、相机和帧通知仍与具体 GL 视口绑定 | 需要改造视口，不能只替换 Geometry 内部调用 |

当前并不存在可直接替换的渲染接口：RobotSceneRenderer 继承 QOpenGLFunctions，draw() 接收 QMatrix4x4；DemoUi 与兼容门面直接持有 Robot3dGLWidget。已有分层减少了改造范围，但没有消除视口耦合。

源码统计：3dmodel 下 18 个头文件/实现文件约 2,110 行；新 rendering 模块 5 个文件约 184 行。统计包含兼容门面与部分历史代码，用于判断范围，不代表全部需要重写。

## 2. 两条工具链路线

### 路线 A：保留 Qt 5.9.9 / MSVC2015 v140 x64 / C++11

**本机已验证 VTK 9.2.6 Release 路线；9.3.1 虽通过版本门槛，实际模板编译失败，不能视为本机可直接使用。**

核对到 VTK 9.3.1 的编译器检查允许 MSVC 19.0（VS2015），并设置 C++11；GUISupportQt 的最低 Qt 要求是 5.9。因此不能把“VTK 9.x”整体判定为不兼容现有工具链。以上来自固定版本的[编译器检查](https://github.com/Kitware/VTK/blob/v9.3.1/CMake/vtkCompilerChecks.cmake)和 [Qt 模块构建文件](https://github.com/Kitware/VTK/blob/v9.3.1/GUISupport/Qt/CMakeLists.txt)。

这里的“可行”仅表示通过声明的版本门槛；VS2015 的具体更新版本、Windows SDK、所选第三方依赖和 Qt/驱动组合仍需实际构建确认。VTK 9.2.6 也具有相同的主要版本门槛，可在 9.3.1 遇到具体阻碍时作为候选，见其[编译器检查](https://github.com/Kitware/VTK/blob/v9.2.6/CMake/vtkCompilerChecks.cmake)。

VTK 8.2.0 也有 Qt 集成和 C++11 路线；本次已完成 9.2.6 原型验证，没有继续回退 8.2。

实施要求：

- 用本机 v140 x64 和同一套 Qt 5.9.9 源码构建所需 VTK 模块；分别准备 Debug / Release，匹配 /MDd 与 /MD。
- 通过 CMake 查找安装后的 VTK package，显式链接所需模块，并正确执行 VTK 模块初始化。
- 关闭不需要的 Python、Java、MPI、示例及大型模块，先减少依赖面。
- 增加独立依赖路径和部署步骤，明确分发 VTK DLL、Qt 插件与关联运行库。
- 使用独立构建目录，保持原 OpenGL 后端可构建、可运行。

优点是应用层与 Qt 界面变更较少；代价是继续维护历史工具链和固定 VTK 版本的构建。适合现有产品必须沿用 Qt 5.9 / v140 的情形。

### 路线 B：升级编译器与 C++ 标准，按需要升级 Qt

**长期演进更合适，初期工作量更大。**

可选的具体原型组合为 VS2022 x64 + C++17 + Qt 6.8 系列 + VTK 9.5.2。这里给出的是已核对的版本组合候选，不声称它们是当前最新版，也不代表已经在本机验证。

VTK 9.5.2 要求至少 VS2017，并设置 C++17，不能直接由当前 v140 构建，见[固定版本编译器检查](https://github.com/Kitware/VTK/blob/v9.5.2/CMake/vtkCompilerChecks.cmake)。其 [Qt 模块](https://github.com/Kitware/VTK/blob/v9.5.2/GUISupport/Qt/CMakeLists.txt)同时支持 Qt5 / Qt6；[Qt 6.8 官方平台表](https://doc.qt.io/qt-6.8/supported-platforms.html)列出了 Windows x64 / MSVC2022。因此升级 VTK 并不必然要求同时升级 Qt，保留 Qt5 是可单独验证的过渡选项。

若采用 Qt6，需要处理：

- CMake 中精确限定 MSVC_VERSION=1900、Qt5 5.9.9 EXACT 和 C++11 的逻辑。
- .vscode 下写死的 Qt、编译器、构建目录、调试器与部署配置。
- Qt6 的 OpenGLWidgets 模块及旧 Qt API 差异。
- 同一编译器/运行库下的 Assimp 构建与验证；当前包是历史 MSVC2015 Release DLL，不能未经验证直接当作新依赖组合的一部分。
- 新工具链下先建立现有功能基线，再迁移 VTK，便于定位变化来源。

## 3. 推荐的封装边界

~~~mermaid
flowchart TD
    P[现有 Presenter 与应用服务] --> S[RobotSceneSnapshot]
    S --> V[统一 Qt 视口门面]
    V --> O[现有 OpenGL 视口与 Renderer]
    V --> K[VTK 视口与 VtkRobotSceneRenderer]
    K --> A[Actor / Mapper / PolyData / Camera]
~~~

建议增加 RobotViewport 门面，内部选择 OpenGL 或 VTK 视口，并把叠加控件保留在共同的 QWidget 容器中。门面统一模型准备、场景提交、视角操作、截图和 ready / failed / frame revision 等通知；底层上下文和 Render() 生命周期由各自后端负责。

建议形成 robot3d_rendering_opengl、robot3d_rendering_vtk 和公共视口接口，通过 ROBOT3D_RENDER_BACKEND 一类构建选项选择后端。第一阶段使用构建时或启动时选择，避免同时处理动态切换上下文。

VTK 类型使用 Pimpl / 私有实现封装；不进入 domain、application、RobotSceneSnapshot 或 Presenter 的公开接口。现有 Qt 类型可留在视口层，不必为了接入 VTK 把整个界面体系重新设计。

仅把现有 Geometry 换名为 vtkActor、同时继续在 View 中直接操作 Actor，会削弱已有封装。应保持“输入快照 → 后端更新 → 帧通知”的结构。

## 4. 当前功能与 VTK 的对应关系

以下是建议的实现映射，尚未实现：

| 现有能力 | VTK 实现方向 | 迁移要点 |
| --- | --- | --- |
| base.obj 与 6 个 joint.obj | 每个连杆一组 vtkPolyData / Mapper / Actor | 模型加载一次，关节变化只更新矩阵 |
| 可选 tool.obj / 末端工件 | 独立 Actor 或 Actor 集合 | 跟随末端链，保持独立显隐 |
| 目标机器人 | 另一套 Actor，共享静态 PolyData | 独立关节变换和材质，避免重复读取模型 |
| TCP / 路点 | vtkSphereSource、vtkGlyph3DMapper | 路点成批管理，避免大量单独 Actor |
| 轨迹 | vtkPolyData + vtkPolyLine | 点集合变化时更新，保留 1000 点容量语义 |
| 坐标轴 | vtkAxesActor 或线段 Actor | 区分场景坐标轴与屏幕角落方向标 |
| 安全面 | 四角构成的 PolyData + Opacity | 透明排序及遮挡效果单独验收 |
| 球/锥等工具包络 | 对应 Source + Transform | 对齐中心、朝向、尺寸和颜色 |
| 旋转 / 平移 / 缩放 | vtkCamera + 定制交互适配 | 默认 VTK 交互手感不等于当前手感 |
| 导帧 | 统一 capture 接口；底层 Qt 或 VTK 截图 | 保持 PNG 与状态 revision 一致 |

Actor 可通过用户变换矩阵定位，见 [vtkProp3D](https://github.com/Kitware/VTK/blob/v9.3.1/Rendering/Core/vtkProp3D.h)；批量标记可以使用 [vtkGlyph3DMapper](https://github.com/Kitware/VTK/blob/v9.3.1/Rendering/Core/vtkGlyph3DMapper.h)。这些机制可减少自行维护的图元和缓冲区代码，但不替代机器人运动学。

初始模块可围绕 CommonCore、CommonDataModel、CommonTransforms、FiltersCore、FiltersSources、RenderingCore、RenderingOpenGL2、InteractionStyle、GUISupportQt 选择；轴标注、图片输出和文件读取再分别评估 RenderingAnnotation、IOImage、IOGeometry / IOImport。最终以具体实现依赖闭包为准。

## 5. 必须处理的关键问题

### 5.1 关节变换不能直接套普通 DH Actor 层级

[robotgeometry.cpp](../3dmodel/robotgeometry.cpp) 第 173～221 行目前每次恢复原始顶点，累乘绕指定轴心的旋转矩阵，再逐顶点修改并上传 VBO；第 390～406 行按 DH 长度构造轴心与方向。原始网格与这些轴心属于当前零位几何约定，不能假定每个 OBJ 都已经处于标准 DH 的局部坐标系。

建议首先提取当前显示用连杆变换计算，保留矩阵乘法顺序，再对 Actor 设置矩阵。不要同时使用已累乘的世界矩阵与父子 Assembly 变换，否则可能重复施加祖先变换。模型关节链与 TCP 正解的一致性，应由数值样本独立核对。

### 5.2 CPU 网格接口缺少拓扑

[RobotMeshLoader.cpp](../src/robot3d/rendering/RobotMeshLoader.cpp) 第 15～25 行只复制顶点、法线和漫反射颜色，没有导出 aiFace 索引；当前绘制使用 glDrawArrays(GL_TRIANGLES)。

本次实际扫描发现：

| 机型 | OBJ 顶点数合计 | 三角面数合计 | OBJ 文件总字节 |
| --- | ---: | ---: | ---: |
| G6-1-1 | 2,592 | 2,306 | 288,212 |
| G6L-3-20 | 2,680 | 2,300 | 292,784 |
| X20-1-3 | 2,324 | 2,048 | 256,267 |

三组资源均有非连续面索引。因此构建 VTK Cells 时必须读取真实面索引，不能把现有顶点数组每三个顶点直接当作一个三角面。以上数量为原始 OBJ 文本统计，未把材质分组、法线分裂后的导入顶点数混为一谈。

首选保留 Assimp 入口，为 CPU mesh 增加三角形索引及必要的子网格/材质信息，再转换成 vtkPolyData。这样可以缩小同时变更解析器的范围。

若改用原生 VTK 读取，vtkOBJReader 与 vtkOBJImporter 的职责不同：前者读取几何多边形数据，后者处理 OBJ/MTL 及材质形成的 Actor。不能把“OBJ 能打开”等同于“材质和连杆分组已保留”。参见 [9.3.1 OBJReader](https://github.com/Kitware/VTK/blob/v9.3.1/IO/Geometry/vtkOBJReader.h) 与 [9.3.1 OBJImporter](https://github.com/Kitware/VTK/blob/v9.3.1/IO/Import/vtkOBJImporter.h)。

### 5.3 单位和旋转语义已有历史兼容约定

领域坐标为 mm、TCP 为 ZYX 欧拉角度数；Scene 中仍有明确命名的 LegacyAxisAngleDeg 适配。[RobotSceneRenderer.cpp](../src/robot3d/rendering/RobotSceneRenderer.cpp) 第 19～25 行按三个旋转分量组成的向量解释轴角，不能直接换成 VTK 的三个欧拉旋转调用。

G_SCALE=0.04 分散在网格、球、线、面和位移矩阵入口。迁移初期需逐入口记录实际单位，保证每条链只缩放一次；不要在未核对前把所有标量统一乘 0.04。先对齐已有显示，再单独处理历史数学语义修正。

### 5.4 Qt 与 VTK 的上下文、调度和显卡行为

采用 QVTKOpenGLNativeWidget + vtkGenericOpenGLRenderWindow。官方文档要求该 Widget 使用 Generic 窗口，并在 QApplication 初始化前设置合适的 QSurfaceFormat，见[固定版本 Widget 文档源码](https://github.com/Kitware/VTK/blob/v9.3.1/GUISupport/Qt/QVTKOpenGLNativeWidget.h)。

9.3.1 的默认格式请求桌面 OpenGL 3.2 Core，见[适配器实现](https://github.com/Kitware/VTK/blob/v9.3.1/GUISupport/Qt/QVTKRenderWindowAdapter.cxx)。本项目旧 shader 使用 attribute / varying / gl_FragColor，不能假设在新 Core 上下文中可以直接混合绘制。两后端的格式应按实际选择分别设置。

此外，当前代码通过 dirty_ + update() 在 paintGL 中更新绘制；VTK 适配器通常依赖显式 Render() 产生新图像，单纯沿用 QWidget::update() 可能显示旧帧。应在 GUI 线程合并快照、更新场景并调度 Render()，重绘事件只负责呈现。

.vscode/qt-msvc.ps1 第 58～59 行记录了本机 Intel 驱动与 Qt shader 磁盘缓存的历史问题。VTK 仍使用图形驱动，不能据此认为问题会自动消失。原型必须实际验证上下文格式、shader 编译、FBO、缩放和隐藏/恢复；现有 OpenGL 测试成功不能证明 VTK 上下文兼容。

### 5.5 模型准备失败、帧版本和资源释放

Presenter 在 view.prepareModel 成功后提交机型状态；现有 Renderer 在候选模型和 shader 准备成功后交换资源。VTK 后端必须保持失败时保留旧场景的语义，并处理 VTK ErrorEvent；不能只依赖 C++ 异常。

VTK 的部分 GPU 初始化是延迟执行的，读取成功不代表首帧可以渲染。候选场景验证、提交时点与失败回滚需要明确设计。

保留 rendererReady、renderFailed 和 revision 关联的帧通知，适配 DemoUi 的导帧流程。渲染完成事件与 Qt 屏幕合成不是同一时刻，截图路径应等待对应 revision 已完成所需的绘制。

兼容门面 GetRobot3dGLWidget 当前返回具体指针，不能在新后端中伪装返回另一个无关类型。应新增统一视口入口，并保留旧接口供原 OpenGL 宿主使用，或明确迁移调用方。窗口关闭重建、aboutToBeDestroyed、观察者注销和 VTK 资源释放均需回归。

## 6. 收益与代价

适合引入 VTK 的需求是统一图元/材质/相机管理，以及继续增加拾取、标注、裁剪、测量、点云或科学数据可视化。它可以减少手写 OpenGL 缓冲区、shader 与多种图元维护的工作。

当前单机型仅约两千余三角面、少量连杆和最多约 1000 个轨迹点，从数据规模看没有证据说明 VTK 会带来明显帧率提升。Actor 矩阵替代 CPU 顶点重算有优化空间，但最终帧时、内存、启动速度和部署体积必须测量。VTK 自身还会增加依赖、构建和 DLL 部署成本。

如果未来仍只需要当前机器人模型与基础轨迹显示，现有分层后继续完善 OpenGL Renderer 也合理。若计划持续扩展交互和数据可视化，VTK 的工程收益更明确。它不提供本项目所需的机器人逆解、运动规划或工业安全判定的自动替代。

## 7. 实施步骤与验收

1. **原型门槛**：用候选工具链构建最小 Qt + VTK 窗口；加载一个完整七连杆机型，驱动单轴/多轴并截帧；验证本机驱动与 DLL 部署。首帧失败时先解决工具链/驱动，不扩大迁移。
2. **建立边界**：增加统一视口与后端选择，保持 scene / domain / application 边界；给现有 OpenGL 路径建立对照输入。
3. **模型迁移**：补真实面索引、材质与连杆变换；覆盖三种机型、零位、逐轴 90° 与组合姿态。
4. **场景与交互迁移**：TCP、坐标轴、工具、路点、轨迹、安全面、目标机器人、安装视角及 UI 叠加；对齐显隐和交互行为。
5. **回归与部署**：迁移 GUI 测试、截图与 revision 等待；确认模型加载失败保留旧场景、连续切换、隐藏恢复、关闭重建、资源释放和干净运行目录启动。

建议验收时同时核对连杆矩阵、TCP 数值、图像中轴心/轮廓/颜色与操作行为。材质光照实现不同，不能把逐像素相等作为唯一标准。采集相同尺寸下的帧时、首次加载耗时、内存与实际分发目录体积，再决定是否默认启用 VTK。

工作量为基于当前代码规模的估算，假设一名熟悉 Qt/C++、OpenGL 与 VTK 的工程师，覆盖现有功能，不含新机器人业务功能：

| 路线 | 可运行原型 | 完整接入、回归与部署累计 |
| --- | --- | --- |
| A：保留现有工具链 | 约 2～3 人日 | 约 8～15 人日 |
| B：升级到 VS2022 / Qt6 / C++17 | 约 5～8 人日 | 约 15～25 人日 |

编译器依赖修补、驱动问题或宿主旧接口范围超出当前 Demo 时，需要重新估算。若路线 B 只升级编译器、暂留 Qt5，工作量可低于完整 Qt6 迁移，但仍需建立独立构建与运行验证。

## 8. 后续实施与本机实测（2026-10-15）

已按用户要求保留 OpenGL 实现，新增独立 VTK 渲染层，通过 RobotViewport 在运行时切换；操作说明见 [VTK双渲染后端使用说明](VTK双渲染后端使用说明.md)。

- 本机保留 Qt 5.9.9 / MSVC2015 v140 x64 / C++11，VTK 9.2.6 Release SDK 与双后端应用完整构建成功。
- VTK 9.3.1 在 vtkImplicitArray 模板实例化处失败；9.2.6 在 ParallelDIY 中有两处 v140 成员模板解析兼容修补，已由 tools/patch-vtk-v140.py 自动应用、校验并记录。关闭 VTK_ENABLE_WRAPPING，避免生成的头文件扫描程序读取透明加密源码。
- 本机 Intel Iris Xe 实际创建 OpenGL 3.2 CompatibilityProfile，VTK 出图及 GL 错误检查通过。
- 原 OpenGL Debug / Release 均通过 MVP 52 项、GUI 36 项；双后端 Release 通过这两组以及新增 VTK 61 项检查。
- VTK 回归覆盖三种机型、真实面索引数量、组合关节姿态的六连杆矩阵对照、状态/视角保持、相同截图尺寸、工具/轨迹/路点/安全面/目标机器人、模型准备失败保留原场景、反复切换、模拟、隐藏恢复和窗口重建。
- 分别启动两个后端并运行 30 步模拟，导出 PNG / JSON；除 renderer 字段外业务状态完全一致，初始帧和最终帧均为 901×809。已目视检查图像，轮廓和位置对齐，光照及表面细节存在预期差异。
- 原 Geometry、Shader、RobotSceneRenderer 和 RobotMeshLoader 保持原实现；原视口头文件仅增加相机状态读写接口。既有加密项目源码仍保持加密，Markdown / PowerShell / JSON 按本机策略为明文。
- 2026-10-19 已补充完成 VTK Debug SDK、应用和 VS Code 自动依赖构建链验证，Debug CTest 4/4 组、204 项检查通过。最新入口见双后端使用说明。路线 B 未实施，尚未进行性能基准或其他显卡/产品宿主的全面验证。

**当前建议：可以在保留原工具链与原实现的前提下，使用已验证的 VTK 9.2.6 双后端版本进行效果对比。是否切换默认后端，应结合实际场景的效果、帧时和部署成本决定。工具链升级路线仍可作为后续独立迁移工作。**
