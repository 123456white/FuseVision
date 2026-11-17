# FuseVision 环境与启动指南

从原项目根目录README整理而来，归档日期：2026-11-15。

[返回文档总入口](README.md) · [功能与按钮](logic/FEATURES.md) · [技术栈](codebase/STACK.md) · [目录与入口](codebase/STRUCTURE.md) · [实际架构](codebase/ARCHITECTURE.md) · [测试范围](codebase/TESTING.md)

当前实现边界以功能分析为准：深度学习9个子页，其中后5页为占位；18个历史页面键仅作兼容，现统一采用未登录只读/登录可编辑；传统视觉主要为界面骨架。依赖或SDK存在不表示业务流程已接通。

## 环境要求

| 工具 | 最低版本 | 用途 |
|------|----------|------|
| CMake | 3.21；VS 2026 解决方案需要 4.2+ | 构建系统 |
| Ninja | 仅 Ninja 入口需要加入 PATH | 命令行构建生成器 |
| MSVC | VS 2022 (17.x) 或 VS 2026 (18.x) | 安装 x64 C++ 桌面开发工具 |
| Git | 任意 | 克隆仓库与 vcpkg 依赖工具 |

可选（传统视觉模块）：
- Halcon SDK（设置 `HALCONROOT` + `HALCONARCH` 环境变量，或放入 `sdk/halcon/` 目录）
- 海康 MVS SDK（默认路径 `sdk/MVS/`）

## 快速开始

### 1. 打开项目

以下命令均在FuseVision项目根目录运行；阅读本文件时无需切换到docs目录执行。

`vcpkg/` 是被 Git 忽略的本地依赖目录，不是已登记的 Git 子模块。首次检出且目录尚不存在时，先执行：

```powershell
git clone https://github.com/microsoft/vcpkg.git vcpkg
.\vcpkg\bootstrap-vcpkg.bat
```

已有 `vcpkg/` 时无需重复克隆。后续配置会按 `vcpkg.json` 自动安装 Qt、OpenCV 等依赖。

### 2. 环境检查

```powershell
.\scripts\check-env.ps1
```

确保 `[OK]` 全部通过后再继续。

### 3. 配置 & 编译

使用一键构建脚本：

```powershell
.\CmakeBuild.ps1
```

交互式菜单选项：
- `1` — Debug 构建并运行
- `2` — Release 构建并运行
- `3` — 清理 + Debug 重建
- `4` — 清理 + Release 重建
- `5` — 备份 src 目录

自动化构建与测试（无需交互，也不启动常驻窗口）：

```powershell
.\CmakeBuild.ps1 -Configuration Debug -NoRun -Test
# Release 使用相同入口
.\CmakeBuild.ps1 -Configuration Release -NoRun -Test
```

脚本以自身所在目录定位项目，并通过 `vswhere` 自动检测 VS C++ 工具；已有构建优先使用缓存中的编译器。
配置、编译或测试失败都会返回非零退出码。`-Clean` 可清理对应配置后重建。

BUILD_TESTING默认ON，自动通过vcpkg的tests feature安装GoogleTest；当前34个存储GTest与6组既有回归共40项。测试使用临时目录、INI与隔离数据库；执行范围和筛选方式见[测试策略](codebase/TESTING.md)，结果不能替代完整可见桌面验收。纯应用构建可在独立构建目录配置 `-DBUILD_TESTING=OFF`；不需要另行下载/链接GoogleTest。

项目、模型、数据集、账户库、日志、用户配置和恢复文件的命名/位置规则统一见[存储契约](logic/DATA-CONTRACTS.md#工作目录与运行文件归属)。项目、模型和数据集原生配置必须符合完整1.0新标准；旧无标记配置、绝对根引用及yolo布局拒绝打开，不自动升级、搬移或删除。请新建数据；外部COCO/YOLO/LabelMe等格式导入导出保留。

或手动命令：

以下 Ninja 命令需要在 VS 的 x64 开发者 PowerShell / 命令提示符中执行；普通 PowerShell 使用上面的一键构建脚本，它会自动初始化 MSVC 环境。

```powershell
# Debug
cmake --preset windows-x64-debug
cmake --build --preset windows-debug

# Release
cmake --preset windows-x64-release
cmake --build --preset windows-release
```

### 4. 运行

```powershell
.\out\build\windows-x64-debug\bin\Debug\FuseVision.exe
```

### Visual Studio 编译与调试

Visual Studio 和命令行共用根目录的 `CMakeLists.txt`，源码、Qt 资源、依赖部署及测试只维护一套。

生成可双击打开的原生 VS 解决方案：

```powershell
# 检查 VS 入口所需工具（不要求 Ninja 在 PATH 中）
.\scripts\check-env.ps1 -Generator VisualStudio

# 自动检测 VS 2022 / 2026，只配置并生成解决方案
.\CmakeBuild.ps1 -Generator VisualStudio -Configuration Debug -ConfigureOnly

# 使用 MSBuild 编译并执行测试
.\CmakeBuild.ps1 -Generator VisualStudio -Configuration Debug -NoRun -Test
.\CmakeBuild.ps1 -Generator VisualStudio -Configuration Release -NoRun -Test
```

脚本会打印生成的解决方案路径，位于 `out/build/windows-vs2022-x64/` 或 `out/build/windows-vs2026-x64/`，文件名为 `FuseVision.sln` 或 `FuseVision.slnx`（由 CMake/VS 版本决定）。用对应版本的 Visual Studio 打开，选择 `x64` 和 `Debug` / `Release`，启动项目为 `FuseVision`；Qt 的 MOC/UIC/RCC 由 CMake 生成的规则处理。

也可以在 Visual Studio 中选择“打开文件夹”，打开项目根目录后选择 `windows-x64-debug` / `windows-x64-release` CMake 预设，直接构建和调试。

显式指定 VS 版本时使用以下预设（本机需安装相应版本）：

```powershell
# VS 2022
cmake --preset windows-vs2022-x64
cmake --build --preset windows-vs2022-debug
ctest --preset windows-vs2022-debug

# VS 2026（CMake 4.2+）
cmake --preset windows-vs2026-x64
cmake --build --preset windows-vs2026-release
ctest --preset windows-vs2026-release
```

VS 解决方案采用多配置生成器：同一输出目录包含 Debug 和 Release，程序位于该目录下的 `bin/Debug/` 或 `bin/Release/`。使用 `-Clean` 会清理整套解决方案构建目录。Ninja 的两种配置继续使用各自独立的目录。`-ConfigureOnly` 不能与 `-Test` 同时使用。

### 管理账户与只读启动

启动直接进入只读界面，无需登录。左下角点击登录后解除只读，退出登录恢复只读；账户设置页可修改密码，没有角色或权限矩阵。

数据库目录在系统设置保存后下次启动生效，文件名固定为fusevision.db；空配置使用exe/data。已有目标库直接复用；空目标从最近活动库建立包含已提交WAL页的一致副本，保留原库与密码，不合并已有目标。记录的来源缺失、目录不可用或迁移失败会显示原因并停止启动；修复路径后重试。重置回默认目录且已有默认库时，使用该库自身账户。

新数据库仅创建一个账户：`superadmin`，默认密码`admin123`。已有数据库沿用superadmin的原密码；没有该账户时沿用角色值最高、ID最早的现有账户，实际名称显示在登录框。不会清空历史账户或重置其密码，但历史其他账户不再用于解锁。原“记住密码”不再读取，登录后清除历史明文记住字段。

## CMake Presets

| Preset | 平台 | 构建类型 |
|--------|------|----------|
| `windows-x64-debug` | Windows x64 | Debug |
| `windows-x64-release` | Windows x64 | Release |
| `windows-vs2022-x64` | Windows x64 / VS 2022 | Debug + Release |
| `windows-vs2026-x64` | Windows x64 / VS 2026 | Debug + Release |
| `linux-x64-debug` | Linux x64 | Debug |
| `linux-x64-release` | Linux x64 | Release |
| `macos-x64-debug` | macOS x64 | Debug |
| `macos-x64-release` | macOS x64 | Release |

## 运行时外观、日志与CI

### 主题系统

- **ThemePalette**：50+ 语义化颜色变量（背景 / 文字 / 边框 / 主题色 / 语义色 / 浮层 / 菜单 / 状态栏 / 滚动条）
- **18 类 QSS 构建方法**：Global / Widget / Button / Table / Form / Menu / StatusBar / Splitter / GroupBox / ToolBar / Dialog / TabBar / Tree / ToolButton / ScrollBar / SidebarBtn / LogMonitor
- **Windows DWM**：标题栏 `DWMWA_USE_IMMERSIVE_DARK_MODE` + `DWMWA_CAPTION_COLOR` 跟随主题

### 日志系统

- **双输出**：Debug 模式输出控制台（彩色）+ 文件，Release 模式仅写文件
- **每日滚动**：`daily_file_sink_mt` 按天切分日志，无文件数量限制
- **运行时配置**：保存系统设置后即时切换日志路径和级别，无需重启
- **LogMonitor**：可折叠面板，HTML 带颜色渲染，最多 1000 行

## CI/CD

每次 push / PR 到 `main` 分支自动触发 GitHub Actions 构建：

- **Runner**：windows-2022
- **缓存策略**：vcpkg 工具链缓存与依赖二进制缓存
- **二进制缓存**：`vcpkg/bincache`，后续运行跳过耗时编译
- **检查**：环境自检 → Ninja 构建及 CTest → VS 2022/MSBuild 构建及 CTest → 上传两种构建产物及测试日志
- **手动触发**：工作流支持 `workflow_dispatch`

本地单独执行测试：

```powershell
ctest --preset windows-debug
ctest --preset windows-release
```

`startup-smoke` 使用 Qt offscreen 平台、内存 SQLite、临时项目目录和独立测试日志，验证默认用户、错误密码拒绝、权限初始化、登录对话框和主窗口各页面初始化后退出。
测试不修改现有用户数据库。它覆盖启动与基础认证，不覆盖登录后的业务操作、相机采集、模型训练或第三方 SDK 的完整功能。
Windows 测试会将 PATH 限制为系统目录并清空外部 Qt 插件路径，避免借用 Conda、Qt SDK 等开发环境中的运行库。
当前流水线只构建、测试和上传产物，没有生产环境自动部署。

2026-11-15 在验证分支 `codex/ci-validation-20261115` 实测：首次 Ninja 构建约 117 分钟；缓存命中后，提交 `9264b74` 的 Ninja + VS 2022 双入口流水线耗时 5 分 45 秒，两个入口的启动测试各 1/1 通过。两个下载包均校验 SHA-256 一致并独立启动成功。此结果对应上述验证提交。

## API文档与代码规范

源码布局与依赖方向见[架构](codebase/ARCHITECTURE.md)和[目录](codebase/STRUCTURE.md)。新增代码遵循根目录`.editorconfig`、`.clang-format`及[代码惯例](codebase/CONVENTIONS.md)；关键公共接口用Doxygen说明输入、返回/失败语义、所有权与线程要求。不要对历史代码做无关全量格式化。

本机已配置Doxygen 1.18.0。在项目根目录运行：

```powershell
./scripts/build-docs.ps1 -DoxygenPath D:/software/doxygen-1.18.0.windows.x64.bin/doxygen.exe
# 或设置DOXYGEN_ROOT后使用已配置的构建目录
cmake --build out/build/windows-x64-release --target docs
```

其他环境可设置`DOXYGEN_ROOT`为包含doxygen.exe的目录，或在CMake配置时传入`-DDOXYGEN_EXECUTABLE=<路径>`。未安装Doxygen不影响应用编译。生成入口为`out/docs/doxygen/html/index.html`，XML为`out/docs/doxygen/xml/index.xml`；输出均在忽略目录out。文档警告会令生成失败，详情在`out/docs/doxygen/warnings.log`。当前生成完整声明索引，已补充模块及关键接口注释，不代表历史所有成员注释完整。

## License

MIT

## 业务文档阅读与自更新

API索引之外，业务说明由当前MD生成到out/docs/site；首次运行`./scripts/docs/setup.ps1`，再执行`python -X utf8 scripts/docs/docflow.py sync --html`、`python -X utf8 scripts/docs/docflow.py check --diagrams`。用`python -X utf8 scripts/docs/docflow.py serve`在本机阅读完整图表。会话维护与Codex项目Hooks的启用/降级见[维护说明](maintenance/README.md)，规则见[规范](maintenance/STANDARD.md)。
