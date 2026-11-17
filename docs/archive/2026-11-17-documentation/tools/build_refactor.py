"""Evidence-based proposal. Does not execute the implementation plan."""
import sys
sys.dont_write_bytecode = True
from build_analysis import ROOT, LOGIC, FEATURES, EVIDENCE, source, links, put
import json, re

def doc(path,text):
    def sub(m):
        p,s=m.group(1).split('|',1)
        return links([source(p,s)])
    put(path,re.sub(r'\{code:([^}]+)\}',sub,text))

ISSUES=[
 dict(id='ISS-001',priority='P0',title='标注状态、渲染与撤销分裂',status='实际操作验证 + 静态代码确认',features=['F23','F24','F25','F26'],
 fact='导入已有掩码后，现代RectTool通过Transaction把模型形状增至2；保存后新矩形内(500,300)灰度0，重开模型只剩1。界面dirty=false而模型dirty=true。',
 cause='openImage将旧m_shapes复制进模型并使模型dirty，随后只清界面m_modified；新工具修改m_model，列表/保存/部分render和Undo仍读取m_shapes与旧栈。',
 impact='新增标注可能不落盘；列表、统计、撤销与画面不一致；切图/返回可能跳过保存。',
 scope='AnnotationManagement、AnnotationModel、AnnotationRender、六种Tools；labels和样本统计。',
 verify='既有掩码→新增矩形/笔画→Undo/Redo→保存重开，形状与像素一致；保存前dirty=true，成功后两处clean；切图不丢编辑。',
 evidence=[('src/DeepLearningWidget/AnnotationManagement.cpp','AnnotationManagement::openImage'),('src/DeepLearningWidget/AnnotationManagement.cpp','AnnotationManagement::saveAnnotations'),('src/DeepLearningWidget/AnnotationManagement.cpp','AnnotationManagement::onUndo'),('src/DeepLearningWidget/Tools/RectTool.cpp','RectTool::endRect')]),
 dict(id='ISS-002',priority='P0',title='保存结果不传播与离页缺少阻断',status='静态代码确认',features=['F24','F26'],
 fact='mask.save返回值不检查；JSON打开失败不提示，write结果不检查；仍更新数据集并清m_modified；onSave无条件记已保存。导航/返回无法得知失败。',
 cause='saveAnnotations返回void，持久化结果与界面/统计更新没有成功边界。',impact='磁盘不可写或短写时可能显示已保存且允许离开。此失败分支本次未模拟。',
 scope='AnnotationManagement::saveAnnotations/onSave/onNavigate*/onReturnToGallery；Dataset::onAnnotationUpdated。',
 verify='把labels路径替换为普通文件等确定性失败夹具；失败后dirty保留、不更新统计、不离页，原目标不损坏；可重试或显式放弃。',
 evidence=[('src/DeepLearningWidget/AnnotationManagement.cpp','AnnotationManagement::saveAnnotations'),('src/DeepLearningWidget/AnnotationManagement.cpp','AnnotationManagement::onReturnToGallery')]),
 dict(id='ISS-003',priority='P1',title='新建模型的通知被屏蔽',status='实际操作验证 + 静态代码确认',features=['F09','F11'],
 fact='隔离QSignalSpy记录modelCreated=0、currentModelChanged=0，而hasModel=true，selectedModel=SurfaceSeg。',cause='onNewModel中QSignalBlocker作用域直到函数末尾，包含onCardClicked与emit modelCreated。',
 impact='新建后页面看似选中，但数据集/状态栏订阅者未获通知；双击模型能继续流程。',scope='ModelManagement::onNewModel和DeepLearningWidget信号订阅。',verify='新建成功两种通知各一次；订阅者模型一致；创建取消或失败各为0。',
 evidence=[('src/DeepLearningWidget/ModelManagement.cpp','QSignalBlocker blk(this)'),('src/DeepLearningWidget/DeepLearningWidget.cpp','&ModelManagement::modelCreated')]),
 dict(id='ISS-004',priority='P1',title='项目/模型删除目标与上下文未闭合',status='静态代码确认',features=['F07','F08','F12'],
 fact='项目删除优先已打开m_project，而卡片可能预览另一个项目；删除清本页但未emit projectClosed。模型删除也未向数据集/标注集中通知上下文失效。',cause='预览、当前状态与子页副本分别持有，删除没有统一生命周期协议。',
 impact='可能删到非预览目标；子页继续展示/写入已删除路径。未执行删除复现，不将风险写成已发生。',scope='Project/ModelManagement、DeepLearningWidget、Dataset/AnnotationManagement。',verify='打开A预览B时删除明确显示并删除B；关闭/删除当前对象先处理未保存标注；成功后全部子页清空，非当前删除保留当前。',
 evidence=[('src/DeepLearningWidget/ProjectManagement.cpp','ProjectManagement::onDeleteProject'),('src/DeepLearningWidget/ModelManagement.cpp','ModelManagement::onDeleteModel'),('src/DeepLearningWidget/DeepLearningWidget.cpp','&ProjectManagement::projectClosed')]),
 dict(id='ISS-005',priority='P1',title='类别变更未保持磁盘ID语义',status='静态代码确认',features=['F15','F16'],
 fact='onDeleteLabel移除类别数组；onLabelOrderChanged只重映射内存samples.categoryIds；saveDatasetConfig按序重写id。未见对应PNG/JSON全量重编码。',cause='展示顺序和持久化类别ID绑定，变更没有数据迁移或阻止规则。',
 impact='多类别标注重开后可能被解释成不同类别；未执行破坏性类别变更。',scope='Dataset类别维护、config、Annotation::syncLabels及导出。',verify='两类别磁盘夹具下删除/重排被明确拒绝且文件哈希不变；只有名称编辑/新增仍可使用；空数据允许既有重排。',
 evidence=[('src/DeepLearningWidget/DatasetManagement.cpp','DatasetManagement::onDeleteLabel'),('src/DeepLearningWidget/DatasetManagement.cpp','DatasetManagement::onLabelOrderChanged'),('src/DeepLearningWidget/DatasetManagement.cpp','DatasetManagement::saveDatasetConfig')]),
 dict(id='ISS-006',priority='P0',title='模块读写授权覆盖不一致',status='实际操作验证 + 静态代码确认',features=['F02','F04','F05','F35','F36'],
 fact='普通用户项目canWrite=false，原新建按钮仍enabled=true。深度学习仅按canRead隐藏页；项目/模型/数据集/标注写处理器未见canWrite守卫，系统设置亦无Guard。',cause='Guard仅部分模块接入；Registry刷新缓存不发权限变更，Guard主要监听会话。',
 impact='用户可访问不符合权限矩阵的写入口；未以普通用户执行真实创建/删除。',scope='FuseVision导航、DeepLearning四管理页、SystemSettings、PermissionRegistry/Guard、UserManagement权限编辑。',verify='无read不能显示/进入；read-only全部写操作禁用且直接调用槽也拒绝；同会话改授权后即时刷新，拒绝时文件/DB/配置哈希不变。',
 evidence=[('src/DeepLearningWidget/DeepLearningWidget.cpp','DeepLearningWidget::applyPermissions'),('src/DeepLearningWidget/ProjectManagement.cpp','ProjectManagement::onNewProject'),('src/MainWindows/SystemSettingsWidget.cpp','SystemSettingsWidget::onSave'),('src/core/PermissionRegistry.cpp','PermissionRegistry::refreshPermissions')]),
 dict(id='ISS-007',priority='P1',title='新增用户名缺登录入口',status='静态代码确认 / 产品选择待Q01确认',features=['F01','F34'],
 fact='添加用户允许任意非空用户名；Login角色下拉固定映射user/admin/superadmin。',cause='账户标识与角色显示混合，无法提交新增账号名。',impact='成功添加的账号无法通过现登录页选中；此处并未更改登录模式。',scope='Login、UserManagement和Session用户显示。',verify='Q01确定账号输入方案后：内存新增alice→登录成功，角色以DB为准；三种原账号仍能登录。',
 evidence=[('src/MainWindows/Login.cpp','LoginDialog::doLogin'),('src/MainWindows/UserManagementWidget.cpp','UserManagementWidget::onAddUser')]),
 dict(id='ISS-008',priority='P2',title='模型表单字段未被创建流程消费',status='静态代码确认',features=['F09'],
 fact='新建模型有描述和路径字段；调用createModel未读取它们，.fvdl description硬编码空串。',cause='UI表单与Config函数契约不匹配。',impact='用户填写描述后丢失，选择位置产生误导。',scope='ModelManagement::onNewModel，ModelConfig::createModel/FvdlInfo。',verify='描述含中文且重开保持；路径若不支持自定义则作为只读派生结果展示且无可选Browse。',
 evidence=[('src/DeepLearningWidget/ModelManagement.cpp','ModelManagement::onNewModel'),('src/DeepLearningWidget/ModelConfig.cpp','ModelConfig::createModel')]),
 dict(id='ISS-009',priority='P2',title='设置保存/重置的生效范围不明确',status='静态代码确认',features=['F36','F37','F38'],
 fact='保存提示除数据库外立即生效，但项目页构造时缓存根目录。重置只写五种路径且未重建Logger，不重置级别/Python/主题。',cause='持久化设置、当前活动配置、页面路径副本缺少统一生效协议。',impact='用户无法判断何时使用新路径，重置显示路径可能与活动Logger不同。',scope='SystemSettings、SettingsManager、ProjectManagement与Logger。',verify='明确当前/重启生效：日志重建立即；所有业务根与DB重启生效；界面准确提示；重置遵循相同规则，不移动文件。',
 evidence=[('src/MainWindows/SystemSettingsWidget.cpp','SystemSettingsWidget::onSave'),('src/MainWindows/SystemSettingsWidget.cpp','SystemSettingsWidget::onReset'),('src/DeepLearningWidget/ProjectManagement.cpp','ProjectManagement::ProjectManagement')]),
 dict(id='ISS-010',priority='P2',title='数据/标注Widget承担重复持久化与格式职责',status='静态代码确认',features=['F13','F14','F20','F21','F24'],
 fact='DatasetManagement.cpp5239行、AnnotationManagement.cpp2586行；导入/统计/转换/序列化与建界面同类；AnnotationIO已有相似方法但未接入管理链。',cause='多次功能增量堆叠于Widget，新旧迁移未完成。',impact='错误传播和状态一致性难以独立验证；并非证明运行性能差。',scope='Dataset/Annotation的现有文件处理及格式代码，不含未实现训练/相机功能。',verify='提取前后相同fixture的样本顺序、类别、PNG像素、JSON/YAML语义一致；Widget只负责输入/展示，错误路径可独立验证。',
 evidence=[('src/DeepLearningWidget/DatasetManagement.cpp','DatasetManagement::onImportImages'),('src/DeepLearningWidget/DatasetManagement.cpp','DatasetManagement::writeCocoJson'),('src/DeepLearningWidget/AnnotationIO.cpp','AnnotationIO::saveCOCO')])
]

md=['# 改进问题清单（未修改业务代码）','', 'P0=数据保存/授权优先，P1=流程和上下文一致性，P2=表单契约/职责整理。优先级为规划建议；实际发生范围按证据状态限定。', '', '| 问题 | 优先级 | 发现 | 证据状态 |','|---|---|---|---|']
md += [f"| [{i['id']}](#{i['id'].lower()}) | {i['priority']} | {i['title']} | {i['status']} |" for i in ISSUES]
for i in ISSUES:
    i['sources']=[source(*e) for e in i.pop('evidence')]
    md += ['',f'<a id="{i["id"].lower()}"></a>',f"## {i['id']} {i['title']}",'',f"**状态/优先级**：{i['status']} / {i['priority']}。对应{'、'.join(i['features'])}。",'',f"**事实**：{i['fact']}",'',f"**机制**：{i['cause']}",'',f"**影响**：{i['impact']}",'',f"**影响范围**：{i['scope']}",'',f"**后续验证**：{i['verify']}",'',f"**代码证据**：{links(i['sources'])}"]
md += ['', '## 记录但暂不纳入必须实施', '', '- Q01–Q04涉及账号模式与任务语义，保留产品决策，不擅自扩展功能。','- 记住密码明文、无盐SHA256、默认凭据为确切现状；凭据迁移需独立兼容方案，本次未承诺实现。','- 日志命名与清理过滤器、数据库FK启用、LabelMe脚本路径兼容性仅有静态疑点，待运行验证；不列为已复现问题。','- 数据检查等五独立页、传统视觉为占位。没有据此要求开发训练/相机/通讯服务。', '', '[规格](spec-process-workflow-consistency.md) · [实施计划](refactor-workflow-consistency-1.md) · [运行观察](../logic/evidence/runtime-ui.json)']
put('docs/refactor/ISSUES.md','\n'.join(md))
put('docs/refactor/issues.json',json.dumps(ISSUES,ensure_ascii=False,indent=2))

doc('docs/refactor/spec-process-workflow-consistency.md',r'''
---
title: FuseVision 核心工作流一致性改进规格
version: '1.0'
date_created: '2026-11-14'
last_updated: '2026-11-14'
owner: FuseVision 项目维护者
tags: [process, refactor, specification]
---

# Introduction

本规格是规划建议，针对当前工程→模型→数据→标注链路的证据问题。分析发现矩形进入内存但未保存、新建模型通知为0、read-only仍有项目写入口；其他问题按静态证据描述。本次仅编写规格，未实施任何需求。

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
- **CON-002**：所有测试使用临时目录、内存DB和临时设置，不读写生产项目；本轮分析产物不能被当作修复已完成。
- **GUD-001**：ISS-007的账号输入模式仅是候选：Q01确认后可将账号标识与角色分离；确认前不实施登录UI变更，也不封禁已有新增账号数据。

## 4. Interfaces & Data Contracts

以下是建议内部接口，不是已存在实现：

```cpp
struct SaveResult { bool ok; QString path; QString error; };
SaveResult saveCurrentAnnotation();
// Guard: 每次写操作入口使用当前会话与module键查询，不缓存永久allow。
// permissionChanged(userId) -> 当前用户的PermissionGuard::refresh()
// projectClosed/modelClosed -> 编排器清dataset与annotation上下文
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

单元级验证AnnotationModel编辑/dirty/Undo及类别保护；Qt Test集成验证真实Widget信号/权限/切图；文件级验证PNG像素与JSON语义重开；CTest继续运行startup-smoke。fixture每例QTemporaryDir，SQLite内存库，显式临时QSettings，包含已有掩码+新矩形、两类别与无标注、损坏JSON、写失败路径。不得只断言“文件存在”。

后续将独立回归target接CTest并进入现有Windows CI。门禁以AC-001至AC-008完整通过和既有smoke通过为准，不虚设当前不存在的覆盖率百分比。尚未有大数据性能基准，不强制线程化或承诺速度提升。

## 7. Rationale & Context

运行证据已区分“创建成功”和“通知成功”、“文件可读”和“新增标注持久化”。因此先修复数据一致性与授权，再提取职责。类别暂采用禁止有标签时重排/删除的保守方案，减少未明确迁移规范导致的数据重写风险。新账号登录、任务语义和占位页没有被顺带扩展。

证据：{code:src/DeepLearningWidget/AnnotationManagement.cpp|AnnotationManagement::saveAnnotations}；{code:src/DeepLearningWidget/ModelManagement.cpp|QSignalBlocker blk(this)}；{code:src/DeepLearningWidget/DatasetManagement.cpp|DatasetManagement::onLabelOrderChanged}；[运行观察](../logic/evidence/runtime-ui.json)。

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
''')

doc('docs/refactor/refactor-workflow-consistency-1.md',r'''
---
goal: 修复FuseVision现有核心流程的一致性并提取必要职责
version: '1.0'
date_created: '2026-11-14'
last_updated: '2026-11-14'
owner: FuseVision 项目维护者
status: 'Planned'
tags: [refactor, workflow, data-integrity, authorization]
---

# Introduction

![Status: Planned](https://img.shields.io/badge/status-Planned-blue)

当前仅完成分析和交付文档；下列任务均未执行。先保护标注数据及授权，再修复上下文、类别/表单/设置，最后提取现有文件职责。每阶段独立交付可运行版本；依赖明确列出，禁止并行修改同一函数。Q01账号选择不纳入本轮可执行任务。

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
| TASK-001 | 在拟新增tests/annotation_workflow_test.cpp建立Qt Test target；复用docs/logic/evidence的合成图片/掩码，调用原Widget完成RectTool两次点击，断言模型数/保存像素/重开一致。先确认当前版本该回归失败；测试数据全部临时。 | 否 | — |
| TASK-002 | 修改src/DeepLearningWidget/AnnotationManagement.cpp的openImage(142)、renderOverlay(2171)、updateShapeList(1559)、onDeleteSelectedShapes(1630)、onCanvasKeyPressed(1370)、onUndo/onRedo(2381/2386)，统一读写AnnotationModel；加载后清模型Undo并markClean，移除可编辑legacy集合/旧栈，六工具保持同一模型。确认编译后无活动路径继续修改旧m_shapes。 | 否 | — |
| TASK-003 | 在AnnotationManagement.h/.cpp将saveAnnotations(2195)改为SaveResult，PNG/JSON采用可检查提交（QSaveFile或同目录临时提交），成功才markClean及通知Dataset::onAnnotationUpdated(219)。失败显示路径/原因、保留dirty，onSave和自动保存仅成功写成功日志。 | 否 | — |
| TASK-004 | 修改openImage/onNavigateNext/onNavigatePrev/onNavigateTo/onReturnToGallery(142/861/869/876/884)，接入保存/放弃/取消决策，保存失败不切图/离页；追加无形状、加载已有标签、连续编辑、失败重试、Undo保存重开回归。 | 否 | — |

### Implementation Phase 2

- **GOAL-002**：P0授权闭合。完成标志：AC-005通过；TASK-005先于006；修改标注处理器的TASK-006依赖Phase1，其他共享授权改动可独立验证。

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-005 | 在src/core/PermissionRegistry.h/.cpp的refreshPermissions(87)增加携userId的变更通知；PermissionGuard.h/.cpp的watch/refresh订阅并只刷新活动用户，Session切换仍保留；UserManagementWidget::onPermissionChanged(419)以同一通知刷新，避免递归更新矩阵。 | 否 | — |
| TASK-006 | 在ProjectManagement、ModelManagement、DatasetManagement、AnnotationManagement和SystemSettingsWidget写槽首行检查当前模块canWrite，并使各写控件按Guard更新。数据导出/转换、LabelMe启动、样本/类别修改、设置save/reset/clean也归写操作；FuseVision::onMenuItemClicked(319)与DeepLearningWidget::applyPermissions(628)加read入口校验。为直接槽调用添加拒绝且磁盘/DB不变断言。 | 否 | — |

### Implementation Phase 3

- **GOAL-003**：P1上下文和类别契约。完成标志：AC-003/004通过。TASK-007可独立于TASK-009；TASK-008依赖TASK-004与TASK-007，且必须在其后修改相同管理类。

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-007 | 缩小src/DeepLearningWidget/ModelManagement.cpp::onNewModel中QSignalBlocker(662)作用域，只屏蔽卡片内部调整，解除后明确发currentModelChanged及modelCreated各一次；DeepLearningWidget接收器不重复绑定；QSignalSpy测试成功各1、取消/失败各0。 | 否 | — |
| TASK-008 | ProjectManagement::onDeleteProject(767)按显式选中卡片确定目标并在确认显示绝对路径；删除当前对象前由DeepLearningWidget协调TASK-004保存决策。成功发projectClosed；ModelManagement增加modelClosed；编排器清Dataset当前模型/Annotation数据源。非当前删除不清当前上下文。测试A已开/B预览、取消/失败/成功及删除模型后写入口禁用。 | 否 | — |
| TASK-009 | DatasetManagement::onDeleteLabel(2991)与onLabelOrderChanged(3062)调用统一hasPersistedAnnotations递归检查labels的png/json/txt/xml；非空时恢复原显示顺序、提示原因且不save。添加两类别fixture哈希不变/空labels可变/末尾新增与重命名保持ID测试。 | 否 | — |

### Implementation Phase 4

- **GOAL-004**：P2表单与设置准确。完成标志：AC-006/007通过。TASK-010与011修改不同子系统，可并行；TASK-010应在TASK-007之后，TASK-011在TASK-006之后。

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-010 | ModelManagement::onNewModel(454)读取descEdit并扩展ModelConfig::createModel(18)写description；派生路径显示只读并移除Browse。load/saveModel保证已有字段与中文描述兼容；测试空/中文描述、原.fvdl重开和实际路径。 | 否 | — |
| TASK-011 | SystemSettingsWidget::onSave/onReset(259/281)共用应用策略：Logger立刻重建，业务数据根/DB提示重启后生效；按钮与文案明确“重置路径”。SettingsManager保留下一启动持久化值与当前活动根区别；不移动数据、不改变已打开工程。测试临时设置下保存、重置、重启模拟与Logger目标。 | 否 | — |

### Implementation Phase 5

- **GOAL-005**：P2按证据提取职责并关闭回归。完成标志：AC-008及全部前述AC通过；依赖Phase1–4通过，TASK-012→013→014顺序。先测基准，再移动实现，不同时改格式。

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-012 | 为DatasetManagement::loadDatasetConfig(274)、onImportImages/onImportFolder(2283/2430)、onFormatConvert(3188)、writeCocoJson/writeYoloFormat/writeMaskFormat(4483/4706/4902)建立PNG/JSON/YOLO fixture语义基准，固定排序/类别/导出内容，不把耗时当作功能断言。 | 否 | — |
| TASK-013 | 新增src/DeepLearningWidget/DatasetFileService.h/.cpp提取已存在枚举/复制/配对，Widget只收集对话框参数与显示结果；新增AnnotationStore.h/.cpp承接TASK-003保存实现。检查AnnotationIO逐图JSON/COCO差异后只复用契约相同部分，不盲替换。更新该模块CMakeLists。提取后跑TASK-012基准及全部状态/授权回归。 | 否 | — |
| TASK-014 | 在顶层CMakeLists注册独立回归target并使.github/workflows/build.yml执行现有CTest命令即可包含新测试；更新docs/logic中已修复项的证据状态、features JSON与导图，README将实际9标签/18权限和占位范围写准确。可见Windows人工走核心链并记录结果。 | 否 | — |

## 3. Alternatives

- **ALT-001**：保留两套标注状态并每次双向同步；拒绝，因为继续引入同步顺序和两套Undo故障面。
- **ALT-002**：立即全面引入稳定类别ID及数据迁移；本轮采用有磁盘标签时阻止删除/重排，减少未定义格式/迁移风险，后续另行规格。
- **ALT-003**：先拆整个Widget或实现训练服务；拒绝，因为没有行为基准且超出已证据问题范围。

## 4. Dependencies

- **DEP-001**：可构建的当前Qt/MSVC/vcpkg环境及现有业务库；不新增外部服务。
- **DEP-002**：Phase1保存决策供Phase3上下文关闭复用；Phase2授权在提取模块后仍必须覆盖服务入口。
- **DEP-003**：Q01任意账号登录、Q02/Q03任务语义、Q04额外产品约束暂不实施；本计划各任务不依赖这些决策。

## 5. Files

- **FILE-001**：src/DeepLearningWidget/AnnotationManagement.h/.cpp、AnnotationModel.h/.cpp、AnnotationRender及Tools，拟新增AnnotationStore.h/.cpp。
- **FILE-002**：src/DeepLearningWidget/ProjectManagement、ModelManagement、ModelConfig、DeepLearningWidget及对应.h/.cpp。
- **FILE-003**：src/DeepLearningWidget/DatasetManagement、拟新增DatasetFileService、模块CMakeLists。
- **FILE-004**：src/core/PermissionRegistry、PermissionGuard、SettingsManager及src/MainWindows/FuseVision、UserManagementWidget、SystemSettingsWidget。
- **FILE-005**：拟新增tests/annotation_workflow_test.cpp和相关数据/授权回归文件、顶层CMakeLists、.github/workflows/build.yml、README及docs。

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
''')

put('docs/logic/evidence/refactor-source-references.json',json.dumps(EVIDENCE,ensure_ascii=False,indent=2))
print(f'Wrote {len(ISSUES)} issues, 11-section spec and 5-phase/14-task Planned implementation plan')
