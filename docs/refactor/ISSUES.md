# 改进问题清单（2026-11-17 当前状态）

原历史分析逐项复核后实施。P0为保存与权限，P1为上下文/类别，P2为表单、设置与文件职责。没有将过期行号作为当前证据。历史事实保留于 issues.json 的 history_2026_11_14，当前验证范围见 [VALIDATION](../logic/VALIDATION.md)。

2026-11-17原生标注增量：ISS-001/002的权威模型和保存契约继续保持，补齐UUID选择、拖动撤销/取消、矩形交互、视野保持及保存下一张失败恢复；LabelMe运行功能明确退役，已有JSON格式兼容保留。新增24项行为断言及offscreen截图检查通过，见[实施记录](feature-native-annotation-1.md)。完整可见Windows与人工手感仍需补验。

2026-11-17修复：当前核对发现的ISS-011～014已修复；新增回归先复现四项失败，再验证修复，Release构建和CTest 6/6通过。见[修复验证](../logic/VALIDATION.md#2026-11-17-四项问题修复)。原始诊断保留，已修复仅针对列出的断言范围，不表示整个模块没有其他缺陷。

| 问题 | 优先级 | 当前状态 |
|---|---|---|
| [ISS-001](#iss-001) 标注状态、渲染与撤销分裂 | P0 | 已修复；自动业务回归通过 |
| [ISS-002](#iss-002) 保存结果不传播与离页缺少阻断 | P0 | 已修复；自动业务回归通过，入口集成部分静态确认 |
| [ISS-003](#iss-003) 新建模型的通知被屏蔽 | P1 | 已修复；自动回归通过 |
| [ISS-004](#iss-004) 项目/模型删除目标与上下文未闭合 | P1 | 已修复；自动回归通过 |
| [ISS-005](#iss-005) 类别变更未保持磁盘ID语义 | P1 | 已修复；自动回归通过 |
| [ISS-006](#iss-006) 模块读写授权覆盖不一致 | P0 | 已改为访客只读/登录可写；执行守卫与本轮自动回归通过 |
| [ISS-007](#iss-007) 新增用户名缺登录入口 | P1 | 按明确的单账户需求关闭；不实现任意用户名登录 |
| [ISS-008](#iss-008) 模型表单字段未被创建流程消费 | P2 | 已修复；自动回归通过 |
| [ISS-009](#iss-009) 设置保存/重置的生效范围不明确 | P2 | 部分完成；数据库启动接入与迁移已补齐 |
| [ISS-010](#iss-010) 数据/标注Widget承担重复持久化与格式职责 | P2 | 必要提取已完成；基准通过；保留部分Widget编排 |
| [ISS-011](#iss-011) 普通JSON导入未验证语义即覆盖旧标签 | P1 | 已修复；新增自动回归通过 |
| [ISS-012](#iss-012) 旋转框平移未移动旋转中心 | P1 | 已修复；新增自动回归通过 |
| [ISS-013](#iss-013) 数据库路径配置未接入启动 | P1 | 已修复；新增自动回归通过 |
| [ISS-014](#iss-014) 异常分类导入布局未接入图库 | P1 | 分类链路已接通；采用明确新模式 |
| [ISS-017](#iss-017) 动态图像插件缺少运行依赖 | P1 | 已修复；五格式隔离启动测试通过 |
| [ISS-016](#iss-016) 标注导航与页面销毁生命周期崩溃 | P1 | 已修复；Debug/Release自动回归通过 |
| [ISS-015](#iss-015) 原生配置缺格式守卫、绝对引用与工作目录不一致 | P1 | 已修复；存储GTest及真实Widget回归通过 |

<a id="iss-001"></a>
## ISS-001 标注状态、渲染与撤销分裂

**状态**：已修复；自动业务回归通过。

**当前实现/限制**：AnnotationModel统一形状/dirty/撤销；列表、工具、渲染与保存同源，加载与保存基线一致。

**验证**：矩形与选择处理器、画笔/橡皮/填充/多边形、PNG和JSON重开；修复填充覆盖旧笔画，Esc取消预览。

**未完成或边界**：原生Windows手动交互与大图工具性能未验。

**当前代码**：[AnnotationModel::reset](../../src/deeplearning/model/AnnotationModel.cpp:287)；[AnnotationManagement::saveAnnotations](../../src/deeplearning/widgets/AnnotationManagement.cpp:1422)。

<a id="iss-002"></a>
## ISS-002 保存结果不传播与离页缺少阻断

**状态**：已修复；自动业务回归通过，入口集成部分静态确认。

**当前实现/限制**：AnnotationStore返回路径/原因，QSaveFile提交；成功才清dirty和更新统计。统一保存/放弃/取消，保存失败不离开。

**验证**：模拟labels为普通文件，验证失败保留内容/统计、取消和失败阻断、恢复重试、放弃重载。

**未完成或边界**：退出登录的取消/保存失败/重试/放弃已通过offscreen回归；窗口关闭及原生文件对话框全流程需可见桌面补验；断电不等于普通写失败。

**当前代码**：[AnnotationStore::save](../../src/deeplearning/storage/AnnotationStore.cpp:82)；[AnnotationManagement::prepareToLeave](../../src/deeplearning/widgets/AnnotationManagement.cpp:1685)。

<a id="iss-003"></a>
## ISS-003 新建模型的通知被屏蔽

**状态**：已修复；自动回归通过。

**当前实现/限制**：解除发成功通知时的信号屏蔽；创建成功通知各一次，取消零通知，数据集只绑定当前模型。

**验证**：真实新建模型对话框驱动，检查活动模型、currentModelChanged/modelCreated和取消计数。

**未完成或边界**：新建失败各分支未逐一注入系统级故障。

**当前代码**：[ModelManagement::onNewModel](../../src/deeplearning/widgets/ModelManagement.cpp:462)。

<a id="iss-004"></a>
## ISS-004 项目/模型删除目标与上下文未闭合

**状态**：已修复；自动回归通过。

**当前实现/限制**：项目/模型预览与活动对象分离，删除选中对象，确认含路径；成功清当前上下文，非当前保留。删除改为恢复目录移动，登记失败回滚。

**验证**：A活动/B选中、取消、恢复目录阻塞、当前项目/模型删除通知；样本精确配对删除及手动恢复。

**未完成或边界**：恢复目录无自动还原/保留期管理UI；跨文件提交中断需按恢复记录人工处理。

**当前代码**：[ProjectManagement::onDeleteProject](../../src/deeplearning/widgets/ProjectManagement.cpp:654)；[ModelManagement::onDeleteModel](../../src/deeplearning/widgets/ModelManagement.cpp:761)。

<a id="iss-005"></a>
## ISS-005 类别变更未保持磁盘ID语义

**状态**：已修复；自动回归通过。

**当前实现/限制**：标签目录递归存在标注或有未保存编辑时禁止删除/重排；重命名和追加保持已有编号，不自动迁移。

**验证**：类别重排/删除被拒绝；重命名和新增后标签字节不变，重开仍引用原类别槽位；COCO稀疏ID映射。

**未完成或边界**：不支持已标注数据的破坏性类别迁移；PNG最多255个非背景类别。

**当前代码**：[DatasetManagement::onLabelOrderChanged](../../src/deeplearning/widgets/DatasetManagement.cpp:2755)；[DatasetManagement::hasPersistedAnnotations](../../src/deeplearning/widgets/DatasetManagement.cpp:3785)。

<a id="iss-006"></a>
## ISS-006 模块读写授权覆盖不一致

**状态**：2026-11-17已改为单账户登录态控制；15组自动业务回归及2项CTest通过。

**当前实现/限制**：所有页面在未登录时可读，登录后统一可写。AccessControl和PermissionGuard共用会话状态，保留业务执行处写守卫。旧权限表保留但不读写，不再注册数据库权限或缓存角色权限。导入、保存、导出、转换与设置均需要登录；LabelMe运行入口已退役，格式读取兼容保留。

**验证**：2026-11-17已有回归覆盖访客直接调用写处理器时文件/数据库不变，登录后按钮统一解锁、退出恢复只读及旧禁止权限记录不影响新规则；本次文档迁移未重跑业务测试。

**未完成或边界**：可见Windows原生桌面完整操作仍需补验；多用户CRUD已取消，LabelMe外部进程退出不再属于当前运行功能的验收范围。

**当前代码**：AccessControl命名空间的[canWrite](../../src/core/AccessControl.h:15)；[PermissionGuard::canWrite](../../src/ui/PermissionGuard.cpp:12)。

<a id="iss-007"></a>
## ISS-007 新增用户名缺登录入口

**状态**：按2026-11-17用户明确的单账户需求关闭；不是实现了任意用户名登录。

**当前实现/限制**：删除多用户增删、角色和权限矩阵界面；账户页仅状态与修改自身密码。启动直接只读浏览；固定账户登录后解除只读。新库仅创建superadmin/admin123，旧库优先复用superadmin及其原密码，否则选择最高历史role、最早ID的账户。旧行不删除、不重置密码，其他历史账号不再认证。

**验证**：已有单账户回归覆盖新库唯一账户、旧账号拒绝、取消/错误密码保持只读和成功登录解除只读。退出登录先处理未保存内容，保存失败或取消阻止退出；对应offscreen记录见验证专题。本次文档迁移未重跑业务测试。

**未完成或边界**：没有新增账号或忘记密码恢复流程。明文记住密码行为已移除，成功登录或改密清理旧记住字段；历史其他账号保留但不能登录。旧缺陷事实保留在issues.json的历史字段。

**当前代码**：[DatabaseManager::singleAccount](../../src/core/DatabaseManager.cpp:81)；[LoginDialog::onLoginClicked](../../src/app/Login.cpp:35)；[账户设置](../../src/app/UserManagementWidget.cpp)。

<a id="iss-008"></a>
## ISS-008 模型表单字段未被创建流程消费

**状态**：已修复；自动回归通过。

**当前实现/限制**：createModel接收并持久化中文描述；路径派生只读，移除无效浏览按钮；重开保留描述。

**验证**：真实模型创建对话框填中文描述，加载.fvdl核对内容和通知。

**未完成或边界**：未逐一截图检查所有缩放比例下的表单布局。

**当前代码**：[ModelConfig::createModel](../../src/deeplearning/storage/ModelConfig.cpp:22)。

<a id="iss-009"></a>
## ISS-009 设置保存/重置的生效范围不明确

**状态**：部分完成；ISS-013已补齐数据库启动配置与保留账户迁移。

**当前实现/限制**：日志立即生效，项目默认根首次使用固定；重置仅五项路径，不迁移项目。数据库目录下次启动消费；空目标从最近活动库建立一致副本，已有目标直接复用；传统视觉路径无执行功能消费，不能声称全部路径重启生效。日志清理匹配真实无.log文件名并保护当前日志。

**验证**：临时QSettings保存重读、写失败回滚内存并重试；活动根保持、无迁移、取消重置；实际Logger文件和过期选择器。

**未完成或边界**：确认重置后的实际写入、完整进程重启和真实ACL场景未验；QSettings多键不是断电事务。

**当前代码**：[SystemSettingsWidget::onSave](../../src/app/SystemSettingsWidget.cpp:265)；[SystemSettingsWidget::expiredLogs](../../src/app/SystemSettingsWidget.cpp:387)。

<a id="iss-010"></a>
## ISS-010 数据/标注Widget承担重复持久化与格式职责

**状态**：必要提取已完成；基准通过；保留部分Widget编排。

**当前实现/限制**：先复现COCO类别表与引用不一致，再提取AnnotationStore与DatasetFileService，统一读取/编码、配对复制回滚、暂存导出。未重写全架构或引入无依据线程。

**验证**：导出图像配对、编号、坐标、旋转角点、空标注、选中范围、RLE重读、失败无半成品；导入失败重试/删除恢复。

**未完成或边界**：聚合COCO与配置仍由DatasetManagement编排；压缩RLE、LabelMe circle等明确不支持；全分区导入不在当前支持范围。

**当前代码**：[DatasetFileService::importFiles](../../src/deeplearning/storage/DatasetFileService.cpp:75)；[DatasetFileService::exportFiles](../../src/deeplearning/storage/DatasetFileService.cpp:276)。

<a id="iss-011"></a>
## ISS-011 普通JSON导入未验证语义即覆盖旧标签

**状态**：2026-11-17已修复；新增自动回归通过，P1。

**当前实现/限制**：普通JSON/PNG在备份与覆盖前调用统一AnnotationStore读取器，检查类别、几何、支持形状和尺寸；保留自动发现新类别。LabelMe坐标必须为数值。语义失败不修改旧图像/标签；文件提交仍为按组暂存与回滚，不承诺断电事务。

**验证**：[原始诊断](../logic/evidence/current-audit-20261117-probe.log)保留；[新增回归](../../tests/current_issues_regression.cpp)先复现失败，再断言负宽bbox、负/小数类别、非数值点和circle失败且旧配对字节保持；有效新LabelMe类别与二值掩码重开通过。[Release结果](../logic/evidence/issue-fixes-final-release-20261117.log)。

**未完成或边界**：不支持全部LabelMe形状或压缩RLE；未逐一验收全部图像格式与原生文件选择器。

**当前代码**：[DatasetFileService::importFiles](../../src/deeplearning/storage/DatasetFileService.cpp:75)；[AnnotationStore::load](../../src/deeplearning/storage/AnnotationStore.cpp:131)。

<a id="iss-012"></a>
## ISS-012 旋转框平移未移动旋转中心

**状态**：2026-11-17已修复；新增自动回归通过，P1。

**当前实现/限制**：translateShape与pasteFromClipboard同步平移bbox及非空rotCenter；空中心继续隐式采用bbox中心，事务与UUID规则保持。

**验证**：[新增回归](../../tests/current_issues_regression.cpp)先复现90°框错误位移；修复后首角点为独立计算的(115,80)，显式中心随动，复制偏移、不同UUID、撤销/重做、保存重开及隐式中心保持通过。[Release结果](../logic/evidence/issue-fixes-final-release-20261117.log)。

**未完成或边界**：未执行可见桌面拖动手感验收；多选整体变换和旋转角度拖动仍未实现。

**当前代码**：[AnnotationModel::translateShape](../../src/deeplearning/model/AnnotationModel.cpp:205)；[AnnotationModel::pasteFromClipboard](../../src/deeplearning/model/AnnotationModel.cpp:310)。

<a id="iss-013"></a>
## ISS-013 数据库路径配置未接入启动

**状态**：2026-11-17已修复；隔离独立进程回归通过，P1。

**当前实现/限制**：默认初始化读取paths/database目录并追加fusevision.db；空配置使用exe/data。已有目标直接复用；空目标从database/last_opened_file（首次为旧默认库）经VACUUM INTO建立一致副本并保留来源。记忆来源缺失、目录或迁移失败时停止并显示原因；不以默认新账户回落。显式文件或:memory:路径保持调用者契约，不改活动路径记忆。

**验证**：[新增回归](../../tests/current_issues_regression.cpp)将exe复制到临时runtime启动8次独立子进程，覆盖默认→配置路径、重开密码保持、自定义再次迁移、来源连接仍打开时已提交WAL密码快照、缺失来源、复用已有目标、普通文件阻塞目录。[Release结果](../logic/evidence/issue-fixes-final-release-20261117.log)；未访问真实账户库。

**未完成或边界**：已有目标不合并或覆盖，重置回默认目录时沿用该目标自身账户；可见完整应用的设置→重启交互与真实ACL未人工验收。传统视觉两个路径仍只有持久化配置。

**当前代码**：[main](../../src/main.cpp:107)；[DatabaseManager::initDatabase](../../src/core/DatabaseManager.cpp:21)；[SystemSettingsWidget::onSave](../../src/app/SystemSettingsWidget.cpp:265)。

<a id="iss-014"></a>
## ISS-014 异常分类导入布局未接入图库

**状态**：2026-11-17已修复；分类目录、图库、预览、统计和导出回归通过，P1。

**当前实现/限制**：沿用新异常模型OK/NG分类布局，扫描train/val/test下固定类别目录；样本保留相对路径、目录类别和split。分类导入选择类别/划分，坏图失败、同名跳过；文件夹导入读取六个固定目录。分类双击及右键只读预览，禁止类别修改与区域标注；导出保留划分、同名异类图像和classify YAML。按用户取消旧数据兼容的决定，异常模型仅接受明确classification配置，拒绝旧二值掩码回退，不改写原文件。

**验证**：[新增回归](../../tests/current_issues_regression.cpp)先复现图库0样本，修复后3图/类别计数1与2，预览真实图像且无标注信号；全量与选中导出保留目录/字节；导入重载、配置重开、删除和restore.json还原及访客拒绝已通过。原[Release历史结果](../logic/evidence/issue-fixes-final-release-20261117.log)包含当时的旧掩码兼容；当前测试改为拒绝错误模式且原文件字节保持，见[新标准结果](../logic/VALIDATION.md#2026-11-17-严格原生存储标准)。

**未完成或边界**：未开发训练、预测、自动划分或区域mask的新模式选择界面；文件选择器与可见桌面未人工验收。只支持固定目录直属图像，不承诺任意递归分类结构。

**当前代码**：[ModelConfig::createModel](../../src/deeplearning/storage/ModelConfig.cpp:22)；[DatasetManagement::convertAnomalyClassification](../../src/deeplearning/widgets/DatasetManagement.cpp:3346)；[DatasetManagement::loadDatasetConfig](../../src/deeplearning/widgets/DatasetManagement.cpp:286)；[DatasetFileService::exportFiles](../../src/deeplearning/storage/DatasetFileService.cpp:276)。

<a id="iss-015"></a>
## ISS-015 原生配置缺格式守卫、绝对引用与工作目录不一致

**状态**：2026-11-17已修复；34个GoogleTest与6组既有回归共40项Release CTest通过，P1。

**当前实现/限制**：StorageContract统一项目/模型/数据集原生配置的document_type、format_version=字符串1.0及必填字段守卫，仅接受完整1.0标准，缺标记/未知类型版本/旧布局拒绝读取和改写。成功项目保存和状态更新改用根内相对引用，模型与数据集保存保留未知根字段；新标注工作目录为native平铺images/labels，分类为六目录，新模型立即生成完整数据集配置，明确annotation/classification且与模型匹配；新YAML使用相对path及UTF-8/LF。新建拒绝Windows设备名；日志目录中的点不再被误判为文件，仅显式.log路径使用自定义前缀。

**验证**：[GTest源码](../../tests/storage_regression.cpp)与[真实Widget datasetStorage](../../tests/current_issues_regression.cpp)保留先红后绿证据；覆盖移动重开、旧绝对引用/旧yolo布局/无标记配置拒绝、必填字段、新建数据集配置、缺失配置拒写、模式匹配及无效内存/磁盘配置不覆盖、扩展字段、原子失败、分类/标注目录和日志路径。结果见[本轮验证](../logic/VALIDATION.md#2026-11-17-严格原生存储标准)。

**未完成或边界**：本轮没有批量搬移旧项目/账户/设置、改写旧YAML或自动吸收嵌套分区；原生工作索引不等于训练配置。恢复UI/保留期、并发冲突检测、断电事务和Q04备份策略未实现；Debug已由本轮回归补验，完整可见Windows操作未验。

**当前代码**：[validate](../../src/deeplearning/storage/StorageContract.h:16)；[ProjectConfig::saveProject](../../src/deeplearning/storage/ProjectConfig.cpp:111)；[ModelConfig::createModel](../../src/deeplearning/storage/ModelConfig.cpp:22)；[DatasetManagement::saveDatasetConfig](../../src/deeplearning/widgets/DatasetManagement.cpp:367)；[Logger::init](../../src/core/Logger.cpp:40)。

<a id="iss-016"></a>
## ISS-016 标注导航与页面销毁生命周期崩溃

**状态**：2026-11-17已修复；Debug与Release自动回归通过，P1。

**当前实现/限制**：openImage按值持有模型ID与图片路径，避免刷新样本或保存对话框使引用失效。析构阶段先停自动保存、断开模型与撤销栈到页面的信号并取消未提交工具，避免子对象销毁后按钮仍被更新。

**验证**：[Debug基线](../logic/evidence/full-functional-debug-baseline-20261117.log)40项中两项崩溃；[堆栈](../logic/evidence/full-functional-debug-stack-20261117.log)分别定位openImage的QString赋值和QUndoStack析构时的按钮更新。[最终Debug](../logic/evidence/full-functional-debug-final-20261117.log)与[Release](../logic/evidence/full-functional-release-final-20261117.log)41/41通过，含连续前后导航、保存失败重试及持有撤销记录的页面销毁。

**未完成或边界**：自动Qt事件测试不代替完整可见桌面、高DPI与相机验收。

**当前代码**：[AnnotationManagement::openImage](../../src/deeplearning/widgets/AnnotationManagement.cpp:107)；[AnnotationManagement::~AnnotationManagement](../../src/deeplearning/widgets/AnnotationManagement.cpp:65)。

<a id="iss-017"></a>
## ISS-017 动态图像插件缺少运行依赖

**状态**：2026-11-17已修复；新增隔离编解码回归通过，P1。

**当前实现/限制**：原部署仅复制Qt图像插件和JPEG库，TIFF/WebP/Jasper传递依赖未随应用部署。新增CMake脚本扫描已部署类型对应的vcpkg插件依赖，复制清单内依赖且拒绝未解析依赖，排除Windows系统库。脚本变更触发重新链接部署。

**验证**：[新增编解码红阶段](../logic/evidence/full-functional-codecs-before-20261117.log)40/41、[运行日志](../logic/evidence/full-functional-codecs-red-runtime-20261117.log)明确TIFF失败；[修复后Release](../logic/evidence/full-functional-codecs-fixed-release-20261117.log)41/41。startup-smoke新增PNG/JPEG/BMP/TIFF/WebP真实写入/重读，交付ZIP解压后在系统PATH与无外部Qt插件路径下复跑。

**未完成或边界**：Windows与MSVC系统运行库仍是目标环境要求；未验证全部第三方SDK或相机驱动。

**当前代码**：[main](../../src/main.cpp:107)；[fusevision_deploy_runtime](../../cmake/DeployRuntime.cmake:15)。

## 待确认的产品决策与推荐


| 决策 | 当前限制 | 推荐方案（尚未实施） |
|---|---|---|
| Q01 账户模式（已决定） | 2026-11-17明确只需一个账户；未登录只读、登录解除只读 | 已实施单账户；不再新增用户名、角色或逐模块授权。 |
| Q02 异常检测（新标准已实施） | 新模型固定OK/NG分类，取消旧mask配置回退 | 将来是否新增模式选择或区域定位能力仍需产品决定。 |
| Q03 矩形命名 | 当前Fixed→obb，Free→detect，与部分中文标签直觉不一致 | 以“水平框detect / 旋转框OBB”直述能力，确认后只做有兼容方案的名称/映射调整。 |
| Q04 审计与备份 | 历史审计/备份要求未确认；角色人数不属于当前单账户范围 | 明确审计字段、保留期和恢复目标，再设计备份恢复；不重新引入角色矩阵。 |
| Q05 原生存储标准（用户已决定） | 2026-11-17明确开发阶段无需兼容旧数据 | 已实施完整1.0新标准；旧原生配置拒绝且保留原文件，外部交换格式继续支持。 |

仅暂停依赖上述决策的改动。未扩展训练、预测、模型导出、数据增强或传统视觉执行引擎。明文记住密码已停用；无盐摘要、默认初始凭据仍需独立兼容方案；本轮不宣称完成认证安全迁移。

ISS-011～014已完成本轮修复。后续优先补完整桌面验收；Q02未来模式界面另议。并行的验收工作是可见Windows保存/导航/退出登录/关闭、高DPI与触控板；其未验证状态不能标为现存故障。随后优化任务命名（Q03）、审计/备份（Q04）、恢复入口与保留期、格式/分区支持和真实负载基准。图库逐张创建控件及同步掩码处理有性能风险，先测量再决定分页/后台任务。认证兼容迁移包括带盐密码派生、首次默认凭据处理与恢复方案，单独设计，不借本次核对更改账户。

内容优化：明确区分独立“数据检查”占位页与数据集内已有质量检查；训练/预测/导出占位入口展示开发状态。F25、F13/F14、F36及数据契约已同步修复行为与剩余限制，既有成功测试不外推到未覆盖分支。不要运行旧生成脚本覆盖本状态。
