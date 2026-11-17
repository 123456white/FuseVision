---
goal: 修复FuseVision现有核心流程的一致性并提取必要职责
version: '1.0'
date_created: '2026-11-14'
last_updated: '2026-11-15'
owner: FuseVision 项目维护者
status: 'Implemented - pending manual acceptance'
tags: [refactor, workflow, data-integrity, authorization]
---

# Introduction

实施状态：已实施主要代码与自动回归；保留明确的人工验收项。

下列阶段已顺序实施，保持原任务编号供追踪。TASK-014的可见桌面人工验收和TASK-011的确认重置/完整重启仍未完成；其余按自动测试覆盖范围标记，不能把“已编码”当作“全场景验收”。

## 1. Requirements & Constraints

- **REQ-101**：实现规格REQ-001/002，单一标注模型、可检查保存和离页决策，P0。
- **REQ-102**：实现规格REQ-005，统一入口及处理器授权与实时通知，P0。
- **REQ-103**：实现规格REQ-003/004，模型通知、删除上下文和类别保护，P1。
- **REQ-104**：实现规格REQ-006/007，表单与设置生效契约，P2。
- **REQ-105**：实现规格REQ-008，按fixture提取已存在的文件与格式职责，P2。
- **CON-101**：当前任务只规划，执行需后续获得业务修改任务；保留用户当前未提交修改，不能用reset/覆盖恢复工作区。
- **CON-102**：保持现有JSON/PNG/YAML兼容和四任务映射；不实现占位页、网络服务或额外相机/训练功能。

## 2. Implementation Steps

### Implementation Phase 1

- **GOAL-001**：P0标注一致性。完成标志：规格AC-001/002通过；依赖原Release可构建环境。TASK-001→002→003→004顺序。

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-001 | 在拟新增tests/annotation_workflow_test.cpp建立Qt Test target；复用docs/logic/evidence的合成图片/掩码，调用原Widget完成RectTool两次点击，断言模型数/保存像素/重开一致。先确认当前版本该回归失败；测试数据全部临时。 | 已实施/自动验证 | 2026-11-15 |
| TASK-002 | 修改src/deeplearning/widgets/AnnotationManagement.cpp的openImage、renderOverlay、updateShapeList、onDeleteSelectedShapes、onCanvasKeyPressed、onUndo/onRedo，统一读写AnnotationModel；加载后清模型Undo并markClean，移除可编辑legacy集合/旧栈，六工具保持同一模型。确认编译后无活动路径继续修改旧m_shapes。 | 已实施/自动验证 | 2026-11-15 |
| TASK-003 | 在AnnotationManagement.h/.cpp将saveAnnotations改为SaveResult，PNG/JSON采用可检查提交（QSaveFile或同目录临时提交），成功才markClean及通知Dataset::onAnnotationUpdated。失败显示路径/原因、保留dirty，onSave和自动保存仅成功写成功日志。 | 已实施/自动验证 | 2026-11-15 |
| TASK-004 | 修改openImage/onNavigateNext/onNavigatePrev/onNavigateTo/onReturnToGallery，接入保存/放弃/取消决策，保存失败不切图/离页；追加无形状、加载已有标签、连续编辑、失败重试、Undo保存重开回归。 | 已实施/自动验证 | 2026-11-15 |

### Implementation Phase 2

- **GOAL-002**：P0授权闭合。完成标志：AC-005通过；TASK-005先于006；修改标注处理器的TASK-006依赖Phase1，其他共享授权改动可独立验证。

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-005 | 在src/core/PermissionRegistry.h/.cpp的refreshPermissions增加携userId的变更通知；PermissionGuard.h/.cpp的watch/refresh订阅并只刷新活动用户，Session切换仍保留；UserManagementWidget::onPermissionChanged以同一通知刷新，避免递归更新矩阵。 | 已实施/自动验证 | 2026-11-15 |
| TASK-006 | 在ProjectManagement、ModelManagement、DatasetManagement、AnnotationManagement和SystemSettingsWidget写槽首行检查当前模块canWrite，并使各写控件按Guard更新。数据导出/转换、LabelMe启动、样本/类别修改、设置save/reset/clean也归写操作；FuseVision::onMenuItemClicked与DeepLearningWidget::applyPermissions加read入口校验。为直接槽调用添加拒绝且磁盘/DB不变断言。 | 已实施/自动验证 | 2026-11-15 |

### Implementation Phase 3

- **GOAL-003**：P1上下文和类别契约。完成标志：AC-003/004通过。TASK-007可独立于TASK-009；TASK-008依赖TASK-004与TASK-007，且必须在其后修改相同管理类。

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-007 | 缩小src/deeplearning/widgets/ModelManagement.cpp::onNewModel中旧QSignalBlocker作用域，只屏蔽卡片内部调整，解除后明确发currentModelChanged及modelCreated各一次；DeepLearningWidget接收器不重复绑定；实际信号计数测试成功各1、取消/失败各0。 | 已实施/自动验证 | 2026-11-15 |
| TASK-008 | ProjectManagement::onDeleteProject按显式选中卡片确定目标并在确认显示绝对路径；删除当前对象前由DeepLearningWidget协调TASK-004保存决策。成功发projectClosed；ModelManagement以currentModelChanged(empty)清理；编排器清Dataset当前模型/Annotation数据源。非当前删除不清当前上下文。测试A已开/B预览、取消/失败/成功及删除模型后写入口禁用。 | 已实施/自动验证 | 2026-11-15 |
| TASK-009 | DatasetManagement::onDeleteLabel与onLabelOrderChanged调用统一hasPersistedAnnotations递归检查labels的png/json/txt/xml；非空时恢复原显示顺序、提示原因且不save。添加两类别fixture哈希不变/空labels可变/末尾新增与重命名保持ID测试。 | 已实施/自动验证 | 2026-11-15 |

### Implementation Phase 4

- **GOAL-004**：P2表单与设置准确。完成标志：AC-006/007通过。TASK-010与011修改不同子系统，可并行；TASK-010应在TASK-007之后，TASK-011在TASK-006之后。

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-010 | ModelManagement::onNewModel读取descEdit并扩展ModelConfig::createModel写description；派生路径显示只读并移除Browse。load/saveModel保证已有字段与中文描述兼容；测试空/中文描述、原.fvdl重开和实际路径。 | 已实施/自动验证 | 2026-11-15 |
| TASK-011 | SystemSettingsWidget::onSave/onReset共用应用策略：Logger立刻重建，业务数据根/DB提示重启后生效；按钮与文案明确“重置路径”。SettingsManager保留下一启动持久化值与当前活动根区别；不移动数据、不改变已打开工程。测试临时设置下保存、重置、重启模拟与Logger目标。 | 部分完成 | 2026-11-15 |

### Implementation Phase 5

- **GOAL-005**：P2按证据提取职责并关闭回归。完成标志：AC-008及全部前述AC通过；依赖Phase1–4通过，TASK-012→013→014顺序。先测基准，再移动实现，不同时改格式。

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-012 | 为DatasetManagement::loadDatasetConfig、onImportImages/onImportFolder、onFormatConvert、writeCocoJson/writeYoloFormat/writeMaskFormat建立PNG/JSON/YOLO fixture语义基准，固定排序/类别/导出内容，不把耗时当作功能断言。 | 已实施/自动验证 | 2026-11-15 |
| TASK-013 | 新增src/deeplearning/storage/DatasetFileService.h/.cpp提取已存在枚举/复制/配对，Widget只收集对话框参数与显示结果；新增AnnotationStore.h/.cpp承接TASK-003保存实现。检查AnnotationIO逐图JSON/COCO差异后只复用契约相同部分，不盲替换。更新该模块CMakeLists。提取后跑TASK-012基准及全部状态/授权回归。 | 已实施/自动验证 | 2026-11-15 |
| TASK-014 | 在顶层CMakeLists注册独立回归target并使.github/workflows/build.yml执行现有CTest命令即可包含新测试；更新docs/logic中已修复项的证据状态、features JSON与导图，README将实际9标签/18权限和占位范围写准确。可见Windows人工走核心链并记录结果。 | 部分完成 | 2026-11-15 |

## 3. Alternatives

- **ALT-001**：保留两套标注状态并每次双向同步；拒绝，因为继续引入同步顺序和两套Undo故障面。
- **ALT-002**：立即全面引入稳定类别ID及数据迁移；本轮采用有磁盘标签时阻止删除/重排，减少未定义格式/迁移风险，后续另行规格。
- **ALT-003**：先拆整个Widget或实现训练服务；拒绝，因为没有行为基准且超出已证据问题范围。

## 4. Dependencies

- **DEP-001**：可构建的当前Qt/MSVC/vcpkg环境及现有业务库；不新增外部服务。
- **DEP-002**：Phase1保存决策供Phase3上下文关闭复用；Phase2授权在提取模块后仍必须覆盖服务入口。
- **DEP-003**：Q01任意账号登录、Q02/Q03任务语义、Q04额外产品约束暂不实施；本计划各任务不依赖这些决策。

## 5. Files

- **FILE-001**：src/deeplearning/widgets/AnnotationManagement.h/.cpp、AnnotationModel.h/.cpp、AnnotationRender及Tools，拟新增AnnotationStore.h/.cpp。
- **FILE-002**：src/deeplearning/widgets的ProjectManagement、ModelManagement、DeepLearningWidget，以及storage/ModelConfig对应.h/.cpp。
- **FILE-003**：src/deeplearning/widgets/DatasetManagement、storage/DatasetFileService、模块CMakeLists；当前路径按2026-11-17整理同步。
- **FILE-004**：src/core的PermissionRegistry、SettingsManager，src/ui/PermissionGuard，以及src/app的FuseVision、UserManagementWidget、SystemSettingsWidget。
- **FILE-005**：拟新增tests/workflow_regression.cpp、顶层CMakeLists、.github/workflows/build.yml、现有docs。

## 6. Testing

- **TEST-001**：标注已有/新建、Undo/Redo、删除最后形状、保存重开与失败重试；规格AC-001/002。
- **TEST-002**：read/write三组合、会话切换、权限动态修改、直接槽拒绝，失败前后文件/DB哈希不变；AC-005。
- **TEST-003**：新建通知计数、工程/模型删除生命周期、取消保存阻断、非当前删除；AC-003。
- **TEST-004**：两类别磁盘fixture的类别保护、空目录变更、编辑名称与新类别；AC-004。
- **TEST-005**：描述round trip、设置/重置生效协议与无数据移动；AC-006/007。
- **TEST-006**：提取前后语义基准、startup-smoke、Windows可见界面核心链；AC-008。LabelMe进程/硬件不作为该计划通过前提，授权入口需覆盖。

## 7. Risks & Assumptions

- **RISK-001**：替换legacy保存可能改变隐藏形状/灰度ID，必须先固定fixture再改；备份测试副本，禁止直接转换生产标签。
- **RISK-002**：移除模型旧栈可能影响快捷键/自动保存；六工具、列表、切图必须同时消费模型并回归。
- **RISK-003**：当前工作区已有多处用户修改，实施前记录基线并只编辑授权范围；遇同函数并行修改按上述依赖串行整合。
- **RISK-004**：Qt offscreen不能证明原生窗口嵌入/字体/DWM兼容，Windows可见窗口另做人工记录。
- **ASSUMPTION-001**：本轮保留工程/模型磁盘布局和三角色权限键，未经Q02/Q03不改任务语义。
- **ASSUMPTION-002**：有标注时拒绝类别删除/重排是临时保守规则，后续确需迁移时另写规格并回滚设计。

## 8. Related Specifications / Further Reading

[需求与验收规格](spec-process-workflow-consistency.md)；[证据问题清单](ISSUES.md)；[架构](../codebase/ARCHITECTURE.md)；[运行核心流程](../logic/CORE-FLOW.md)；[验证范围](../logic/VALIDATION.md)。


## 2026-11-15 实施进度

初始工作区与哈希：`docs/logic/evidence/workflow-start-baseline.json`。未reset、提交、推送或删除真实数据。

| 阶段 | 关键实施 | 验证与限制 |
|---|---|---|
| P0标注 | 唯一模型/撤销、保存快照、原子标签、失败保留与离开决策、工具像素合并 | dirty/矩形/选择/六工具/空标注/失败重试通过；原生关闭等补验 |
| P0权限 | AccessControl、读入口、执行处写校验、会话/权限刷新 | 直接拒绝处理器与磁盘/用户数不变、授权变化通过；LabelMe进程未验 |
| P1上下文类别 | 通知各一次、预览分离、选中删除与恢复目录、类别保护 | A/B删除、取消/失败/当前清理、类别变更后重开语义通过 |
| P2表单设置 | 描述、派生路径、即时/重启提示、QSettings失败重试、日志匹配 | 保存重读/无迁移/日志/取消重置通过；确认重置/完整重启补验 |
| P2文件职责 | AnnotationStore、DatasetFileService；COCO编号/配对、导出暂存 | 代表样本、稀疏ID/RLE/空标注/有损提示/失败无半成品通过；非压缩格式边界明确 |

构建与逐项断言集中于[验证记录](../logic/VALIDATION.md)。不因没有大数据性能证据引入线程。聚合COCO、配置和界面仍保留必要编排，未重写整个Widget。

## 2026-11-17 账户需求调整

用户已明确仅需一个账户：所有界面访客可查看，登录后统一可写。此前read/write组合、任意用户名入口和角色人数的实施要求由此调整：取消角色及权限矩阵、不再新增/删除用户，历史数据保留但停用授权；账户页仅改密。ISS-007按需求变化关闭，不宣称实现任意用户名登录。Q02/Q03和审计/备份问题继续保留。核心写处理器守卫、保存失败保留及退出登录前未保存决策不变。

本轮Release编译、CTest 2/2和15组业务回归通过，含四主页面/九子页浏览、访客直接写入拒绝、登录/退出和改密。实际退出登录的取消、保存失败、恢复重试及放弃均覆盖。实施与验证结果集中更新[验证记录](../logic/VALIDATION.md)，不新建重复规格。
