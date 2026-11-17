# 功能关系与测试追踪

最近核对：2026-11-17；范围：ISS-011～014先红后绿及Release CTest 6/6通过，详见[修复验证](VALIDATION.md#2026-11-17-四项问题修复)。历史诊断探针保留。

新标准存储增量：34个GoogleTest与6组既有回归共40项通过，更新TC-017/018；[本轮结果](VALIDATION.md#2026-11-17-严格原生存储标准)记录先红后绿、独立JUnit与范围限制。

[测试策略与命令](../codebase/TESTING.md) · [原始验证](VALIDATION.md) · [功能关系](FEATURE-RELATIONS.md) · [状态迁移](STATE-MACHINES.md)

“已有用例”表示在源码中定位到断言；下表不将历史运行结果提升为新工作区验收结论。变更后应重新执行受影响测试，并记录新的独立证据。

| 用例ID | 功能/关系/状态 | 已有真实测试位置 | 关键断言 | 证据/缺口 |
|---|---|---|---|---|
| TC-001 | F07/F11/F17；REL-001 | [WorkflowRegression::integratedFlow](../../tests/workflow_regression.cpp) | 项目→模型→图库→编辑→重开→导出在同一上下文 | 见既有workflow记录；完整可见桌面未验 |
| TC-002 | F23/F24/F25；REL-003/006；ST-ANN-001/002/003 | [widget load edit save reopen分组](../../tests/workflow_regression.cpp) | 加载clean、编辑dirty、保存像素与重开一致；撤销/形状列表同步 | 历史workflow与数据回归；不是只检查文件存在 |
| TC-003 | F24/F26/F27；REL-003；ST-ANN-004/005 | [workflow保存失败/重试](../../tests/workflow_regression.cpp)、[native main](../../tests/native_annotation_regression.cpp) | labels被普通文件阻塞时保持dirty与旧内容；重试成功后前进；末图停留 | native记录包含24项行为断言；原生窗口/高DPI需人工 |
| TC-004 | F01/F04/F35；REL-005；ST-AUTH-001/002 | [WorkflowRegression::singleAccountFlow与deniedHandlers](../../tests/workflow_regression.cpp) | 错误密码/取消、访客直接写拒绝、退出取消/失败保持会话、成功后只读 | 旧角色矩阵不再是当前规则；账号安全迁移另议 |
| TC-005 | F08/F12；REL-001；ST-CTX-001 | [WorkflowRegression::selectedDeletion与modelCreation](../../tests/workflow_regression.cpp) | A活动/B选中删除、恢复目录失败、关闭通知及成功通知次数 | 真实文件系统故障种类未穷举 |
| TC-006 | F13/F14/F18；REL-002；ST-IO-001 | [WorkflowRegression::transferRecovery](../../tests/workflow_regression.cpp) | 配对跳过/失败保持、提交前撤权、修复重试、restore.json | 只支持已定义的导入布局；不承诺任意分区递归 |
| TC-007 | F15/F16；REL-004 | [WorkflowRegression::categoryProtection与cocoRoundTrip](../../tests/workflow_regression.cpp) | 禁止重排/删除；重命名/追加保持槽位；COCO稀疏编号按名称映射 | 类别批量迁移未实现 |
| TC-008 | F20/F21；REL-004；ST-IO-001 | [WorkflowRegression::exportSemantics](../../tests/workflow_regression.cpp)、[数据回归main](../../tests/data_contract_regression.cpp) | COCO引用/配对、YOLO归一化/旋转角点、空标注、选中范围、失败无提交 | 压缩RLE/部分LabelMe形状不支持；有损转换有边界 |
| TC-009 | F19/F24；REL-006 | [WorkflowRegression::cocoRoundTrip与categoryProtection](../../tests/workflow_regression.cpp) | 坏图/非法类别定位；同图两形状实例数2；统计来自已提交标签 | 不作为完整训练数据质量验收 |
| TC-010 | F36/F37/F38；REL-007 | [WorkflowRegression::settingsAndLogs](../../tests/workflow_regression.cpp) | 设置失败恢复/重试、活动根不迁移、取消重置、实际日志文件匹配 | 确认重置/完整进程重启仍需人工 |
| TC-011 | 模块边界/数据层独立性 | [check_module_boundaries.py](../../tests/check_module_boundaries.py)、[数据回归](../../tests/data_contract_regression.cpp) | include与CMake边界、数据目标无Widgets | 不代表代码质量全部达标 |
| TC-012 | 文档自更新与证据保护 | [test_docflow.py](../../tests/test_docflow.py) | 会话前脏改动不误归因、后续改动使review失效、断链/过期产物失败、钩子循环保护 | 本轮运行结果见[维护验收](../maintenance/MIGRATION.md) |
| TC-013 | F13/F14；REL-002；ISS-011 | [invalidImport](../../tests/current_issues_regression.cpp) | 非法bbox/类别/形状/坐标失败且旧配对字节保持；有效新类别与二值mask重开 | 先复现失败，修复后通过；全格式与文件选择器未验 |
| TC-014 | F25；REL-003；ISS-012 | [rotatedMove](../../tests/current_issues_regression.cpp) | 显式中心同步移动/复制；独立角点值、撤销/重做、保存重开及隐式中心 | 先复现失败，修复后通过；可见拖动未验 |
| TC-015 | F36；REL-007；ST-DB-001；ISS-013 | [database及子进程main](../../tests/current_issues_regression.cpp) | 保存目录被默认初始化消费；密码/WAL快照/再次迁移/重开保持；缺失来源或坏目录拒绝 | 8次隔离子进程通过；完整可见应用重启交互未验 |
| TC-016 | F20→F17/F21；REL-002；ST-CLS-001；ISS-014 | [classification](../../tests/current_issues_regression.cpp) | 分类目录→图库/类别/只读预览→全量/选中导出；重载、删除恢复、权限及旧mask模式拒绝 | 先复现失败，修复后通过；原生文件选择器未验 |

## 存储规范新增追踪

| 用例ID | 功能/关系 | 真实测试位置 | 关键断言 | 证据/缺口 |
|---|---|---|---|---|
| TC-017 | F05/F06/F09/F10/F36；ISS-015 | [storage_regression.cpp](../../tests/storage_regression.cpp) | 34个GTest：完整版本/类型/必填字段、便携引用、移动重开、旧文件拒绝不写、外部路径拒绝、新建完整数据集配置、未知字段、native/分类目录、UTF-8/LF、原子失败、设备名、日志路径 | Release通过；不等于全部磁盘故障或训练验收 |
| TC-018 | F15/F16/F17；ISS-015 | [WorkflowRegression::datasetStorage](../../tests/current_issues_regression.cpp) | 真实Widget合规配置读取不写、最新扩展字段保留、无标记/缺失配置/错误模式/未知版本类型拒写、修正后重试 | Release offscreen通过；原生文件选择器未验 |

## 尚未覆盖为产品验收的部分

可见Windows连续操作、原生文件对话框、高DPI/触控板、真实大图性能、完整进程重启、XMind桌面兼容性和相机硬件。训练/预测等占位能力没有可宣称通过的业务测试。LabelMe进程已退役，不继续列为当前运行功能的必验项。

后续新增用例先关联F编号、关系或迁移及业务断言；只有运行记录与对应版本匹配时才记录通过。不以本表行数计算全产品覆盖率。

## 执行门禁映射

每行由scripts/ci/verify_traceability.py消费：要求JUnit中真实通过、无跳过且包含断言输出。多行表示同一追踪需同时满足多个分组；storage正则检查本次发现的全部GoogleTest，不以固定数量代替断言。TC-012使用独立的documentation-tools CTest，startup-smoke另要求存在且通过。门禁只证明以下自动范围。

| 用例ID | 功能/关系/状态 | 已有真实测试位置 | 关键断言 | 证据/缺口 |
|---|---|---|---|---|
| TC-019 | F03/F16/F17/F38；REL-004/007 | [additionalFunctions](../../tests/workflow_regression.cpp) | 主题写入隔离设置、日志转义/上限/清空、无标签类别重排失败恢复与重试、已标注/未标注筛选全选 | Debug自动通过；桌面视觉与大数据性能另验 |

| 类型 | 用例 | 执行测试（正则） | 必须输出 |
|---|---|---|---|
| EXEC | TC-001 | `workflow-regression` | `PASS integrated project model gallery editor reopen export` |
| EXEC | TC-002 | `workflow-regression` | `PASS widget load edit save reopen` |
| EXEC | TC-003 | `native-annotation-regression` | `PASS failed save-and-next retains edits and committed file` |
| EXEC | TC-004 | `workflow-regression` | `PASS guest pages single account login logout and password` |
| EXEC | TC-004 | `workflow-regression` | `PASS guest denied handlers and login state` |
| EXEC | TC-005 | `workflow-regression` | `PASS selected project and model deletion` |
| EXEC | TC-005 | `workflow-regression` | `PASS model creation signals` |
| EXEC | TC-006 | `workflow-regression` | `PASS paired import recovery` |
| EXEC | TC-007 | `workflow-regression` | `PASS category protection` |
| EXEC | TC-007 | `workflow-regression` | `PASS COCO sparse IDs empty labels RLE and quality` |
| EXEC | TC-008 | `workflow-regression` | `PASS export semantic baseline` |
| EXEC | TC-008 | `data-contract-regression` | `PASS data-only paired export independently reread` |
| EXEC | TC-009 | `workflow-regression` | `PASS COCO sparse IDs empty labels RLE and quality` |
| EXEC | TC-010 | `workflow-regression` | `PASS settings and actual log retention` |
| EXEC | TC-011 | `module-boundaries` | `PASS module include boundaries` |
| EXEC | TC-011 | `data-contract-regression` | `PASS data-only undo storage round trip and failure` |
| EXEC | TC-012 | `documentation-tools` | `OK` |
| EXEC | TC-013 | `current-issues-regression` | `PASS ISS-011` |
| EXEC | TC-014 | `current-issues-regression` | `PASS ISS-012` |
| EXEC | TC-015 | `current-issues-regression` | `PASS ISS-013` |
| EXEC | TC-016 | `current-issues-regression` | `PASS ISS-014` |
| EXEC | TC-017 | `storage\..+` | `[  PASSED  ] 1 test.` |
| EXEC | TC-018 | `current-issues-regression` | `PASS storage-config` |
| EXEC | TC-019 | `workflow-regression` | `PASS theme logs filters and reorder recovery` |
