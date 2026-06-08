# FuseVision

智能机器视觉一体化平台 — 集成深度学习（YOLO）、传统图像处理（Halcon / OpenCV）、工业相机采集与用户权限管理的桌面应用程序。

## 功能模块

| 模块 | 说明 |
|------|------|
| **深度学习** | 项目管理 / 模型管理 / 数据集管理 / 数据标注 / 数据拆分 / 模型训练 / 模型预测 / 模型导出（8 标签页） |
| **传统视觉** | 流程编辑器 / 相机采集 / 相机标定 / 相机建模 / 通讯设置 / 系统设置，三区布局（资源管理 + 编辑器 + 视觉工具） |
| **用户管理** | 三档角色（用户 / 管理员 / 超级管理员）、模块级读写权限矩阵（QSplitter 可拖拽） |
| **系统设置** | 日志路径 / 数据库路径 / 日志级别，DL 与传统视觉数据存储位置，保存后即时生效 |
| **日志监控** | 页面底部可折叠日志面板（QSplitter 拖拽调整高度，最多保留 1000 行） |
| **主题系统** | 亮色 / 暗色主题（自动检测 + 手动切换），Windows 标题栏 DWM 适配 |

## 技术栈

- **UI 框架**：Qt 6.10（Widgets + SQL）
- **视觉库**：OpenCV 4.12、Halcon（可选）
- **工业相机**：海康 MVS SDK（Windows x64）
- **日志**：spdlog 1.17
- **数据库**：SQLite 3（用户 & 权限持久化，SHA-256 密码哈希）
- **构建系统**：CMake 3.21+ + Ninja
- **包管理**：vcpkg（manifest 模式，项目内嵌）
- **CI/CD**：GitHub Actions（Windows 2025 + 二进制缓存）

## 项目结构

```
FuseVision/
├── src/
│   ├── main.cpp                  # 应用入口（9 步启动流程）
│   ├── MainWindows/
│   │   ├── FuseVision.h/cpp      # 主窗口（侧边栏导航 + QStackedWidget）
│   │   ├── Login.h/cpp           # 登录对话框（模态）
│   │   ├── UserManagementWidget.h/cpp  # 用户管理 + 权限矩阵
│   │   └── SystemSettingsWidget.h/cpp  # 系统设置
│   ├── DeepLearningWidget/
│   │   └── DeepLearningWidget.h/cpp    # 深度学习模块（8 标签页 + LogMonitor）
│   ├── TraditionalWidget/
│   │   └── TraditionalWidget.h/cpp     # 传统视觉模块（三区布局 + LogMonitor）
│   └── core/                     # 核心基础设施
│       ├── Logger.h/cpp          # 日志封装（spdlog，控制台 + 文件双输出）
│       ├── DatabaseManager.h/cpp # SQLite 数据访问层（用户 CRUD + 权限 CRUD）
│       ├── SessionManager.h/cpp  # 用户会话管理（信号驱动单例）
│       ├── PermissionRegistry.h/cpp # 权限注册表 + 内存缓存
│       ├── PermissionGuard.h/cpp # Widget 级权限代理（watch / canRead / canWrite）
│       ├── PermissionInfo.h      # 权限数据结构
│       ├── ThemeManager.h/cpp    # 主题管理（18 类 QSS 构建 + DWM 适配）
│       ├── SettingsManager.h/cpp # 配置持久化（QSettings）
│       └── LogMonitor.h/cpp      # 可折叠日志监控面板
├── sdk/                          # 第三方 SDK 与工具
│   ├── MVS/                      # 海康相机 SDK（头文件 + 静态库）
│   ├── halcon/                   # Halcon SDK（可选）
│   ├── yolo/                     # YOLO 模型 & Ultralytics 框架
│   └── labelme/                  # LabelMe 标注工具（Python 子模块）
├── res/                          # 图标资源（亮色 / 暗色双套 PNG）
├── scripts/
│   └── check-env.ps1             # 开发环境自检脚本
├── .github/workflows/
│   └── build.yml                 # CI/CD 构建流水线
├── CMakePresets.json             # CMake 预设（Win/Linux/macOS × Debug/Release）
├── CMakeLists.txt                # 顶层 CMake 构建文件
├── vcpkg.json                    # vcpkg 依赖清单（Qt6 / spdlog / OpenCV）
├── CmakeBuild.ps1                # 本地一键构建脚本（交互式菜单）
└── FuseVision.qrc                # Qt 资源文件
```

## 环境要求

| 工具 | 最低版本 | 用途 |
|------|----------|------|
| CMake | 3.21 | 构建系统 |
| Ninja | 任意 | 构建生成器 |
| MSVC | VS 2022 (17.x) | C++ 编译器 |
| Git | 任意 | 克隆仓库 + 子模块 |

可选（传统视觉模块）：
- Halcon SDK（设置 `HALCONROOT` + `HALCONARCH` 环境变量，或放入 `sdk/halcon/` 目录）
- 海康 MVS SDK（默认路径 `sdk/MVS/`）

## 快速开始

### 1. 克隆仓库

```bash
git clone --recurse-submodules https://github.com/your-org/FuseVision.git
cd FuseVision
```

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

或手动命令：

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

### 默认登录凭据

| 角色 | 用户名 | 密码 |
|------|--------|------|
| 超级管理员 | `superadmin` | `admin123` |
| 管理员 | `admin` | `manager123` |
| 用户 | `user` | `123` |

## CMake Presets

| Preset | 平台 | 构建类型 |
|--------|------|----------|
| `windows-x64-debug` | Windows x64 | Debug |
| `windows-x64-release` | Windows x64 | Release |
| `linux-x64-debug` | Linux x64 | Debug |
| `linux-x64-release` | Linux x64 | Release |
| `macos-x64-debug` | macOS x64 | Debug |
| `macos-x64-release` | macOS x64 | Release |

## 架构设计

### 启动流程（9 步）

```
1. 控制台 UTF-8 编码  →  2. QApplication + Fusion 风格
3. ThemeManager 初始化 →  4. SettingsManager + Logger（日志路径/级别从 QSettings 恢复）
5. DatabaseManager（SQLite 建表 + 种子用户）
6. 权限数据迁移       →  7. 注册权限模块（17 个模块点）
8. 登录对话框（模态）→  9. 主窗口 → 事件循环
```

### 权限体系

三档角色 + 模块级读写控制：

| 角色 | 用户管理 | 系统设置 | 深度学习 | 传统视觉 |
|------|:---:|:---:|:---:|:---:|
| 超级管理员 | RW | RW | RW | RW |
| 管理员 | RW | R | RW | RW |
| 用户 | - | - | 按配置 | 按配置 |

**数据流**：`SessionManager::login()` → `PermissionRegistry::refreshPermissions(uid)` 预热缓存 → `sessionChanged` 信号 → `PermissionGuard::refresh()` → 差异对比 → `changed()` 信号 → Widget `applyPermissions()`

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

- **Runner**：windows-2025
- **缓存策略**：三层缓存（vcpkg 工具链 → 源码下载 → 编译产物）
- **二进制缓存**：`vcpkg/bincache`，后续运行跳过耗时编译

首次 CI 约需 30-60 分钟（Qt6 初始编译），缓存命中后缩短至 5-10 分钟。

## License

MIT
