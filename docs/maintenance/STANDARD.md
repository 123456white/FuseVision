# FuseVision 文档维护规范

状态：生效。版本：2.0。最近核对：2026-11-17；范围：目录分工、单一维护入口和项目内自更新机制。

[文档中心](../README.md) · [维护操作](README.md) · [项目规则](../../AGENTS.md) · [迁移与验收](MIGRATION.md)

## 1. 适用范围与原则

本规范仅适用于FuseVision项目。中文Markdown维护业务说明；代码、配置、测试和真实运行结果用于核实实现。用户明确需求优先，文档维护不构成额外修改业务、发布、清理已有工作或修改全局Codex配置的授权。

同一规则只在一个专题完整说明，其他专题链接过去；不复制多份账号规则、文件字段、命令或验证结论。按真实影响增量更新，不因每次工具调用或纯格式修改机械刷新整套资料和日期。

## 2. 目录职责

| 位置 | 主要职责 |
|---|---|
| docs/README.md | 唯一阅读入口、当前范围、专题导航 |
| docs/GETTING-STARTED.md | 环境、构建、启动、账号、测试与生成入口 |
| docs/maintenance/ | 本规范、维护说明、配置、迁移记录、自动目录与生成/归档清单 |
| docs/logic/ | 功能清单、跨功能关系、状态、数据契约、算法、测试追踪、截图和导图 |
| docs/codebase/ | 当前架构、源码结构、技术栈、惯例、外部集成、测试策略与风险 |
| docs/refactor/ | 问题状态、功能规格、实施计划、重要取舍和完成记录 |
| docs/requirements/ | 原始历史需求；不得直接当作现行实现或未经确认的新需求 |
| docs/archive/ | 迁移前快照和旧任务脚本；原字节保护，不执行、不恢复旧快照 |
| docs/logic/evidence、screenshots | 稳定路径上的原始证据和媒体；不由自更新重写 |
| docs/Doxyfile、docs/doxygen/ | API索引配置及分组；接口说明仍维护在源码Doxygen注释 |
| scripts/docs/ | 当前维护、校验、导出与Hooks适配程序，测试位于tests/test_docflow.py |
| out/docs/、out/docs-tools/ | 阅读产物、会话状态与本机工具依赖，不纳入项目源文件 |

第三方SDK/依赖自带文档不参与归档。保留已被代码注释、历史报告引用的业务文档稳定路径；迁移工具不意味着其旧相对路径仍可执行。

## 3. 维护源与生成关系

| 内容 | 人工维护源 | 工具生成/保护 |
|---|---|---|
| 功能说明 | logic/FEATURES.md的完整字段、步骤与代码引用 | features.json、XMind功能节点 |
| 问题当前状态 | refactor/ISSUES.md | issues.json当前字段与导图问题节点；历史字段从归档读取 |
| 主流程/架构/关系/状态/算法 | 对应MD；FEATURE-RELATIONS的起点/终点/关联线表 | 导图说明节点、总览关联线、业务HTML |
| 导图布局与图片选择 | maintenance/docflow.json | 导图JSON与.xmind；不在配置重复维护业务行为 |
| 文档导航 | 当前MD文件集合与各自标题 | maintenance/CATALOG.md及HTML导航/搜索 |
| API | 源码注释、Doxyfile、groups.dox | Doxygen HTML/XML；不属于MD业务说明生成器 |
| 原始结果 | 真实执行产生的日志、JSON和截图 | 只读取、链接与摘要保护；不“自动修正”成通过 |

具体生成入口为`python -X utf8 scripts/docs/docflow.py sync --html`。JSON和.xmind现在是派生产物，不再手工双向修改；若误改生成物，check会失败，重新从源生成。button-index.json仍是特定历史截图的坐标证据，不从新界面猜测坐标。

FEATURES.md采用标识明确的字段：位置、入口、场景、前提、输入与校验、证据状态、按钮编号、操作流程、结果、失败/取消/边界、数据/接口/算法、验证范围、代码。修改或新增功能保持此格式；字段缺失时生成失败，不从旧JSON补造当前事实。

ISSUES.md每条保留状态、当前实现/限制、验证、未完成或边界、当前代码；优先级来自问题总表。兼容保留的旧impact/cause/scope等字段属于原问题描述，不能据此推断当前故障。

## 4. 编号、链接与证据

保留F功能、P页面、B按钮和ISS问题编号；退役保留编号及去向，不复用。关系用REL、迁移用ST、追踪用TC编号；测试函数不必为编号重命名。不同规格中的REQ/TASK编号引用时带文件名。

文档内用仓库相对链接；交付文件链接用绝对路径。代码证据记录路径、类/函数和必要行号，目录/符号变动后更新当前引用；历史证据保留当时信息。check校验路径、锚点、行号范围和功能/问题代码符号与引用行是否一致，不证明每条调用链语义正确。

新增或实质更新的专题写明：最近核对日期、核对范围、代码依据、验证依据。只核对局部不声称全篇重新验证；未提交工作区不得冒充某个commit的已验结果。

实现状态分计划、占位、已实现、已退役；验证另分静态、自动、人工、历史及未验证。用例存在不是已运行，历史通过不等于当前代码通过，offscreen不等于可见Windows操作，Doxygen零警告不代表所有成员注释完整。

## 5. 功能关系与测试闭环

FEATURES.md回答“用户如何操作”，CORE-FLOW.md回答“主链路如何走”，FEATURE-RELATIONS.md回答“谁依赖谁、失败影响谁”，STATE-MACHINES.md回答“什么条件下允许迁移”，DATA-CONTRACTS.md定义存储/编号/坐标等规则，ALGORITHMS.md解释处理假设与限制，TEST-TRACEABILITY.md关联功能/关系/迁移、真实测试、业务断言和执行证据。

关键测试应断言保存重开一致、失败保留编辑和旧文件、取消保持上下文、访客写入前后文件不变、格式编号/几何符合契约。不要只验证按钮可点击或文件存在，不按用例/截图数量宣称全产品覆盖率。未知与未验单列，不穷举无意义组合。

## 6. 变更与更新映射

| 变更 | 优先核对 |
|---|---|
| 页面、按钮、快捷键、入口/可用条件 | FEATURES、CORE-FLOW；外观变化时补真实截图 |
| 编辑、保存、取消、切换与异常恢复 | CORE-FLOW、STATE-MACHINES、FEATURE-RELATIONS、TEST-TRACEABILITY |
| 文件格式、类别、坐标、存储与算法 | DATA-CONTRACTS、ALGORITHMS、相关功能与追踪 |
| 登录、账户、读写与配置生效 | FEATURES、DATA-CONTRACTS；必要时启动指南/集成说明 |
| 模块移动、依赖边界 | ARCHITECTURE、STRUCTURE、CONVENTIONS与当前符号引用 |
| 构建、依赖、测试命令 | GETTING-STARTED、STACK、TESTING对应章节 |
| 缺陷修复与进度 | ISSUES、对应规格/计划、回归和新执行证据 |
| 测试新增/变更 | TESTING、TEST-TRACEABILITY；无行为变化不改功能结论 |
| 文档工具与Hooks | maintenance/README、STANDARD、MIGRATION和工具回归 |
| 纯文案/格式/内部实现 | 判断是否有说明影响；无影响记录no-impact及具体理由 |

机器候选映射在maintenance/docflow.json中，按路径提示影响面；它不会理解所有语义。AI必须结合diff判断，不能把映射命中当作所有文档必须修改，或未命中当作一定无影响。

## 7. 每个会话的工作流程

1. 读取AGENTS.md、文档中心和本规范；Hooks已启用时SessionStart建立内容基线，手动模式在修改前执行begin。
2. 保护已有脏改动。内容基线追踪会话后的新增/修改/删除，不把整个git diff HEAD归入本次任务；resume/compact不重置未完成基线。
3. 完成一组业务变更后，AI按diff更新对应MD；脚本不自行改写业务规则、产品决策或验收结论。
4. 运行sync --html、check --diagrams；有业务变更另跑相应业务测试，并保存新的独立结果。
5. 用review记录updated、no-impact或deferred及理由。updated必须有实际MD改动，deferred是明确遗留而不是同步完成。后续源文件变化使旧review失效。
6. Stop或手动finish生成/检查后校验review，成功才推进会话基线。未完成时最多自动要求继续一次，再有问题需明确交付限制，避免无限循环。

维护命令、Hooks信任步骤、无Hooks的降级方式见README。规则及自动化仅作用于本仓库，不写用户全局配置、不自动提交或发布。

## 8. 原始证据与自动化边界

迁移前快照、旧工具、原始需求/日志/截图按archive-manifest.json记录摘要。新验证使用独立任务名/日期/运行ID，不能覆盖旧证据，不能事后重写原始测量或补造基线。所有验证使用隔离数据，不保存真实密码、令牌或生产隐私。

当前check是只读校验；旧validate_analysis.py已归档，它绑定旧任务且会写历史报告，不能充当当前入口。当前sync只写固定派生产物和out；HTML构建不改变MD，Hooks不修改业务代码。源码接口注释变化时按需另跑Doxygen，不因文档工具测试通过宣称软件业务重新通过。

Hooks必须由Codex信任后才会运行；本项目只提供配置、适配脚本和本地验证，不修改信任记录或绕过审查。CI工作流已配置，只有远端实际运行才可宣称CI通过。
