# Codebase Structure

当前目录核对：2026-11-17。原目录到现目录的一对一路径映射见 [module-path-mapping.json](../logic/evidence/module-path-mapping.json)。原工作区未清理、reset 或提交，迁移前源文快照保留在 out/module-refactor-baseline/source.zip。

## 1. Top-Level Map

```text
src/
├── main.cpp
├── app/                 主窗口、登录、账户与系统设置
├── core/                不依赖 Widgets 的通用服务
├── ui/                  通用主题、控件和授权提示
├── deeplearning/
│   ├── domain/          几何、样本、项目和模型值对象
│   ├── model/           标注状态、事务和撤销
│   ├── render/          图像叠加和掩码
│   ├── storage/         配置与标签存储、文件传输
│   ├── tools/           标注工具
│   └── widgets/         页面、画布和协调器
└── traditional/         传统视觉布局、SDK 消费边界
```

CMakeLists.txt 是构建组合根，cmake 定义依赖发现、部署、边界检查和文档目标。tests 包含真实 Widget 回归、无 Widgets 数据回归、模块检查及Qt原生画布事件测试。根FuseVision.qrc保持资源名称兼容。sdk/vcpkg为第三方，不整理为自研业务源码。

2026-11-17增量：tests/storage_regression.cpp使用GoogleTest独立发现存储用例，storage/StorageContract.h统一原生配置守卫；运行数据目录及每类文件用途集中于[数据契约](../logic/DATA-CONTRACTS.md#工作目录与运行文件归属)，不与源码/构建目录混用。

## 2. Entry Points

src/main.cpp 初始化 Qt、主题、设置、日志、数据库和页面键，以访客只读启动 FuseVision。登录是主窗口可取消的动作。FuseVision::initPagesAsync 创建四主页面；DeepLearningWidget 创建九子页，前四页为已实现核心业务，其他训练等入口仍为占位。

文档入口为 docs/README.md；当前自更新工具在scripts/docs，规范/配置在docs/maintenance，旧工具已移入docs/archive。scripts/build-docs.ps1 可从任意工作目录生成 Doxygen；CMake docs 目标与之使用同一配置。

## 3. Module Boundaries

Core 不向 UI、深度学习或传统视觉引用。UiCommon 不依赖业务。Data 不包含 Widget 页面，文件服务使用领域样本类型；工具只借用模型/渲染器。各业务页面只在自己的模块内编排。主窗口 .cpp 负责组合，公共头用前向声明降低传递包含。详见[架构](ARCHITECTURE.md)。

模块 CMake 使用显式源文件列表和 target_link_libraries 的 PUBLIC/PRIVATE 区分。没有保留一套重复的旧目录转发头；当前 include 与文档引用统一到新路径。历史日志保留当时路径。

## 4. Naming and Organization Rules

目录按职责使用小写名称，类型/文件保持 PascalCase，成员 m_、槽 onXxx 不做无意义重命名。项目/模型配置格式、资源键、任务名称及单账户规则保持兼容。注释和文档规范见[CONVENTIONS](CONVENTIONS.md)。项目执行规则见 [AGENTS.md](../../AGENTS.md)，文档维护见 [维护规范](../maintenance/STANDARD.md)。

## 5. Evidence

[src 组合](../../src/CMakeLists.txt)、[主窗口头](../../src/app/FuseVision.h)、[数据类型](../../src/deeplearning/domain/DatasetTypes.h)、[include 边界测试](../../tests/check_module_boundaries.py)、[迁移基线](../logic/evidence/module-refactor-start-baseline.json)。扫描器原始结果为 .codebase-scan.txt，包含第三方目录，不能作为自研代码规模统计。
