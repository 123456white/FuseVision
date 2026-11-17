# 核心流程实施与验证（2026-11-17 更新）

文档维护入口更新（2026-11-17）：当前工具、文档/导图生成见[迁移与验收](../maintenance/MIGRATION.md)。此前文档迁移未重跑软件业务测试；其后的当前核对结果见下节。历史软件测试与旧工具命令按原阶段保留；旧`docs/logic/tools/validate_analysis.py`已归档，不作为当前重验命令。

[文档入口](../README.md) · [问题状态与产品决策](../refactor/ISSUES.md) · [业务实施计划](../refactor/refactor-workflow-consistency-1.md) · [当前机器校验](evidence/native-annotation-validation.json) · [单账户历史记录](evidence/single-account-validation.json) · [2026-11-15机器记录](evidence/workflow-validation.json)


## 2026-11-17 严格原生存储标准

用户明确“运行数据与文件格式无需兼容旧数据，开发阶段可执行新数据标准”。本次在已完成的存储规范基础上移除原生旧格式回退；手动docflow会话strict-storage-20261117沿用修改前内容基线。此前兼容策略与测试日志均为历史结果，保留不覆盖。当前规则见[数据契约](DATA-CONTRACTS.md)，问题与决定见[ISS-015/Q05](../refactor/ISSUES.md#iss-015)。

| 验证 | 实际执行与结果 | 证据及限制 |
|---|---|---|
| 生产修改前 | 新测试编译成功，40项中12项预期失败 | [红阶段](evidence/strict-storage-red-20261117.log)：仍接受旧格式/绝对根/旧yolo、缺少必填校验、未立即生成dataset_config及模式回退 |
| Release构建及全套CTest | 应用和受影响目标重编译/链接，40/40通过，6.33秒 | [构建测试](evidence/strict-storage-green-20261117.log)、[完整CTest](evidence/strict-storage-ctest-20261117.log)、[JUnit](evidence/strict-storage-junit-20261117.xml)；34个GTest与6组回归，时间不是性能基准 |
| 原生文档和路径 | 缺标记、旧绝对根、旧yolo、项目外引用、缺少必填字段均拒绝；原文件字节保持；合规相对引用移动重开与扩展字段保留通过 | [存储GTest](../../tests/storage_regression.cpp)使用真实临时文件；不是全部ACL/断电/并发验证 |
| 新模型与数据集 | 创建即写完整1.0数据集配置，annotation明确模式；classification生成OK/NG两类；缺失配置不静默补建，模式与模型布局不匹配拒绝保存且字节保持 | [真实Widget回归](../../tests/current_issues_regression.cpp)含读取不写、最新未知根字段及修正后重试；配置保存不等于整批图像/标签事务 |
| 既有功能 | 原生标注、数据契约、模块边界、综合工作流、启动、ISS-011～014回归全部通过 | 保留几何、导入导出、权限拒绝、恢复文件及8个隔离数据库子进程；异常模式旧兼容断言改为新标准拒绝 |
| API文档 | Doxygen成功，warnings.log为0字节 | [生成日志](evidence/strict-storage-doxygen-20261117.log)；不等于产品验收 |
| 文档与导图 | sync --html生成31页，check --diagrams通过，6张Mermaid图解析成功 | 当前功能/问题JSON和XMind由MD生成，代码引用按现有符号位置核对；不代替业务测试 |

[源码与证据摘要](evidence/strict-storage-verification-20261117.json)记录本次源码和原始输出摘要。测试使用临时数据、INI和隔离数据库，三个截图证据变量置空；未删除、迁移或改写真实项目/账户/设置。外部COCO、YOLO、LabelMe及PNG读写能力保留。Debug、完整可见Windows桌面、原生文件对话框、硬件和真实负载本轮未验；训练、预测、自动划分等占位能力没有扩展。

## 2026-11-17 功能回归与存储规范

用户要求测试当前功能（可用GTest）并规范软件存储文件。本轮按运行数据范围实施，不整理无关源码/安装目录；手动docflow会话test-storage-20261117在修改前建立基线，保护会话前大量未提交工作。当前工作区不是已提交版本，未访问真实项目、用户配置或账户库。

| 验证 | 实际结果 | 证据与边界 |
|---|---|---|
| 原功能基线 | Release构建成功，原6组CTest全部通过，4.47秒 | [基线日志](evidence/test-storage-baseline-20261117.log)；不是全产品人工验收 |
| 首批存储红阶段 | 新增23个GTest，其中19个失败；原6组仍通过 | [标准化文本读取后的红阶段](evidence/test-storage-red-normalized-20261117.log)；版本拒写、路径、目录、设备名及日志误判均有反例；[首次原始输出](evidence/test-storage-red-20261117.log)还包括测试读取Windows CRLF的断言差异，随后先统一读取再确认19个真实失败 |
| 追加数据集/旧文件红阶段 | 27个GTest+6组回归中4项失败 | [补测日志](evidence/test-storage-dataset-red-20261117.log)；无效原模型被覆盖、状态更新未标准化路径、数据集字段类型未校验、真实Widget未保存类型/版本标记 |
| YAML换行红阶段 | 两类新建YAML为CRLF，2项预期失败 | [YAML反例](evidence/test-storage-yaml-red-20261117.log)；修复写入方式后为LF |
| 最终Release构建与CTest | 应用及受影响目标重编译/链接，33/33通过，5.96秒 | [构建/测试日志](evidence/test-storage-final-release-20261117.log)、[原始完整输出](evidence/test-storage-final-ctest-20261117.log)、[JUnit](evidence/test-storage-final-junit-20261117.xml)；27个独立GTest+6组既有回归，不视为33个完整产品场景或性能基准 |
| 项目/模型/目录 | 旧绝对引用与移动重开、读不改写、保存相对化、外部绝对引用、未知根字段、非法内存/磁盘配置拒写、旧yolo兼容、native/分类布局、LF YAML、设备名及日志路径均通过 | [GTest源码](../../tests/storage_regression.cpp)；数据层真实文件API，不用Mock；未遍历全部磁盘/ACL/断电故障 |
| 数据集配置真实Widget | 无标记旧文件打开字节保持、显式保存标记、保留最新磁盘扩展字段、未知版本/类型/字段拒写、修正后重试通过 | [datasetStorage](../../tests/current_issues_regression.cpp)；配置拒写不等于整批标签/图像操作的跨文件事务 |
| 原有功能回归 | 原生画布、数据契约、模块边界、综合工作流、启动冒烟和ISS-011～014全部复跑通过 | 包括权限、标注重开/失败恢复、分类导出与8个隔离数据库子进程；完整断言范围仍见[测试策略](../codebase/TESTING.md) |
| API生成 | Doxygen成功，warnings.log为0字节 | [生成日志](evidence/test-storage-doxygen-20261117.log)；生成成功不等于业务验收 |
| 文档与导图 | sync --html生成31页，check --diagrams通过，577个本地链接及6张Mermaid图 | 功能/问题JSON、XMind从MD生成；代码符号引用已对照当前定义更新；不替代业务测试 |

新代码只规范原生项目/模型/数据集配置与新建目录；版本与兼容规则集中于[DATA-CONTRACTS](DATA-CONTRACTS.md)，缺陷登记[ISS-015](../refactor/ISSUES.md#iss-015)。账户库位置/快照、QSettings平台存储、原生标签元数据和恢复数组契约沿用已有实现。新增[源码与结果摘要](evidence/test-storage-verification-20261117.json)记录实际测试计数、源文件与证据摘要；历史证据独立保留。

普通测试的三个截图证据环境变量置空；用例全部使用临时数据、INI、内存或临时文件数据库。Debug、完整可见Windows桌面、原生文件选择器、高DPI/触控板、相机和真实负载未验。训练、预测、自动数据划分、增强和传统视觉执行仍为占位，不由现有测试通过推断完成。

## 2026-11-17 四项问题修复

用户授权“修复”后，针对当次核对发现的ISS-011～014实施。原有未提交内容由手动docflow会话fix-current-issues-20261117建立修改前基线，历史诊断和截图不覆盖；新增用例为[current_issues_regression.cpp](../../tests/current_issues_regression.cpp)。

| 验证 | 实际执行与结果 | 证据与限制 |
|---|---|---|
| 修改前回归 | 新current-issues-regression分别报四项预期失败；原有5项通过 | [红阶段日志](evidence/issue-fixes-before-20261117.log)；无生产代码修改时执行 |
| Release构建与全套CTest | 应用及受影响目标重编译/链接；6/6通过，总4.84秒 | [最终构建测试日志](evidence/issue-fixes-final-release-20261117.log)与[详细CTest输出](evidence/issue-fixes-final-ctest-20261117.log)；时间不是性能基准 |
| JSON/PNG导入 | 非法bbox、负/小数类别、非数值LabelMe点、不支持circle均在覆盖前失败，旧配对逐字节保持；有效新名称、二值掩码及只含隐藏形状的原生PNG/JSON重开 | 真实读取器与文件服务，无模拟；未逐一验收全部格式 |
| 旋转几何 | 90°首角点平移至独立计算的(115,80)；中心、复制偏移、UUID、撤销/重做与JSON重开保持 | 数据模型/导出语义；可见桌面拖动手感未验 |
| 数据库启动 | 8次隔离子进程验证保存目录读取、默认及自定义来源迁移、已提交WAL密码快照、重开、来源保留、已有目标复用、缺失来源与坏路径拒绝 | exe复制到临时runtime，INI及文件库隔离；没有访问真实账户；未操作完整应用原生设置/重启界面 |
| OK/NG分类 | 3图统计1/2类计数；真实Qt只读预览且不发标注信号；全量/选中导出保留划分、重复基名与图片字节；导入重载、配置重开、删除恢复、访客拒绝、旧mask兼容 | 真实Widget与文件服务；未驱动原生文件选择器、训练或预测 |

首次增量构建曾因qsizetype到int初始化收缩转换失败，见[中间构建日志](evidence/issue-fixes-first-pass-20261117.log)；修正显式索引转换后通过。随后扩展数据库WAL/再次迁移断言；兼容性补测发现仅含隐藏形状的原生标签无可见类别，见[补测失败日志](evidence/issue-fixes-native-import-before-20261117.log)。补齐原生元数据类别发现后再次全套6/6通过。中间成功结果另见[扩展前日志](evidence/issue-fixes-second-pass-20261117.log)，不替代最终日志。

普通测试运行时三个截图证据环境变量置空。Debug、可见Windows桌面、高DPI/触控板、相机、真实负载和XMind客户端本轮未验；训练、预测、增强及传统视觉执行引擎没有扩展。分类策略依据现有新模型classification布局及用户本轮修复授权，保留显式旧掩码数据，不自动迁移；未来模式选择界面仍待产品决定。

以下“当前功能核对”为修复前历史证据，原输出和快照保留，不代表修复后的实现状态。复验当前实现使用CmakeBuild.ps1 -Configuration Release -NoRun -Test；历史probe源码只用于解释原始反例。

[修复后源码与证据摘要](evidence/issue-fixes-20261117-verification.json)记录当前源文件/日志哈希，并确认原四份审计证据哈希保持。

文档及API生成：Doxygen HTML/XML成功且warnings.log为空，见[生成日志](evidence/issue-fixes-final-doxygen-20261117.log)；当前Markdown链接、代码符号、派生镜像与六张Mermaid图由docflow检查。

## 2026-11-17 当前功能核对

核对对象为当前未提交工作区，保留会话开始前已有的全部业务重构和文档改动；没有以Git HEAD代表当前实现。本次没有修改src、CMake或既有测试，只修正文档、登记问题并新增独立证据。会话基线为out/docs/sessions中的audit-current-20261117，采用手动begin/review/finish，不声称项目Hooks已触发。

| 当前能力 | 核对结论 | 依据与边界 |
|---|---|---|
| 项目/模型配置 | 创建、打开、预览、描述和可恢复删除已有实现 | 既有workflow/data测试复跑；不表示所有原生文件对话框人工通过 |
| 数据集与类别 | 普通images/labels配对、类别保护、图库、质量提示及格式导出已有实现 | 普通JSON覆盖语义漏检已复现；完整分区导入未实现 |
| 原生标注 | 六工具、撤销/重做、保存/重开、离页决策、连续导航已有实现 | native/workflow复跑；旋转框显式中心平移错误另由探针复现 |
| 账户与设置 | 访客只读、单账户解锁/改密、日志配置已有实现 | 数据库路径设置未被启动消费；传统视觉路径只保存，尚无执行消费链 |
| 未完成能力 | 深度学习后五页、数据增强、传统视觉执行仍为占位 | 检查DeepLearningWidget::initUI、onAugmentClicked、TraditionalWidget；数据集内质量检查不等于独立“数据检查”页已实现 |
| 异常检测模式 | classification与二值mask入口并存，未统一 | 分类目录到图库断链已复现；Q02尚待决定，不能宣称完整分类闭环 |

实际执行：

| 验证 | 结果 | 独立证据/限制 |
|---|---|---|
| Release配置、增量构建与CTest | 成功；Ninja提示no work to do，5/5通过，总约3.50秒 | [本次日志](evidence/current-audit-20261117-release-build.log)；当前源码由构建系统判定无需重编译，不冒称全量重编；时间不是性能基准 |
| 诊断探针编译与执行 | 编译/运行退出码0，四项缺陷观测复现 | [源码](evidence/current-audit-20261117-probe.cpp)、[构建脚本](evidence/current-audit-20261117-build-probe.ps1)、[原始输出](evidence/current-audit-20261117-probe.log)；退出码0表示探针执行完成，不表示被测功能正确 |
| 静态语义核对 | 确认启动空参调用、导入浅校验、bbox单独平移、classification写入与images枚举不一致 | [ISS-011～014](../refactor/ISSUES.md)含当前符号/行号；后续实现变化需重验 |
| 文档结构与派生一致性 | sync --html成功；修改后check --diagrams通过，520个本地链接、6个Mermaid图 | 已同步功能/问题JSON、XMind及31页HTML；review/finish另由会话记录，此项不代替产品验收 |

诊断输入与观测：

1. ISS-011：临时64×64图像，已有合法JSON标签；导入bbox=[1,2,-3,4]的新标签并允许覆盖。结果import_ok=1、reopen_ok=0、previous_valid_label_replaced=1。文件传输通过，但旧有效标签已替换为无法重开的标签。
2. ISS-012：bbox=[80,95,40,10]、rot_center=[100,100]、rotation=90；用真实AnnotationModel事务平移(10,0)，再由当前OBB编码器计算顶点。实际首角点移动(0,10)，中心未变。复制分支有同类静态风险，本轮未单独驱动复制手势。
3. ISS-013：临时INI保存并重载自定义数据库目录，再调用与正常启动相同的默认数据库初始化；实际仍打开探针exe旁的data/fusevision.db，configured_path_used=0。探针exe位于out/current-audit-20261117，不访问应用真实账户库；未做完整应用的两进程重启验收。
4. ISS-014：实际创建异常模型，train/OK和train/NG各存1张图，真实DatasetManagement等待异步载入后gallery_samples=0。验证的是专用导入产生布局的消费链，没有操作原生文件选择器或宣称执行了整个分类导入UI。

项目、图像和QSettings均在QTemporaryDir，数据库只写隔离探针输出目录；历史日志和截图未覆盖，三个截图证据环境变量在本轮CTest置空。源码与证据摘要见[核对清单](evidence/current-audit-20261117-snapshot.json)。本轮未运行Debug、可见Windows桌面、高DPI/触控板、相机、真实负载或XMind客户端。既有回归5/5通过与发现四项新缺陷同时成立，说明其断言没有覆盖这些边界。

复跑先执行当前Release构建测试，再运行探针；保存新结果时使用独立文件名，不覆盖本节原始证据：

```powershell
$env:FUSEVISION_WORKFLOW_EVIDENCE_DIR=''
$env:FUSEVISION_ACCOUNT_EVIDENCE_DIR=''
$env:FUSEVISION_NATIVE_EVIDENCE_DIR=''
./CmakeBuild.ps1 -Configuration Release -NoRun -Test
./docs/logic/evidence/current-audit-20261117-build-probe.ps1
python -X utf8 scripts/docs/docflow.py check --diagrams
```

## 2026-11-17 Qt原生连续标注（此前阶段）

用户选择“一切以推荐及参考软件为准”后，沿用 Qt Widgets/AnnotationModel 实施原生工作区。LabelMe/Python 环境扫描、进程启动、HWND嵌入、自研适配及部署已退役；不删除第三方sdk、历史配置或数据。LabelMe rectangle/polygon JSON读取兼容保留。

已修复：悬停追踪、空格平移两种释放顺序、零滚轮及鼠标锚点缩放；矩形支持拖放和两次点击，反向 Enter 正确完成；多边形显示已放置边链；按模型命中旋转框，角点允许收缩；UUID选择连接画布/列表，拖动不重建列表且一次手势一个撤销；失焦恢复未提交拖动/笔画。显示叠加开关和视野不改变标注，亮度/对比度不重置视野；同模型默认保持视野。保存并下一张仅成功后前进，末张停留，失败保留编辑并可重试。快捷键覆盖工作区及列表焦点。

| 验证类别 | 实际执行与结果 | 边界 |
|---|---|---|
| 修改前基线 | [244文件哈希](evidence/native-annotation-start-baseline.json)及out/native-annotation-baseline.zip；[红绿前置回归](evidence/native-annotation-regressions-before-fix.log)复现四项缺陷/旧入口 | 不以Git HEAD替代已有未提交内容；只有五个明确退役的自研适配/测试文件删除 |
| Release编译 | [构建日志](evidence/native-annotation-release-build.log)，应用和测试重新编译/链接成功 | 未构建Debug |
| 自动CTest | [详细日志](evidence/native-annotation-ctest-release.log)：5/5通过，原15组workflow全部通过；新native用例24项行为断言及1项截图保存检查 | 五项为native-annotation、data-contract、module-boundaries、workflow、startup；旧LabelMe路径测试退役 |
| 失败恢复与语义 | 临时工程两图、内存SQLite、隔离INI；未完成绘制阻止保存、目录被普通文件占用时文件字节/编辑保持、修复后列表焦点Ctrl+Enter成功、末图停留、PNG重开UUID/类别/几何一致、访客处理器拒绝 | 合成1000×800样本，无真实数据性能结论 |
| UI观察 | [真实Qt工作区offscreen截图](screenshots/P05-native-annotation-offscreen-20261117.png)已查看，工具选中和新按钮对应当前状态 | 未进行可见Windows人工连续标注、触控板、高DPI或参考软件逐项对照 |
| 文档与保留 | [机器校验](evidence/native-annotation-validation.json)19项通过；363当前符号行号；XMind9分图/321节点/195代码引用，原截图和历史证据保留 | 未在XMind桌面客户端打开；图中日期历史节点不表示当前入口 |
| Doxygen | 1.18.0生成HTML/XML、10模块、56类/结构，warnings.log为空 | EXTRACT_ALL不等于所有历史成员注释完整；没有运行全库clang-format |

扩展回归期间曾因新测试未等待数据集异步载入、Windows offscreen字体未初始化而失败，见[测试前置条件失败记录](evidence/native-annotation-harness-failure.log)。补齐事件等待和字体后通过；该记录不作为业务保存缺陷的证据。

本轮核心流程在合成样本及真实Qt处理器自动测试范围满足编辑→保存→图库→重开→导出闭环；完整可见桌面产品验收仍未完成。人工补验：启动Release应用，在测试项目连续标注多张大图，验证鼠标悬停、两次点击/拖放、空格/中键、缩放、切类别、保存失败重试、登录/退出及关闭提示；分别在100%/150%缩放和实际触控板上操作。不要使用生产项目制造失败。

参考DLT 26.03的连续操作原则；本轮不实现AI辅助、mask裁切/拆分、多选整体变换或旋转角度拖动，不声称完整复刻。Q02异常分类/掩码、Q03矩形命名映射、Q04审计备份仍待产品决定；现有建议保留于ISSUES.md。

当前重验使用上方“当前功能核对”命令；运行普通测试时将FUSEVISION_WORKFLOW_EVIDENCE_DIR、FUSEVISION_ACCOUNT_EVIDENCE_DIR、FUSEVISION_NATIVE_EVIDENCE_DIR留空以免改写已有截图。此前native机器报告保留原样，不再运行归档验证器刷新它。

## 2026-11-17 模块整理与松耦合（历史阶段）

已实施：64个文件按core/ui/app/deeplearning/traditional迁移；提取AnnotationShape、DatasetTypes、ProjectTypes、ModelTypes，消除模型/存储反向包含业务Widget；通用授权判定与界面拒绝弹窗分开。深度学习Data目标仅依赖Core及Qt Gui，传统视觉与深度学习互不引用，视觉SDK要求由传统目标私有消费。未开发训练、预测或传统算法执行引擎；单账户规则、标注编号和磁盘格式保持兼容。关键文件及过程见[实施记录](../refactor/refactor-module-boundaries.md)、[架构](../codebase/ARCHITECTURE.md)与[路径映射](evidence/module-path-mapping.json)。

| 验证类别 | 实际执行与结果 | 边界 |
|---|---|---|
| 基线与回归识别 | 修改前CTest 2/2通过；新模块检查在旧结构失败，保留[失败记录](evidence/module-boundaries-before.log)；[工作区哈希](evidence/module-refactor-start-baseline.json)与out源文快照保留 | 不以Git HEAD代替用户已有未提交内容 |
| Release编译 | [构建日志](evidence/module-refactor-release-build.log)，退出码0；应用及拆分静态目标重新编译/链接成功 | 本轮未构建Debug或重新跑VS/MSBuild入口 |
| 自动CTest | [详细日志](evidence/module-refactor-ctest-release.log)：5/5通过；workflow-regression原15组全部PASS | Widgets使用offscreen，不等于可见Windows桌面验收 |
| 独立数据层 | QCoreApplication用例；QT_WIDGETS_LIB存在时编译失败。配置描述、undo/redo、PNG/JSON身份与几何、保存失败保持及COCO图像/类别/实例配对重读通过 | 合成小样本，无真实性能结论 |
| 依赖边界 | include白名单通过；CMake配置检查禁止的直接/接口链接 | 静态边界约束，不宣称所有运行时/单例耦合已消除 |
| LabelMe路径 | C++测试源码回退、部署优先和缺失反馈契约；Python路径测试2/2通过；应用产物实际包含适配脚本 | 未启动LabelMe/PyQt，未验证嵌入、外部保存或相机 |
| Doxygen | 本机1.18.0；PowerShell入口和CMake docs目标均成功；HTML/XML可读、10组、关键类型可定位，warnings.log为空 | EXTRACT_ALL提供完整声明索引；未要求所有历史成员注释完整，无Graphviz图 |
| 文档与保留检查 | 当前源码行号、Markdown链接、XMind9分图/图片/关联线、历史证据哈希及原始142文件快照检查通过；归一化比较覆盖迁移C++实现 | 归一化比较忽略include、注释和授权调用重命名；AnnotationShape定义提取复原比较，LabelMe路径为有意修正 |

LabelMe目录迁移时确认原硬编码源码路径将失效，因此增加统一解析及自研脚本部署；缺少适配脚本反馈后中止启动。其余C++函数实现经上述归一化比较一致。这是机械保留检查与行为回归的组合，不能单靠哈希证明所有业务意图。

当前验证命令：

```powershell
./CmakeBuild.ps1 -Configuration Release -NoRun -Test
./scripts/build-docs.ps1 -DoxygenPath D:/software/doxygen-1.18.0.windows.x64.bin/doxygen.exe
python -X utf8 docs/logic/tools/validate_analysis.py
```

若要重跑详细CTest且保留历史截图，先将`FUSEVISION_WORKFLOW_EVIDENCE_DIR`及`FUSEVISION_ACCOUNT_EVIDENCE_DIR`置空。新的验证器只输出module-refactor-validation.json，不覆盖以前的报告。JSON/XMind同步当前路径与模块分图，业务流程及历史截图保留；未用XMind桌面客户端打开。根.editorconfig和.clang-format已建立，本机未找到clang-format，未执行全库格式化。

代表性核心链“项目→模型→图库→标注→保存→返回→重开→导出”自动回归仍通过；完整产品闭环继续受可见桌面、原生对话框、外部LabelMe和实际重启未验收的限制。后续先补这些人工验证，再按明确需求提取剩余Widget编排；无性能证据前不加线程。Q02异常检测契约、Q03矩形任务名称/映射、Q04历史审计与备份仍待产品确认。账户Q01已按此前单账户要求实现。

## 2026-11-17 单账户简化（历史结果）

用户明确仅需一个账户。当前实现为启动直接只读浏览、全部页面可访问，登录后统一解锁写操作，退出登录恢复只读。多用户增删、角色选择、权限矩阵及明文记住密码已移除。新库仅superadmin/admin123；旧库优先复用superadmin，否则最高历史role/最早ID账户，保留原密码及历史记录，不进行删除或改密迁移。ISS-007按新产品范围关闭，不表示实现了任意用户名登录。

关键变更为AccessControl、PermissionGuard、SessionManager、DatabaseManager、LoginDialog、FuseVision及账户设置页。Registry仅保留页面键兼容接口，不再创建/查询/缓存模块权限。

本轮构建/测试：**Release编译成功，CTest 2/2通过，15组业务回归全部PASS。** 见[本轮构建日志](evidence/single-account-release-build.log)及[逐项CTest日志](evidence/single-account-ctest-release.log)；详细运行约2.74秒，不作为性能基准。初始工作区见[single-account-start-baseline.json](evidence/single-account-start-baseline.json)。vcpkg因本机依赖ABI变化重建82项依赖，首次配置约30分钟；业务编译成功后，新增账户用例因跳过图库的数据源绑定而失败。已保存[该测试失败记录](evidence/single-account-regressions-before-fix.log)，改为通过实际图库入口后复跑通过。该失败属于测试调用缺少前置条件，不能冒充旧认证代码的缺陷复现；本轮未执行旧认证实现的红绿对照。

新增/调整自动断言已通过：访客四主页面及九子页访问、只读通过图库打开图片标注且项目/模型/数据集配置字节保持、直接写入口拒绝、旧账户拒绝、单账户正确/错误密码与取消登录、旧禁止权限不影响登录解锁、实际主窗口退出登录取消/保存失败/恢复重试/放弃，以及自身密码修改失败与成功。使用临时工程、内存SQLite、临时INI，不操作真实账户数据库。原有标注、类别、删除、导入导出、设置和集成链也已复跑。可见Windows桌面与LabelMe仍需人工补验。

本轮截图见[单账户登录](screenshots/P00-single-account-20261117.png)、[已登录账户设置](screenshots/P12-single-account-20261117.png)、[退出后的只读状态](screenshots/P01-guest-readonly-20261117.png)。已检查实际图像，均来自Qt offscreen渲染，不代表Windows原生桌面验收。复跑当前测试及截图：

```powershell
$env:FUSEVISION_WORKFLOW_EVIDENCE_DIR=''
$env:FUSEVISION_ACCOUNT_EVIDENCE_DIR='D:/project/FuseVision/docs/logic/screenshots'
& D:/software/cmake/bin/ctest.exe --test-dir out/build/windows-x64-release --verbose
```

当前第12组为“guest denied handlers and login state”，第15组为“guest pages single account login logout and password”；下文11月15日角色授权用例说明仅代表历史结果。导图同步当前账户流程并嵌入3张新截图；归档结构校验见本轮机器记录，未用XMind客户端打开。

## 2026-11-15 核心流程阶段验证（历史结果）

该日 Release 编译成功；CTest **2/2 通过**，其中 workflow-regression 包含 **15 组业务回归**，另一项为 startup-smoke。真实 Qt Widgets 在 offscreen 平台运行，使用合成图片、临时目录、内存 SQLite 和隔离 INI。代表性样本的核心链已自动通过；可见 Windows 桌面、外部 LabelMe 和完整重启尚未验收，不能据此宣称全部产品闭环已完成。

2026-11-14 的 runtime-ui.json、validation.json、旧截图及原分析报告保留为历史证据，不能作为修复后的通过记录。当前证据使用 workflow-* 文件；旧生成器包含历史描述，不应直接运行覆盖当前文档。

## 工作区与验证基线

开始修改前记录了已有改动和 261 个非 SDK 文件的 SHA-256，见 [workflow-start-baseline.json](evidence/workflow-start-baseline.json)。工作区原来已有大量修改、未跟踪文件及根目录 README.md/Deeplearn.md 删除；本轮未恢复它们，也未 reset、提交或推送。机器校验检查初始文件保留、范围外文件哈希、引用和证据结构；它不能逐行证明修改过的同一函数中所有历史意图，兼容性以代码复核及业务回归为证。

先复现/增加能识别缺口的断言，再修正相关路径。[失败基线合集](evidence/workflow-regressions-before-fixes.log) 保留多个实施中失败快照，包括 dirty、模型通知、类别保护、描述、COCO 编号、过期原生元数据、填充覆盖和类别实例统计。合集含当时其他用例的通过记录，不表示所有问题来自同一次运行。

## 分阶段结果

| 阶段 | 修复及关键文件 | 已执行验证 | 限制 |
|---|---|---|---|
| 一：P0 标注与保存 | AnnotationModel 唯一形状和撤销源；AnnotationManagement 统一 dirty、保存/放弃/取消；AnnotationStore 校验并原子保存；修正工具合并及取消预览 | 新增、移动、删除、撤销/重做、PNG/JSON 重开；类别/坐标/掩码和原生形状一致；模拟写失败保留内容并恢复重试；空标注；外部修改不读取过期缓存 | 窗口关闭/退出登录主要静态确认；断电中断未模拟 |
| 二：P0 权限 | AccessControl、PermissionRegistry/Guard、各业务处理器和页面入口检查，动态更新控件 | 直接调用只读账号的项目/模型/数据集/类别/标注/设置写入口，拒绝后文件字节和用户数不变；授予/撤销/无 read/登出刷新；导入提交前撤权 | 用户 CRUD 全分支、顶层导航及 LabelMe 权限联动部分仅静态确认 |
| 三：P1 上下文与类别 | Project/ModelManagement 区分预览和活动对象；RecoveryStore 可恢复移动；类别删除/重排保护 | 成功新建模型通知各一次，取消零次；A 活动/B 选中、取消、失败、当前删除清下游；重命名和追加不改旧标签编号；样本删除恢复清单 | 无自动还原/保留期管理 UI；类别迁移未开发 |
| 四：P2 表单与设置 | ModelConfig 保存描述；派生路径只读；SettingsManager/SystemSettingsWidget/Logger 明确生效与失败恢复 | 中文描述落盘重开；隔离设置重读及失败重试；活动根保持、不迁移项目；取消重置；实际无 .log 日志名的过期匹配 | 确认重置后的写入、完整进程重启和真实 ACL 故障需补验；QSettings 多键无断电事务保证 |
| 五：P2 文件基准与职责 | DatasetFileService 配对复制/回滚、共同格式读取与暂存导出；AnnotationStore；DatasetManagement 质量检查和派生统计 | 同名整组跳过/覆盖前校验/失败重试；稀疏 COCO 类别映射、旋转角点、归一化、空标注、图像配对、选中范围、RLE 独立读取；坏图/非法类/孤立标签定位；同图同类两个实例统计为 2 | 有损转换明确提示；部分外部格式不支持；仍为同步执行，没有真实性能证据，未增加线程 |

阶段三发现删除后画布仍可能显示旧图，已修正 AnnotationCanvas 的空图像清理，并由集成回归覆盖。阶段五发现统计依赖旧配置/图片数，现改为从已提交标签读取可见实例：同图同类两个形状使样本实例数和类别实例数都为 2；保存成功、载入和类别检测均刷新，失败不提交成功统计。

## 自动测试与实际界面边界

[tests/workflow_regression.cpp](../../tests/workflow_regression.cpp) 使用真实 QApplication、业务 Widget、处理器、工具和 QTimer 模态框驱动，并校验磁盘内容及重读结果。15 组用例覆盖：

1. AnnotationModel dirty、撤销与重做。
2. 标注页面的矩形/选择/删除/保存重开，失败、取消、放弃及重试。
3. JSON 旋转框/多边形身份，替换失败保留原字节。
4. 类别保护、重命名/追加保持编号，以及同类多实例统计。
5. 实际创建模型对话框、通知计数、中文描述和取消。
6. COCO/YOLO 导出编号、角点、有损提示、范围、空标注与失败不提交。
7. PNG/JSON 外部修改使原生形状缓存失效。
8. 画笔、橡皮、填充、多边形及原生 round trip。
9. 成对导入跳过/失败/撤权/重试，精确删除与 restore.json 手动还原。
10. 稀疏 COCO 类别、负样本、RLE 独立解码、路径穿越拒绝和质量定位。
11. A 打开/B 选中项目与模型删除、取消、失败及当前上下文清理。
12. 直接调用受保护处理器拒绝写入及动态授权变化。
13. 隔离设置存储、失败重试、生效范围与实际日志清理选择器。
14. **集成链**：实际项目/模型卡片 → 图库 → 标注矩形 → 取消离页 → 保存返回 → 重开 → 选中 COCO 导出 → 删除样本清空画布且无法孤立保存。
15. 只读账号项目按钮状态。

这不是 15 次人工桌面操作，也不是覆盖率声明。[修复后标注截图](screenshots/P05-workflow-reopened-20261115.png) 与 [图库截图](screenshots/P04-workflow-gallery-20261115.png) 来自第 14 组的实际 offscreen Widget；历史截图仍标为历史。XMind 只检查 JSON、ZIP、图片及关联线结构，未在客户端打开。

## 复跑命令及证据

在项目根目录 PowerShell 执行：

```powershell
./CmakeBuild.ps1 -Configuration Release -NoRun -Test
```

当前构建使用 MSVC 19.50、Qt 6.10.2。结果保存在 [Release 构建日志](evidence/workflow-release-build.log)，[逐项 CTest 日志](evidence/workflow-ctest-release.log) 中 15 组均为 PASS，CTest 2/2 通过（本次详细运行约 2.12 秒；不作为性能基准）。如只复跑已构建测试并更新截图：

```powershell
$env:FUSEVISION_WORKFLOW_EVIDENCE_DIR='D:/project/FuseVision/docs/logic/screenshots'
& D:/software/cmake/bin/ctest.exe --test-dir out/build/windows-x64-release --verbose
```

测试用临时目录中的同名普通文件阻塞目标目录，可靠复现保存/设置失败，无须依赖管理员 ACL。QSettings 的用户和系统 scope 都指向临时 INI，数据库为 `:memory:`。未删除真实项目、批量转换真实标签或覆盖用户原配置。

文档更新后，仅导出已更新 JSON 并运行当前校验器：

```powershell
node C:/Users/21012/.codex/skills/ui-logic-xmind/scripts/export_xmind.mjs docs/logic/FuseVision-功能与代码.json docs/logic/FuseVision-功能与代码.xmind --force
python -X utf8 docs/logic/tools/validate_analysis.py
git diff --check
```

机器校验结果见 [workflow-validation.json](evidence/workflow-validation.json)。格式/链接校验与业务回归分列，均不能替代以下人工验收。

## 未执行、限制及补验步骤

| 未验证项 | 原因与补验 |
|---|---|
| 可见 Windows 窗口、原生文件对话框、关闭及退出登录全过程 | 本轮以 offscreen 自动测试为证；使用临时工程副本，分别选择保存/放弃/取消，制造写失败后确认仍留在页面；退出登录后检查只读状态与全部页面可访问范围 |
| 确认重置和完整重启 | 当前只验证隔离配置重读、取消重置及内存生效策略；以独立测试用户配置确认仅五路径重置，重启核对默认根及旧工程原位打开 |
| LabelMe 启动/嵌入/外部编辑/撤权退出 | 未运行第三方进程；准备兼容 Python/LabelMe 后在副本中核对外部保存、异常退出、回到本程序重开及权限变化；外部进程未保存缓冲不在本程序事务内 |
| 相机、Debug、生产数据与性能 | 无硬件/真实负载验证，本轮未执行 Debug 构建；另行建立代表性大图/大目录基准后再判断后台任务需求 |
| XMind 客户端 | 已检查归档资源、节点和关联线；仍需客户端打开确认排版和图片显示 |

已知格式/恢复边界：导出目录必须为空；无原生元数据的普通 PNG 无法恢复原始实例划分；YOLO 部分几何转换有损并提示；压缩字符串 RLE、LabelMe circle 不支持；普通目录导入不等于完整 train/val/test 分区导入。删除移动支持按恢复记录手动还原，但没有自动还原/保留期 UI。多文件配对及设置不是断电事务，进程中断后的恢复需另做故障注入。本轮未开发数据划分、训练、预测、模型导出、增强或传统视觉执行引擎。

## 待确认与下一步

2026-11-15时ISS-007未修复；2026-11-17用户已明确改为单账户，故按需求变化关闭。Q02异常分类/掩码主模式、Q03矩形命名/映射与Q04审计备份要求仍待产品选择。角色人数限制不再作为单账户界面的需求。详细推荐见 [ISSUES](../refactor/ISSUES.md)。

下一优先级为 P0 可见桌面的保存/退出登录/关闭和 LabelMe 权限补验；P1 产品决策及中断恢复验证；P2 完整分区、压缩 RLE 和真实负载基准。代表样本的自动核心链满足本轮断言；全部支持格式和桌面场景的闭环验收仍需上述补验。

## 2026-11-17 完整功能关系回归与CI/CD

依据FEATURE-RELATIONS、STATE-MACHINES及TEST-TRACEABILITY的REL-001～007和TC-001～018逐项复跑，新增TC-019覆盖主题、日志、筛选选择与类别重排失败恢复。训练/预测/模型导出等占位、退役LabelMe进程、相机和完整可见桌面不冒充已测功能。

- [Debug初始日志](evidence/full-functional-debug-baseline-20261117.log)：38/40，两项访问异常；[诊断堆栈](evidence/full-functional-debug-stack-20261117.log)定位导航样本引用失效与页面析构后撤销栈信号，修复见ISS-016。
- [最终Debug](evidence/full-functional-debug-final-20261117.log)、[JUnit](evidence/full-functional-debug-junit-20261117.xml)及[图谱门禁](evidence/full-functional-debug-traceability-20261117.json)：41/41通过，19个TC、24条执行映射。
- [最终Release](evidence/full-functional-release-final-20261117.log)、[JUnit](evidence/full-functional-release-junit-20261117.xml)及[图谱门禁](evidence/full-functional-release-traceability-20261117.json)：41/41通过。
- documentation-tools包含32个Python测试；6项CI工具测试验证缺失/跳过/断言丢失等失败门禁与打包文件白名单；actionlint 1.7.7检查两份workflow通过。
- [本地Release包验证](evidence/full-functional-package-local-20261117.log)：实际打包、临时解压、系统PATH及无外部插件路径启动成功，未访问真实账户库或改写用户设置。

[Visual Studio 2026 Debug日志](evidence/full-functional-vsdebug-20261117.log)、[JUnit](evidence/full-functional-vsdebug-junit-20261117.xml)与[门禁结果](evidence/full-functional-vsdebug-traceability-20261117.json)均通过41/41。本机Doxygen生成通过，见[日志](evidence/full-functional-doxygen-20261117.log)；[32项Python工具测试](evidence/full-functional-python-20261117.log)通过。仓库.gitattributes固定源码LF并保留历史证据原字节，避免跨机器检出改变保护摘要。

远端CI/CD运行状态在完成后追加；本地结果不能代替远端成功。完整可见Windows连续操作、原生文件对话框、高DPI/触控板、大图性能及硬件仍需人工或对应设备补验。

本轮继续发现并修复ISS-017：交付插件缺TIFF/WebP等传递DLL，五格式读写加入startup-smoke。见[编解码红阶段](evidence/full-functional-codecs-before-20261117.log)、[失败运行日志](evidence/full-functional-codecs-red-runtime-20261117.log)和[修复Release](evidence/full-functional-codecs-fixed-release-20261117.log)。新增TC-020后门禁为20个TC、25条映射。[Windows原生平台三组](evidence/full-functional-windows-platform-20261117.json)全部退出0，分别覆盖原生编辑事件、完整业务工作流及当前问题回归。

首轮远端运行44042443238的[文档失败日志](evidence/full-functional-ci-first-docs-failure-20261117.log)记录26项中4失败2错误：TEMP短路径别名与规范路径比较误判，测试夹具已改为resolve，保留原越界断言。该轮取消以免在已失败门禁下继续消耗构建；不计为通过，待后续修复提交重新执行。

补修后的最终本地证据：[Ninja Debug](evidence/full-functional-final-debug-20261117.log)、[Release](evidence/full-functional-final-release-20261117.log)、[VS2026 Debug](evidence/full-functional-final-vsdebug-20261117.log)各41/41通过；对应[Debug映射](evidence/full-functional-final-debug-traceability-20261117.json)、[Release映射](evidence/full-functional-final-release-traceability-20261117.json)、[VS映射](evidence/full-functional-final-vsdebug-traceability-20261117.json)均20个TC和25条核验。独立[Debug JUnit](evidence/full-functional-final-debug-junit-20261117.xml)、[Release JUnit](evidence/full-functional-final-release-junit-20261117.xml)、[VS JUnit](evidence/full-functional-final-vsdebug-junit-20261117.xml)保留完整断言输出。[最终本地交付包](evidence/full-functional-final-package-20261117.log)解压后五格式编解码与启动通过。此前final前的日志仅对应各自阶段。
