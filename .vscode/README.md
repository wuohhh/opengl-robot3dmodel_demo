# VTK / Qt 5.9.9 / MSVC2015 x64 开发配置

本工程以根目录的 `CMakeLists.txt` 为构建入口，不需要 .pro 文件。任务脚本会加载 VS2015 的 amd64 环境，统一启用 VTK，使用 CMake + JOM 编译，并准备模型、Assimp 和 VTK DLL。

## 在 VS Code 中使用

| 操作 | 入口 |
|---|---|
| 编译 Debug | Ctrl+Shift+B（默认任务 VTK: Build Debug） |
| 编译 Release | “终端 → 运行生成任务”选择 VTK: Build Release |
| 调试 | F5，选择 VTK Debug 或 VTK Release，自动先构建 |
| 不附加调试器运行 | Ctrl+F5，或“终端 → 运行任务”选择 VTK: Run Debug / VTK: Run Release |
| 重新构建 | VTK: Rebuild Debug / VTK: Rebuild Release |
| 清理生成文件 | VTK: Clean Debug / VTK: Clean Release |
| 仅配置 CMake | VTK: Configure Debug / VTK: Configure Release |

需安装 Microsoft C/C++ 扩展（ms-vscode.cpptools）。首次打开可接受工作区的扩展推荐。

`c_cpp_properties.json` 提供 VTK Debug / VTK Release 两组 IntelliSense 配置，可用“C/C++: Select a Configuration”切换；各自读取对应编译数据库，并提供本机头文件路径兜底。实际编译参数由 CMake 决定。

## 输出与工作目录

```text
build/vscode-qt5.9.9-msvc2015_64-vtk/
├── Debug/
│   ├── bin/robot3ddemo.exe、robot3ddemo.pdb、Assimp 与 VTK DLL
│   └── compile_commands.json
├── Release/
│   ├── bin/robot3ddemo.exe、robot3ddemo.pdb、Assimp 与 VTK DLL
│   └── compile_commands.json
└── robotType/                构建时复制的模型与 config.xml
```

Run 任务、F5、Ctrl+F5 均设置 `DEMO_RENDERER=vtk`，工作目录统一为 `build/vscode-qt5.9.9-msvc2015_64-vtk`。程序相对路径 `./robotType/...` 从这里查找，运行日志也写在这里。两种配置分别使用 Qt Debug / Release DLL，Qt 插件通过环境变量从本机 Qt 安装目录加载。

Debug 使用 /MDd 和调试符号；Release 使用 /MD、/O2 优化，并额外启用 /Zi 和 /DEBUG 生成 PDB。优化后的源码行可能被合并或移除，Release 下部分局部变量不可见。

## 工具链与依赖

| 项目 | 本机路径 |
|---|---|
| Qt | `C:/Qt/Qt5.9.9/5.9.9/msvc2015_64` |
| MSVC 初始化 | `C:/Program Files (x86)/Microsoft Visual Studio 14.0/VC/vcvarsall.bat`，参数 amd64 |
| CMake | `C:/CMake/bin/cmake.exe`（要求至少 3.20，当前验证使用 3.30） |
| JOM | `C:/Qt/Qt5.9.9/Tools/QtCreator/bin/jom.exe` |
| Assimp | `assimp/MSVC2015_64/assimp-vc130-mt.lib` 与同名 DLL |

Assimp 3.2 的 x64 包复制自本机原项目 `CGX-TP/CGX-TP/move3D/model3D/assimp/MSVC2015_64`，现已随当前工程保存，构建不再依赖原项目路径。包内文件保留历史名称 vc130；已检查 DLL 为 x64、PE 链接器版本 14.0，依赖 MSVCP140/VCRUNTIME140，与现有头文件一致。

原包只有 Release 版 Assimp，两种主程序配置均链接该 DLL。本项目使用的导入接口传递字符指针、Assimp 数据结构，不跨 DLL 传递 STL 容器；Debug 主程序仍使用 Qt 调试库。没有 Assimp 自身的 PDB，调试以本项目源码为主。

旧的 `assimp/lib/libassimp.dll.a` 与 `bin/libassimp.dll` 是 MinGW 32 位依赖，本套配置不使用它们。CMake 会拒绝非 MSVC2015 或非 x64 的编译器。

换电脑时同步调整 `qt-msvc.ps1`、`launch.json`、`c_cpp_properties.json` 中的本机路径。任务配置在独立进程中导入编译器环境，不修改系统环境变量。CMake Tools 自动配置已关闭，使用上述任务可确保加载正确的 VS2015 环境。

## 命令行

从项目根目录运行（首次构建前先准备对应配置的 VTK SDK；VS Code 的 Build / Rebuild / Configure 任务会自动执行此依赖步骤）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-vtk.ps1 -Configuration Debug -IfMissing
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-vtk.ps1 -Configuration Release -IfMissing
powershell -NoProfile -ExecutionPolicy Bypass -File .vscode/qt-msvc.ps1 -Configuration Debug -Action Build
powershell -NoProfile -ExecutionPolicy Bypass -File .vscode/qt-msvc.ps1 -Configuration Release -Action Build
powershell -NoProfile -ExecutionPolicy Bypass -File .vscode/qt-msvc.ps1 -Configuration Debug -Action Run
```

脚本始终启用 VTK，兼容已有命令中的 `-WithVTK` 参数。`-Action` 还支持 Configure、Rebuild、Clean；`-Jobs` 控制并行数（默认最多 4）。Clean 通过 CMake 清理该配置的编译产物，保留配置缓存和共用模型目录。

参考：[CMake 的 MSVC 运行库选择](https://cmake.org/cmake/help/v3.30/prop_tgt/MSVC_RUNTIME_LIBRARY.html)、[VS Code C++ 调试配置](https://code.visualstudio.com/docs/cpp/launch-json-reference)。

## 本机兼容性与验证

Qt 5.9.9 的着色器磁盘缓存会在本机 Intel OpenGL 驱动中触发崩溃。Run 任务和两组调试配置均设置 `QT_DISABLE_SHADER_DISK_CACHE=1`；保留桌面 OpenGL 渲染，启动时重新编译着色器。

CMake 使用自身的头文件依赖扫描，避免中文版 VS2015 的 `/showIncludes` 编码导致依赖过滤失败。此设置也传入 CMake 编译器自检项目，Debug / Release 均已确认生成头文件依赖表。

本机已验证 Debug / Release 编译、PDB 生成、Run 任务启动、模型渲染、90 步关节模拟、PNG 帧导出及正常退出；Windows CDB 调试器均命中 `main.cpp:45`，并确认载入对应版本的 Qt 和 Windows 平台插件。

文件存储状态：新增的 `.vscode` 配置和 MSVC 依赖副本为明文；`CMakeLists.txt` 由白名单 Python 写入后保持加密，已确认 CMake 可正常读取。该项是工具链适配阶段的历史记录；后续 MVP 重构的源码与验证见 [MVP 实施记录](../docs/MVP重构实施记录.md)。`.gitignore` 额外允许 `robotType/**/*.obj`，防止把机器人模型误当编译目标文件忽略。

已进一步通过 VS Code 扩展宿主执行实际调试配置（含 preLaunchTask 自动构建）：Debug 与 Release 均命中 `main.cpp:45`，断点原因为 breakpoint，源码校验开启。为避免本机 Qt/CMake 扩展后台扫描工具包时弹出选择框，工作区关闭了自动工具包扫描；其它项目和用户全局设置不变。

## MVP 分层与回归

当前构建将核心、基础设施、展示器、渲染和壳层编入独立库；原 Build/Run/F5 入口保持可用。默认同时生成 robot3d_mvp_tests 与 robot3d_gui_tests，可通过 CMake 的 ROBOT3D_BUILD_TESTS 选项关闭。

```powershell
& 'C:\CMake\bin\ctest.exe' --test-dir build/vscode-qt5.9.9-msvc2015_64-vtk/Debug --output-on-failure
& 'C:\CMake\bin\ctest.exe' --test-dir build/vscode-qt5.9.9-msvc2015_64-vtk/Release --output-on-failure
```

测试截图输出在各配置的 mvp-gui-validation/，GUI 测试使用真实桌面 OpenGL，不连接机器人。

## VTK SDK 与调试

Debug / Release 的 Build、Rebuild、Configure 任务会先执行对应的 **VTK dependency: Build Debug / Release**，自动构建缺失的 VTK SDK。依赖任务使用 `-IfMissing`，已安装 SDK 通过检查后直接复用。Run 任务先执行对应的 Build 任务。

按 F5 时选择 **VTK Debug** 或 **VTK Release**，preLaunchTask 指向对应配置的构建链，并从 VTK 后端启动。Ctrl+Shift+B 对应 **VTK: Build Debug**。

Debug 应用位于 `build/vscode-qt5.9.9-msvc2015_64-vtk/Debug/bin/robot3ddemo.exe`。SDK 构建目录由 `build/deps/vtk-build-root-Debug.json` 记录，安装路径由 `build/deps/vtk-sdk-Debug.json` 记录，均为本机生成文件。Debug 使用独立 SDK 和 /MDd，不复用 Release VTK 库。

代码补全可通过“C/C++: Select a Configuration”选择 **VTK Debug** 或 **VTK Release**，使用对应的编译数据库和 VTK 头文件路径。

完整用法见 [VTK 双后端说明](../docs/VTK双渲染后端使用说明.md)。

2026-10-19 本机已执行上述 VTK Debug 构建任务链，SDK 复用检查与应用构建均通过；Debug CTest 4/4 组、204 项检查通过，程序使用 F5 配置中的环境完成 VTK 启动、30 步模拟与导帧。应用和 VTK 库的 PDB 已生成。
