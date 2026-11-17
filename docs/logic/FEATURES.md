# FuseVision 功能、页面与按钮清单

最近核对：2026-11-17；范围：当前代码与内容核对，重点为导入、旋转几何、配置生效及异常分类链路。
ISS-011～014已修复，Release构建与CTest 6/6通过，见[本次修复验证](VALIDATION.md#2026-11-17-四项问题修复)。验证限于所列断言；历史原始证据保留，可见桌面验收未完成。

<!-- docflow:features -->
项目：FuseVision
记录日期：2026-11-17
范围：单账户与Qt原生连续标注；运行时LabelMe已退役，历史数据读取兼容保留。

[主流程](CORE-FLOW.md) · [数据契约](DATA-CONTRACTS.md) · [验证记录](VALIDATION.md) · [维护方式](../maintenance/README.md)

| 功能ID | 模块/页 | 功能 | 证据状态 |
|---|---|---|---|
| F01 | 登录与共享能力/P00 | [单账户登录](#f01) | 自动回归通过（2026-11-17，offscreen） + 静态复核 |
| F02 | 登录与共享能力/P01 | [模块导航与状态栏](#f02) | 自动回归通过（2026-11-17，offscreen） + 静态复核 |
| F03 | 登录与共享能力/P01 | [亮暗主题与日志面板](#f03) | 部分自动验证 + 静态复核；见边界 |
| F04 | 登录与共享能力/P01 | [登录与退出登录](#f04) | 自动回归通过（2026-11-17，offscreen） + 静态复核 |
| F05 | 深度学习/P02 | [新建项目](#f05) | 实际操作验证 + 静态代码确认 |
| F06 | 深度学习/P02 | [就地打开项目](#f06) | 部分自动验证 + 静态复核；见边界 |
| F07 | 深度学习/P02 | [项目预览与正式打开](#f07) | 自动回归（限定范围） + 静态代码复核 |
| F08 | 深度学习/P02 | [删除项目](#f08) | 自动回归（限定范围） + 静态代码复核 |
| F09 | 深度学习/P03 | [新建模型](#f09) | 自动回归（限定范围） + 静态代码复核 |
| F10 | 深度学习/P03 | [打开与登记模型](#f10) | 部分自动验证 + 静态复核；见边界 |
| F11 | 深度学习/P03 | [预览模型与进入数据集](#f11) | 自动回归（限定范围） + 静态代码复核 |
| F12 | 深度学习/P03 | [删除模型](#f12) | 自动回归（限定范围） + 静态代码复核 |
| F13 | 深度学习/P04 | [导入数据集文件](#f13) | 自动回归（限定范围） + 静态代码复核 |
| F14 | 深度学习/P04 | [导入数据集文件夹](#f14) | 自动回归（限定范围） + 静态代码复核 |
| F15 | 深度学习/P04 | [类别新增编辑删除导入](#f15) | 自动回归（限定范围） + 静态代码复核 |
| F16 | 深度学习/P04 | [拖动类别顺序](#f16) | 自动回归（限定范围） + 静态代码复核 |
| F17 | 深度学习/P04 | [浏览筛选选择与进入标注](#f17) | 实际操作验证 + 静态代码确认 |
| F18 | 深度学习/P04 | [删除样本与右键菜单](#f18) | 自动回归（限定范围） + 静态代码复核 |
| F19 | 深度学习/P04 | [刷新信息与质量提示](#f19) | 自动回归（限定范围） + 静态代码复核 |
| F20 | 深度学习/P04 | [标注格式转换](#f20) | 自动回归（限定范围） + 静态代码复核 |
| F21 | 深度学习/P04 | [导出数据集](#f21) | 自动回归（限定范围） + 静态代码复核 |
| F22 | 深度学习/P04 | [数据增强入口](#f22) | 静态代码确认 |
| F23 | 深度学习/P05 | [加载图像与标注工具](#f23) | 自动回归通过（2026-11-17，offscreen） + 静态复核 |
| F24 | 深度学习/P05 | [保存与自动保存](#f24) | 自动回归通过（2026-11-17，offscreen） + 静态复核 |
| F25 | 深度学习/P05 | [撤销重做与形状列表](#f25) | 自动回归通过（2026-11-17，offscreen） + 静态复核 |
| F26 | 深度学习/P05 | [视图调整与图像导航](#f26) | 自动回归通过（2026-11-17，offscreen） + 静态复核 |
| F27 | 深度学习/P05 | [Qt原生连续标注工作区](#f27) | 自动回归通过（2026-11-17，offscreen） + 静态复核 |
| F28 | 深度学习/P06 | [数据检查占位页](#f28) | 实际操作验证 + 静态代码确认 |
| F29 | 深度学习/P07 | [数据拆分占位页](#f29) | 实际操作验证 + 静态代码确认 |
| F30 | 深度学习/P08 | [模型训练占位页](#f30) | 实际操作验证 + 静态代码确认 |
| F31 | 深度学习/P09 | [模型预测占位页](#f31) | 实际操作验证 + 静态代码确认 |
| F32 | 深度学习/P10 | [模型导出占位页](#f32) | 实际操作验证 + 静态代码确认 |
| F33 | 传统视觉/P11 | [工具栏与三区布局](#f33) | 实际操作验证 + 静态代码确认 |
| F34 | 账户设置/P12 | [账户状态与修改密码](#f34) | 自动回归通过（2026-11-17，offscreen） + 静态复核 |
| F35 | 账户设置/P12 | [统一只读与编辑状态](#f35) | 自动回归通过（2026-11-17，offscreen） + 静态复核 |
| F36 | 系统设置/P13 | [路径与日志级别保存](#f36) | 自动回归（限定范围） + 静态代码复核 |
| F37 | 系统设置/P13 | [重置五项路径](#f37) | 部分自动验证 + 静态复核；见边界 |
| F38 | 系统设置/P13 | [清理旧日志](#f38) | 部分自动验证 + 静态复核；见边界 |

<a id="f01"></a>
## F01 单账户登录

位置：登录与共享能力 / P00
入口：登录 / 保持只读
场景：只读浏览期间登录并解除只读。
前提：应用已以访客只读启动；登录框显示唯一账户，不可选择角色。
输入与校验：输入管理账户密码；不自动填充或记住。
证据状态：自动回归通过（2026-11-17，offscreen） + 静态复核
按钮编号：无；历史坐标见截图索引

操作流程：

1. DatabaseManager::singleAccount选择唯一兼容账户，validateLogin只认证该账户。
2. 错误密码或取消保持访客；成功建立会话，所有已实现写操作统一解锁。

结果：当前上下文保留，状态栏显示可编辑。

失败/取消/边界：新库superadmin/admin123；已有库沿用原密码，旧账户保留但不参与登录。密码算法沿用SHA-256。

数据/接口/算法：SQLite唯一管理账户；成功登录清理旧QSettings记住字段，无远程认证。

验证范围：真实LoginDialog错误密码、取消与成功的offscreen回归通过；记住密码不自动填充，成功后旧字段清理。

代码：[LoginDialog::onLoginClicked](../../src/app/Login.cpp:35)；[DatabaseManager::singleAccount](../../src/core/DatabaseManager.cpp:81)；[DatabaseManager::validateLogin](../../src/core/DatabaseManager.cpp:88)。

<a id="f02"></a>
## F02 模块导航与状态栏

位置：登录与共享能力 / P01
入口：深度学习 / 传统视觉 / 账户设置 / 系统设置
场景：在一级业务模块间切换。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归通过（2026-11-17，offscreen） + 静态复核
按钮编号：B01-05 / B01-06 / B01-07 / B01-08

操作流程：

1. 访客可访问四个一级页面和九个深度学习子页，保留只读项目/模型/标签浏览。
2. 登录统一启用写操作，退出后恢复只读；页面不隐藏、不因旧权限表禁用。

结果：访客可浏览全部页面；状态栏区分只读/可编辑，打开上下文保持一致。

失败/取消/边界：可见Windows原生桌面与外部进程仍需补验。

数据/接口/算法：共享 SessionManager、PermissionRegistry、ThemeManager；无业务数据写入。

验证范围：四主页面与九子页均可访问；访客通过图库打开图片标注；项目/模型/数据集配置字节保持不变。

代码：[FuseVision::onMenuItemClicked](../../src/app/FuseVision.cpp:333)；[FuseVision::initPagesAsync](../../src/app/FuseVision.cpp:378)；[DeepLearningWidget::applyPermissions](../../src/deeplearning/widgets/DeepLearningWidget.cpp:297)。

<a id="f03"></a>
## F03 亮暗主题与日志面板

位置：登录与共享能力 / P01
入口：切换主题 / 折叠日志面板
场景：调整阅读环境并查看模块操作记录。
前提：侧边栏底部主题按钮；深度学习/传统视觉底部日志折叠按钮。
输入与校验：主题从保存值或系统检测恢复；日志最多1000文本块。
证据状态：部分自动验证 + 静态复核；见边界
按钮编号：B01-01 / B01-09

操作流程：

1. 主题会持久化偏好，未登录时主题写操作禁用；日志面板浏览不受限。
2. 主题按钮调用 ThemeManager::setTheme 并保存 ui/theme
3. themeChanged 驱动页面重新套用语义调色板和图标
4. 日志面板接收显式消息并HTML转义；可折叠、拖动分隔条

结果：窗口配色切换；日志面板显隐改变。

失败/取消/边界：日志面板不是spdlog文件全部记录的实时镜像；只显示接入的业务消息。登录后主题保存仍为静态复核。

数据/接口/算法：QSettings ui/theme；spdlog；LogMonitor 文本缓存；Windows DWM。

验证范围：自动验证访客直接调用主题处理器被拒绝，当前主题不变；未执行登录后的主题持久化操作。

代码：[FuseVision::onToggleTheme](../../src/app/FuseVision.cpp:189)；[ThemeManager::setTheme](../../src/ui/ThemeManager.cpp:154)；[LogMonitor::log](../../src/ui/LogMonitor.cpp:113)。

<a id="f04"></a>
## F04 登录与退出登录

位置：登录与共享能力 / P01
入口：登录，解除只读 / 退出登录，恢复只读
场景：无需重启切换只读与可编辑状态。
前提：应用已启动；无需已有项目即可登录或退出登录。
输入与校验：唯一管理账户密码，或未保存内容的保存/放弃/取消选择。
证据状态：自动回归通过（2026-11-17，offscreen） + 静态复核
按钮编号：B01-10

操作流程：

1. 未登录时打开单账户密码框；取消或失败保持只读。
2. 已登录时先完成未保存标注的保存/放弃/取消；保存失败或取消阻止退出，否则恢复只读。

结果：登录解除只读，退出恢复只读；取消或失败保持原状态和上下文。

失败/取消/边界：主窗口处理器在offscreen验证；可见Windows原生桌面仍待补验。

数据/接口/算法：同一进程的 SessionManager；原项目/数据集上下文继续存在。

验证范围：实际主窗口登录/退出处理器的取消、保存失败、恢复重试及放弃通过；会话和页面状态一致。

代码：[FuseVision::onSwitchUser](../../src/app/FuseVision.cpp:199)；[SessionManager::login](../../src/core/SessionManager.cpp:6)；[SessionManager::logout](../../src/core/SessionManager.cpp:13)。

<a id="f05"></a>
## F05 新建项目

位置：深度学习 / P02
入口：新建项目 / OK / Cancel
场景：建立一个视觉任务的容器。
前提：深度学习→项目管理→新建项目；必须已登录，按钮与处理器同时校验。
输入与校验：项目名称必填，trim后 ^[A-Za-z0-9_]{1,64}$，拒绝Windows设备名CON/PRN/AUX/NUL/COM1～9/LPT1～9；描述可空；根目录从设置读取；默认名称为空。
证据状态：实际操作验证 + 静态代码确认
按钮编号：B02D-01 / B02D-02 / B02-02

操作流程：

1. 显示项目名/描述对话框
2. ProjectConfig::createProject 校验名称和目录冲突
3. 建立项目目录并写 project.fvproj
4. 刷新项目卡片；尚未自动打开项目

结果：项目列表出现新卡片，用户继续双击打开。

失败/取消/边界：取消对话框不创建；非法名、同名目录、创建/写入失败显示创建失败。文件创建不是跨文件事务。

数据/接口/算法：<project_data>/<项目名>/project.fvproj；原生JSON写document_type=fusevision.project、format_version=字符串1.0及相对根，具体见DATA-CONTRACTS。

验证范围：实际点击新建项目，在临时目录创建 UI_Logic_Demo，截图P02；原业务目录未改动。

代码：[ProjectManagement::onNewProject](../../src/deeplearning/widgets/ProjectManagement.cpp:537)；[ProjectConfig::createProject](../../src/deeplearning/storage/ProjectConfig.cpp:19)。

<a id="f06"></a>
## F06 就地打开项目

位置：深度学习 / P02
入口：打开项目
场景：恢复本地项目或接收其他路径的工程。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：部分自动验证 + 静态复核；见边界
按钮编号：B02-03

操作流程：

1. 按明确选中的项目文件就地打开；项目根取文件所在目录，合规相对模型引用在内存解析，拒绝旧绝对根引用；不自动迁移或选择同名目标。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：配置加载和项目卡片链路自动验证；原生文件选择器未手动操作。 支持范围与产品Q02–Q04见ISSUES.md。

数据/接口/算法：.fvproj、models.<id>.model_file；相对引用在内存解析为绝对路径；原生格式标记与必填字段必须完整，旧绝对根及无标记配置拒绝，不在打开时重写。

验证范围：配置加载和项目卡片链路自动验证；本轮GTest覆盖移动重开、旧根引用拒绝、版本/类型/字段拒绝与读取字节保持；原生文件选择器未手动操作。

代码：[ProjectManagement::onOpenProject](../../src/deeplearning/widgets/ProjectManagement.cpp:607)；[ProjectConfig::loadProject](../../src/deeplearning/storage/ProjectConfig.cpp:62)。

<a id="f07"></a>
## F07 项目预览与正式打开

位置：深度学习 / P02
入口：项目卡片单击 / 项目卡片双击
场景：先查看项目摘要，再进入模型列表。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归（限定范围） + 静态代码复核
按钮编号：无；历史坐标见截图索引

操作流程：

1. 单击预览与正式活动项目分离；双击先处理未保存，再通知模型/数据集清理并绑定新项目。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：集成Widget项目→模型→数据集链路通过；A打开B选择删除通过。 支持范围与产品Q02–Q04见ISSUES.md。

数据/接口/算法：项目JSON；内存 m_previewProject/m_project；Qt signals。

验证范围：集成Widget项目→模型→数据集链路通过；A打开B选择删除通过。

代码：[ProjectManagement::onCardClicked](../../src/deeplearning/widgets/ProjectManagement.cpp:689)；[ProjectManagement::onCardDoubleClicked](../../src/deeplearning/widgets/ProjectManagement.cpp:732)；[DeepLearningWidget::onProjectOpened](../../src/deeplearning/widgets/DeepLearningWidget.cpp:326)。

<a id="f08"></a>
## F08 删除项目

位置：深度学习 / P02
入口：删除项目
场景：清理项目记录或整个项目目录。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归（限定范围） + 静态代码复核
按钮编号：B02-04

操作流程：

1. 确认选中卡片名称及完整路径；当前项目先处理标注；移到恢复目录后才清活动上下文。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：A活动/B选中、取消、当前项目保存决策阻断和成功通知通过；未删除真实项目。 支持范围与产品Q02–Q04见ISSUES.md。

数据/接口/算法：项目目录或 .fvproj；其他页持有的项目副本。

验证范围：A活动/B选中、取消、当前项目保存决策阻断和成功通知通过；未删除真实项目。

代码：[ProjectManagement::onDeleteProject](../../src/deeplearning/widgets/ProjectManagement.cpp:654)；[&ProjectManagement::projectClosed](../../src/deeplearning/widgets/DeepLearningWidget.cpp:165)。

<a id="f09"></a>
## F09 新建模型

位置：深度学习 / P03
入口：新建模型 / 创建模型 / 浏览 / 实例分割 / 异常检测 / 物体检测(非自由矩形) / 物体检测(自由矩形)
场景：为项目选择任务类型并建立数据工作区。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归（限定范围） + 静态代码复核
按钮编号：B03D-01 / B03D-02 / B03D-03 / B03D-04 / B03D-05 / B03D-06 / B03-02

操作流程：

1. 名称/类型校验，描述持久化；派生路径只读；成功通知各一次，取消无成功通知。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：模型对话框驱动、中文描述重开、通知及取消计数通过。 支持范围与产品Q02–Q04见ISSUES.md。

数据/接口/算法：.fvdl写fusevision.model/1.0；实例/检测创建native平铺images/labels及相对YAML工作索引，分类创建train/val/test下OK/NG六目录；新建同时写完整带模式的数据集配置；YOLO训练分区由导出生成。

验证范围：模型对话框驱动、中文描述重开、通知及取消计数通过；新增GTest覆盖两类新建目录、UTF-8/LF YAML及设备名拒绝。

代码：[ModelManagement::onNewModel](../../src/deeplearning/widgets/ModelManagement.cpp:462)；[ModelConfig::createModel](../../src/deeplearning/storage/ModelConfig.cpp:22)。

<a id="f10"></a>
## F10 打开与登记模型

位置：深度学习 / P03
入口：打开模型
场景：将现有.fvdl加入当前项目。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：部分自动验证 + 静态复核；见边界
按钮编号：B03-03

操作流程：

1. 已登记模型按明确文件读取；外部模型复制到暂存目录，拒绝同名目标，配置登记失败回撤，不提前切换。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：当前模型加载与卡片激活自动验证；外部文件选择/复制失败路径主要静态复核。 支持范围与产品Q02–Q04见ISSUES.md。

数据/接口/算法：.fvdl、项目JSON、模型目录。

验证范围：当前模型加载与卡片激活自动验证；GTest覆盖无标记配置及旧yolo拒绝、未知版本/类型、非法任务和字段拒绝覆盖；外部文件选择/复制失败路径主要静态复核。

代码：[ModelManagement::onOpenModel](../../src/deeplearning/widgets/ModelManagement.cpp:673)；[ModelConfig::loadModel](../../src/deeplearning/storage/ModelConfig.cpp:179)。

<a id="f11"></a>
## F11 预览模型与进入数据集

位置：深度学习 / P03
入口：模型卡片单击 / 模型卡片双击
场景：决定当前标注和导出的模型上下文。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归（限定范围） + 静态代码复核
按钮编号：无；历史坐标见截图索引

操作流程：

1. 单击只预览；双击激活并发送currentModelChanged，数据集只订阅该信号，避免重复绑定。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：集成项目/模型/图库及当前模型删除清理通过。 支持范围与产品Q02–Q04见ISSUES.md。

数据/接口/算法：FvdlInfo、DatasetManagement当前模型、Qt signals。

验证范围：集成项目/模型/图库及当前模型删除清理通过。

代码：[ModelManagement::onCardClicked](../../src/deeplearning/widgets/ModelManagement.cpp:800)；[ModelManagement::onCardDoubleClicked](../../src/deeplearning/widgets/ModelManagement.cpp:830)；[DatasetManagement::setCurrentModel](../../src/deeplearning/widgets/DatasetManagement.cpp:122)。

<a id="f12"></a>
## F12 删除模型

位置：深度学习 / P03
入口：删除模型
场景：清理不再使用的模型记录或目录。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归（限定范围） + 静态代码复核
按钮编号：B03-04

操作流程：

1. 删除明确选中模型，校验路径及权限；可恢复移动，项目登记失败尝试回滚；仅当前删除清空数据集/标注。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：A活动/B选中、取消、恢复目录阻塞失败、当前对象成功清理均通过。 支持范围与产品Q02–Q04见ISSUES.md。

数据/接口/算法：模型目录/.fvdl及.fvproj索引。

验证范围：A活动/B选中、取消、恢复目录阻塞失败、当前对象成功清理均通过。

代码：[ModelManagement::onDeleteModel](../../src/deeplearning/widgets/ModelManagement.cpp:761)。

<a id="f13"></a>
## F13 导入数据集文件

位置：深度学习 / P04
入口：导入数据集文件
场景：将原始图像和可选标签纳入模型工作区。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归（限定范围） + 静态代码复核
按钮编号：B04-02 / B04-09

操作流程：

1. 图像与同基名标签按组校验/暂存/提交；同名成对覆盖或整体跳过，失败回滚并按路径报告/重试。COCO先映射外部类别ID。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：回归覆盖成对跳过、语义非法JSON失败且旧配对字节保持、提交前撤权、修复重试与稀疏COCO/空标注；有效新类别和二值掩码可重开。分类导入先选OK/NG及划分，解码校验后复制并跳过同名目标。原生文件选择器未人工补验。

数据/接口/算法：<项目>/<模型>/images、labels、dataset_config.json；QImageReader和JSON解析。

验证范围：成对跳过、损坏标签失败、提交前撤权、修复重试、稀疏COCO/空标注通过；原生文件选择器未手动补验。

代码：[DatasetManagement::onImportImages](../../src/deeplearning/widgets/DatasetManagement.cpp:2185)；[DatasetManagement::ensureLabelTypeBeforeImport](../../src/deeplearning/widgets/DatasetManagement.cpp:1898)；[DatasetManagement::detectLabelsFromAnnotations](../../src/deeplearning/widgets/DatasetManagement.cpp:1930)。

<a id="f14"></a>
## F14 导入数据集文件夹

位置：深度学习 / P04
入口：导入数据集文件夹
场景：批量接收已整理数据集。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归（限定范围） + 静态代码复核
按钮编号：B04-03 / B04-10

操作流程：

1. 继续收集直属images/和labels/文件，共用配对导入；取消不提交，不自动递归导入分区。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：普通images/labels直属文件复用语义校验及配对传输；分类文件夹读取train/val/test下OK/NG直属文件并报告失败。文件夹对话框与全部图像格式未逐一人工实测。

数据/接口/算法：images/*、labels/*；文件名用于配对。

验证范围：共享文件服务自动通过；文件夹对话框与全部图像格式未逐一实测。

代码：[DatasetManagement::onImportFolder](../../src/deeplearning/widgets/DatasetManagement.cpp:2250)。

<a id="f15"></a>
## F15 类别新增编辑删除导入

位置：深度学习 / P04
入口：+ 添加 / 编辑 / 删除 / 导入
场景：定义当前数据集的类别名称和颜色。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归（限定范围） + 静态代码复核
按钮编号：B04-11 / B04-12 / B04-13 / B04-14

操作流程：

1. 新增追加，重命名不改编号；有标签或未保存编辑时拒绝删除。失败恢复类别内存；COCO类别按名称映射。

结果：分类模式类别固定为OK、NG，类别修改按钮及直接处理器均禁用。 仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：持久化标签下删除拒绝、重命名/追加后标签字节及重开语义保持通过。 支持范围与产品Q02–Q04见ISSUES.md。

数据/接口/算法：dataset_config.json保存fusevision.dataset/1.0与categories；保留未知根字段，拒绝覆盖损坏或不支持版本；类别槽位与标签像素/JSON ID关联。

验证范围：持久化标签下删除拒绝、重命名/追加后标签字节及重开语义保持通过；datasetStorage覆盖合规配置只读、扩展字段保留、旧配置/缺失配置/无效配置拒写和修正后重试。

代码：[DatasetManagement::onAddLabel](../../src/deeplearning/widgets/DatasetManagement.cpp:2610)；[DatasetManagement::onEditLabel](../../src/deeplearning/widgets/DatasetManagement.cpp:2642)；[DatasetManagement::onDeleteLabel](../../src/deeplearning/widgets/DatasetManagement.cpp:2675)；[DatasetManagement::onImportLabels](../../src/deeplearning/widgets/DatasetManagement.cpp:2706)。

<a id="f16"></a>
## F16 拖动类别顺序

位置：深度学习 / P04
入口：标签列表拖动
场景：调整类别显示及编号顺序。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归（限定范围） + 静态代码复核
按钮编号：无；历史坐标见截图索引

操作流程：

1. 有持久化标注（递归检查）或未保存内容时禁止重排并恢复列表；空数据才可重排，保存失败回滚。

结果：分类类别顺序固定，禁止拖动重排。 仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：已有标签下重排被拒绝并恢复；空目录重排保存失败尚未单独注入。 支持范围与产品Q02–Q04见ISSUES.md。

数据/接口/算法：内存样本类别与dataset_config；磁盘标注未跟随重写。

验证范围：已有标签下重排被拒绝并恢复；空目录重排保存失败尚未单独注入。

代码：[DatasetManagement::onLabelOrderChanged](../../src/deeplearning/widgets/DatasetManagement.cpp:2755)；[DatasetManagement::saveDatasetConfig](../../src/deeplearning/widgets/DatasetManagement.cpp:367)。

<a id="f17"></a>
## F17 浏览筛选选择与进入标注

位置：深度学习 / P04
入口：列表视图 / 网格视图 / 标签筛选 / 全选 / 缩略图双击
场景：查找待标注样本并批量选择。
前提：数据集右侧图库/列表、标签筛选、全选与缩略图。
输入与校验：当前模型样本；类别筛选按内存categoryIds；无额外文本参数。
证据状态：实际操作验证 + 静态代码确认
按钮编号：B04-04

操作流程：

1. 视图/筛选只改变呈现
2. 选中项进入m_selectedIndices供删除和导出使用
3. 区域标注模式双击发openAnnotationRequested(modelId,path)，编排器打开标注。
4. 分类模式双击只读预览图像与目录类别，不进入标注。

结果：分类模式展示目录类别及数量并只读预览；区域标注模式在标注页显示图像、类别及已有形状。

失败/取消/边界：空图库无缩略图；模型切换清选择。不要把当前筛选范围自动等同导出范围，导出未选时取全部样本。

数据/接口/算法：样本列表、选中索引、AnnotationManagement。

验证范围：实际调用缩略图双击处理槽并进入标注，确认画布hasImage；其他筛选分支静态。

代码：[DatasetManagement::refreshGallery](../../src/deeplearning/widgets/DatasetManagement.cpp:1277)；[DatasetManagement::onThumbnailDoubleClicked](../../src/deeplearning/widgets/DatasetManagement.cpp:2488)；[DatasetManagement::onSelectAll](../../src/deeplearning/widgets/DatasetManagement.cpp:2520)。

<a id="f18"></a>
## F18 删除样本与右键菜单

位置：深度学习 / P04
入口：删除选中 / 打开所在目录 / 复制路径 / 重新标注
场景：维护图库或定位源文件。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归（限定范围） + 静态代码复核
按钮编号：B04-05

操作流程：

1. 两个入口共用处理器；精确配对PNG/JSON/txt/XML，不按前缀误删；移动恢复目录，单组失败回滚，成功清旧画布。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：精确配对、邻名保留、restore.json人工还原、拒绝不写、编排器删除后清画布通过。 支持范围与产品Q02–Q04见ISSUES.md。

数据/接口/算法：images及同名labels；QDesktopServices、剪贴板。

验证范围：精确配对、邻名保留、restore.json人工还原、拒绝不写、编排器删除后清画布通过。

代码：[DatasetManagement::onDeleteSelected](../../src/deeplearning/widgets/DatasetManagement.cpp:2537)；[DatasetManagement::showContextMenu](../../src/deeplearning/widgets/DatasetManagement.cpp:1723)。

<a id="f19"></a>
## F19 刷新信息与质量提示

位置：深度学习 / P04
入口：刷新 / 打开文件夹 / 路径点击复制
场景：外部编辑标签后更新统计并查找缺失文件。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归（限定范围） + 静态代码复核
按钮编号：无；历史坐标见截图索引

操作流程：

1. 从已提交标签的可见形状重算样本和类别实例数；检查未标注坏图、缺失配对、孤立标签、非法类别和格式错误的完整路径。

结果：分类每图按目录类别计1，不要求配对mask；坏图仍报告。 仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：坏图、孤立标签、非法类别可定位通过；未扩展为独立数据检查执行引擎。 支持范围与产品Q02–Q04见ISSUES.md。

数据/接口/算法：QImageReader、labels/*json/png；样本统计缓存。

验证范围：坏图、孤立标签、非法类别可定位通过；同图同类两个形状的样本和类别实例数均为2；未扩展独立检查引擎。

代码：[DatasetManagement::onRefreshImageInfo](../../src/deeplearning/widgets/DatasetManagement.cpp:2819)；[DatasetManagement::computeStats](../../src/deeplearning/widgets/DatasetManagement.cpp:3664)；[DatasetManagement::runQualityCheck](../../src/deeplearning/widgets/DatasetManagement.cpp:3716)；[DatasetManagement::refreshAnnotationStatistics](../../src/deeplearning/widgets/DatasetManagement.cpp:3687)。

<a id="f20"></a>
## F20 标注格式转换

位置：深度学习 / P04
入口：格式转换
场景：将已有标注变成当前任务需要的YOLO文本。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归（限定范围） + 静态代码复核
按钮编号：B04-06

操作流程：

1. PNG/JSON/YOLO使用统一读取与编码；需要真实配对尺寸；相同txt跳过，覆盖不同内容需确认，格式损失报告。
2. 分类模式入口选择OK/NG与train/val/test后导入图像，不生成YOLO框标签。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：共享YOLO编码器验证类别/归一化/OBB角点及有损警告；classification模式入口提供OK/NG导入，图库、统计与分类导出已接通。原生模式必须符合新标准，异常分类拒绝旧二值mask回退；外部PNG标签读取保留；全部转换对话框仍需人工补验。

数据/接口/算法：detectInputFormat、validateCurrentFormat及三种转换器；data.yaml任务类型。

验证范围：共享YOLO编码器验证类别/归一化/OBB角点及有损警告；所有转换对话框和分区实际运行仍需补验。

代码：[DatasetManagement::onFormatConvert](../../src/deeplearning/widgets/DatasetManagement.cpp:2890)；[DatasetManagement::validateCurrentFormat](../../src/deeplearning/widgets/DatasetManagement.cpp:2984)；[DatasetManagement::convertPngMaskToTarget](../../src/deeplearning/widgets/DatasetManagement.cpp:3334)；[DatasetManagement::convertAnomalyClassification](../../src/deeplearning/widgets/DatasetManagement.cpp:3346)。

<a id="f21"></a>
## F21 导出数据集

位置：深度学习 / P04
入口：导出
场景：将当前数据交给其他标注/训练工具。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归（限定范围） + 静态代码复核
按钮编号：B04-08

操作流程：

1. 有选中时仅所选，否则全体；空目录内暂存整批后改名提交。COCO带图像/一致类别/RLE，YOLO按既有task，PNG保留原文件。

结果：classification导出保留phase/class目录与data.yaml(task=classify)，同名图像可位于不同类别/划分；选中导出与整批暂存提交同样适用。 仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：独立目录重读、类别引用/图像/坐标/空标注/选中范围/失败不提交通过；掩码到YOLO等有损，不承诺无损。 支持范围与产品Q02–Q04见ISSUES.md。

数据/接口/算法：writeMaskFormat、writeCocoJson、writeYoloFormat。

验证范围：独立目录重读、类别引用/图像/坐标/空标注/选中范围/失败不提交通过；掩码到YOLO等有损，不承诺无损。

代码：[DatasetManagement::onExportDataset](../../src/deeplearning/widgets/DatasetManagement.cpp:3480)；[DatasetManagement::writeCocoJson](../../src/deeplearning/widgets/DatasetManagement.cpp:3551)；[DatasetManagement::writeYoloFormat](../../src/deeplearning/widgets/DatasetManagement.cpp:3563)；[DatasetManagement::writeMaskFormat](../../src/deeplearning/widgets/DatasetManagement.cpp:3575)。

<a id="f22"></a>
## F22 数据增强入口

位置：深度学习 / P04
入口：数据增强
场景：界面展示未来增强能力入口。
前提：数据集底部数据增强。
输入与校验：无可执行增强参数。
证据状态：静态代码确认
按钮编号：B04-07

操作流程：

1. 点击后仅显示后续版本提示

结果：显示说明弹窗。

失败/取消/边界：未执行旋转/翻转/噪声等实际增强，不产生扩增样本。

数据/接口/算法：无增强算法调用。

验证范围：未执行该操作；以下为静态代码确认。

代码：[DatasetManagement::onAugmentClicked](../../src/deeplearning/widgets/DatasetManagement.cpp:3463)。

<a id="f23"></a>
## F23 加载图像与标注工具

位置：深度学习 / P05
入口：画笔 / 橡皮 / 多边形 / 矩形 / 填充 / 选择
场景：在图像上创建和编辑区域。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归通过（2026-11-17，offscreen） + 静态复核
按钮编号：B05-12 / B05-13 / B05-14 / B05-15 / B05-16 / B05-17

操作流程：

1. AnnotationModel 为唯一权威状态；矩形拖放/两次点击/反向 Enter 完成，多边形连续边链和顶点预览，工具与类别保持。
2. 画笔轮廓跟随光标；每次拖动一个撤销；Esc 或拖动失焦恢复预览前状态。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：可见Windows、高DPI、触控板和人工连续标注手感未验；没有AI辅助。

数据/接口/算法：AnnotationCanvas、AnnotationModel、AnnotationRender和Tools；QImage/QPainter，无模型推理。

验证范围：真实画布事件覆盖平移/焦点/缩放；六工具和保存重开保留原回归。

代码：[AnnotationManagement::openImage](../../src/deeplearning/widgets/AnnotationManagement.cpp:107)；[AnnotationManagement::onCanvasMousePressed](../../src/deeplearning/widgets/AnnotationManagement.cpp:211)；[RectTool::endRect](../../src/deeplearning/tools/RectTool.cpp:66)；[AnnotationManagement::renderOverlay](../../src/deeplearning/widgets/AnnotationManagement.cpp:1375)。

<a id="f24"></a>
## F24 保存与自动保存

位置：深度学习 / P05
入口：保存标注 / Ctrl+S / 保存并下一张 / Ctrl+Enter
场景：保留本张标注并更新数据集统计。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归通过（2026-11-17，offscreen） + 静态复核
按钮编号：B05-03

操作流程：

1. AnnotationStore 校验并用 QSaveFile 提交；成功才清修改标记并刷新统计。
2. 保存并下一张只在保存成功后前进；失败保留编辑/当前图片，恢复后可重试；最后一张保存后停留。
3. 未完成矩形/多边形阻止保存，不静默放弃；离开提示暂停自动保存。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：30秒自动保存实际等待、断电恢复和真实数据负载未验。

数据/接口/算法：labels/<basename>.png/json；dataset_config统计；AnnotationRender::compileMask。

验证范围：失败保留文件字节和编辑、恢复后快捷键前进、末图停留、PNG重开形状身份通过。

代码：[AnnotationManagement::saveAnnotations](../../src/deeplearning/widgets/AnnotationManagement.cpp:1422)；[AnnotationManagement::onSave](../../src/deeplearning/widgets/AnnotationManagement.cpp:504)；[DatasetManagement::currentLabelOffset](../../src/deeplearning/widgets/DatasetManagement.cpp:219)。

<a id="f25"></a>
## F25 撤销重做与形状列表

位置：深度学习 / P05
入口：撤销 / 重做 / 删除选中 / 标注列表右键 / Ctrl+C / Ctrl+V / Ctrl+D
场景：纠正标注或复制形状。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归通过（2026-11-17，offscreen） + 静态复核
按钮编号：B05-02 / B05-04 / B05-05

操作流程：

1. 按 UUID 复用列表项并保持选择；画布命中和列表删除作用于同一对象。
2. 拖动中不重建列表；工具不再重复请求全图渲染，选中控制点独立显示并按屏幕像素缩放。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：列表支持多选删除；旋转框移动/复制同步显式中心，角点屏幕位移、撤销/重做、保存重开回归通过。多选整体变换和旋转角度拖动未实现。

数据/接口/算法：唯一 AnnotationModel、QUndoStack 和 UUID 对应的界面选择。

验证范围：列表对象与选择保持、画布选中删除、一次拖动一个撤销、失焦取消和角点缩小通过。

代码：[AnnotationManagement::updateShapeList](../../src/deeplearning/widgets/AnnotationManagement.cpp:1166)；[AnnotationManagement::onUndo](../../src/deeplearning/widgets/AnnotationManagement.cpp:510)；[AnnotationManagement::onDeleteSelectedShapes](../../src/deeplearning/widgets/AnnotationManagement.cpp:722)；[AnnotationModel::undo](../../src/deeplearning/model/AnnotationModel.cpp:341)。

<a id="f26"></a>
## F26 视图调整与图像导航

位置：深度学习 / P05
入口：适应窗口 / 原始大小 / 放大 / 缩小 / 第一张 / 上一张 / 下一张 / 最后一张 / 返回数据集管理 / 保持视野 / 标注可见
场景：检查细节并连续标注。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归通过（2026-11-17，offscreen） + 静态复核
按钮编号：B05-06 / B05-07 / B05-08 / B05-09 / B05-11 / B05-18 / B05-19 / B05-20 / B05-21

操作流程：

1. 中键或空格+左拖平移，两种松开顺序均正确结束；滚轮以鼠标为锚点，零滚动量不缩放。
2. 同模型切图默认保持缩放及相对图像中心，可关闭；亮度/对比度仅改变底图显示，不重置视野。
3. H切换叠加层检查原图，不改标注visible或修改状态。
4. 切图/返回/切项目模型/退出共用保存、放弃、取消；保存失败阻止离开。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：视野和显示偏好仅本次会话；跨模型/清空上下文适应窗口。

数据/接口/算法：m_sourceImage/m_displayImage；样本列表；标签目录。

验证范围：平移结束、零滚轮/锚点缩放、亮度视野保持、保存切图保持和显示不改数据通过。Debug/Release补验连续前后导航，刷新列表时目标路径保持有效，见[ISS-016](../refactor/ISSUES.md#iss-016)。

代码：[AnnotationManagement::onNavigateTo](../../src/deeplearning/widgets/AnnotationManagement.cpp:820)；[AnnotationManagement::onReturnToGallery](../../src/deeplearning/widgets/AnnotationManagement.cpp:599)；[AnnotationManagement::applyImageAdjustments](../../src/deeplearning/widgets/AnnotationManagement.cpp:914)。

<a id="f27"></a>
## F27 Qt原生连续标注工作区

位置：深度学习 / P05
入口：保存并下一张 / 保持视野 / 标注可见
场景：在同一Qt工作区内连续完成编辑和复核。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：当前图像、类别、原生工具和会话内视野偏好。
证据状态：自动回归通过（2026-11-17，offscreen） + 静态复核
按钮编号：B05-22 / B05-23 / B05-24

操作流程：

1. 原生工作区直接连接 AnnotationModel 和 AnnotationStore；无需Python、QProcess或外部窗口。
2. Ctrl+Enter保存并下一张，H查看原图，B/E/P/R/F/S或V切工具，[ ]调整笔刷；这些快捷键在类别或形状列表焦点下仍有效。
3. LabelMe运行入口、环境扫描、适配脚本和部署退役；已有LabelMe rectangle/polygon JSON仍可导入。

结果：完成一张后连续处理下一张；失败留在当前图片，可修复后重试。

失败/取消/边界：参考DLT 26.03交互原则；没有本机参考软件对照，不宣称完整复刻。AI辅助、mask裁切拆分、多选变换和旋转角度拖动未实现。

数据/接口/算法：Qt Widgets/QGraphicsView、权威AnnotationModel与原生PNG/JSON；历史labelmePythonPath配置保留但不再使用。

验证范围：Debug/Release补验持有撤销记录的页面销毁，销毁前停自动保存并断开模型更新信号。原生入口无外部launch槽断言、画布事件、失败重试与重开、访客拒绝和offscreen截图；可见桌面未验。

代码：[AnnotationManagement::onSaveAndNext](../../src/deeplearning/widgets/AnnotationManagement.cpp:585)；[AnnotationManagement::onCanvasKeyPressed](../../src/deeplearning/widgets/AnnotationManagement.cpp:219)；[AnnotationCanvas::wheelEvent](../../src/deeplearning/widgets/AnnotationCanvas.cpp:144)；[DeepLearningWidget::initUI](../../src/deeplearning/widgets/DeepLearningWidget.cpp:61)。

<a id="f28"></a>
## F28 数据检查占位页

位置：深度学习 / P06
入口：数据检查
场景：了解当前版本的模块边界。
前提：深度学习顶部同名标签；需该权限点canRead。
输入与校验：无参数表单、无执行按钮。
证据状态：实际操作验证 + 静态代码确认
按钮编号：无；历史坐标见截图索引

操作流程：

1. 点击标签切换至占位Widget

结果：显示待开发说明。

失败/取消/边界：没有已接通的检查/拆分/训练/预测/模型导出流程。数据集页面有局部质量统计及数据导出，不代表这些独立页已完成。

数据/接口/算法：页面Widget与权限键；无训练器/推理器调用。

验证范围：实际逐页显示并截图，确认占位状态。

代码：[DeepLearningWidget::initUI](../../src/deeplearning/widgets/DeepLearningWidget.cpp:61)。

<a id="f29"></a>
## F29 数据拆分占位页

位置：深度学习 / P07
入口：数据拆分
场景：了解当前版本的模块边界。
前提：深度学习顶部同名标签；需该权限点canRead。
输入与校验：无参数表单、无执行按钮。
证据状态：实际操作验证 + 静态代码确认
按钮编号：无；历史坐标见截图索引

操作流程：

1. 点击标签切换至占位Widget

结果：显示待开发说明。

失败/取消/边界：没有已接通的检查/拆分/训练/预测/模型导出流程。数据集页面有局部质量统计及数据导出，不代表这些独立页已完成。

数据/接口/算法：页面Widget与权限键；无训练器/推理器调用。

验证范围：实际逐页显示并截图，确认占位状态。

代码：[DeepLearningWidget::initUI](../../src/deeplearning/widgets/DeepLearningWidget.cpp:61)。

<a id="f30"></a>
## F30 模型训练占位页

位置：深度学习 / P08
入口：模型训练
场景：了解当前版本的模块边界。
前提：深度学习顶部同名标签；需该权限点canRead。
输入与校验：无参数表单、无执行按钮。
证据状态：实际操作验证 + 静态代码确认
按钮编号：无；历史坐标见截图索引

操作流程：

1. 点击标签切换至占位Widget

结果：显示待开发说明。

失败/取消/边界：没有已接通的检查/拆分/训练/预测/模型导出流程。数据集页面有局部质量统计及数据导出，不代表这些独立页已完成。

数据/接口/算法：页面Widget与权限键；无训练器/推理器调用。

验证范围：实际逐页显示并截图，确认占位状态。

代码：[DeepLearningWidget::initUI](../../src/deeplearning/widgets/DeepLearningWidget.cpp:61)。

<a id="f31"></a>
## F31 模型预测占位页

位置：深度学习 / P09
入口：模型预测
场景：了解当前版本的模块边界。
前提：深度学习顶部同名标签；需该权限点canRead。
输入与校验：无参数表单、无执行按钮。
证据状态：实际操作验证 + 静态代码确认
按钮编号：无；历史坐标见截图索引

操作流程：

1. 点击标签切换至占位Widget

结果：显示待开发说明。

失败/取消/边界：没有已接通的检查/拆分/训练/预测/模型导出流程。数据集页面有局部质量统计及数据导出，不代表这些独立页已完成。

数据/接口/算法：页面Widget与权限键；无训练器/推理器调用。

验证范围：实际逐页显示并截图，确认占位状态。

代码：[DeepLearningWidget::initUI](../../src/deeplearning/widgets/DeepLearningWidget.cpp:61)。

<a id="f32"></a>
## F32 模型导出占位页

位置：深度学习 / P10
入口：模型导出
场景：了解当前版本的模块边界。
前提：深度学习顶部同名标签；需该权限点canRead。
输入与校验：无参数表单、无执行按钮。
证据状态：实际操作验证 + 静态代码确认
按钮编号：无；历史坐标见截图索引

操作流程：

1. 点击标签切换至占位Widget

结果：显示待开发说明。

失败/取消/边界：没有已接通的检查/拆分/训练/预测/模型导出流程。数据集页面有局部质量统计及数据导出，不代表这些独立页已完成。

数据/接口/算法：页面Widget与权限键；无训练器/推理器调用。

验证范围：实际逐页显示并截图，确认占位状态。

代码：[DeepLearningWidget::initUI](../../src/deeplearning/widgets/DeepLearningWidget.cpp:61)。

<a id="f33"></a>
## F33 工具栏与三区布局

位置：传统视觉 / P11
入口：流程文件 / 相机配置 / 相机标定 / 相机建模 / 通讯设置 / 系统设置 / 采集 / 预处理 / 定位 / 测量 / 识别
场景：查看传统视觉规划入口。
前提：侧边栏传统视觉；工具栏按对应读写权限控制enabled。
输入与校验：无实际算法参数；资源树显示示例流程和未连接相机。
证据状态：实际操作验证 + 静态代码确认
按钮编号：B11-01 / B11-02 / B11-03 / B11-04 / B11-05 / B11-06 / B11-08 / B11-09 / B11-10 / B11-11 / B11-12 / B11-16

操作流程：

1. 构造资源树、编辑器占位、视觉工具箱
2. PermissionGuard关注七个传统权限键
3. applyPermissions控制动作可用状态

结果：显示流程编辑器/图像显示及五类工具的待开发布局。

失败/取消/边界：工具栏动作未接通业务triggered处理；不能采集相机、标定或执行视觉流程。本次只观察，不将SDK存在视为业务可用。

数据/接口/算法：Qt工具栏/树/堆叠页；sdk/MVS、Halcon尚无src业务调用证据。

验证范围：实际显示原始三区布局和占位文案。

代码：[TraditionalWidget::initToolBar](../../src/traditional/TraditionalWidget.cpp:95)；[TraditionalWidget::applyPermissions](../../src/traditional/TraditionalWidget.cpp:258)。

<a id="f34"></a>
## F34 账户状态与修改密码

位置：账户设置 / P12
入口：修改密码
场景：管理本机操作账号。
前提：访客可查看状态；登录后可输入密码修改。
输入与校验：当前密码、新密码（至少6位）、确认密码。
证据状态：自动回归通过（2026-11-17，offscreen） + 静态复核
按钮编号：无；历史坐标见截图索引

操作流程：

1. 校验登录、当前密码、最小长度及两次新密码一致。
2. 比较并更新唯一账户hash，失败保留原密码；成功清空输入并清理旧记住字段。

结果：新密码可登录，旧密码失效；没有用户增删或角色编辑入口。

失败/取消/边界：旧账户数据不删除；没有新增用户、管理员重置其他用户密码或忘记密码流程。

数据/接口/算法：唯一管理账户与会话；历史user_permissions保留但不读取。

验证范围：访客处理器拒绝、错误旧密码、确认不一致及成功改密均通过；隔离数据库恢复测试密码。

代码：[UserManagementWidget::onChangePassword](../../src/app/UserManagementWidget.cpp:33)；[DatabaseManager::changePassword](../../src/core/DatabaseManager.cpp:104)。

<a id="f35"></a>
## F35 统一只读与编辑状态

位置：账户设置 / P12
入口：无独立按钮入口（共享读写状态）
场景：查看当前只读或可编辑状态。
前提：任何登录态均可读；唯一账户已登录即可写。
输入与校验：SessionManager当前会话。
证据状态：自动回归通过（2026-11-17，offscreen） + 静态复核
按钮编号：无；历史坐标见截图索引

操作流程：

1. 不再查询数据库角色权限矩阵或创建模块权限记录。
2. AccessControl/PermissionGuard统一依赖登录状态，写处理器保留实际拒绝。

结果：未登录不能修改，登录即可操作所有已实现的写功能。

失败/取消/边界：历史users和user_permissions不清空；其他账户不认证；训练等占位功能不会因解锁而获得实现。

数据/接口/算法：唯一管理账户与会话；历史user_permissions保留但不读取。

验证范围：直接写处理器拒绝且文件/数据库不变；登录/退出动态刷新；旧禁止权限不影响新规则。

代码：[canWrite](../../src/core/AccessControl.h:15)；[PermissionGuard::canWrite](../../src/ui/PermissionGuard.cpp:12)。

<a id="f36"></a>
## F36 路径与日志级别保存

位置：系统设置 / P13
入口：浏览... / 保存配置
场景：设置项目/日志/数据库及传统视觉数据位置。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：自动回归（限定范围） + 静态代码复核
按钮编号：B13-01 / B13-02 / B13-03 / B13-04 / B13-05 / B13-08

操作流程：

1. 日志路径/级别立即生效，项目默认根首次使用固定且不迁移已有项目。保存失败恢复内存并可重试。数据库目录下次启动消费；空目标从最近活动库建立保留密码的一致副本，原库保留；已有目标直接复用。传统视觉路径仅持久化，尚无执行消费链。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：临时QSettings保存重读、阻塞路径失败重试、Logger切换及活动根不变通过；隔离独立进程已验证数据库目录读取、默认及自定义来源迁移、WAL已提交密码、重开和无效路径拒绝。可见完整应用的设置到重启交互仍未人工验收。

数据/接口/算法：QSettings FuseVisionTeam/FuseVision沿用平台存储（Windows默认注册表）；SQLite当前连接。日志路径按目录处理，含logs.v1等带点目录；显式.log路径才使用自定义前缀。

验证范围：临时QSettings、Logger与活动根回归通过；数据库使用隔离独立进程验证迁移及重开，完整可见应用的设置到重启交互待人工验收。

代码：[SystemSettingsWidget::onSave](../../src/app/SystemSettingsWidget.cpp:265)；[SettingsManager::save](../../src/core/SettingsManager.cpp:42)；[ProjectManagement::ProjectManagement](../../src/deeplearning/widgets/ProjectManagement.cpp:162)。

<a id="f37"></a>
## F37 重置五项路径

位置：系统设置 / P13
入口：重置
场景：恢复默认路径配置。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：部分自动验证 + 静态复核；见边界
按钮编号：B13-07

操作流程：

1. 仅重置日志/数据库/项目/传统数据/相机路径；日志级别、Python、主题保留。确认后共用保存策略，取消无变更。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：取消重置自动通过；确认重置写入和真实重启未执行。 支持范围与产品Q02–Q04见ISSUES.md。

数据/接口/算法：QSettings路径字段；当前Logger与数据库仍有自己的活动状态。

验证范围：取消重置自动通过；确认重置写入和真实重启未执行。

代码：[SystemSettingsWidget::onReset](../../src/app/SystemSettingsWidget.cpp:296)。

<a id="f38"></a>
## F38 清理旧日志

位置：系统设置 / P13
入口：清理旧日志 / 清理 / 取消
场景：删除超过保留期的日志文件。
前提：未登录可浏览；写操作要求唯一管理账户已登录，且所需项目/模型上下文有效。
输入与校验：使用当前项目、模型、明确选择对象和当前权限；校验规则见DATA-CONTRACTS.md。
证据状态：部分自动验证 + 静态复核；见边界
按钮编号：B13-06

操作流程：

1. 匹配实际活动Logger的basename_日期文件（兼容.log）；文件日期与mtime均过期，保护当前日志和符号链接，失败列路径。

结果：仅在对应提交成功后更新成功状态；具体结果与验证边界见本条运行记录。

失败/取消/边界：实际Logger文件名与过期选择器自动验证；清理对话框/删除失败分支静态复核。 支持范围与产品Q02–Q04见ISSUES.md。

数据/接口/算法：配置日志目录、QFileInfo修改时间、QFile::remove。

验证范围：实际Logger文件名与过期选择器自动验证；清理对话框/删除失败分支静态复核。

代码：[SystemSettingsWidget::onCleanLogs](../../src/app/SystemSettingsWidget.cpp:308)；[Logger::init](../../src/core/Logger.cpp:40)。
