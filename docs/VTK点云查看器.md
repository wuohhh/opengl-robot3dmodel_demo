# VTK 点云查看器

在启用 VTK 的 Demo 中，点击主界面右侧顶部的 **点云查看器…** 按钮即可打开独立弹窗。机器人主视口仍可使用 OpenGL 或 VTK，点云弹窗固定使用 VTK 渲染。

## 使用

1. 点击 **选择文件…**，或直接在路径框输入完整路径。
2. 点击 **加载显示**（路径框按 Enter 也可）。文件解析在后台进行，完成后自动适应窗口。
3. 调整 **点大小**（1～20 px）、**文件原色 / 统一颜色**，或点击 **选择颜色…**。
4. 通过 **显示点云** 控制显隐，**视角复位 / 适应窗口** 恢复默认观察方向，**清空点云** 释放已加载的数据。

鼠标左键拖动旋转，中键或 Shift + 左键拖动平移，滚轮或右键拖动缩放。右侧同时显示有效点数、颜色信息和 XYZ 坐标范围。

加载失败会在窗口下方显示原因，并保留此前成功加载的点云及视角。NaN / Inf 坐标被跳过，成功后显示跳过数量。关闭弹窗后再次点击入口，会恢复同一个窗口和点云；加载过程中也可以关闭后重开。清空后保留文件路径，便于重新加载。

## 文件格式

| 格式 | 支持范围 |
| --- | --- |
| PCD | `DATA ascii`、`binary`、`binary_compressed`（LZF）；按 `FIELDS / SIZE / TYPE / COUNT` 读取，支持字段重排及多分量属性 |
| PLY | `ascii`、`binary_little_endian`、`binary_big_endian`；读取 `vertex` 坐标与颜色，忽略面拓扑，支持顶点之前的标量或列表元素 |
| XYZ / TXT | 每行 `x y z` 或 `x y z r g b`，列数须一致；空白、逗号、分号均可分隔；支持空行、`#` 注释与整行 `//` 注释 |

坐标字段使用 `x / y / z`。PCD 颜色支持打包的 `rgb / rgba`（float32 或 32 位整数）和独立 `r / g / b`；PLY 支持 `red / green / blue` 或 `r / g / b`。RGB 为 0～255 整数，alpha 不用于透明显示。没有 RGB 的点云使用统一颜色。文本头支持 UTF-8 BOM，文件路径支持中文。

示例 XYZ RGB：

```text
# x y z r g b
0 0 0 255 0 0
10 0 0 0 255 0
0 10 0 0 0 255
```

坐标按文件数值原样显示，不推断单位，也不应用机器人场景的缩放。单文件最多 1000 万个点，PCD 二进制数据和单次二进制读取最多 512 MiB；超过限制、文件截断、缺失坐标及不合法数值会报告错误。当前为单文件浏览，不提供多点云叠加、编辑或导出。

## 构建与运行

沿用已有 VTK 9.2.6 SDK，无需增加 VTK IO 模块或 PCL 依赖。Qt Concurrent 仅在启用 VTK 时用于后台读取。

```powershell
.\.vscode\qt-msvc.ps1 -Configuration Release -Action Build -WithVTK
.\.vscode\qt-msvc.ps1 -Configuration Release -Action Run -WithVTK
```

SDK 首次准备方式见 [VTK 双渲染后端使用说明](VTK双渲染后端使用说明.md)。普通 OpenGL 构建仍可编译运行，点云按钮会禁用，并在悬停提示中说明需启用 VTK。

## 实现与验证

- `domain/PointCloud.h`：与 Qt / VTK 无关的点云数据。
- `infrastructure/PointCloudFileLoader`：格式解析、颜色读取和输入校验，不依赖 VTK。
- `rendering/vtk/VtkPointCloudViewport`：VTK PolyData、顶点、RGB、相机及渲染交互。
- `presentation/PointCloudDialog`：文件选择、后台加载、显示控制和状态反馈。
- `demoui.cpp`：右侧按钮与弹窗复用。

`robot3d_pointcloud` 测试覆盖中文路径、BOM、各格式与编码、字段顺序、RGB、LZF、无效坐标、损坏和截断文件及点数限制；`robot3d_vtk` 在实际 OpenGL 上下文中验证弹窗、后台加载、点云图像、显示控制、失败保留场景、关闭重开及清空。原有机器人回归保留。

```powershell
& 'C:\CMake\bin\ctest.exe' --test-dir build/vscode-qt5.9.9-msvc2015_64-vtk/Release --output-on-failure
& 'C:\CMake\bin\ctest.exe' --test-dir build/vscode-qt5.9.9-msvc2015_64/Release --output-on-failure
```

截图保存在 VTK Release 构建目录的 `vtk-validation/pointcloud-dialog.png` 和 `pointcloud-rgb.png`；测试点云为同目录的 `点云显示测试.xyz`。

2026-10-16 本机 Release 验证：VTK 构建 4/4 组测试通过，普通 OpenGL 构建 3/3 组测试通过；已目视检查主窗口与点云弹窗截图。C++ 源码及 CMake 文件均通过白名单 Python 读写并核验存储层加密头；Markdown 使用说明为明文文件。
