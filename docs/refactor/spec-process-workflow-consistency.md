---
title: FuseVision 核心工作流一致性改进规格
version: '1.1'
date_created: '2026-11-14'
last_updated: '2026-11-15'
owner: FuseVision 项目维护者
tags: [process, refactor, specification]
---

# Introduction

本规格已用于2026-11-15实施，需求编号保留。实现和自动验证见[验证记录](../logic/VALIDATION.md)，未执行的可见桌面/重置重启验收仍保留。历史缺陷复现已与当前状态分开，ISS-007仍待产品决策。

## 1. Purpose & Scope

读者为项目维护者、后续实施人员和验收人员。目标是保留现有功能与数据语义，消除已证实状态/授权/保存缺口。范围含项目、模型、数据集、标注、系统设置和相关共享能力；不实现训练/预测/相机等占位模块，不新增远程服务或更换技术栈。Q01账号模式未定，其对应建议暂缓，其他需求不依赖此决策。

## 2. Definitions

| 词 | 定义 |
|---|---|
| 活动上下文 | 当前正式打开的project与model身份及其有效路径，区别于卡片预览 |
| 标注单一状态源 | AnnotationModel持有图形、dirty、Undo/Redo；视图和序列化都读取它 |
| SaveResult | ok、path、error的保存结果；失败不清dirty、不提交统计 |
| 持久化类别ID | PNG像素/JSON category_id承载的类别含义，不得随显示顺序静默改变 |
| 授权守卫 | 处理器执行前查询当前userId/module的canRead/canWrite |
| round trip | 保存后重新加载并比较形状语义和目标文件内容 |

## 3. Requirements, Constraints & Guidelines

- **REQ-001**：对应ISS-001。AnnotationModel为唯一可编辑形状集合；六工具、列表、渲染、复制/删除、保存及Undo/Redo均访问该集合。加载成功markClean并清导入产生的撤销历史；修改后dirty=true；保存成功后markClean；禁止额外m_modified独立漂移。
- **REQ-002**：对应ISS-002。保存返回SaveResult，PNG与JSON失败均向用户显示错误并保留dirty。仅成功后更新Dataset统计。切图/返回/上下文关闭遇到保存失败必须停留；用户可重试或显式确认放弃，不得隐式继续。
- **REQ-003**：对应ISS-003/004。模型新建成功时currentModelChanged与modelCreated各一次。项目/模型关闭和删除经过统一上下文变更入口；当前对象失效后清空下游dataset/annotation。删除目标为明确选中对象，确认框显示全路径；无选择不删除。
- **REQ-004**：对应ISS-005。保守保护现有文件契约：labels目录存在任何有效标注文件时，拒绝删除/重排类别并给出原因；名称编辑及末尾新增可继续。空标注目录允许既有重排。迁移/稳定ID重设计不在此轮范围，禁止只改config而不改标签。
- **REQ-005**：对应ISS-006。所有已实现模块的入口按read控制可访问，写控件按write启用；每个写处理器再次守卫。对外部LabelMe/转换/导出等会写文件操作也要求write。授权改变必须广播受影响userId，活动Guard即时刷新。
- **REQ-006**：对应ISS-008。模型描述必须写入.fvdl且重开保持；路径继续派生为<project>/<model>/<model>.fvdl并只读展示，移除误导的自定义浏览，不引入新模型布局。
- **REQ-007**：对应ISS-009。保存与重置均明确生效协议：日志路径/级别立即重建Logger，数据库及项目/传统/相机数据根下次启动生效；当前业务页保留原有效根，不移动数据。将“重置”准确标为重置路径；不暗示重置级别/Python/主题。
- **REQ-008**：对应ISS-010。在前述行为稳定后，提取既有数据枚举/导入文件处理和标注保存为可独立验证的模块，保留Widget交互与原格式语义。不得未经round trip证据就以AnnotationIO替换当前自定义JSON。
- **CON-001**：保持.fvproj/.fvdl/现有PNG/逐图JSON读兼容；禁止把当前异常检测或矩形task映射按推测互换。
- **CON-002**：所有测试使用临时目录、内存DB和临时设置，不读写生产项目；历史分析产物不能被当作当前修复的验证。
- **GUD-001**：ISS-007的账号输入模式仅是候选：Q01确认后可将账号标识与角色分离；确认前不实施登录UI变更，也不封禁已有新增账号数据。

## 4. Interfaces & Data Contracts

以下是规格接口示意；实际实现为 AnnotationStore::save 返回 AnnotationSaveResult，AnnotationManagement::saveAnnotations/prepareToLeave 返回 bool：

```cpp
struct SaveResult { bool ok; QString path; QString error; };
SaveResult saveCurrentAnnotation();
// Guard: 每次写操作入口使用当前会话与module键查询，不缓存永久allow。
// permissionChanged(userId) -> 当前用户的PermissionGuard::refresh()
// projectClosed/currentModelChanged(empty) -> 编排器清dataset与annotation上下文
```

SaveResult成功要求文件完全写出并提交；JSON可用QSaveFile，PNG先写同目录临时文件或写入QSaveFile设备后commit。错误文本含目标路径和失败原因，不输出密码。Dataset::onAnnotationUpdated只接收已提交结果。项目/模型关闭前调用标注的保存决策入口，失败/取消不得删除当前对象或清上下文。

类别保护按磁盘有无*.png/*.json/*.txt/*.xml等现存标签文件判断（递归包含分区，排除目录及临时文件）；宁可阻止不明确变更，不自动重编码。名称编辑只改名称，不改变ID。持久化字段以[数据契约](../logic/DATA-CONTRACTS.md)为基准。

## 5. Acceptance Criteria

- **AC-001**：Given已有1个掩码形状，When完成新矩形、Undo/Redo、保存重开，Then两形状均恢复，(500,300)等新矩形内部非背景像素保持，列表/图像/统计一致；对应REQ-001。
- **AC-002**：Given不可写目标（以labels同名普通文件夹具制造，避免管理员权限差异），When保存/切图/返回，Then显示失败、dirty保持、统计不变、仍在当前图；显式放弃才允许离开；对应REQ-002。
- **AC-003**：Given工程A已开且预览B，When确认删除B，Then只删除B；删除当前工程或模型时，下游全部清空且禁用写入口；新建模型通知各一次；对应REQ-003。
- **AC-004**：Given两类别已有磁盘标签，When删除类别或拖动顺序，Then被拒绝，PNG/JSON/config哈希不变且重开语义不变；空labels允许重排；对应REQ-004。
- **AC-005**：Given canRead=false或canWrite=false，When导航/按钮/直接槽调用，Then分别拒绝阅读或写入；不改变文件/配置/DB。活动用户权限修改后无需重登即可生效；对应REQ-005。
- **AC-006**：Given新建模型填中文描述，When保存并重开，Then描述不丢、展示派生路径与实际文件一致；对应REQ-006。
- **AC-007**：Given保存新路径或重置路径，When观察当前运行与下一次启动，Then日志即时切换、业务根/DB下次切换，提示准确且不迁移原文件；对应REQ-007。
- **AC-008**：GivenPNG、逐图JSON、COCO/YOLO导出基准fixture，When提取模块前后各执行，Then样本排序/类别、PNG像素、JSON/YAML结构语义保持一致（时间戳字段除外）；对应REQ-008。

## 6. Test Automation Strategy

单元级验证AnnotationModel编辑/dirty/Undo及类别保护；QApplication/Widget集成验证真实Widget信号/权限/切图；文件级验证PNG像素与JSON语义重开；CTest继续运行startup-smoke。fixture每例QTemporaryDir，SQLite内存库，显式临时QSettings，包含已有掩码+新矩形、两类别与无标注、损坏JSON、写失败路径。不得只断言“文件存在”。

后续将独立回归target接CTest并进入现有Windows CI。门禁以AC-001至AC-008完整通过和既有smoke通过为准，不虚设当前不存在的覆盖率百分比。尚未有大数据性能基准，不强制线程化或承诺速度提升。

## 7. Rationale & Context

运行证据已区分“创建成功”和“通知成功”、“文件可读”和“新增标注持久化”。因此先修复数据一致性与授权，再提取职责。类别暂采用禁止有标签时重排/删除的保守方案，减少未明确迁移规范导致的数据重写风险。新账号登录、任务语义和占位页没有被顺带扩展。

证据：[src/deeplearning/widgets/AnnotationManagement.cpp AnnotationManagement::saveAnnotations](../../src/deeplearning/widgets/AnnotationManagement.cpp:1413)；[src/deeplearning/widgets/ModelManagement.cpp QSignalBlocker blk(this)](../../src/deeplearning/widgets/ModelManagement.cpp)；[src/deeplearning/widgets/DatasetManagement.cpp DatasetManagement::onLabelOrderChanged](../../src/deeplearning/widgets/DatasetManagement.cpp:2752)；[运行观察](../logic/evidence/runtime-ui.json)。

## 8. Dependencies & External Integrations

- **EXT-001**：本地文件系统，支持同目录提交与可解释的写失败；无新云服务。
- **INF-001**：Qt事件循环、图像编解码、Sql及测试工具；Windows CI沿用现有流水线。
- **DAT-001**：现有工程/模型/数据集格式与合成回归fixture，保持向后读取。
- **PLT-001**：现有C++/Qt桌面架构；LabelMe只验证授权入口，不要求本轮测试安装外部环境。
- **DEP-001**：Q01–Q04仅限制账号模式/任务语义等未入必做需求，不阻塞本规格REQ-001至REQ-008。

## 9. Examples & Edge Cases

示例：用户编辑已有标签后断开写入路径。保存返回`{ok:false,path:".../labels/a.png",error:"..."}`，保留模型dirty；点击下一张仍停留并提示重试/放弃/取消。用户取消后原上下文不变；显式放弃后只丢弃当前未保存编辑，不删除原标签。

零形状保存必须清除原有形状语义（生成空掩码/空annotations），不能因集合空直接跳过。隐藏形状是否导出维持当前已定义规则并用fixture固定。

## 10. Validation Criteria

需求/AC/任务映射完整；八项AC及startup-smoke通过；所有失败路径能解释且不损坏原文件；无生产项目改动；现有四任务映射保持。Windows可见窗口回归与XMind客户端检查是独立人工项，不能由offscreen/ZIP结构检查替代。

## 11. Related Specifications / Further Reading

[问题与证据](ISSUES.md)；[实施计划](refactor-workflow-consistency-1.md)；[架构](../codebase/ARCHITECTURE.md)；[核心流程](../logic/CORE-FLOW.md)；[功能清单](../logic/FEATURES.md)。

## 2026-11-15 实现补充

PNG原生形状元数据与JSON摘要绑定见数据契约；外部像素修改优先当前像素。删除改为恢复目录移动；样本配对含restore.json，不自动清空。项目按明确选择路径就地打开，取消原自动迁移流程。导出要求空目录并整批暂存提交；COCO导出补带图像并修正类别引用。掩码→YOLO等不可表达的内容报告有损限制，不承诺无损。

AC-001/002/003/004/006有自动行为断言；AC-005主要写入口和动态刷新有断言，外部LabelMe及顶层导航需人工；AC-007保存/重读/失败重试有断言，确认重置及完整重启需人工；AC-008代表样本基准通过，完整可见Windows主链未执行。因此不标记“全部产品验收通过”。

## 2026-11-17 账户需求调整

用户已明确仅需一个账户：所有界面访客可查看，登录后统一可写。此前read/write组合、任意用户名入口和角色人数的实施要求由此调整：取消角色及权限矩阵、不再新增/删除用户，历史数据保留但停用授权；账户页仅改密。ISS-007按需求变化关闭，不宣称实现任意用户名登录。Q02/Q03和审计/备份问题继续保留。核心写处理器守卫、保存失败保留及退出登录前未保存决策不变。

实施与验证结果集中更新[验证记录](../logic/VALIDATION.md)，不新建重复规格。
