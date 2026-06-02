# FuseVision

智能机器视觉一体化平台 — 集成传统图像处理（Halcon / OpenCV）、深度学习（YOLO）、工业相机采集与用户权限管理的桌面应用程序。

## 功能模块

| 模块 | 说明 |
|------|------|
| **深度学习** | 模型训练、推理（基于 YOLO） |
| **传统视觉** | 工业相机采集（海康 MVS SDK）、图像处理（Halcon / OpenCV） |
| **用户管理** | 三档角色（用户 / 管理员 / 超级管理员）、模块级读写权限控制 |
| **系统设置** | 日志路径、数据库路径配置 |
| **主题切换** | 亮色 / 暗色主题自动检测与手动切换 |
| **数据标注** | 集成 LabelMe 图像标注工具 |

## 技术栈

- **UI 框架**：Qt 6.10（Widgets + SQL + Network）
- **视觉库**：OpenCV 4.12、Halcon（可选）
- **深度学习**：YOLO / Ultralytics
- **相机 SDK**：海康 MVS SDK（Windows x64）
- **日志**：spdlog 1.17
- **数据库**：SQLite 3（用户 & 权限持久化）
- **构建系统**：CMake 3.21+ + Ninja
- **包管理**：vcpkg（manifest 模式）
- **CI/CD**：GitHub Actions（Windows 2025 + 二进制缓存）

## 项目结构

```
FuseVision/
├── src/
│   ├── MainWindows/          # 主窗口、登录、用户管理、系统设置
│   ├── DeepLearningWidget/   # 深度学习模块（训练 / 推理）
│   ├── TraditionalWidget/    # 传统视觉模块（相机采集 / 图像处理）
│   ├── core/                 # 核心基础设施
│   │   ├── DatabaseManager   # SQLite 数据访问层
│   │   ├── Logger            # 日志封装（spdlog）
│   │   ├── PermissionGuard   # 权限门控
│   │   ├── PermissionRegistry # 权限注册表
│   │   ├── SessionManager    # 用户会话管理（信号驱动）
│   │   ├── SettingsManager   # 配置持久化（QSettings）
│   │   └── ThemeManager      # 主题管理（亮色 / 暗色）
│   ├── MVS/                  # 海康相机 SDK（头文件 + 静态库）
│   ├── yolo/                 # YOLO 模型 & Ultralytics 框架
│   ├── labelme/              # LabelMe 标注工具（Python 子模块）
│   └── VisionPro/            # VisionPro 集成 Demo（C#）
├── res/                      # 图标资源
├── scripts/
│   └── check-env.ps1         # 开发环境自检脚本
├── .github/workflows/
│   └── build.yml             # CI/CD 构建流水线
├── CMakePresets.json         # CMake 预设配置
├── CMakeLists.txt            # 顶层 CMake 构建文件
├── vcpkg.json                # vcpkg 依赖清单
└── CmakeBuild.ps1            # 本地一键构建脚本
```

## 环境要求

| 工具 | 最低版本 | 用途 |
|------|----------|------|
| CMake | 3.21 | 构建系统 |
| Ninja | 任意 | 构建生成器 |
| MSVC | VS 2022 (17.x) | C++ 编译器 |
| Git | 任意 | 克隆 vcpkg + 子模块 |
| Python | 3.10+ | YOLO / LabelMe |

可选（传统视觉模块）：
- Halcon SDK（设置 `HALCONROOT` + `HALCONARCH` 环境变量）
- 海康 MVS SDK（默认路径 `src/MVS/`）

## 快速开始

### 1. 克隆仓库

```bash
git clone --recurse-submodules git@github.com:your-org/FuseVision.git
cd FuseVision
```

### 2. 环境检查

```powershell
.\scripts\check-env.ps1
```

确保 `[OK]` 全部通过后再继续。

### 3. 安装 vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git vcpkg
.\vcpkg\bootstrap-vcpkg.bat
```

### 4. 配置 & 编译

Debug 模式：

```powershell
cmake --preset windows-x64-debug
cmake --build --preset windows-debug --config Debug
```

Release 模式：

```powershell
cmake --preset windows-x64-release
cmake --build --preset windows-release --config Release
```

或使用一键脚本：

```powershell
.\CmakeBuild.ps1
```

### 5. 运行

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

## 权限体系

三档角色 + 模块级读写控制：

| 角色 | 用户管理 | 系统设置 | 深度学习 | 传统视觉 |
|------|:---:|:---:|:---:|:---:|
| 超级管理员 | RW | RW | RW | RW |
| 管理员 | RW | R | RW | RW |
| 用户 | - | - | 按配置 | 按配置 |

## CI/CD

每次 push / PR 到 `main` 分支自动触发 GitHub Actions 构建：

- **Runner**：windows-2025
- **缓存策略**：三层缓存（vcpkg 工具链 → 源码下载 → 编译产物）
- **二进制缓存**：`vcpkg/bincache`，后续运行跳过耗时编译

首次 CI 约需 30-60 分钟（Qt6 初始编译），缓存命中后缩短至 5-10 分钟。

## License

MIT
