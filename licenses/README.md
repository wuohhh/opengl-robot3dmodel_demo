本目录随应用部署，原文许可单独保存。

OCCT 7.9.1，未修改内核，动态链接。官方 tag V7_9_1，commit 36c80dbe7b199348e2f0a2d6932a462a37fd0b90。源代码：https://github.com/Open-Cascade-SAS/OCCT/tree/V7_9_1 。源码构建命令与依赖配置见 tools/build-occt.ps1；本机当前验证 SDK 为官方 Windows vc14 Release 预编译包，不是本机源码构建产物。官方包归档SHA256记录在 build/deps/occt-sdk-Release.json；依赖来源与逐文件SHA256在SDK的runtime-Release.json中。

本文件不是完整产品发布许可审查结论。官方预编译包包含TKService传递依赖（字体、图像、视频、OpenVR、TBB、jemalloc），这些依赖须随部署整理对应许可/源码义务；正式发布优先用源码裁剪构建关闭不用的功能，并重新验证。相关源代码和各自许可证以官方上游为准。

应用保留已有 Qt、VTK、Assimp 依赖，许可材料也须随正式发布包完整提供。当前工件功能对STEP/IGES使用OCCT，对STL/OBJ/PLY使用已有Assimp。没有启用OCCT Viewer、TKOpenGl或DRAW。