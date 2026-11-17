# Technology Stack

初始分析2026-11-14；2026-11-17按模块整理后的代码复核。工作区含既存未提交变更。

## Core Sections (Required)

### 1) Runtime Summary

FuseVision是本地机器视觉桌面应用。当前可用范围集中在工程/模型配置、数据管理与标注UI、账号与设置；训练/预测及传统视觉执行链尚未接通。业务数据落本地文件与SQLite，没有发现业务HTTP API。

### 2) Production Frameworks and Dependencies

| 依赖 | 当前证据 | 用途/限制 |
|---|---|---|
| C++20 | CMakePresets的CMAKE_CXX_STANDARD=20 | 应用语言 |
| Qt Widgets/Core/Gui/Sql | 当前Release Qt6CoreConfigVersionImpl=6.10.2 | 窗口、图像、SQLite；Qt Test仅分析工具 |
| OpenCV | 当前Release OpenCVConfig-version=4.12.0；顶层REQUIRED | 通过FuseVisionVisionSdk集中向传统视觉提供，未见已接通的cv::业务执行链 |
| spdlog | 当前安装1.17.0，manifest最低1.16 | 日志文件/调试控制台 |
| SQLite | Qt QSQLITE驱动 | users与user_permissions；未独立锁定SQLite运行版本 |
| Halcon / MVS | cmake查找脚本、sdk目录 | 可选构建接入/资源，并非已完成视觉流程 |
| LabelMe运行入口（已退役） | 原生Qt标注已替代进程适配与部署 | 仅保留rectangle/polygon JSON数据兼容，不再是运行依赖 |
| Doxygen | 本机1.18.0；docs/Doxyfile | 可选文档工具，生成HTML/XML，不参与应用运行 |
| GoogleTest | 本轮安装1.17.0#2；vcpkg tests feature | 仅BUILD_TESTING时查找/链接，34个无Widgets存储行为用例，CTest逐例发现 |

manifest的version>=是最低约束，不能当作本机安装版本。vcpkg baseline固定fc3be1ebea7eaeb3071fe716ac65713af1f3a146。

### 3) Development Toolchain

CMake最低3.21，预设Ninja；Windows x64 MSVC。本机现有业务构建用MSVC19.50.35728（VS18 BuildTools），与README最低VS2022要求不矛盾。本次分析工具单独用Visual Studio 18 2026生成器链接原Release静态库；没有切换业务构建生成器。工程也声明Linux/macOS预设，未验证这两个平台，本轮未作跨平台业务验收；原Windows句柄嵌入入口已退役。

### 4) Key Commands

在仓库根PowerShell执行，开发环境需具备CMake/Ninja/MSVC和vcpkg依赖：

```powershell
.\CmakeBuild.ps1 -Configuration Release -NoRun -Test
# 或使用已配置工具链
cmake --preset windows-x64-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
.\out\build\windows-x64-release\bin\Release\FuseVision.exe
```

本轮Release重新编译成功，CTest五项通过，包括无Widgets数据层、模块边界、原生标注、业务回归和启动冒烟；此处沿用原阶段记录，本次文档任务未重跑。未重新编译外部Halcon/MVS SDK。文档命令见[启动指南](../GETTING-STARTED.md)，证据与限制见[验证报告](../logic/VALIDATION.md)。

### 5) Environment and Config

QSettings存路径、级别、主题、Python；默认业务目录在可执行文件旁。数据库路径由main组合/传入；启动直接以访客只读浏览，登录单账户后解锁，取消登录保持访客。FUSEVISION_STARTUP_SMOKE_TEST=1走隔离测试分支；QT_QPA_PLATFORM=offscreen用于无可见桌面验证。Halcon可用HALCONROOT/HALCONARCH配置；Doxygen可用DOXYGEN_ROOT或CMake缓存DOXYGEN_EXECUTABLE指定。现有默认凭据见[启动指南](../GETTING-STARTED.md)。

### 6) Evidence

[CMakeLists.txt:4 cmake_minimum_required](../../CMakeLists.txt:4)；[CMakePresets.json:14 CMAKE_CXX_STANDARD](../../CMakePresets.json:14)；[vcpkg.json:23 builtin-baseline](../../vcpkg.json:23)；[src/main.cpp:107 int main](../../src/main.cpp)；[src/core/SettingsManager.cpp:29 SettingsManager::load](../../src/core/SettingsManager.cpp:30)。

精确本机版本证据为out/build/windows-x64-release/vcpkg_installed/x64-windows/share中的Qt6Core/Qt6CoreConfigVersionImpl.cmake、opencv4/OpenCVConfig-version.cmake及spdlog/spdlogConfigVersion.cmake；它们是构建产物，不纳入可移植业务配置契约。

### 文档维护工具（非应用运行依赖）

Python标准库负责生成/检查与会话基线；Markdown 3.11.3生成阅读HTML，Node依赖由scripts/docs/package-lock.json锁定（Mermaid 11.17.2、xmind-generator 1.0.1）。它们安装在out/docs-tools，文档命令与限制见[维护操作](../maintenance/README.md)。不改变应用Qt/C++依赖。
