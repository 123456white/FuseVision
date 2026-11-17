"""Generate the narrative analysis and evidence-backed refactoring proposal."""
import sys
sys.dont_write_bytecode = True
from build_analysis import ROOT, LOGIC, FEATURES, EVIDENCE, source, links, put
import json, re

def doc(path,text):
    def sub(m):
        p,s=m.group(1).split('|',1)
        return links([source(p,s)])
    put(path,re.sub(r'\{code:([^}]+)\}',sub,text))

doc('docs/logic/README.md',r'''
# FuseVision 项目功能与代码导航

分析日期：2026-11-14（Asia/Tokyo）。分析对象为当前工作区，包含用户此前未提交的修改；本次只新增分析文档、图像与验证工具，不修复业务实现。

建议先看 [核心流程](CORE-FLOW.md)，对照 [编号截图](SCREENSHOTS.md)，再用 [中文XMind](FuseVision-功能与代码.xmind) 展开分图。每个功能节点的备注包含可用条件、参数、步骤、失败/取消、文件与代码行号。

| 交付 | 用途 |
|---|---|
| [FEATURES.md](FEATURES.md) / [features.json](features.json) | 38项功能，按软件→模块→页面→入口组织 |
| [FuseVision-功能与代码.xmind](FuseVision-功能与代码.xmind) / [同源JSON](FuseVision-功能与代码.json) | 可编辑多分图，嵌入原始真实截图，附流程关联线 |
| [SCREENSHOTS.md](SCREENSHOTS.md) / [button-index.json](button-index.json) | 21张真实界面、原图及编号副本、实际控件位置/可用状态 |
| [DATA-CONTRACTS.md](DATA-CONTRACTS.md) | 工程/模型/数据集/标注/权限存储及格式边界 |
| [架构分析](../codebase/ARCHITECTURE.md) | 入口、页面编排、业务、算法和持久化关系 |
| [问题清单](../refactor/ISSUES.md) | 事实、影响、优先级和验证范围 |
| [功能改进规格](../refactor/spec-process-workflow-consistency.md) | 有证据问题的需求与验收 |
| [分阶段计划](../refactor/refactor-workflow-consistency-1.md) | 后续实施任务，本次状态Planned |
| [验证报告](VALIDATION.md) | CTest、隔离操作、XMind结构和源文件完整性检查 |

七份代码库分析：[技术栈](../codebase/STACK.md) · [目录与入口](../codebase/STRUCTURE.md) · [实际架构](../codebase/ARCHITECTURE.md) · [代码惯例](../codebase/CONVENTIONS.md) · [外部集成](../codebase/INTEGRATIONS.md) · [测试现状](../codebase/TESTING.md) · [风险与意图问题](../codebase/CONCERNS.md)。

## 证据状态

- **实际操作验证**：原应用Widget/事件处理器在Qt offscreen平台实际执行；只覆盖记录中列明的动作，不能替代Windows可见桌面、真实生产数据或硬件验收。
- **静态代码确认**：已追踪控件绑定、处理器及文件/数据库写入，未实测该分支。
- **待确认**：运行条件/产品意图不足，保留编号问题，不写成已实现事实。
- **规划建议**：仅存在于改进规格与计划，尚未执行。

## 当前实现的业务范围

四个一级模块为深度学习、传统视觉、用户管理、系统设置。深度学习实际有9个子页，前4页承载项目/模型/数据集/标注，后5页为占位。模型“新建”是配置和目录创建，不是模型训练。数据集有导入、类别、转换、导出等本地文件功能；数据增强只有提示。传统视觉为三区布局和工具栏骨架，未见相机或视觉算法业务调用。

最先完成并实际走通的路径：创建工程→双击工程→创建模型→双击模型→导入图像和掩码→进入标注→完成矩形→保存→重新载入→返回数据集。发现保存结果缺失新增矩形，因此这是一条**已执行但含失败断点的核心流程**，不能宣称标注保存验收通过。

## 截图与验证隔离方式

截图由 [capture_ui.cpp](tools/capture_ui.cpp) 链接现有Release业务静态库、加载原QRC资源，通过Widget::grab得到。数据集、日志与模型写入QTemporaryDir；SQLite为`:memory:`。原构造函数可能读取Windows原生QSettings，不能声称所有QSettings读取已隔离；本次不提交登录、不保存设置、不切换主题，且在内存覆盖路径，不执行设置持久化。截图中用户与项目为隔离验证数据。

部分入口由原槽函数触发，真实文件选择/格式确认对话框由Qt测试定时器填写；未将其描述为完整人工鼠标端到端测试。主要页面及项目/模型双击、矩形工具坐标事件由Qt测试事件驱动。第三方LabelMe进程、相机、训练及生产数据迁移没有执行。

## 阅读时需特别注意

1. 标注现代模型与旧形状/撤销栈并存，已复现新增形状未落盘；成功日志不可靠。
2. 普通用户canWrite=false时项目新建仍enabled，处理器也未见相应校验。
3. 新建模型通知被QSignalBlocker吞掉；本次通过双击模型继续后续验证。
4. 项目/模型删除缺集中上下文清理，类别删除/重排未同步重写标注ID，均为静态证据问题，未执行破坏性操作。

## 待确认的产品意图

- **Q01 [ASK USER]**：保留三种角色快捷登录，还是允许登录“用户管理”新增的任意账号？现状仅能登录三个固定用户名。规格提供账号输入方案，采用前需确认。
- **Q02 [ASK USER]**：异常检测应以OK/NG图像分类还是二值区域掩码为主？当前目录/task采用classify，导入说明及标注又采用掩码。
- **Q03 [ASK USER]**：“非自由矩形/自由矩形”的产品命名是否对应现有obb/detect映射？文档保留实际映射，不替用户改名。
- **Q04 [ASK USER]**：功能记录中的“每角色至少保留一人”、账号审计字段、迁移/备份是否仍是现行需求？本次只记录实现差距，没有擅自纳入必须开发范围。

[TODO] 在XMind客户端检查实际布局/跨版本兼容性；可见Windows窗口与真实多类别/多任务数据回归；LabelMe环境与窗口嵌入；日志清理命名兼容性。以上缺口不妨碍当前静态整理交付。
''')

doc('docs/logic/CORE-FLOW.md',r'''
# 从创建工程到标注返回：一个完整核心流程

所有路径以隔离样本UI_Logic_Demo/SurfaceSeg为例；合成图800×520，不是生产缺陷样本。步骤中“已验证”仅指原Widget的offscreen执行。截图按钮编号详见[图册](SCREENSHOTS.md)。

```mermaid
flowchart LR
  A[登录与主窗口] --> B[P02 新建项目 F05]
  B --> C[双击工程 F07]
  C --> D[P03 新建模型 F09]
  D -->|新建通知被阻断| X[上下文通知缺口]
  D -->|双击模型 F11| E[P04 导入图像与标签 F13]
  E -->|双击缩略图 F17| F[P05 绘制 F23]
  F --> G[保存 F24]
  G -->|文件可读但新形状缺失| H[重开验证失败]
  G --> I[返回数据集 F26]
  E --> J[格式转换 F20]
  E --> K[数据导出 F21]
```

## 第一步：登录与确定上下文

原始启动依次初始化配置/主题/日志、数据库、18个权限点，再弹登录框。初始登录取消退出；切换用户取消则保留会话。当前登录框只映射固定user/admin/superadmin。验证程序直接在内存数据库认证并调用SessionManager::login建立超级管理员会话，未向真实数据库提交登录。

代码：{code:src/main.cpp|int main}；{code:src/MainWindows/Login.cpp|LoginDialog::doLogin}。

## 第二步：P02 新建并打开工程（F05、F07，已验证）

点“新建项目”，输入UI_Logic_Demo及可选描述。名称仅允许1–64位字母/数字/下划线；目录已存在则失败。确认后创建project.fvproj并刷新卡片，**尚未正式打开**。双击卡片后发projectOpened，将项目传入模型/数据集页，并跳模型管理。

单击只预览，m_previewProject与正式m_project分开；不要仅看左侧信息判断当前打开项目。页面截图[P02](screenshots/P02-project-created.png)，弹窗[P02D](screenshots/P02-new-project-dialog.png)。

代码：{code:src/DeepLearningWidget/ProjectManagement.cpp|ProjectManagement::onNewProject}；{code:src/DeepLearningWidget/ProjectManagement.cpp|ProjectManagement::onCardDoubleClicked}。

## 第三步：P03 创建模型并双击（F09、F11，已验证）

默认实例分割，输入SurfaceSeg。创建模型目录、.fvdl、data.yaml及分区骨架，登记工程模型。界面虽有描述/路径字段，创建函数只接收名字、类型、工程根和工程文件，描述没有落盘。

运行观察：模型已经选中，但modelCreated和currentModelChanged计数均为0。QSignalBlocker在发信号时仍存活。继续**双击模型卡片**，原modelOpened和openDatasetRequested链路正常，数据集页获得当前模型。

代码：{code:src/DeepLearningWidget/ModelManagement.cpp|ModelManagement::onNewModel}；{code:src/DeepLearningWidget/ModelManagement.cpp|ModelManagement::onCardDoubleClicked}；{code:src/DeepLearningWidget/ModelConfig.cpp|ModelConfig::createModel}。

## 第四步：P04 导入一对图像与掩码（F13，已验证）

点“导入数据集文件”对应的原槽，确认说明，选择surface_demo.png图像；确认同时导标签，按对话框确认灰度掩码，选择同名PNG标签。文件复制到模型的images/labels，写dataset_config.json、识别class_1、刷新图库，项目data_imported标志更新。

格式说明/文件选择/标签类型选择可取消；同名覆盖存在逐文件跳过，不能假设整体导入具有事务性。样本计数实际为1。原始验证数据保留[图像](evidence/fixture-image.png)与[标签](evidence/fixture-mask.png)。

代码：{code:src/DeepLearningWidget/DatasetManagement.cpp|DatasetManagement::onImportImages}；{code:src/DeepLearningWidget/DatasetManagement.cpp|DatasetManagement::detectLabelsFromAnnotations}。

## 第五步：P05 绘制、保存、重开（F17、F23、F24，发现失败）

触发缩略图双击处理器后，编排器setDatasetSource、openImage，再切标注页。旧掩码载为一个形状。选择矩形，在图像坐标约(400,240)与(610,370)两次点击完成；仅一次拖拽不会提交RectTool。现代AnnotationModel形状数变2。

点保存后，labels/surface_demo.png可读取，然而新增矩形内(500,300)灰度仍为0；重新加载只剩原掩码形状（观察值以runtime-ui.json为准）。模型isDirty仍true，而界面hasUnsavedChanges在绘制前后可能为false：openImage导入形状使模型dirty，然后只清界面m_modified，后续模型已dirty不再发true边沿通知。

根因可沿代码阅读：工具写m_model→数据变更可按m_model重绘→工具requestRender又走renderOverlay(m_shapes)→列表/saveAnnotations/顶部Undo仍使用旧数据与旧栈。保存还忽略写入结果，所以该流程不能作为保存功能通过证据。

证据：[运行观察](evidence/runtime-ui.json)、[保存掩码](evidence/saved-mask.png)、[保存后界面](screenshots/P05-annotation-saved.png)、[重开界面](screenshots/P05-annotation-reopened.png)。

代码：{code:src/DeepLearningWidget/Tools/RectTool.cpp|RectTool::endRect}；{code:src/DeepLearningWidget/AnnotationModel.cpp|AnnotationModel::markDirty}；{code:src/DeepLearningWidget/AnnotationManagement.cpp|AnnotationManagement::saveAnnotations}。

## 第六步：返回图库与其他出口（F26已验证；F20/F21静态）

“返回数据集管理”按m_modified尝试保存后直接返回，编排器重新识别标签。没有基于保存结果的返回阻断，也没有保存/放弃/取消三选。格式转换和数据导出能从P04走原处理器：转换写labels/*.txt且保留原格式；导出有选中时只选中，无选中则全体。两者本次未运行，具体条件见FEATURES.md。

主流程不会进入真实训练：P06–P10是占位页。顶部“模型导出”与P04“导出数据集”是不同能力。

代码：{code:src/DeepLearningWidget/AnnotationManagement.cpp|AnnotationManagement::onReturnToGallery}；{code:src/DeepLearningWidget/DeepLearningWidget.cpp|&AnnotationManagement::returnToGallery}。
''')

doc('docs/logic/DATA-CONTRACTS.md',r'''
# 业务数据与文件契约（当前实现）

## 从页面到存储

| 页面/操作 | 持久化对象 | 实际消费方 |
|---|---|---|
| P02工程 | project.fvproj，JSON | ProjectConfig、ModelManagement、DatasetManagement |
| P03模型 | <模型>/<模型>.fvdl，data.yaml，项目models/current_model_id | ModelConfig、模型卡片、DatasetManagement |
| P04数据 | images/*、labels/*、dataset_config.json | 图库、统计、格式转换、导出、AnnotationManagement |
| P05标注 | labels/<basename>.png或.json | 数据集重扫、标签探测、下次标注加载 |
| P12账号/权限 | SQLite users、user_permissions | Login、Session、PermissionRegistry |
| P13配置 | QSettings路径/日志/Python；主题另有ui/theme | 应用启动、Logger、各页缓存路径 |

## 工程与模型：配置不是训练结果

工程默认含project_name/description/created_time/modified_time/paths.project_root/current_model_id/models。每个models条目以模型名为键，含model_file、dataset_folder与data_imported/annotated/split_done/trained四个初始false状态。不同处理器会重载或重写其中子集，不能假设写回保持未知字段。

.fvdl有format_version=1.0、model_name、model_type、description（创建时硬编码空串）、project_ref/dataset_ref、dataset_layout、architecture、inference_config、training_versions/current_version等。默认结构字段如resnet50/FPN、置信度0.5、NMS0.4只是配置模板，没有已接通训练/推理实现。data.yaml写入失败未被完整传播。工程、模型、YAML更新不是一个原子事务。

代码：{code:src/DeepLearningWidget/ProjectConfig.cpp|ProjectConfig::buildDefaultFvproj}；{code:src/DeepLearningWidget/ModelConfig.cpp|ModelConfig::createModel}。

## 任务映射与目录差异

| 界面任务名 | 当前data.yaml task | 模型创建骨架 | 数据管理/标注约定 |
|---|---|---|---|
| 实例分割 | segment | images/{train,val,test}与labels/{train,val,test} | 平铺images/labels，灰度类别掩码 |
| 异常检测 | classify | {train,val,test}/{OK,NG} | 导入说明二值掩码；格式转换另有OK/NG分类引导 |
| 物体检测(非自由矩形) | obb | images/labels分区 | JSON边界框/转换；命名意图待Q03确认 |
| 物体检测(自由矩形) | detect | images/labels分区 | JSON旋转框说明与映射存在待确认语义 |

表述保留实际代码，不按常识交换两个检测类型。模型新建骨架与数据文件夹导入直接子文件扫描是不同路径；本次未验证多任务/多分区互操作。

代码：{code:src/DeepLearningWidget/ModelConfig.cpp|ModelConfig::createModel}；{code:src/DeepLearningWidget/DatasetManagement.cpp|DatasetManagement::onImportFolder}。

## 标签和像素ID

dataset_config记录categories（保存ID按序号+1）、image_count、label_type、instance_count等；内存m_labelNames/m_labelColors/m_labelInstanceCounts按零基索引对应。currentLabelOffset实际恒为1；注释提及Halcon minGray不代表已从Halcon读动态offset。

灰度PNG中背景0，类别通常为内存索引+1。二值保存时非零映射255，但导入说明可能说异常为1；这不应被解释为所有二值文件自动归一的契约。COCO类JSON保存category_id加offset。AnnotationManagement写的是逐图JSON结构（images/annotations、可含rotation/rot_center），不是完整标准COCO数据集；聚合COCO由数据集导出函数生成。

类别删除或顺序调整时配置ID会重排，内存样本索引可能更新，但未见同步修改全部磁盘PNG/JSON，这会破坏类别语义，见ISS-005。

代码：{code:src/DeepLearningWidget/DatasetManagement.cpp|DatasetManagement::saveDatasetConfig}；{code:src/DeepLearningWidget/DatasetManagement.cpp|DatasetManagement::currentLabelOffset}；{code:src/DeepLearningWidget/AnnotationManagement.cpp|AnnotationManagement::saveAnnotations}。

## 支持格式必须按入口判断

- 文件导入对话框接受图像及PNG/JSON标签；文件夹导入只取images和labels直接子文件。
- 格式转换探测YOLO txt、PNG、LabelMe JSON，输出YOLO txt；异常检测单独转换分类目录。
- 数据集导出提供COCO/YOLO，掩码类额外提供原生PNG目录。
- AnnotationIO实现了PNG/COCO/YOLO/PascalVOC/LabelMe等帮助函数，但在src中未找到界面业务对这些类静态方法的调用；不能据此宣称当前标注保存菜单支持这些全格式。

代码：{code:src/DeepLearningWidget/DatasetManagement.cpp|DatasetManagement::onFormatConvert}；{code:src/DeepLearningWidget/DatasetManagement.cpp|DatasetManagement::onExportDataset}；{code:src/DeepLearningWidget/AnnotationIO.cpp|AnnotationIO::savePascalVOC}。

## SQLite与权限

users仅有id/username/password_hash/role；无登录IP、设备、登录时间或last_login。密码为无盐SHA-256。user_permissions主键(user_id,module_name)，含can_read/can_write。表声明ON DELETE CASCADE，但当前initDatabase未见启用PRAGMA foreign_keys，不能仅凭声明承诺删除用户后所有权限行自动清除；这项未运行验证。

模块注册为所有用户补默认权限，角色0默认可读不可写，角色1/2默认可读可写；这比README角色表更宽松。Registry无记录默认false,false；refreshPermissions更新缓存不发变更信号。Guard维护另一个快照，主要由Session::sessionChanged触发刷新。

代码：{code:src/core/DatabaseManager.cpp|DatabaseManager::createTables}；{code:src/core/DatabaseManager.cpp|DatabaseManager::initModulePermissions}；{code:src/core/PermissionRegistry.cpp|PermissionRegistry::refreshPermissions}；{code:src/core/PermissionGuard.cpp|PermissionGuard::watch}。

## 设置与日志

SettingsManager为单例内存缓存，load/save经QSettings同步；setter自身不持久化。Windows原构造采用原生格式，注释“.ini”不是保证。Login另有FuseVisionLogin命名空间，remember=true写明文lastPassword。系统设置保存重新初始化日志，但数据库路径重启才生效；项目页构造时缓存项目根，需要另外刷新机制。

清理旧日志只匹配fusevision_*.log；Logger的dailyBasePath基名未加.log，命名兼容性待实测，不将其作为已复现清理故障。

代码：{code:src/core/SettingsManager.cpp|SettingsManager::SettingsManager}；{code:src/MainWindows/Login.cpp|LoginDialog::saveSettings}；{code:src/MainWindows/SystemSettingsWidget.cpp|SystemSettingsWidget::onCleanLogs}；{code:src/core/Logger.cpp|Logger::init}。
''')

doc('docs/codebase/STACK.md',r'''
# Technology Stack

分析日期2026-11-14；当前工作区，含既存未提交变更。

## Core Sections (Required)

### 1) Runtime Summary

FuseVision是本地机器视觉桌面应用。当前可用范围集中在工程/模型配置、数据管理与标注UI、账号与设置；训练/预测及传统视觉执行链尚未接通。业务数据落本地文件与SQLite，没有发现业务HTTP API。

### 2) Production Frameworks and Dependencies

| 依赖 | 当前证据 | 用途/限制 |
|---|---|---|
| C++20 | CMakePresets的CMAKE_CXX_STANDARD=20 | 应用语言 |
| Qt Widgets/Core/Gui/Sql | 当前Release Qt6CoreConfigVersionImpl=6.10.2 | 窗口、图像、SQLite；Qt Test仅分析工具 |
| OpenCV | 当前Release OpenCVConfig-version=4.12.0；顶层REQUIRED | 依赖已安装，未见src图像业务cv::调用 |
| spdlog | 当前安装1.17.0，manifest最低1.16 | 日志文件/调试控制台 |
| SQLite | Qt QSQLITE驱动 | users与user_permissions；未独立锁定SQLite运行版本 |
| Halcon / MVS | cmake查找脚本、sdk目录 | 可选构建接入/资源，并非已完成视觉流程 |
| Python / LabelMe | QProcess调用 | 仅外部标注入口，环境未实测 |

manifest的version>=是最低约束，不能当作本机安装版本。vcpkg baseline固定fc3be1ebea7eaeb3071fe716ac65713af1f3a146。

### 3) Development Toolchain

CMake最低3.21，预设Ninja；Windows x64 MSVC。本机现有业务构建用MSVC19.50.35728（VS18 BuildTools），与README最低VS2022要求不矛盾。本次分析工具单独用Visual Studio 18 2026生成器链接原Release静态库；没有切换业务构建生成器。工程也声明Linux/macOS预设，未验证这两个平台，Windows句柄嵌入需平台处理。

### 4) Key Commands

在仓库根PowerShell执行，开发环境需具备CMake/Ninja/MSVC和vcpkg依赖：

```powershell
.\CmakeBuild.ps1 -Configuration Release -NoRun -Test
# 或使用已配置工具链
cmake --preset windows-x64-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
.\out\build\windows-x64-release\bin\Release\FuseVision.exe
```

本次使用已存在Release产物运行CTest和隔离分析，startup-smoke 1/1通过；未重建全量第三方SDK。分析工具复现见[验证报告](../logic/VALIDATION.md)。

### 5) Environment and Config

QSettings存路径、级别、主题、Python；默认业务目录在可执行文件旁。数据库路径由main组合/传入，取消初始登录退出。FUSEVISION_STARTUP_SMOKE_TEST=1走隔离测试分支；QT_QPA_PLATFORM=offscreen用于无可见桌面验证。Halcon可用HALCONROOT/HALCONARCH配置。凭据不在文档重复列出；现有默认凭据见README，部署环境应单独治理。

### 6) Evidence

{code:CMakeLists.txt|cmake_minimum_required}；{code:CMakePresets.json|CMAKE_CXX_STANDARD}；{code:vcpkg.json|builtin-baseline}；{code:src/main.cpp|int main}；{code:src/core/SettingsManager.cpp|SettingsManager::load}。

精确本机版本证据为out/build/windows-x64-release/vcpkg_installed/x64-windows/share中的Qt6Core/Qt6CoreConfigVersionImpl.cmake、opencv4/OpenCVConfig-version.cmake及spdlog/spdlogConfigVersion.cmake；它们是构建产物，不纳入可移植业务配置契约。
''')

doc('docs/codebase/STRUCTURE.md',r'''
# Codebase Structure

## Core Sections (Required)

### 1) Top-Level Map

| 路径 | 业务意义 |
|---|---|
| src/main.cpp | 启动、权限注册、登录/测试分支 |
| src/MainWindows | 主导航、登录、用户管理、系统设置 |
| src/DeepLearningWidget | 工程→模型→数据→标注完整UI与文件业务 |
| src/DeepLearningWidget/Tools | 六种交互标注工具 |
| src/TraditionalWidget | 传统视觉布局/权限骨架 |
| src/core | 日志、设置、主题、会话、授权、数据库 |
| cmake、CmakeBuild.ps1、CMakePresets.json | 依赖发现、运行库部署、构建/测试 |
| res、FuseVision.qrc | 亮/暗图标与应用资源 |
| sdk、vcpkg | 外部SDK与包管理，不当作自研源码统计 |
| .github/workflows/build.yml | Windows构建、启动冒烟、上传产物 |
| docs/codebase、docs/logic、docs/refactor | 本次新增分析、真实附图、导图和后续规格 |

### 2) Entry Points

主入口main→登录→FuseVision。FuseVision::initPagesAsync创建四个顶层页面；DeepLearningWidget构造9个子页，前4个为实际管理类。传统工具栏只是视觉入口，未接业务triggered处理。Bundled scan未正确识别src/main.cpp入口，本文以源码核实结果为准。

### 3) Module Boundaries

src/CMakeLists拆为UI/DeepLearning/Traditional/Core静态库。模块名不等于分层：DatasetManagement同时读写文件、转换标注、算统计与建界面；AnnotationManagement同时编排工具、处理文件、维护两份状态；编排器直接接入这些Widget。当前不是独立领域服务+仓储的严格架构。

自研.cpp/.h规模：DeepLearning36文件15127行，MainWindows8文件2026行，core19文件2254行，Traditional2文件374行。DatasetManagement.cpp5239行，AnnotationManagement.cpp2586行。扫描报告149文件20644行包含扫描范围中配置/资源等，不称为纯C++行数。

### 4) Naming and Organization Rules

文件通常与Widget/Manager/Config类同名；m_为成员，onXxx为槽，initXxx为构造UI，applyTheme/applyPermissions为刷新。资源亮/暗图标以_D区分。历史backup、.vs、.trae、vcpkg、sdk及新docs未纳入自研结构扫描。无适用AGENTS.md：已查仓库及D:/、D:/project祖先；因此按用户当前限制执行。

### 5) Evidence

{code:src/main.cpp|int main}；{code:src/CMakeLists.txt|add_subdirectory(core)}；{code:src/MainWindows/FuseVision.cpp|FuseVision::initPagesAsync}；{code:src/DeepLearningWidget/DeepLearningWidget.cpp|DeepLearningWidget::initUI}；[原始扫描](../logic/evidence/codebase-scan.txt)。
''')

doc('docs/codebase/ARCHITECTURE.md',r'''
# Architecture

## Core Sections (Required)

### 1) Architectural Style

单进程Qt Widgets桌面应用，以signals/slots编排Widget，用单例共享基础设施，工程/模型部分由静态Config类负责持久化。多处UI类直接做业务与文件I/O；没有已接通独立训练/推理后端。

### 2) System Flow

```mermaid
flowchart TD
  Main[main: 应用/设置/数据库/18权限/登录] --> FW[FuseVision 主窗口]
  FW --> DL[DeepLearningWidget 编排]
  FW --> TV[TraditionalWidget 占位]
  FW --> US[UserManagementWidget]
  FW --> SS[SystemSettingsWidget]
  DL --> PM[ProjectManagement]
  PM -->|projectOpened| MM[ModelManagement]
  PM --> PC[ProjectConfig → fvproj]
  MM --> MC[ModelConfig → fvdl/yaml/fvproj]
  MM -->|当前模型/打开数据集| DS[DatasetManagement]
  DS -->|openAnnotationRequested| AN[AnnotationManagement]
  AN --> Tools[AnnotationCanvas / 六工具 / AnnotationModel]
  AN --> Old[旧 m_shapes / 旧Undo / 保存]
  Tools --> Render[AnnotationRender / QImage]
  DS --> Files[images / labels / dataset_config]
  Old --> Files
  AN -->|returnToGallery| DS
  AN -->|LabelMe请求| DL
  DL --> Proc[QProcess / Win32嵌入]
  US --> DB[DatabaseManager / SQLite]
  SS --> Settings[SettingsManager / QSettings]
  DL --> Perm[PermissionRegistry / Guard / Session]
```

启动顺序：UTF8→QApplication/Fusion→主题/QSS→设置/Logger→数据库/迁移→权限注册→登录→主窗口。initPagesAsync虽命名Async，实际在GUI线程依次new并processEvents，不是后台工作线程。主要文件导入、统计与转换也在界面路径执行。

### 3) Layer/Module Responsibilities

| 层/能力 | 当前类与职责 | 实际边界 |
|---|---|---|
| 入口 | main、FuseVision | 初始化全局对象和模块页 |
| 界面编排 | DeepLearningWidget | 跨项目/模型/数据集/标注信号、LabelMe进程 |
| 工程/模型业务 | ProjectManagement/ModelManagement + Config | Widget决定流程，Config读写本地JSON/YAML |
| 数据业务 | DatasetManagement | 导入/配对/类别/转换/导出/统计全部混在Widget |
| 交互标注 | AnnotationManagement/Canvas/Tools/Model | 新工具模型与旧状态迁移未完成 |
| 算法 | AnnotationRender、工具及Dataset转换方法 | QImage/QPainter绘制、掩码/几何转换；非YOLO推理 |
| 数据层 | DatabaseManager、QSettings、QFile/QJson | SQLite只用于账号/权限，业务项目主要文件 |
| 共享 | ThemeManager/Logger/LogMonitor/Session/Permission | 缓存与广播的覆盖度不一致 |

### 4) Reused Patterns

Config静态方法、Meyers单例、Qt modeless页面+modal对话框、卡片单/双击分流、signals/slots广播、PermissionGuard快照、RAII Transaction生成Undo命令。AnnotationModel是可复用数据模型，但现有保存/列表未全面消费它；AnnotationIO的多格式实现也未接入当前管理页面调用链。

### 5) Known Architectural Risks

1. 双标注状态和双Undo导致显示/脏状态/保存不一致，已在隔离样本复现（ISS-001/002）。
2. 新建模型QSignalBlocker屏蔽通知；项目/模型删除仅清本地状态，缺稳定上下文生命周期（ISS-003/004）。
3. 类别序号与持久化ID绑定，删除/重排只改内存与配置；文件语义可能漂移（ISS-005）。
4. 授权在模块间不一致，Widget的enabled不能替代处理器授权（ISS-006）。
5. 数据集5239行、标注2586行；应先围绕已复现问题收敛状态，再按已存在文件流程提取职责，不为占位页凭空建服务（ISS-010）。

### 6) Evidence

{code:src/DeepLearningWidget/DeepLearningWidget.cpp|&DatasetManagement::openAnnotationRequested}；{code:src/DeepLearningWidget/AnnotationManagement.cpp|AnnotationManagement::saveAnnotations}；{code:src/DeepLearningWidget/Tools/RectTool.cpp|RectTool::endRect}；{code:src/DeepLearningWidget/ModelManagement.cpp|QSignalBlocker blk(this)}；[运行证据](../logic/evidence/runtime-ui.json)。
''')

doc('docs/codebase/CONVENTIONS.md',r'''
# Coding Conventions

## Core Sections (Required)

### 1) Naming Rules

当前惯例是PascalCase类/文件、camelCase方法、m_成员、s_静态资源数组、onXxx槽与initXxx/applyXxx辅助方法。业务名中文字符串与权限键相同，如深度学习.数据集管理；字符串改名会影响授权数据。不能只重命名UI而忽略DB键。

### 2) Formatting and Linting

主要为4空格缩进、花括号独立行、头文件include guard和分区注释。未找到项目根.clang-format/.clang-tidy或CI格式/lint门禁；.clangd是编辑器编译解析配置。当前不声称有强制统一格式。UTF8注释/字符串常见QString::fromUtf8，MSVC启用/utf-8。

### 3) Import and Module Conventions

Widget引用同模块头与core/基础类；CMake静态库组织链接。主题通过语义调色板和ThemeManager::themeChanged刷新；图标QRC有亮暗两套。工具类用AnnotationTool上下文和signal请求重绘，但管理类仍保留legacy实现，这是迁移现状而非推荐继续复制的规范。

### 4) Error and Logging Conventions

Config方法多返回bool并填QString* errorOut；UI用QMessageBox显示并Logger记录；DB用预编译QSqlQuery。错误策略不统一：AnnotationManagement忽略PNG/JSON写入失败仍清dirty，DatasetConfig写失败部分静默。日志文件与LogMonitor显式消息是两条链路，不能以文件日志保证用户看到失败。

### 5) Testing Conventions

现有CTest通过环境变量运行同一exe的startup-smoke；offscreen、内存DB、临时路径后短时退出。仓库未见独立业务测试套件。新分析工具也使用Qt Test事件和临时样本，但当前只在docs，未接CI、不作为现有业务回归覆盖率。

### 6) Evidence

{code:src/DeepLearningWidget/ProjectConfig.cpp|ProjectConfig::createProject}；{code:src/DeepLearningWidget/AnnotationManagement.cpp|AnnotationManagement::saveAnnotations}；{code:src/core/DatabaseManager.cpp|DatabaseManager::addUser}；{code:CMakeLists.txt|add_test}；{code:src/DeepLearningWidget/Tools/AnnotationTool.h|class AnnotationTool}。
''')

doc('docs/codebase/INTEGRATIONS.md',r'''
# External Integrations

## Core Sections (Required)

### 1) Integration Inventory

| 集成 | 入口 | 状态 |
|---|---|---|
| Qt QSQLITE | DatabaseManager | 已接通本地用户/授权；内存运行已验证 |
| 本地文件 | Config与Dataset/Annotation | 工程、模型、数据集；核心隔离流程已执行并发现保存缺口 |
| LabelMe/Python | onLaunchLabelMe→QProcess→HWND | 静态接通入口；实际环境/脚本路径/窗口嵌入待确认 |
| Windows DWM/SetParent | ThemeManager、DeepLearningWidget | 平台专有；offscreen不能验证原生窗口效果 |
| OpenCV/Halcon/MVS/YOLO | CMake/sdk资源 | OpenCV为构建依赖；未见src接通相机/训练/推理业务 |
| GitHub Actions | build.yml | CI配置存在，Windows构建/测试/上传；未查询远端最近运行结果 |

### 2) Data Stores

账号/权限SQLite；设置QSettings（Windows通常Native注册表）；图像/标签/配置本地目录。没有数据库存储完整视觉工程，也未见云对象存储或远程训练服务。输入同名图像/标签通过basename配对，跨扩展同名等需进一步验证。

### 3) Secrets and Credentials Handling

users.password_hash是无盐SHA-256；种子凭据在仓库源码/README中。记住密码会明文写QSettings lastPassword。没有发现凭据管理器集成。这里记录真实行为，未执行密码迁移、数据库修改或安全加固。

### 4) Reliability and Failure Behavior

LabelMe waitForStarted5秒，退出terminate后最多3秒kill；HWND依赖进程输出协议。工程复制与多文件写入缺事务/回滚，标注写失败没有向调用者返回失败。外部Python环境选择可能持久化，验证未点该按钮。SQLite表声明FK级联但未见PRAGMA开启，不能承诺删除清理结果。

### 5) Observability for Integrations

spdlog控制台/文件、Logger封装、模块LogMonitor是主要可观测手段。未见指标/远程追踪/崩溃服务。启动冒烟覆盖基本本地依赖加载，不覆盖相机、第三方进程和真实业务数据。

### 6) Evidence

{code:src/DeepLearningWidget/AnnotationManagement.cpp|AnnotationManagement::onLaunchLabelMe}；{code:src/DeepLearningWidget/DeepLearningWidget.cpp|DeepLearningWidget::enterLabelMeMode}；{code:src/core/DatabaseManager.cpp|DatabaseManager::hashPassword}；{code:src/MainWindows/Login.cpp|LoginDialog::saveSettings}；{code:.github/workflows/build.yml|windows-2022}。
''')

doc('docs/codebase/TESTING.md',r'''
# Testing Patterns

## Core Sections (Required)

### 1) Test Stack and Commands

项目CTest注册startup-smoke，使用现有exe和FUSEVISION_STARTUP_SMOKE_TEST=1。已执行`ctest --preset windows-release --output-on-failure`，1/1通过。分析专用Qt Test harness链接现有Release库，在offscreen实际运行核心链路；复现方式见[VALIDATION](../logic/VALIDATION.md)。

### 2) Test Layout

启动断言内嵌在src/main.cpp；顶层CMake设置CTest环境及超时。docs/logic/tools/capture_ui.cpp是本次分析工具，不是已接入CI的正式测试；docs/logic/evidence保存观察JSON、截图、mask像素证据与日志。没有独立tests目录的业务测试集。

### 3) Test Scope Matrix

| 范围 | 本次状态 | 限制 |
|---|---|---|
| 启动、种子用户、错密、权限初始化 | startup-smoke通过 | 认证基础，不是全UI登录 |
| 新建工程/双击工程 | 原Widget执行通过 | 临时目录、有效ASCII名；取消/失败仅静态 |
| 新建/双击模型 | 执行完成；通知缺口复现 | 新建两个信号为0，双击继续 |
| 文件导入1图+PNG标签 | 通过并截图 | 合成800×520，未覆盖其他格式/重名/取消 |
| 矩形保存重开 | 失败复现 | 新形状进入模型，未落盘 |
| 普通用户项目写入口 | 缺口复现 | canWrite=false仍enabled；未用普通用户创建真实项目 |
| 各顶层页/五占位页 | 21状态真实渲染 | offscreen，不能验证原生对话框/DWM |
| 删除、类别重排、转换/导出、设置写、用户CRUD | 静态 | 未执行破坏性或额外写操作 |
| LabelMe、相机、训练、预测 | 未验证或未实现 | 不将依赖可加载视为业务通过 |

### 4) Mocking and Isolation Strategy

不替换业务Widget；仅用合成样本、QTemporaryDir、内存SQLite及Qt事件驱动。原QSettings构造可能读已有原生偏好；不提交登录/保存设置/切主题，内存覆盖运行路径，避免生产配置写入。文件对话框设非原生以便自动填写；因此不涵盖Windows原生文件选择行为。

### 5) Coverage and Quality Signals

没有代码覆盖率报告，不编造百分比。现有smoke通过与业务保存失败可以同时成立。关注流程断言：持久化内容重开一致、信号次数、授权拒绝后文件不变，而非只断言窗口显示/文件存在。全历史改动频次仅辅助定位；最近90天scan为空不代表从未改动。

### 6) Evidence

{code:CMakeLists.txt|add_test}；{code:src/main.cpp|startupSmokeTest}；[CTest日志](../logic/evidence/ctest-release.log)；[运行观察](../logic/evidence/runtime-ui.json)；[全历史频次](../logic/evidence/history-churn.json)；[分析源码](../logic/tools/capture_ui.cpp)。
''')

doc('docs/codebase/CONCERNS.md',r'''
# Codebase Concerns

## Core Sections (Required)

### 1) Top Risks (Prioritized)

| 优先级 | 问题 | 证据与影响 |
|---|---|---|
| P0 | ISS-001 标注双状态/双Undo | 已复现新矩形2→重开1，新增区域像素0；用户工作可能丢失 |
| P0 | ISS-002 保存失败仍清状态 | 静态：PNG结果不检查，JSON失败忽略；离开页面无失败阻断 |
| P0 | ISS-006 授权不一致 | 已复现项目canWrite=false但新建enabled；处理器无守卫 |
| P1 | ISS-003 模型通知被屏蔽 | QSignalSpy计数均0，跨页上下文未及时同步 |
| P1 | ISS-004 删除上下文与目标 | 静态：预览/已打开目标优先级不一致；删除缺闭合广播 |
| P1 | ISS-005 类别ID一致性 | 静态：删除/重排未重写磁盘标注ID |
| P1 | ISS-007 新增账号无法登录 | 静态：任意用户名可新增，登录只映射三个固定名 |
| P2 | ISS-008 模型描述/路径表单 | 静态：字段存在但创建未消费 |
| P2 | ISS-009 设置生效描述 | 静态：路径缓存与提示不一致；重置不重建Logger |
| P2 | ISS-010 数据/标注职责混杂 | 5239/2586行，大Widget承载业务/转换/持久化 |

P0表示建议下一轮优先处理，不代表本次已修复或已发布故障。完整证据、影响与验收见[问题清单](../refactor/ISSUES.md)。

### 2) Technical Debt

新旧Annotation状态迁移未完成；未调用的AnnotationIO与管理页重复序列化；设置setter/持久化/实际页缓存没有统一生效协议；projectClosed信号有接收链但缺发射。README的8标签/17权限与当前9/18不一致。五个深度学习页与传统视觉骨架是未实现范围，未自动纳入重构计划。

### 3) Security Concerns

记住密码明文、无盐SHA256、默认种子凭据可见、授权只在部分UI覆盖。这里只优先将实际越权入口纳入规格；密码迁移/凭据存储策略需单独范围与兼容验证，不把推测的远程攻击面写成已存在漏洞。

### 4) Performance and Scaling Concerns

数据导入/扫描/转换在Widget同步处理；refreshGallery内processEvents及防重入标志表明有重入管理成本。未对大型数据集计时，不能断言具体卡顿阈值。后续先提取纯文件/计算能力，再依据测量决定线程化，不凭空加队列/网络服务。

### 5) Fragile/High-Churn Areas

行数最大的DatasetManagement、AnnotationManagement最需流程回归。全历史统计DeepLearningWidget.cpp/.h各6次、其CMake5次、FuseVision.cpp5次；这只是当前git历史计数，非缺陷率。Bundled scan最近90天未报热点；两种窗口不可混写。

### 6) `[ASK USER]` Questions

- **Q01**：任意账号登录还是三角色固定账户？新增账号登录缺口需产品意图定案。
- **Q02**：异常检测的主数据契约是OK/NG分类还是区域掩码？
- **Q03**：自由/非自由矩形名称与detect/obb映射的期望是什么？
- **Q04**：每角色保留人数、审计字段、迁移/备份记录是否仍生效？

以上不是本次文档交付阻塞条件。后续实施不可擅自交换任务语义或扩建占位功能。

### 7) Evidence

{code:src/DeepLearningWidget/AnnotationManagement.cpp|AnnotationManagement::saveAnnotations}；{code:src/DeepLearningWidget/ModelManagement.cpp|QSignalBlocker blk(this)}；{code:src/DeepLearningWidget/DatasetManagement.cpp|DatasetManagement::onLabelOrderChanged}；{code:src/MainWindows/Login.cpp|LoginDialog::doLogin}；[运行JSON](../logic/evidence/runtime-ui.json)。
''')

put('docs/logic/evidence/source-references.json',json.dumps(EVIDENCE,ensure_ascii=False,indent=2))
print('Wrote core-flow/data contracts/readme and seven codebase documents')
