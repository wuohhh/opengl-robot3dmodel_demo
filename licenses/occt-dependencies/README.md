# OCCT 实际运行依赖许可材料

记录日期：2026-10-30。范围为本轮 OCCT 7.9.1 官方 Windows SDK 的实际第三方运行依赖。此目录提供来源、许可原文、运行配置和核对记录，**不表示已完成法务审批或满足全部对外分发义务**。

## 运行库与材料

| 依赖 | 本轮识别版本 | 材料及上游来源 |
| --- | --- | --- |
| oneTBB | 官方包目录 2021.13.0 | [Apache 2.0 原文](TBB-LICENSE.md)、[上游第三方声明](TBB-NOTICE.md)、[包内说明](TBB-package-README.md)；[对应源码](https://github.com/uxlfoundation/oneTBB/tree/v2021.13.0) |
| jemalloc | 包内 CONTROL 为 5.3.0 / Port-Version 1；DLL 的版本查询返回缺失版本标记，不能据该标记独立确认源码 | [包内版权与 BSD 条款](jemalloc-COPYRIGHT.md)、[构建标记](jemalloc-BUILD-INFO.md)、[CONTROL](jemalloc-CONTROL.md)；[上游 COPYING](https://github.com/jemalloc/jemalloc/blob/5.3.0/COPYING) |
| OpenVR | 官方包目录 1.14.15 | [对应 tag 的 LICENSE](OpenVR-LICENSE.md)；[对应源码](https://github.com/ValveSoftware/openvr/tree/v1.14.15) |
| FreeType | DLL 运行查询 2.13.3 | [包内 LICENSE](FreeType-LICENSE.md)、[对应 tag 的 FTL](FreeType-FTL.md)；[对应源码](https://gitlab.freedesktop.org/freetype/freetype/-/tree/VER-2-13-3) |
| FreeImage | DLL 运行查询 3.18.0 | [包内 FIPL](FreeImage-FIPL.md)、[GPLv2](FreeImage-GPL-2.md)、[GPLv3](FreeImage-GPL-3.md)；[官方项目及源码入口](https://freeimage.sourceforge.io/) |
| FFmpeg | 官方包目录 3.3.4，四个 DLL 的 API 版本见运行记录 | [LGPLv2.1 原文](FFmpeg-LGPL-2.1.md)、[对应源码许可说明](FFmpeg-LICENSE.md)、[实际 DLL 编译配置](FFmpeg-build-configuration.md)；[3.3.4 源码](https://ffmpeg.org/releases/ffmpeg-3.3.4.tar.xz) |

原文以 `.md` 后缀保存，保留来源字节；文件名调整不表示上游存在同名文件。例如 `TBB-NOTICE.md` 来自上游 `third-party-programs.txt`，jemalloc 的许可原文来自包内 `share/jemalloc/copyright`。每份材料的原始来源、成员路径、校验和及取得方式见 [provenance.json](provenance.json)。

## 实际 FFmpeg 配置

在白名单 Python 进程内临时设置 SDK DLL 搜索目录，调用四个实际 DLL 的 `*_license()`、`*_configuration()` 和 `*_version()`。调用后恢复进程 DLL 搜索设置，未修改系统目录。

四个 DLL 均声明 `LGPL version 2.1 or later`，配置中未发现 `--enable-gpl` 或 `--enable-nonfree`，实际配置与哈希完整记录在 [FFmpeg-build-configuration.md](FFmpeg-build-configuration.md)。这些结果只证明本轮 DLL 的运行声明和配置事实；仍须核对与二进制完全对应的源码、补丁和最终分发方式。

## 致谢与后续审查

本软件使用 oneTBB、jemalloc、OpenVR、FreeType、FreeImage 和 FFmpeg。版权主体、免责与条件以各原文为准。

FreeType 的 FTL 与 GPL 是可选许可；FreeImage 提供 FIPL 与 GPL 选择。本目录同时保留可获得的选择材料，最终产品应明确所采用的许可并满足条件，不能因附带多份文本就视为选择或履行完成。FreeType 的 FTL 要求产品文档保留项目致谢；FreeImage 官方要求随产品附所选许可并致谢。可参阅 [FreeType 官方许可说明](https://freetype.org/license.html) 与 [FreeImage 官方许可说明](https://freeimage.sourceforge.io/license.html)。

This software uses the FreeImage open source image library. See https://freeimage.sourceforge.io/ for details.

This software uses libraries from the FFmpeg project. Runtime libraries in this validation build report LGPL version 2.1 or later. See https://ffmpeg.org/ for details.

后续对外分发前，仍须至少确认以下事项：

- FFmpeg 等 LGPL 组件与已部署 DLL 完全对应的源码、补丁、构建说明，以及源码交付、替换/重新链接和最终用户许可条款。
- FreeImage 内置编解码插件、FreeType 内嵌子组件等的实际编译范围、附加版权和传递许可。此目录未冒称已完成静态内嵌代码清单审计。
- FreeType/FreeImage 的最终许可选择与致谢、oneTBB 的适用第三方声明、OpenVR 与 jemalloc 的版权保留。
- 官方 SDK 的完整来源链、各版本补丁和最终发布目录中所有依赖的许可一致性。

Microsoft Debug CRT 仅在 SDK 的 `bind` 中用于本机开发测试，不属于 Release 验证包。其微软 payload SHA 和 DLL 有效签名另记录于 SDK `debug-crt-provenance.json`；不得将它当可对外再分发的运行库。

依赖二进制来自 [OCCT V7_9_1 官方 release](https://github.com/Open-Cascade-SAS/OCCT/releases/tag/V7_9_1) 的 `3rdparty-vc14-64.zip`（asset 257073494）。只读取必要 ZIP 成员并核对 CRC，未从第三方 DLL 站获取文件。清单同时记录 Release 与 Debug 的实际 SHA；Debug 的 jemalloc 与 Release 是不同的官方成员。
