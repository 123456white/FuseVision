---
goal: 按核心通用能力、共享界面、深度学习和传统视觉整理代码并约束依赖
version: 1
date_created: 2026-11-17
last_updated: 2026-11-17
status: Completed
tags: [refactor, architecture, documentation]
---

# Introduction

状态：Completed。按本轮限定范围完成模块整理、边界约束和文档同步。保留既有业务行为、单账户只读规则和磁盘格式，拆开数据与界面反向依赖；使用本机 Doxygen 1.18.0 生成可浏览 API 文档。既有工作区已记录哈希，原文快照位于 out/module-refactor-baseline/source.zip。外部进程、可见桌面与真实负载的补验不作为已完成结果。

## 1. Requirements & Constraints

- **REQ-001**: src/core 只包含无 Widgets 依赖的通用服务；src/ui 承载共享界面组件和授权提示。
- **REQ-002**: src/deeplearning 的 domain/model/render/storage/tools 不包含 QWidget 页面；widgets 承担页面编排。
- **REQ-003**: src/traditional 与深度学习互不依赖；src/app 是组合根，公共头使用前向声明。
- **REQ-004**: Doxygen 分组与实际目录、CMake 目标一致，关键公共接口说明失败语义、调用线程和所有权。
- **CON-001**: 不修改任务映射、文件格式、权限规则，不开发占位执行引擎，不清理既有修改、不提交。
- **CON-002**: 文档集中在 docs；生成 HTML/XML 放入已忽略的 out，不把生成文件混入源码。

## 2. Implementation Steps

### Implementation Phase 1

| Goal | Description |
|---|---|
| GOAL-001 | 记录基线并识别真实跨层依赖 |

| Task | Description | Completed | Date |
|---|---|---|---|
| TASK-001 | 记录工作区和源码快照；运行现有 workflow-regression 与 startup-smoke | 是 | 2026-11-17 |
| TASK-002 | 新增 tests/check_module_boundaries.py，识别界面反向依赖并记录修改前结果 | 是 | 2026-11-17 |

### Implementation Phase 2

| Goal | Description |
|---|---|
| GOAL-002 | 形成可独立编译的核心与深度学习数据目标 |

| Task | Description | Completed | Date |
|---|---|---|---|
| TASK-003 | 提取 AnnotationShape、DatasetTypes、ProjectTypes、ModelTypes，移除存储/模型对 Widget 的引用 | 是 | 2026-11-17 |
| TASK-004 | 迁移源文件至 core/ui/app/deeplearning/traditional，更新活动引用并保留64项路径映射；LabelMe适配脚本随产物部署 | 是 | 2026-11-17 |
| TASK-005 | 拆分 FuseVisionCore、FuseVisionUiCommon、FuseVisionDeepLearningData 与各业务 UI 的 CMake 依赖 | 是 | 2026-11-17 |
| TASK-006 | FuseVision.h 改用前向声明；SDK 链接收敛到传统视觉目标；新增不含 Qt Widgets 的数据契约回归 | 是 | 2026-11-17 |

### Implementation Phase 3

| Goal | Description |
|---|---|
| GOAL-003 | 文档与源码一致且构建、行为和模块边界校验通过 |

| Task | Description | Completed | Date |
|---|---|---|---|
| TASK-007 | 配置 docs/Doxyfile、分组入口和文档构建目标；补关键公共接口 Doxygen 注释 | 是 | 2026-11-17 |
| TASK-008 | 更新现有架构、目录、惯例、集成、测试与风险说明，以及功能图的当前源码位置 | 是 | 2026-11-17 |
| TASK-009 | Release编译；CTest5/5（原15组业务回归）；Doxygen两入口生成；文档/源码保留校验 | 是 | 2026-11-17 |

## 3. Alternatives

- **ALT-001**: 只移动文件不能消除反向依赖，因此同时提取共享类型并拆分链接目标。
- **ALT-002**: 不引入插件容器、通用仓储接口或执行引擎；当前没有相应运行需求和验收样本。

## 4. Dependencies

- **DEP-001**: 已安装的 Qt 6.10.2、MSVC、CMake/Ninja、Python 与 vcpkg 缓存。
- **DEP-002**: 本机 D:/software/doxygen-1.18.0.windows.x64.bin/doxygen.exe；配置支持显式路径覆盖。

## 5. Files

- **FILE-001**: src/core、src/ui、src/app、src/deeplearning、src/traditional 及其 CMakeLists.txt。
- **FILE-002**: CMakeLists.txt、cmake/Documentation.cmake、tests/check_module_boundaries.py、tests/data_contract_regression.cpp。
- **FILE-003**: docs/Doxyfile、docs/codebase、docs/logic 与本实施记录；历史验证原文保留。

## 6. Testing

- **TEST-001**: 模块 include 白名单、无遗留源码引用、CMake 链接边界校验；修改前缺陷基线保留。
- **TEST-002**: 数据目标仅使用 QCoreApplication；编译时拒绝 QT_WIDGETS_LIB，临时目录验证配置、undo/redo、原子保存和导出重读。
- **TEST-003**: 原有 15 组业务回归与启动冒烟全部复跑，不以编译或文档成功替代业务通过。
- **TEST-004**: 本机 Doxygen 生成 HTML/XML；检查实际警告与引用，准确区分 API 索引和完整注释覆盖率。

实际结果：[CTest](../logic/evidence/module-refactor-ctest-release.log)、[机器校验](../logic/evidence/module-refactor-validation.json)、[Doxygen日志](../logic/evidence/module-refactor-doxygen.log)。10个文档分组、零文档警告，9张XMind分图结构校验通过。原始142文件快照/历史证据保留；C++归一化比较包含形状定义提取复原，只有LabelMe路径为有意行为修正。当前仍需补验可见Windows、原生对话框、LabelMe实际进程和相机、Debug、真实数据性能及XMind客户端；未运行clang-format全库格式化。剩余Widget编排和单例耦合见[风险](../codebase/CONCERNS.md)。

## 7. Risks & Assumptions

- **RISK-001**: 文件迁移可能遗失既有未提交内容；使用哈希、原文快照和一对一路径映射核对。
- **RISK-002**: 文档有大量历史路径；当前引用更新，历史证据明确标日期，不伪造旧日志。
- **ASSUMPTION-001**: 本轮仅整理已有传统视觉界面和 SDK 边界；传统算法尚未实现。

## 8. Related Specifications / Further Reading

[架构](../codebase/ARCHITECTURE.md) · [核心流程](../logic/CORE-FLOW.md) · [验证记录](../logic/VALIDATION.md) · [先前业务计划](refactor-workflow-consistency-1.md)
