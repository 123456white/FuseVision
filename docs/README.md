# FuseVision 文档中心

当前入口核对：2026-11-17。文档按“使用软件 → 理解业务 → 理解代码 → 查看证据 → 实施改进”组织；生成物与历史资料分别维护。

本次[四项问题修复](logic/VALIDATION.md#2026-11-17-四项问题修复)：ISS-011～014已修复，Release构建与CTest 6/6通过；原始诊断证据保留。验证覆盖导入前语义、旋转几何、隔离进程数据库迁移与分类数据链路，完整可见桌面仍待人工验收。

随后完成[功能回归与存储规范](logic/VALIDATION.md#2026-11-17-功能回归与存储规范)，并按用户“开发阶段可执行新数据标准”的决定收紧为[严格原生存储标准](logic/VALIDATION.md#2026-11-17-严格原生存储标准)：当前34个GoogleTest与6组回归共40项Release测试通过；原生配置要求完整类型、版本与字段，新模型立即生成数据集配置，旧无标记/旧布局/绝对根引用拒绝且不改写。规则见[存储契约](logic/DATA-CONTRACTS.md)。

本轮[完整功能关系回归与CI/CD](logic/VALIDATION.md#2026-11-17-完整功能关系回归与cicd)补验Debug/Release各41项、20个TC及25条执行映射，发现并修复ISS-016两处生命周期崩溃与ISS-017插件运行依赖，新增主题/日志/筛选/重排恢复断言和运行包验证。远端运行状态以该验证节为准。

| 我想了解 | 阅读入口 |
|---|---|
| 环境、启动、构建与账号 | [启动指南](GETTING-STARTED.md) |
| 软件有哪些页面和按钮 | [功能清单](logic/FEATURES.md) |
| 核心业务如何闭环 | [核心流程](logic/CORE-FLOW.md)、[功能关系](logic/FEATURE-RELATIONS.md) |
| 什么状态下允许什么操作 | [状态图与迁移表](logic/STATE-MACHINES.md) |
| 数据、文件和算法如何处理 | [数据契约](logic/DATA-CONTRACTS.md)、[算法说明](logic/ALGORITHMS.md) |
| 功能之间是否有有效测试 | [测试追踪](logic/TEST-TRACEABILITY.md)、[测试策略](codebase/TESTING.md) |
| 模块和代码之间的关系 | [架构](codebase/ARCHITECTURE.md)、[源码结构](codebase/STRUCTURE.md) |
| 真实界面和验证限制 | [截图索引](logic/SCREENSHOTS.md)、[验证记录](logic/VALIDATION.md) |
| 宏观导图 | [可编辑XMind](logic/FuseVision-功能与代码.xmind)、[生成JSON](logic/FuseVision-功能与代码.json) |
| 问题、计划与产品决策 | [问题清单](refactor/ISSUES.md)、[核心规格](refactor/spec-process-workflow-consistency.md)、[实施计划](refactor/refactor-workflow-consistency-1.md) |
| 文档如何保持更新 | [维护命令与Hooks](maintenance/README.md)、[维护规范](maintenance/STANDARD.md)、[项目规则](../AGENTS.md) |
| 全部当前文档 | [自动目录](maintenance/CATALOG.md) |

## 当前软件范围

已实现项目、模型配置、数据集、Qt原生标注、格式处理、单账户与设置等本地能力。访客可浏览，登录后可写；LabelMe进程入口已退役，部分JSON读取兼容保留。训练、预测、数据划分、模型导出等独立页面及传统视觉执行仍为占位，不能由SDK或按钮存在推断完成。业务验证以对应报告的日期和范围为准。

## 文档目录

```text
docs/
├── README.md、GETTING-STARTED.md  # 总入口与使用方法
├── maintenance/                 # 规范、自更新说明、机器配置和生成清单
├── logic/                       # 功能、关系、状态、算法、测试追踪和导图
│   ├── screenshots/             # 原始截图与历史编号副本
│   └── evidence/                # 原始验证结果，不由生成器重写
├── codebase/                    # 架构、目录、技术栈、惯例、集成与测试策略
├── refactor/                    # 缺陷、规格、计划与实施记录
├── requirements/                # 历史原始需求，非当前实现证明
├── archive/                     # 旧脚本与迁移前快照，只用于追溯
└── Doxyfile、doxygen/            # API索引源配置与分组
```

当前维护工具统一在仓库 `scripts/docs/`。业务HTML生成到 `out/docs/site/`，API HTML/XML仍在 `out/docs/doxygen/`；不手工编辑生成HTML。原始需求见[FUNCTION-RECORD](requirements/FUNCTION-RECORD.md)，迁移记录见[维护验收](maintenance/MIGRATION.md)。

## 常用维护入口

在项目根目录执行，首次先按[维护说明](maintenance/README.md)安装本项目依赖：

```powershell
python -X utf8 scripts/docs/docflow.py sync --html
python -X utf8 scripts/docs/docflow.py check --diagrams
python -X utf8 scripts/docs/docflow.py serve
```

最后一条仅在本机提供HTML阅读服务；终端打印打开地址，Ctrl+C停止。Codex钩子必须经客户端审查信任后才会自动运行；项目规则和手动维护入口同时保留。
