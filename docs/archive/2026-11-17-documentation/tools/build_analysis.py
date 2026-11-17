"""Rebuild analysis documents, numbered screenshots and XMind input (no source writes)."""
from pathlib import Path
import json, re, hashlib, collections
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[3]
LOGIC = ROOT / 'docs/logic'
DATE = '2026-11-14'
FEATURES = []
EVIDENCE = []

def put(path, text):
    p = ROOT / path
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text.strip() + '\n', encoding='utf-8')

def source(path, symbol):
    lines = (ROOT / path).read_text(encoding='utf-8-sig').splitlines()
    matches = [i+1 for i,l in enumerate(lines) if symbol in l]
    if not matches: raise ValueError((path, symbol))
    # Prefer the definition to a signal/slot connection or invocation.
    line = next((i for i in matches if re.match(r'^(?:void|bool|int|QString|QImage|QList|FvdlInfo|FvprojInfo|LoginResult|DatasetManagement::DatasetStats|static void|static bool).*::', lines[i-1])), matches[0])
    e = dict(path=path, symbol=symbol, line=line)
    EVIDENCE.append(e)
    return e

def S(file, symbol, directory='src/DeepLearningWidget'):
    return source(f'{directory}/{file}', symbol)

def links(evs):
    return '；'.join(f"[{e['path']}:{e['line']} {e['symbol']}]({(ROOT/e['path']).as_posix()}:{e['line']})" for e in evs)

def feature(n, module, page, title, buttons, scene, pre, inputs, steps, result, edges, data, evs, runtime='未执行该操作；以下为静态代码确认。', status='静态代码确认'):
    FEATURES.append(dict(id=f'F{n:02}',module=module,page=page,title=title,buttons=buttons,scene=scene,preconditions=pre,inputs=inputs,steps=steps,success=result,boundaries=edges,data=data,sources=evs,runtime=runtime,status=status))

feature(1,'登录与共享能力','P00','登录与记住密码',['登录','取消','记住密码'],
 '启动应用或切换操作者。','登录框的角色下拉为用户/管理员/超级管理员；密码空时不提交。',
 '选择显示角色，输入密码；记住选项从 FuseVisionLogin 配置恢复。角色固定映射 user/admin/superadmin，不能输入新增用户名。',
 ['点击登录后禁用控件并展示加载状态','DatabaseManager::validateLogin 查询用户名并比较 SHA-256','失败恢复控件并提示“用户名或密码错误，请重试！”','成功写记住设置，SessionManager::login 刷新权限缓存并发出 sessionChanged，关闭登录框'],
 '进入主窗口，状态栏显示当前会话。','取消初始登录导致应用退出；切换用户时取消保留旧会话。勾选记住时 lastPassword 为明文；不勾选会移除记住字段。新增账号登录入口缺失。',
 'SQLite users；QSettings FuseVisionTeam/FuseVisionLogin；无远程认证。',
 [S('Login.cpp','LoginDialog::doLogin','src/MainWindows'),S('Login.cpp','LoginDialog::saveSettings','src/MainWindows'),S('DatabaseManager.cpp','DatabaseManager::validateLogin','src/core')],
 '实际显示登录框；未提交登录按钮。测试使用内存数据库认证并建立会话；startup-smoke 验证正确/错误密码。','实际操作验证 + 静态代码确认')
feature(2,'登录与共享能力','P01','模块导航与状态栏',['深度学习','传统视觉','用户管理','系统设置'],
 '在一级业务模块间切换。','主窗口初始化四页后可点侧边栏；深度学习子页按 canRead 显示。',
 '无参数；默认深度学习首页。','切换 QStackedWidget；更改标题；切换状态栏项目区域。'.split('；'),
 '相应页面显示；用户/项目/模型状态供定位当前上下文。','主导航未按用户管理/系统设置 canRead 隐藏；不能将“页面可进入”等同获得写权限。',
 '共享 SessionManager、PermissionRegistry、ThemeManager；无业务数据写入。',
 [S('FuseVision.cpp','FuseVision::onMenuItemClicked','src/MainWindows'),S('FuseVision.cpp','FuseVision::initPagesAsync','src/MainWindows'),S('DeepLearningWidget.cpp','DeepLearningWidget::applyPermissions')],
 '实际切换四个一级页面及九个深度学习页，保留21个界面状态。','实际操作验证 + 静态代码确认')
feature(3,'登录与共享能力','P01','亮暗主题与日志面板',['切换主题','折叠日志面板'],
 '调整阅读环境并查看模块操作记录。','侧边栏底部主题按钮；深度学习/传统视觉底部日志折叠按钮。',
 '主题从保存值或系统检测恢复；日志最多1000文本块。',
 ['主题按钮调用 ThemeManager::setTheme 并保存 ui/theme','themeChanged 驱动页面重新套用语义调色板和图标','日志面板接收显式消息并HTML转义；可折叠、拖动分隔条'],
 '窗口配色切换；日志面板显隐改变。','日志面板不是 spdlog 文件全部记录的实时镜像；只有接入的业务消息进入。未实测主题切换以避免保存用户偏好。',
 'QSettings ui/theme；spdlog；LogMonitor 文本缓存；Windows DWM。',
 [S('FuseVision.cpp','FuseVision::onToggleTheme','src/MainWindows'),S('ThemeManager.cpp','ThemeManager::setTheme','src/core'),S('LogMonitor.cpp','LogMonitor::log','src/core')])
feature(4,'登录与共享能力','P01','切换用户',['切换用户'],
 '无需重启切换当前操作者。','侧边栏 ⇄ 按钮；会话已存在。','与初始登录相同的三种固定账号。',
 ['打开原 LoginDialog','认证成功由对话框覆盖 Session 并广播变更','PermissionGuard 监听 sessionChanged 更新所关注的权限'],
 '状态栏用户更新，已实现的权限代理刷新。','取消不登出；切换时没有集中处理当前标注未保存状态。普通用户 canWrite=false 时项目新建按钮仍可用。',
 '同一进程的 SessionManager；原项目/数据集上下文继续存在。',
 [S('FuseVision.cpp','FuseVision::onSwitchUser','src/MainWindows'),S('SessionManager.cpp','SessionManager::login','src/core'),S('PermissionGuard.cpp','PermissionGuard::refresh','src/core')],
 '直接调用 SessionManager 切换隔离会话，未提交真实登录框；记录 canWrite=false 且新建按钮 enabled=true。','实际操作验证 + 静态代码确认')
feature(5,'深度学习','P02','新建项目',['新建项目','OK','Cancel'],
 '建立一个视觉任务的容器。','深度学习→项目管理→新建项目。写权限未在处理器校验。',
 '项目名称必填，trim后 ^[A-Za-z0-9_]{1,64}$；描述可空；根目录从设置读取；默认名称为空。',
 ['显示项目名/描述对话框','ProjectConfig::createProject 校验名称和目录冲突','建立项目目录并写 project.fvproj','刷新项目卡片；尚未自动打开项目'],
 '项目列表出现新卡片，用户继续双击打开。','取消对话框不创建；非法名、同名目录、创建/写入失败显示创建失败。文件创建不是跨文件事务。',
 '<project_data>/<项目名>/project.fvproj；JSON记录模型、路径和时间。',
 [S('ProjectManagement.cpp','ProjectManagement::onNewProject'),S('ProjectConfig.cpp','ProjectConfig::createProject')],
 '实际点击新建项目，在临时目录创建 UI_Logic_Demo，截图P02；原业务目录未改动。','实际操作验证 + 静态代码确认')
feature(6,'深度学习','P02','打开与迁移项目',['打开项目'],
 '恢复本地项目或接收其他路径的工程。','项目管理→打开项目。','文件选择器筛选 project.fvproj；默认浏览当前项目数据根目录。',
 ['读取项目JSON与路径','按原路径和目标根目录决定直接打开或复制/修补路径','重新注册模型路径并更新卡片','发出 projectOpened，模型和数据集切换上下文'],
 '当前项目被设置，进入模型管理。','取消无操作；无效JSON/文件读失败提示；复制失败可能留下部分目录。迁移路径分支本次只静态阅读。',
 '.fvproj、models.<id>.model_file、目录递归复制。',
 [S('ProjectManagement.cpp','ProjectManagement::onOpenProject'),S('ProjectManagement.cpp','ProjectManagement::copyDirectory'),S('ProjectConfig.cpp','ProjectConfig::loadProject')])
feature(7,'深度学习','P02','项目预览与正式打开',['项目卡片单击','项目卡片双击'],
 '先查看项目摘要，再进入模型列表。','项目管理右侧卡片；必须有可读 project.fvproj。','无参数；单击选择卡片，双击正式打开。',
 ['单击读取预览信息到 m_previewProject，更新左侧信息和模型摘要','单击发 projectPreviewed，不更改正式打开项目','双击读取并设置 m_project，发 projectOpened','DeepLearningWidget 跳转模型管理'],
 '预览与当前项目分别存在，双击后状态栏更新。','不能把单击预览当作打开；删除时当前已打开项目优先，可能与所预览卡片不同（ISS-004）。',
 '项目JSON；内存 m_previewProject/m_project；Qt signals。',
 [S('ProjectManagement.cpp','ProjectManagement::onCardClicked'),S('ProjectManagement.cpp','ProjectManagement::onCardDoubleClicked'),S('DeepLearningWidget.cpp','DeepLearningWidget::onProjectOpened')],
 '实际双击新建卡片并进入模型页；单击预览分支静态确认。','实际操作验证 + 静态代码确认')
feature(8,'深度学习','P02','删除项目',['删除项目'],
 '清理项目记录或整个项目目录。','左侧删除项目；初始禁用，有选择/已打开项目后启用。',
 '三选项：删除整个项目、仅删除项目文件、取消（默认取消）。目标优先取当前已打开项目。',
 ['计算删除目标并显示确认','删除目录或仅project.fvproj','清空本页项目状态并刷新卡片'],
 '本页列表移除工程。','取消不删；文件权限/删除失败提示。实现未 emit projectClosed，下游模型/数据集可能仍指向删除目录；未执行实际删除。',
 '项目目录或 .fvproj；其他页持有的项目副本。',
 [S('ProjectManagement.cpp','ProjectManagement::onDeleteProject'),S('DeepLearningWidget.cpp','&ProjectManagement::projectClosed')])
feature(9,'深度学习','P03','新建模型',['新建模型','创建模型','浏览','实例分割','异常检测','物体检测(非自由矩形)','物体检测(自由矩形)'],
 '为项目选择任务类型并建立数据工作区。','模型管理→新建模型；要求已正式打开项目。',
 '四种任务，默认第一项实例分割；名称trim后1–64位字母数字下划线；描述可选；界面有路径浏览，但描述/路径未传给创建函数。',
 ['填写对话框并确认','ModelConfig 创建模型目录、data.yaml和.fvdl','将模型登记进 project.fvproj 并设置 current_model_id','刷新卡片并自动选中；此时QSignalBlocker仍生效，相关通知被抑制'],
 '模型卡片创建并选中；继续双击可进入数据集。','未开项目/空名/非法名/重名/文件错误提示；取消不创建。对话框描述实际丢失；新建不等于训练模型。',
 '.fvdl、data.yaml、项目models及status；实例/检测创建images/labels分区骨架，异常检测创建OK/NG目录。',
 [S('ModelManagement.cpp','ModelManagement::onNewModel'),S('ModelConfig.cpp','ModelConfig::createModel')],
 '实际创建SurfaceSeg；QSignalSpy记录modelCreated=0、currentModelChanged=0，模型已选中。','实际操作验证 + 静态代码确认')
feature(10,'深度学习','P03','打开与登记模型',['打开模型'],
 '将现有.fvdl加入当前项目。','模型管理→打开模型；先打开项目。','选择.fvdl，文件浏览可取消。',
 ['读取模型配置','判断是否属于本项目或需要复制','写当前项目模型索引并刷新','设置当前模型并通知数据集'],
 '显示模型卡片和信息。','读取失败/不支持配置提示；跨工程目录冲突和复制分支未操作验证；取消退出。',
 '.fvdl、项目JSON、模型目录。',
 [S('ModelManagement.cpp','ModelManagement::onOpenModel'),S('ModelConfig.cpp','ModelConfig::loadModel')])
feature(11,'深度学习','P03','选择模型与进入数据集',['模型卡片单击','模型卡片双击'],
 '决定当前标注和导出的模型上下文。','模型卡片；需有效.fvdl。','单击选中；双击打开并跳数据集。',
 ['加载.fvdl及当前模型信息','单击发currentModelChanged以绑定数据集','双击额外发modelOpened与openDatasetRequested','编排页切到数据集管理'],
 '数据集载入选中模型，主状态栏显示模型。','模型配置缺失/读失败不能正常加载；本次通过双击绕过新建模型信号被抑制的问题。',
 'FvdlInfo、DatasetManagement当前模型、Qt signals。',
 [S('ModelManagement.cpp','ModelManagement::onCardClicked'),S('ModelManagement.cpp','ModelManagement::onCardDoubleClicked'),S('DatasetManagement.cpp','DatasetManagement::setCurrentModel')],
 '实际双击新模型，确认进入标签页索引2。','实际操作验证 + 静态代码确认')
feature(12,'深度学习','P03','删除模型',['删除模型'],
 '清理不再使用的模型记录或目录。','模型管理左侧删除；要求有效选择。','删除整个目录或仅.fvdl；确认可取消。',
 ['确认模型目标与删除方式','删除文件/目录并更新项目models','清除本页当前模型并刷新'],
 '模型列表更新。','未选择提示；删除失败提示；没有集中广播清空数据集/标注上下文，可能继续操作旧路径。本次未删除。',
 '模型目录/.fvdl及.fvproj索引。',
 [S('ModelManagement.cpp','ModelManagement::onDeleteModel')])
feature(13,'深度学习','P04','导入数据集文件',['导入数据集文件'],
 '将原始图像和可选标签纳入模型工作区。','数据集管理→导入数据集文件；必须有当前模型。',
 '图像支持Qt格式和png/jpg/jpeg/bmp/tiff等兜底；可多选。是否导入标签默认由确认框选择；有标签时先选择类型，支持PNG/JSON选择器。',
 ['格式说明→确认或取消','选择图像→询问标签→选择类型与标签文件','处理重名覆盖确认并逐个复制到images/labels','重扫配置、检测类别、校验数量、刷新图库与统计','更新项目data_imported状态并发dataImported'],
 '图库显示导入图像及标签状态。','无模型警告；说明/文件/类型对话框取消退出当前步骤。重名选择No跳过冲突文件，复制流程没有整体回滚，可能部分成功。',
 '<项目>/<模型>/images、labels、dataset_config.json；QImageReader和JSON解析。',
 [S('DatasetManagement.cpp','DatasetManagement::onImportImages'),S('DatasetManagement.cpp','DatasetManagement::ensureLabelTypeBeforeImport'),S('DatasetManagement.cpp','DatasetManagement::detectLabelsFromAnnotations')],
 '实际调用原导入槽并自动填写真实对话框，导入1张800×520合成PNG和同名单通道掩码；allSamples=1。','实际操作验证 + 静态代码确认')
feature(14,'深度学习','P04','导入数据集文件夹',['导入数据集文件夹'],
 '批量接收已整理数据集。','数据集管理→导入文件夹；需当前模型。',
 '根目录必须含images/；labels/可选。只扫描所选images与labels直接子文件，非递归；标签扩展png/json。',
 ['确认格式说明','选择根目录，校验images存在','labels存在则确认标签格式','统计、复制、重扫、刷新'],
 '平铺文件纳入当前模型。','取消无后续复制；缺images警告；空列表警告。模型创建的images/train骨架与本导入的平铺约定不同，不能假定递归导入分区数据。',
 'images/*、labels/*；文件名用于配对。',
 [S('DatasetManagement.cpp','DatasetManagement::onImportFolder')])
feature(15,'深度学习','P04','类别新增编辑删除导入',['+ 添加','编辑','删除','导入'],
 '定义当前数据集的类别名称和颜色。','左侧标签列表按钮；编辑/删除要求选中类别。',
 '新增名称trim、非空且不重名，然后选颜色；颜色选择默认循环调色板。编辑不可重名。导入JSON必须含categories，按name合并。',
 ['收集名称/颜色或读取JSON类别','修改labelNames/labelColors/counts数组','saveDatasetConfig、更新标签面板、发labelsChanged'],
 '类别面板和标注页名称同步。','文本或颜色取消不新增；未选类别提示；删除有确认。删除只移除类别数组，未同步重写所有PNG/JSON类别ID，存在语义错位风险。',
 'dataset_config.categories；类别索引与标签像素/JSON ID关联。',
 [S('DatasetManagement.cpp','DatasetManagement::onAddLabel'),S('DatasetManagement.cpp','DatasetManagement::onEditLabel'),S('DatasetManagement.cpp','DatasetManagement::onDeleteLabel'),S('DatasetManagement.cpp','DatasetManagement::onImportLabels')])
feature(16,'深度学习','P04','拖动类别顺序',['标签列表拖动'],
 '调整类别显示及编号顺序。','左侧类别列表拖动；须类别数匹配。','新顺序以UserRole旧索引构建old→new映射。',
 ['读取新顺序；未变则退出','重排类别名/颜色/计数','重映射内存samples.categoryIds','保存配置、刷新图库、发labelsChanged'],
 '本次会话的类别顺序与筛选索引更新。','只看到内存索引重映射，未见对labels文件内容重编码；重开后旧PNG灰度仍指向旧ID。本次未执行重排。',
 '内存样本类别与dataset_config；磁盘标注未跟随重写。',
 [S('DatasetManagement.cpp','DatasetManagement::onLabelOrderChanged'),S('DatasetManagement.cpp','DatasetManagement::saveDatasetConfig')])
feature(17,'深度学习','P04','浏览筛选选择与进入标注',['列表视图','网格视图','标签筛选','全选','缩略图双击'],
 '查找待标注样本并批量选择。','数据集右侧图库/列表、标签筛选、全选与缩略图。','当前模型样本；类别筛选按内存categoryIds；无额外文本参数。',
 ['视图/筛选只改变呈现','选中项进入m_selectedIndices供删除和导出使用','双击缩略图发openAnnotationRequested(modelId,path)','编排器绑定标注数据源、打开图片、切换数据标注'],
 '标注页显示图像、类别、已有标注。','空图库无缩略图；模型切换清选择。不要把当前筛选范围自动等同导出范围，导出未选时取全部样本。',
 '样本列表、选中索引、AnnotationManagement。',
 [S('DatasetManagement.cpp','DatasetManagement::refreshGallery'),S('DatasetManagement.cpp','DatasetManagement::onThumbnailDoubleClicked'),S('DatasetManagement.cpp','DatasetManagement::onSelectAll')],
 '实际调用缩略图双击处理槽并进入标注，确认画布hasImage；其他筛选分支静态。','实际操作验证 + 静态代码确认')
feature(18,'深度学习','P04','删除样本与右键菜单',['删除选中','打开所在目录','复制路径','重新标注'],
 '维护图库或定位源文件。','右侧删除选中；缩略图右键菜单。','删除以当前m_selectedIndices为目标；目录/路径来自样本绝对路径。',
 ['删除前展示确认并允许取消','删除图像及关联标签后重扫刷新','打开目录使用桌面服务；复制写剪贴板；重新标注跳标注入口'],
 '样本列表与数量更新或打开目标位置。','未选中不给有效删除；取消不删；文件删除失败处理不等同事务回滚。本次未执行删除、系统目录打开或剪贴板写入。',
 'images及同名labels；QDesktopServices、剪贴板。',
 [S('DatasetManagement.cpp','DatasetManagement::onDeleteSelected'),S('DatasetManagement.cpp','DatasetManagement::showContextMenu')])
feature(19,'深度学习','P04','刷新信息与质量提示',['刷新','打开文件夹','路径点击复制'],
 '外部编辑标签后更新统计并查找缺失文件。','左侧图像信息的刷新/打开文件夹；统计区警告提示。','无参数；对当前模型进行扫描。',
 ['重扫配置和标注、校验类别数量','computeStats统计已/未标注及实例数','runQualityCheck检查缺标注、空JSON、孤立标签，以及已标注样本图像可读性','更新统计和质量告警tooltip'],
 '数量和告警刷新。','质量检查不是完整训练数据验证：损坏图像检查位于已标注分支；空JSON判断有限；不存在全量几何/掩码ID校验。无模型不应解释为健康数据。',
 'QImageReader、labels/*json/png；样本统计缓存。',
 [S('DatasetManagement.cpp','DatasetManagement::onRefreshImageInfo'),S('DatasetManagement.cpp','DatasetManagement::computeStats'),S('DatasetManagement.cpp','DatasetManagement::runQualityCheck')])
feature(20,'深度学习','P04','标注格式转换',['格式转换'],
 '将已有标注变成当前任务需要的YOLO文本。','数据集底部格式转换；要求当前模型。',
 '自动探测YOLO txt、PNG掩码、LabelMe JSON；异常检测单独引导OK/NG。确认默认Yes，原标签保留。',
 ['检测输入格式并校验现有格式','已经符合YOLO时提示无需转换','确认目标说明后按来源格式转换','写labels/*.txt，展示逐项转换结果并刷新'],
 '生成当前任务对应YOLO行；原标注保留。','无标签、未知格式、解析失败有提示/结果项；选择No中止。并非任意格式到任意格式转换器。未用真实训练管线验证输出。',
 'detectInputFormat、validateCurrentFormat及三种转换器；data.yaml任务类型。',
 [S('DatasetManagement.cpp','DatasetManagement::onFormatConvert'),S('DatasetManagement.cpp','DatasetManagement::validateCurrentFormat'),S('DatasetManagement.cpp','DatasetManagement::convertPngMaskToTarget'),S('DatasetManagement.cpp','DatasetManagement::convertAnomalyClassification')])
feature(21,'深度学习','P04','导出数据集',['导出'],
 '将当前数据交给其他标注/训练工具。','数据集底部导出；至少一个样本。','有选中则只导出所选，未选则全部；掩码类优先PNG，其次COCO、YOLO；COCO默认annotations.json。',
 ['计算导出索引并选择格式','选择保存JSON或目标文件夹','PNG复制images/labels；COCO写聚合JSON；YOLO写images/labels和data.yaml'],
 '生成所选格式的数据文件。','无样本警告；任一选择对话框取消退出。导出的是数据集，不能等同顶部“模型导出”；转换保真、重名覆盖与大数据量未运行验证。',
 'writeMaskFormat、writeCocoJson、writeYoloFormat。',
 [S('DatasetManagement.cpp','DatasetManagement::onExportDataset'),S('DatasetManagement.cpp','DatasetManagement::writeCocoJson'),S('DatasetManagement.cpp','DatasetManagement::writeYoloFormat'),S('DatasetManagement.cpp','DatasetManagement::writeMaskFormat')])
feature(22,'深度学习','P04','数据增强入口',['数据增强'],
 '界面展示未来增强能力入口。','数据集底部数据增强。','无可执行增强参数。',['点击后仅显示后续版本提示'],
 '显示说明弹窗。','未执行旋转/翻转/噪声等实际增强，不产生扩增样本。',
 '无增强算法调用。',[S('DatasetManagement.cpp','DatasetManagement::onAugmentClicked')])
feature(23,'深度学习','P05','加载图像与标注工具',['画笔','橡皮','多边形','矩形','填充','选择'],
 '在图像上创建和编辑区域。','数据标注左侧六工具；当前图像与数据源存在。','类别默认当前选中索引；画笔尺寸用界面值/[ ]调整；矩形两次点击或Enter完成，宽高<3忽略；多边形双击/Enter闭合，Esc取消。',
 ['openImage加载源图、旧标注到m_shapes，再复制到AnnotationModel','画布鼠标坐标映射为图像坐标并分发给活动工具','工具Transaction写AnnotationModel并触发重绘/脏状态','列表和部分render/save仍使用旧m_shapes，状态有分裂'],
 '新形状可进入内存模型；当前实现不能保证列表、保存与画面一致。','矩形提示“拖拽”但实际mousePress二次完成，单次拖拽仅预览。右键/Esc等取消按工具实现；未对六工具全部进行往返验证。',
 'AnnotationCanvas、AnnotationModel、AnnotationRender和Tools；QImage/QPainter，无模型推理。',
 [S('AnnotationManagement.cpp','AnnotationManagement::openImage'),S('AnnotationManagement.cpp','AnnotationManagement::onCanvasMousePressed'),S('Tools/RectTool.cpp','RectTool::endRect'),S('AnnotationManagement.cpp','AnnotationManagement::renderOverlay')],
 '实际加载PNG、两次点击完成矩形；模型形状数2，旧列表仍1。保存像素验证失败见ISS-001。','实际操作验证 + 静态代码确认')
feature(24,'深度学习','P05','保存与自动保存',['保存标注','Ctrl+S'],
 '保留本张标注并更新数据集统计。','标注顶部保存；30秒无数据变更的单次定时器可自动触发。','当前图像与DatasetSource；labelOffset固定返回1；二值掩码将非零映射255。',
 ['读取旧m_shapes而非现代工具的AnnotationModel','掩码写同名PNG；其他类型写逐图JSON','通知Dataset::onAnnotationUpdated更新类别/实例数','无论文件写入是否成功均清m_modified；手动入口写“标注已保存”日志'],
 '文件可被写出，但新工具修改可能缺失；不能以成功日志或文件存在作为保存成功证据。','PNG save返回值未检查，JSON打开/写失败被忽略；保存后未markClean模型。现有导入掩码仍存在会造成“保存成功”假象。',
 'labels/<basename>.png/json；dataset_config统计；AnnotationRender::compileMask。',
 [S('AnnotationManagement.cpp','AnnotationManagement::saveAnnotations'),S('AnnotationManagement.cpp','AnnotationManagement::onSave'),S('DatasetManagement.cpp','DatasetManagement::currentLabelOffset')],
 '实际保存后文件可读，但新矩形内(500,300)像素0；模型dirty=true，界面dirty=false。','实际操作验证 + 静态代码确认')
feature(25,'深度学习','P05','撤销重做与形状列表',['撤销','重做','删除选中','标注列表右键','Ctrl+C','Ctrl+V','Ctrl+D'],
 '纠正标注或复制形状。','顶部Undo/Redo；右侧标注列表和键盘快捷键。','选中形状；撤销按钮根据旧m_undoStack能力启用；模型另有50条Undo栈。',
 ['列表按m_shapes生成','隐藏/删除/复制等旧路径修改m_shapes','现代工具Transaction写模型自有Undo栈','顶部onUndo/onRedo操作旧栈'],
 '旧路径能更新旧列表；现代工具历史与顶部撤销未统一。','空选择或无撤销历史无动作；不能保证新工具画出的对象可从旧列表删除/撤销。',
 '两份形状、两套QUndoStack及剪贴板状态。',
 [S('AnnotationManagement.cpp','AnnotationManagement::updateShapeList'),S('AnnotationManagement.cpp','AnnotationManagement::onUndo'),S('AnnotationManagement.cpp','AnnotationManagement::onDeleteSelectedShapes'),S('AnnotationModel.cpp','AnnotationModel::undo')])
feature(26,'深度学习','P05','视图调整与图像导航',['适应窗口','原始大小','放大','缩小','第一张','上一张','下一张','最后一张','返回数据集管理'],
 '检查细节并连续标注。','顶部缩放、左侧胶片导航、右侧导航器/亮度对比度、底部缩放。','亮度/对比度默认0；截图透明度60%；缩放由画布实现；索引必须在样本范围。',
 ['缩放或亮度对比度生成显示图，不改原图','导航切图前若m_modified则调用saveAnnotations','返回图库也先按m_modified尝试保存，再发returnToGallery','编排层重新识别标签并切回数据集'],
 '视图或当前图像变化，返回图库刷新统计。','边界索引忽略；切图/返回没有基于写入结果的阻断与保存/放弃/取消选择。m_modified失真时可跳过保存。',
 'm_sourceImage/m_displayImage；样本列表；标签目录。',
 [S('AnnotationManagement.cpp','AnnotationManagement::onNavigateTo'),S('AnnotationManagement.cpp','AnnotationManagement::onReturnToGallery'),S('AnnotationManagement.cpp','AnnotationManagement::applyImageAdjustments')],
 '实际返回数据集页；多图导航、调整参数未覆盖。','实际操作验证 + 静态代码确认')
feature(27,'深度学习','P05','外部LabelMe嵌入',['LabelMe','LabelMe右键选择Python'],
 '调用外部标注器处理当前图像。','顶部LabelMe，要求图像；右键可重选Python。','从设置读取Python路径，无效时发现环境并选择；参数包含图像目录、当前图、类别、输出labels目录。',
 ['探测Python和脚本路径（优先源码src/labelme，备用运行目录下src/labelme）','发labelMeRequested到DeepLearningWidget','QProcess启动并等待5秒，解析输出窗口句柄','Windows SetParent嵌入，退出时terminate，3秒后必要时kill','清理后重扫标签'],
 '外部窗口嵌入，退出刷新图库。','取消环境选择中止；Python/脚本缺失、进程启动失败、窗口句柄未出现为不同失败点。仓库SDK布局与源码硬编码路径不同，须实际验证，未启动外部Python。',
 'QProcess、Python/LabelMe、Win32 HWND、输出PNG/JSON。',
 [S('AnnotationManagement.cpp','AnnotationManagement::onLaunchLabelMe'),S('DeepLearningWidget.cpp','DeepLearningWidget::enterLabelMeMode'),S('DeepLearningWidget.cpp','DeepLearningWidget::exitLabelMeMode')])
for n,page,title in [(28,'P06','数据检查'),(29,'P07','数据拆分'),(30,'P08','模型训练'),(31,'P09','模型预测'),(32,'P10','模型导出')]:
 feature(n,'深度学习',page,title+'占位页',[title], '了解当前版本的模块边界。','深度学习顶部同名标签；需该权限点canRead。','无参数表单、无执行按钮。',['点击标签切换至占位Widget'], '显示待开发说明。','没有已接通的检查/拆分/训练/预测/模型导出流程。数据集页面有局部质量统计及数据导出，不代表这些独立页已完成。','页面Widget与权限键；无训练器/推理器调用。',[S('DeepLearningWidget.cpp','DeepLearningWidget::initUI')], '实际逐页显示并截图，确认占位状态。','实际操作验证 + 静态代码确认')
feature(33,'传统视觉','P11','工具栏与三区布局',['流程文件','相机配置','相机标定','相机建模','通讯设置','系统设置','采集','预处理','定位','测量','识别'],
 '查看传统视觉规划入口。','侧边栏传统视觉；工具栏按对应读写权限控制enabled。','无实际算法参数；资源树显示示例流程和未连接相机。',
 ['构造资源树、编辑器占位、视觉工具箱','PermissionGuard关注七个传统权限键','applyPermissions控制动作可用状态'],
 '显示流程编辑器/图像显示及五类工具的待开发布局。','工具栏动作未接通业务triggered处理；不能采集相机、标定或执行视觉流程。本次只观察，不将SDK存在视为业务可用。',
 'Qt工具栏/树/堆叠页；sdk/MVS、Halcon尚无src业务调用证据。',
 [S('TraditionalWidget.cpp','TraditionalWidget::initToolBar','src/TraditionalWidget'),S('TraditionalWidget.cpp','TraditionalWidget::applyPermissions','src/TraditionalWidget')],
 '实际显示原始三区布局和占位文案。','实际操作验证 + 静态代码确认')
feature(34,'用户管理','P12','添加删除用户与重置密码',['添加用户','删除用户','重置密码'],
 '管理本机操作账号。','侧边栏用户管理；编辑由PermissionGuard的用户管理canWrite决定。','添加用户名trim且非空，密码非空，角色三选；重置密码重复输入校验；数据库用户名UNIQUE。',
 ['有写权限才进入写处理器','添加用户写users并补齐每模块默认权限','删除选中用户需确认，禁止删除当前登录者','重置选中账号密码并保存SHA-256'],
 '刷新用户表或提示操作结果。','取消不写；重名/SQL失败提示；没有保证每角色至少一个账号的检查。新增用户名目前不能在登录页选择。未在真实数据库操作。',
 'users(id,username,password_hash,role)；user_permissions；无登录历史字段。',
 [S('UserManagementWidget.cpp','UserManagementWidget::onAddUser','src/MainWindows'),S('UserManagementWidget.cpp','UserManagementWidget::onDeleteUser','src/MainWindows'),S('UserManagementWidget.cpp','UserManagementWidget::onResetPassword','src/MainWindows')],
 '实际显示内存种子用户；CRUD未执行。','实际操作验证 + 静态代码确认')
feature(35,'用户管理','P12','角色和模块权限矩阵',['角色下拉','可读(Read)','可写(Write)'],
 '为单个用户调整模块授权。','先选用户行；下方矩阵按模块分组。','角色0/1/2；18个模块点。权限矩阵作用于选中用户，不是角色模板。',
 ['选择用户，读取其权限矩阵','角色下拉写users.role，并按新角色重置各模块默认权限','勾选权限写user_permissions，刷新Registry缓存'],
 '选中用户的权限落库。','缓存refresh本身不发权限变更通知；其他Guard主要监听sessionChanged，即时生效范围不完整。界面没有全局按角色保护最后账号。',
 'PermissionRegistry缓存、PermissionGuard快照、SQLite模块权限表。',
 [S('UserManagementWidget.cpp','UserManagementWidget::refreshUserTable','src/MainWindows'),S('UserManagementWidget.cpp','UserManagementWidget::onPermissionChanged','src/MainWindows'),S('PermissionRegistry.cpp','PermissionRegistry::refreshPermissions','src/core')])
feature(36,'系统设置','P13','路径与日志级别保存',['浏览...','保存配置'],
 '设置项目/日志/数据库及传统视觉数据位置。','侧边栏系统设置；本页未见PermissionGuard或写权限校验。','日志、数据库目录、项目、传统数据、相机配置五种路径；日志级别Off/Debug/Info/Warn/Error，默认Info(2)。路径浏览取消保持字段。',
 ['浏览目录仅改文本框','保存将路径标准化并设置SettingsManager内存值','QSettings同步保存','重新初始化Logger使日志路径与级别生效'],
 '提示已保存；数据库连接保留原位置，重启生效。','未提供数据迁移/备份；项目管理缓存根目录，不能将“立即生效”提示扩展到已存在页全部路径。无路径可写性统一校验。',
 'QSettings FuseVisionTeam/FuseVision；原构造在Windows通常为Native注册表而非注释所称.ini；SQLite当前连接。',
 [S('SystemSettingsWidget.cpp','SystemSettingsWidget::onSave','src/MainWindows'),S('SettingsManager.cpp','SettingsManager::save','src/core'),S('ProjectManagement.cpp','ProjectManagement::ProjectManagement')],
 '实际显示设置页；路径在内存覆盖为临时路径；未保存、重置或清理。','实际操作验证 + 静态代码确认')
feature(37,'系统设置','P13','恢复路径默认值',['重置'],
 '恢复默认路径配置。','系统设置→重置。','无确认参数；默认相对applicationDir的logs/data/project_data/traditional_data/traditional_camera。',
 ['将五种路径设置为默认','立即save持久化','loadSettings刷新界面'],
 '五种路径显示为默认值。','不是完整配置重置：不重置日志级别/Python/主题，未重新初始化Logger，也不迁移或备份旧文件。未执行。',
 'QSettings路径字段；当前Logger与数据库仍有自己的活动状态。',
 [S('SystemSettingsWidget.cpp','SystemSettingsWidget::onReset','src/MainWindows')])
feature(38,'系统设置','P13','清理旧日志',['清理旧日志','清理','取消'],
 '删除超过保留期的日志文件。','系统设置→清理旧日志。','保留天数1–365，默认30；仅匹配fusevision_*.log，按lastModified比较。',
 ['弹出保留天数对话框','确认后校验日志目录','逐文件删除匹配且超龄文件','显示删除数量或没有符合条件'],
 '匹配的旧日志被删除。','取消不删；目录不存在警告；Logger构造的文件basename无扩展名，与清理过滤器兼容性待运行验证（不将其列为已复现故障）。本次未清理。',
 '配置日志目录、QFileInfo修改时间、QFile::remove。',
 [S('SystemSettingsWidget.cpp','SystemSettingsWidget::onCleanLogs','src/MainWindows'),S('Logger.cpp','Logger::init','src/core')])

def build_inventory():
    runtime = json.loads((LOGIC/'evidence/runtime-ui.json').read_text(encoding='utf8'))
    font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',11)
    titlefont=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',18)
    pages = {}
    atlas=['# 真实界面与按钮编号','', '日期：2026-11-14。原始应用Widget在Qt offscreen平台真实渲染，非手绘界面。示例图像为明确标注的合成验证数据。原图不加编号，副本只叠加编号。', '', 'P02/P03对应项目/模型主页面；创建弹窗分别P02D/P03D。BPxx-编号在同页按文字或tooltip稳定匹配。隐藏控件未列入当前截图，但完整静态入口见功能清单。', '', '截图不代表所有按钮都已执行；具体操作范围见每个F节点runtime及运行证据。', '']
    button_rows=[]
    for capture in runtime['captures']:
        base=capture['id'][:3]
        if 'new-project-dialog' in capture['id']: base='P02D'
        if 'new-model-dialog' in capture['id']: base='P03D'
        mapping=pages.setdefault(base,{})
        im=Image.open(LOGIC/capture['file']).convert('RGB')
        canvas=Image.new('RGB',(im.width,im.height+46),'#ffffff');canvas.paste(im,(0,46));draw=ImageDraw.Draw(canvas)
        draw.text((10,10),f"{base} | 真实Qt界面编号副本 | 圆点01对应B{base[1:]}-01 | {capture['id']}",font=titlefont,fill='#172554')
        atlas += [f"## {capture['id']} — {capture['description']}",'',f"[原图]({capture['file']}) · [编号副本](screenshots/annotated/{capture['id']}.png)",'', '| 按钮ID | 界面文字/提示 | 可用 |','|---|---|---|']
        occurrences=collections.Counter()
        for c in capture['controls']:
            label=c['tooltip'] or c['text'] or next(iter(c.get('childLabels',[])),c['class'])
            occurrences[label]+=1
            key=label+f" [同页第{occurrences[label]}处]"
            if key not in mapping: mapping[key]=f'B{base[1:]}-{len(mapping)+1:02}'
            bid=mapping[key];c['buttonId']=bid;c['displayLabel']=key
            labelmd=key.replace('\n',' / ').replace('|','/')
            atlas.append(f"| {bid} | {labelmd} | {'是' if c['enabled'] else '否'} |")
            x=c['x'];y=c['y']+46
            draw.rectangle((x,y,x+c['width']-1,y+c['height']-1),outline='#b91c1c',width=1)
            cx=max(12,min(x+c['width']-4,im.width-12));cy=max(57,min(y+2,canvas.height-12))
            draw.ellipse((cx-11,cy-11,cx+11,cy+11),fill='#b91c1c',outline='white')
            draw.text((cx-7,cy-8),bid.rsplit('-',1)[1],font=font,fill='white')
            button_rows.append(dict(capture=capture['id'],page=base,buttonId=bid,**{k:v for k,v in c.items() if k!='buttonId'}))
        dest=LOGIC/'screenshots/annotated'/f"{capture['id']}.png";dest.parent.mkdir(exist_ok=True);canvas.save(dest)
        atlas += ['']
    for f in FEATURES:
        f['buttonIds']=[]
        related=[f['page']]+(['P02D'] if f['id']=='F05' else ['P03D'] if f['id']=='F09' else [])
        def normalized(s):
            return re.sub(r'[^A-Za-z0-9\u4e00-\u9fff+]', '', s)
        wanted={normalized(b) for b in f['buttons']}
        for c in button_rows:
            if c['page'] not in related: continue
            tooltip_head=re.split(r'\n|\s+[(（]',c['tooltip'])[0]
            candidates=[c['text'],tooltip_head]+c.get('childLabels',[])[:1]
            if any(normalized(label) in wanted for label in candidates if label):
                f['buttonIds'].append(c['buttonId'])
        f['buttonIds']=list(dict.fromkeys(f['buttonIds']))
    put('docs/logic/SCREENSHOTS.md','\n'.join(atlas))
    put('docs/logic/button-index.json',json.dumps(button_rows,ensure_ascii=False,indent=2))
    put('docs/logic/evidence/runtime-ui.json',json.dumps(runtime,ensure_ascii=False,indent=2))
    put('docs/logic/features.json',json.dumps(dict(date=DATE,project='FuseVision',scope='current dirty workspace; business source unchanged',features=FEATURES),ensure_ascii=False,indent=2))
    md=['# FuseVision 功能、页面与按钮清单','', '阅读顺序为软件→模块→页面→入口→操作过程。证据来自当前工作区代码与2026-11-14隔离运行；不将README设想作为已完成功能。实际操作验证仅覆盖条目中明确列出的动作，其余分支仍是静态代码确认。', '', '[主链路](CORE-FLOW.md) · [截图与按钮编号](SCREENSHOTS.md) · [数据契约](DATA-CONTRACTS.md) · [机器可读清单](features.json)', '', '| 功能ID | 模块/页 | 功能 | 证据状态 |','|---|---|---|---|']
    md += [f"| {f['id']} | {f['module']}/{f['page']} | [{f['title']}](#{f['id'].lower()}) | {f['status']} |" for f in FEATURES]
    for f in FEATURES:
        md += ['',f"<a id=\"{f['id'].lower()}\"></a>",f"## {f['id']} {f['title']}",'',f"- **位置与入口**：{f['module']} → {f['page']} → {' / '.join(f['buttons'])}。截图按钮：{', '.join(f['buttonIds']) or '非按钮入口或未捕获的对话框，按文字与代码定位'}。",f"- **使用场景**：{f['scene']}",f"- **可用条件**：{f['preconditions']}",f"- **输入、默认与校验**：{f['inputs']}",'', '**处理与状态变化**：','']
        md += [f'{i}. {s}' for i,s in enumerate(f['steps'],1)]
        md += ['',f"**结果**：{f['success']}",'',f"**失败、取消与边界**：{f['boundaries']}",'',f"**数据、接口与算法**：{f['data']}",'',f"**代码证据**：{links(f['sources'])}",'',f"**验证范围**（{f['status']}）：{f['runtime']}"]
    put('docs/logic/FEATURES.md','\n'.join(md))
    return runtime

if __name__ == '__main__':
    build_inventory()
    put('docs/logic/evidence/source-references.json',json.dumps(EVIDENCE,ensure_ascii=False,indent=2))
    print(f'Generated {len(FEATURES)} feature records and screenshot atlas')
