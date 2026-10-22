# OpenGL / VTK 双渲染后端

本工程提供 OpenGlRobotSceneRenderer 与 VtkRobotSceneRenderer 两个场景渲染后端，使用原有着色器和 VTK 9.2.6。两者通过 RobotViewport 门面接收同一份 RobotSceneSnapshot；运动学、应用服务和 Presenter 不依赖 VTK。

默认仍使用 OpenGL。启用 VTK 的构建中，右侧“渲染后端 / 效果对比”下拉框可以直接切换 OpenGL / VTK。切换保留机型、关节、显示选项、旋转、平移、缩放和安装视角备份，不会为了切换重新提交业务状态。模型准备失败时恢复原后端并显示错误。

## 点云查看器

启用 VTK 后，主界面右侧顶部的“点云查看器…”按钮可打开独立的 VTK 点云弹窗，支持 PCD、PLY、XYZ/TXT 文件与显示控制。格式范围、操作和实现位置见 [VTK 点云查看器](VTK点云查看器.md)。点云查看不要求切换机器人主视口的渲染后端。

## 构建

VTK 9.3.1 虽然声明允许 v140，但本机实际编译在 vtkImplicitArray 的模板实例化处失败。本实现选择 9.2.6，避免修改 9.3.1 的隐式数组模板实现。9.2.6 的 Qt 依赖闭包还需要两处 v140 成员模板解析兼容修补：vtkDIYGhostUtilities.cxx 移除非依赖对象 QueryPointWorker 调用前多余的 template；同名 .txx 对 Impl 的调用仅在 _MSC_VER == 1900 时采用旧解析器写法，其他编译器保留标准语法。tools/patch-vtk-v140.py 会检查精确匹配、保留 BOM/换行并读回验证，构建脚本自动执行；不修改算法。依赖构建同时关闭 VTK_ENABLE_WRAPPING，避免新生成的头文件扫描工具读取透明加密文件；C++ 渲染功能不依赖该扫描。

继续使用 Qt 5.9.9 / MSVC2015 v140 x64 / C++11。当前 VS Code 脚本统一启用 VTK；原 OpenGL 构建目录可作为已有的独立配置继续验证。

首次准备 VTK 依赖，随后构建或运行双后端版本：

~~~powershell
.\tools\build-vtk.ps1 -Configuration Release -Jobs 3
.\.vscode\qt-msvc.ps1 -Configuration Release -Action Build
.\.vscode\qt-msvc.ps1 -Configuration Release -Action Run
~~~

源码默认放在项目 build/deps 下。依赖构建和临时编译目录可用 -BuildRoot 指定，例如 -BuildRoot 'X:\robot3d-vtk-deps'；Debug / Release 分别记录到 build/deps/vtk-build-root-配置.json，编译中断后可从原目录继续；成功后还会记录 SDK 路径，应用构建可自动找到。首次编译需要预留数 GB 空间，建议本机使用 -Jobs 3。

Debug 使用相同命令，将 Release 换为 Debug；VTK Debug / Release SDK 分开构建，匹配 /MDd 与 /MD。已有自建 SDK 时，可增加 -VtkDir 参数指向 vtk-config.cmake 所在目录。

在 VS Code 中直接执行“VTK: Build Debug”即可构建 VTK Debug 应用；任务会先执行“VTK dependency: Build Debug”准备缺失的 SDK。依赖任务使用 -IfMissing，SDK 已安装且配置、头文件及关键 DLL / 导入库存在时直接复用。Release 任务采用相同的依赖关系。

按 F5 时选择“VTK Debug”，会先完成上述构建链再从 VTK 后端启动。未设置 DEMO_RENDERER 时，程序默认使用 OpenGL 视口。

若要重新编译 VTK SDK，直接运行 tools/build-vtk.ps1 -Configuration Debug（不带 -IfMissing）。首次构建需要可写且空间充足的依赖目录；可用 -BuildRoot 指定，源码仍保留在项目 build/deps。

主要输出：

| 路径 | 内容 |
| --- | --- |
| build/deps/VTK-9.2.6 | VTK 源码，不纳入版本管理 |
| build/deps/vtk-9.2.6-v140-Release/install | Release VTK SDK |
| build/deps/vtk-9.2.6-v140-Debug/install | Debug VTK SDK |
| build/vscode-qt5.9.9-msvc2015_64 | 原 OpenGL 构建与运行目录 |
| build/vscode-qt5.9.9-msvc2015_64-vtk | 双后端构建与运行目录 |

源码下载固定为 VTK 官方 9.2.6 发布包，校验 SHA-256：
06fc8d49c4e56f498c40fcb38a563ed8d4ec31358d0101e8988f0bb4d539dd12。
下载分块缓存，重复执行可复用已完成的分块。只构建所需模块，不启用 Python、Java、MPI、测试或示例。

## 效果对比

1. 在双后端版本中选定机型和姿态。
2. 停止运动模拟，保持同一时刻的场景。
3. 通过右侧下拉框反复切换 OpenGL / VTK，检查轮廓、轴心、工具、轨迹和交互。
4. 开启模拟后也可以切换，但连续运动时肉眼看到的画面会对应不同的时间点。

环境变量 DEMO_RENDERER 可设为 opengl 或 vtk，默认 opengl。未启用 VTK 的构建收到 vtk 请求时，会提示并使用 OpenGL。

已有 DEMO_DUMP_FRAME、DEMO_DUMP_START、DEMO_DUMP_SIMSTEPS、DEMO_ROBOT_TYPE 继续有效。分别设置 DEMO_RENDERER，并使用相同机型及模拟步数，可导出对应的 PNG 和 JSON；JSON 增加 renderer 字段。程序仍等待对应状态版本的渲染帧再导出。导出前会完成待处理的布局和尺寸更新，避免不同后端的初始化速度导致截图尺寸差异。

## 实现位置

2026-10-22 已统一类名和目录，迁移映射见 [MVP 命名与目录约定](MVP命名与目录约定.md)。`OpenGlRobotViewport` 与 `VtkRobotViewport` 都属于 shell 的具体 View；它们分别调用 rendering 中的场景渲染器。

| 文件 | 职责 |
| --- | --- |
| include/src/robot3d/shell/RobotViewport.* | 统一视口、后端切换、状态与视角转移、通知路由 |
| include/src/robot3d/shell/opengl、shell/vtk | 同层的 OpenGL / VTK Qt 视口与交互 |
| include/src/robot3d/rendering/opengl、rendering/vtk | 两种后端的场景渲染、几何和矩阵实现 |
| tests/VtkRobotViewportTests.cpp | 真实 OpenGL / VTK 切换与场景回归 |
| tools/build-vtk.ps1、tools/fetch-vtk.py、tools/patch-vtk-v140.py | 可重复的依赖准备和构建入口 |

VTK 连杆使用真实 aiFace 索引生成 PolyData，模型准备时构建静态几何，关节更新使用 Actor 的用户矩阵。目标机器人共享静态几何并使用另一套矩阵。路点使用 Glyph，轨迹使用 PolyLine；工具覆盖球、长方体、圆柱和圆锥。

保留原显示链的轴心、矩阵乘法顺序、SceneUnits.h 中 0.04 的场景比例与 LegacyAxisAngleDeg 语义，包括 TCP / 路点的历史位置缩放，方便对照原实现。运动学或单位语义修正应另行提交，不混入后端切换。

两个后端在同一 Qt 顶层窗口内运行。VTK 视口采用 VTK 提供的缓冲格式，并请求桌面 OpenGL 3.2 CompatibilityProfile，以与旧 GLSL 共存；不全局修改旧 OpenGL 视口的格式。仍需兼容实际显卡驱动。

光照、法线处理、抗锯齿和透明合成由不同渲染器执行，颜色和边缘可能有所差异；同样的业务场景不意味着逐像素相等。原 OpenGL 网格接口没有使用面索引，VTK 使用真实拓扑，局部表面差异需要结合模型数据分析。

## 回归验证

~~~powershell
& 'C:\CMake\bin\ctest.exe' --test-dir build/vscode-qt5.9.9-msvc2015_64/Release --output-on-failure
& 'C:\CMake\bin\ctest.exe' --test-dir build/vscode-qt5.9.9-msvc2015_64-vtk/Release --output-on-failure
~~~

原有 MVP 和 OpenGL GUI 测试保留。新增 VTK 回归检查三种机型面数、六个连杆矩阵与原 Geometry 的数值一致性、切换时状态/视角保持、模型准备失败回滚、动态工具/轨迹/路点/安全面/目标机器人、反复切换、模拟、隐藏恢复及 VTK 直接启动。

对比截图位于双后端配置目录的 vtk-validation。

2026-10-15 本机实测：

| 构建 | 结果 |
| --- | --- |
| 原 OpenGL Release | 2/2 组通过：MVP 52 项、GUI 36 项 |
| 原 OpenGL Debug | 2/2 组通过：MVP 52 项、GUI 36 项 |
| 双后端 Release | 3/3 组通过，新增 VTK 61 项检查 |
| 独立进程导帧 | G6L-3-20、30 步模拟，业务 JSON 完全一致（renderer 字段除外），两个后端的初始/最终 PNG 均为 901×809 |

本机实际创建 OpenGL 3.2 CompatibilityProfile，显卡 Intel Iris Xe。已目视检查对比图，模型位置和轮廓对齐，光照及表面细节存在差异。独立导帧结果位于 build/vtk-export-validation；原图可直接打开比较。

2026-10-19 已补充完成 Debug 验证：

- VTK 9.2.6 Debug SDK 与应用均使用 MSVC2015 x64，SDK 的编译参数包含 /MDd、/Zi、/Od，应用及 VTK 的 PDB 已生成。
- 读取 tasks.json 中的实际命令执行“VTK comparison: Build Debug”任务链成功；依赖检查直接复用已安装 SDK，不再重复编译 VTK。
- Debug CTest 4/4 组通过：点云文件 35 项、MVP 52 项、GUI 38 项、VTK 79 项，共 204 项检查。
- 使用 F5 配置中的环境启动 Debug 程序，VTK 后端完成 G6L-3-20 的 30 步模拟、PNG / JSON 导出并正常退出。导帧结果位于 build/vscode-qt5.9.9-msvc2015_64-vtk/Debug/launch-validation。
- 本机 Debug SDK 安装在 build/deps/vtk-9.2.6-v140-Debug/install，已记录对应路径；没有使用 X 盘。

本次未进行性能基准，也未验证升级工具链路线。

## 文件存储

项目 C++ 源码通过白名单 Python 3.7 读写，并保留原文件 BOM 与行尾。本轮统一了类名和目录，具体 View 归 shell、场景渲染归 rendering；几何与着色器行为继续沿用现有约定。LegacyRobot3dViewAdapter / openGlViewport 兼容入口继续使用 OpenGL；Robot3dDemoView 和需要双后端的宿主使用新的 RobotViewport 门面。

Markdown、PowerShell 和 JSON 配置按本机加密策略为明文文件；没有通过解密副本覆盖原源码。构建依赖与产物位于被忽略的 build 目录。
