# 业务数据与文件契约（2026-11-17）

本次修复范围：导入前校验、旋转框平移、数据库配置与异常分类链路。ISS-011～014回归通过，证据见[四项问题修复](VALIDATION.md#2026-11-17-四项问题修复)；其余格式未因此获得完整重新验收。

2026-11-17新标准：用户明确软件尚未完成开发，无需兼容旧原生数据。项目/模型/数据集配置统一采用完整1.0文档，拒绝无标记旧格式与旧布局；[本轮验证](VALIDATION.md#2026-11-17-严格原生存储标准)记录40项Release测试通过。真实历史文件、账户和设置没有删除或批量转换，COCO/YOLO/LabelMe等外部格式继续保留。

## 从页面到存储

模块整理后，AnnotationShape、DatasetSample/LabelTypes、FvprojInfo、FvdlInfo/ModelTaskTypes由`src/deeplearning/domain`提供。数据服务只接收这些类型，不包含业务Widget；编辑模型、渲染、工具和存储同属无Widgets数据目标。`core/AccessControl`判定会话，`ui/WriteAccess`显示拒绝反馈；文件服务在写检查点使用调用方授权回调。磁盘格式与编号以以下契约为准。

| 对象 | 存储 | 权威读取/写入方 |
|---|---|---|
| 项目 | project.fvproj JSON | ProjectConfig，QSaveFile |
| 模型 | `<model>/<model>.fvdl`、data.yaml | ModelConfig，描述实际落盘 |
| 数据集 | images/、labels/、dataset_config.json | DatasetManagement编排，DatasetFileService处理文件 |
| 编辑态 | AnnotationShape列表、保存基线、QUndoStack | AnnotationModel |
| 标签 | labels/同基名.png或.json；可读YOLO txt | AnnotationStore、DatasetFileService::readShapes |
| 管理账户 | SQLite users；历史user_permissions保留但不读取/写入 | DatabaseManager、SessionManager；PermissionRegistry只保留页面键兼容接口 |
| 配置 | QSettings | SettingsManager与主题；不记住登录密码，显式登录成功后清理旧记住字段 |

## 项目、模型和恢复

项目字段要求 project_name、description、created_time、modified_time、paths、models、current_model_id；保存保留未知根字段。`paths.project_root`必须为`.`，活动根取所选project.fvproj父目录。登记键为合法模型名，`model_file`必须是该模型目录内的正斜线相对.fvdl路径，拒绝绝对路径、`..`、非规范路径和项目外引用；`dataset_folder`等于模型名。status含data_imported/annotated/split_done/trained四个布尔字段；current_model_id为空或引用已登记模型。加载只在内存解析model_file为绝对路径，移动项目后仍依据所选文件解析；保存把合法根内绝对内存引用转为相对路径。移除旧绝对根重定位分支。默认目录改变不搬移已有项目；外部模型由现有导入流程复制到项目内后登记。

模型要求model_name、model_type、description、created_time、modified_time、project_ref、dataset_ref、dataset_layout、architecture、training_versions、current_version、inference_config。project_ref为合法项目名，dataset_ref等于model_name；实例/检测布局必须为native，异常分类必须为classification，拒绝旧yolo布局。名称统一为`[A-Za-z0-9_]{1,64}`，拒绝大小写不敏感的CON/PRN/AUX/NUL/COM1～9/LPT1～9设备名。保存先验证磁盘原文件再验证输出；非法输入不覆盖原配置。新建模型同时生成.fvdl、data.yaml和完整dataset_config.json，未登记成功时回滚仅本次新建目录。

### 原生配置版本与编码

| 文件 | 必须的document_type | 必须的format_version | 规则 |
|---|---|---|---|
| project.fvproj | fusevision.project | 字符串1.0 | 相对根与项目内登记引用 |
| `<model>.fvdl` | fusevision.model | 字符串1.0 | 明确任务、引用及native/classification布局 |
| dataset_config.json | fusevision.dataset | 字符串1.0 | 明确模式、标签格式、完整类别数组和统计字段 |

原生配置使用UTF-8、缩进JSON和LF。缺少标记、数字版本、null、未知版本、错误文档类型及缺少必填字段均拒绝读取/覆盖，不提供旧格式自动升级。打开只读；旧文件保留原字节，需使用新标准重新创建数据。

数据集必须含dataset_mode、label_type、categories、image_count和instance_count。mode只能为annotation或classification，并与模型布局一致。annotation标签格式为LabelTypes已定义的PNG/JSON类型；classification标签类型固定为OK/NG分类，类别依次固定OK、NG。类别项含连续1-based的id、唯一非空name、字符串color和整数instance_count（类别计数-1表示未知，其余非负）；顶层image_count/instance_count必须为非负整数。新annotation类别数组可为空；新classification立即写入OK/NG两类和零统计。缺少dataset_config.json时拒绝加载/保存，不再推断模式或静默补建。数据集保存合并最新磁盘对象的未知根字段；非法重载保留已有内存上下文并记录原因，保存拒绝改写。该行为不是并发冲突检测或文件锁。

版本与字段守卫集中于[StorageContract](../../src/deeplearning/storage/StorageContract.h)，由项目/模型读写、项目扫描、数据集读写和模型统计面板共同消费。外部COCO/LabelMe/YOLO不强加此原生配置封装；标签仍沿用fusevision_version=1或PNG元数据规则。恢复restore.json沿用已有数组格式。

### 工作目录与运行文件归属

```text
<project-root>/
  project.fvproj
  <annotation-model>/
    <annotation-model>.fvdl
    data.yaml
    dataset_config.json          # 创建模型时生成完整1.0配置
    images/<sample>.<image-ext>
    labels/<sample>.png|json|txt
  <classification-model>/
    <classification-model>.fvdl
    data.yaml
    dataset_config.json
    train|val|test/OK|NG/<sample>.<image-ext>
```

新标注模型用 `dataset_layout=native`，只创建平铺images/labels；data.yaml为工作目录索引，写 `path: .`、layout/images/labels及任务，不伪造train/val/test分区和background类别。类别权威源是dataset_config.json。新分类模型仍创建六个固定目录；YAML保留task=classify和train/val/test相对引用。新建两类YAML均为UTF-8/LF，path相对于YAML所在模型目录解释。原生索引不是已完成的数据划分或可直接训练配置；训练用YOLO目录/YAML由导出生成。旧yolo布局拒绝打开，原文件仍保留；普通图库只读取images/labels直属样本，不自动吸收嵌套分区。

| 运行对象 | 固定名称或位置规则 | 生命周期/约束 |
|---|---|---|
| 项目根 | 设置paths/project_data；默认exe/project_data | 首次使用固定本次活动根；存量项目不搬移 |
| 账户库 | 设置paths/database下fusevision.db；默认exe/data/fusevision.db | 下次启动消费；一致快照/来源保留规则见下文 |
| 用户配置 | QSettings，FuseVisionTeam/FuseVision | 沿用平台存储（Windows默认注册表）；不另造INI或复制密码 |
| 日志 | 设置paths/log；默认由启动组合根指定 | 既有目录或非.log路径均作目录（含logs.v1），显式.log作自定义文件前缀；实际每日文件仍为basename_YYYY-MM-DD |
| 项目/模型恢复 | 调用方选定的.fusevision-trash，UUID隔离目录 | 同卷改名；不自动清空，不承诺自动恢复UI |
| 样本恢复 | .fusevision-trash内独立批次与restore.json | original为原绝对路径，recovery为批次内文件名；跨机器恢复需人工核对 |
| 构建/文档派生物 | out/build、out/install、out/docs | 不作为项目数据或测试样本根；测试全部使用临时目录/隔离设置和库 |

项目/模型删除采用同卷改名进入 .fusevision-trash，目录名含UUID；当前对象成功移出后清空下游状态。样本恢复子目录含 restore.json，逐项记录 original 与 recovery；单个样本配对提交失败尝试移动回原处，恢复不完整则保留目录并反馈。恢复需手动关闭占用后按记录移回，模型条目可重新打开导入；尚无自动恢复/保留期UI。

## 类别编号与几何

内存和YOLO类别编号从0开始；逐图原生JSON/COCO类别从1开始；PNG背景0、类别像素=index+1，最多255个类别。二值任务把所有非零映射255，读取映射到类别0。保持现有任务映射：InstanceSeg→segment、AnomalyDetect→classify、ObjDetectFixed→obb、ObjDetectFree→detect，命名争议待Q03。

有标签文件或未保存编辑时拒绝类别删除/重排；原生配置拒绝空类别名。重命名不重编码标签；新增追加。外部COCO类别可以稀疏编号，导入按名字映射到稳定槽位，不直接用category_id当数组下标。

bbox为像素[x,y,width,height]；polygon为像素顶点；rotated shape保留bbox/rotation/rot_center。YOLO detect为归一化中心/宽高，segment/obb为归一化顶点；越界与非法数值拒绝转换，不静默裁切。精度为6位小数，不能承诺浮点逐位无损。

ISS-012修复：平移及复制偏移同步更新bbox与非空显式rot_center；空中心仍表示使用bbox中心。撤销/重做及保存重开保持这些字段。90°旋转框首角点屏幕位移和中心、复制偏移已有回归。

## 原生标签与外部编辑

PNG像素仍是标准类别掩码，同时在文本键 `FuseVision.Annotation.v1` 内嵌形状数组，含类型、ID、UUID、可见性、坐标与画笔层PNG，以保留多个形状和隐藏内容。读取时按原生形状重新渲染并比较实际像素；外部编辑改变像素后，忽略过期形状并按当前像素重建类别掩码。

JSON保留images/annotations，并附 fusevision_version=1、fusevision_shapes。新增 fusevision_annotations_sha256 将元数据绑定标准annotations；外部修改annotations后回退读取标准数据。无该摘要的旧原生文件仍可读。隐藏形状仅保留在原生元数据中，不参与标准标注或导出。

不带元数据的PNG只能恢复每类别掩码，不能推断原始实例划分。普通逐图JSON可读bbox、rotation、polygon；LabelMe支持rectangle（两点）和polygon，circle等明确拒绝。COCO非压缩RLE支持列主序与完整长度校验；压缩字符串RLE需要先用外部COCO工具解码，多多边形按掩码合并保留同实例像素。检测JSON不能保存画笔层时明确拒绝，避免丢失区域语义。

## 导入、转换、导出

图像与标签同基名精确配对。普通导入目前选择png/json；不自动递归吸收YOLO train/val/test目录。冲突整体跳过或成对覆盖；同基名不同图像扩展名、多图/多标签、已有其他格式遮蔽均拒绝并提供源/目标路径。每组先暂存与备份再提交，失败恢复，批量完成项可保留。

ISS-011修复：普通JSON/PNG在备份与覆盖前用AnnotationStore::load检查类别、形状、坐标及尺寸，类别发现仍允许追加新LabelMe名称和非负编号槽位（上限65535）。非法bbox、编号、不支持的形状及非数值LabelMe点报告失败，旧图像/标签字节保持；有效新类别、二值掩码及仅含隐藏形状的原生PNG/JSON回归通过；类别发现同时读取原生元数据，不能只看可见像素或标准annotations。语义验证与成对备份均不是断电事务。

ISS-014当前标准：异常模型固定classification布局，dataset_mode明确为classification且label_type为OK/NG分类，扫描train/val/test各自OK/NG直属图像。内存类别0=OK、1=NG，fileName为phase/class/name，split来自目录；每图计1个分类样本。类别固定，不允许新增、改名、删除或重排；双击只读预览，标注入口拒绝分类数据。导入跳过同名目标并报告解码失败；导出保留目录、同名异类图像及data.yaml(task=classify)，支持明确选中范围。用户已取消旧原生数据兼容要求；显式旧二值配置不再作为异常模型回退模式。普通PNG/JSON外部导入及二值标签读取能力仍保留。

导出目标必须为空，COCO写所选JSON名称及图像；YOLO写images/train、labels/train、data.yaml（这里的train只是现有导出布局，并非本轮开发数据划分）；PNG写images/labels。掩码导出仅接收原生PNG标签。COCO类别id与annotations.category_id一致，刷层用非压缩RLE；YOLO从掩码到包围区域有损，detect丢失旋转/轮廓，非四点轮廓到obb可能退化为水平包围框，报告必须展示限制。

无标签图像与显式空标注不同；COCO/YOLO导出都可能表现为空目标，因此报告提示核对负样本。导出失败清理暂存数据，不修改已有目标；独立目录重读测试覆盖编号、坐标、图像、负样本与选中范围。

样本与类别实例统计从已提交标签的可见形状重新读取，不将包含某类别的图片数当实例数，也不将旧配置作为统计权威。相同图片的两个同类形状计为两个实例；原生形状元数据缺失时，普通PNG按每类别掩码恢复，不能推断原实例数。载入、成功保存和类别检测刷新派生统计。

## 权限及配置

所有页面可读，实际写入入口统一要求SessionManager已登录。AccessControl与PermissionGuard都从同一会话判断，不按角色/模块细分。转换、导出、原生标注、密码修改与系统设置均为写操作。会话变更立即刷新按钮，不隐藏页面。退出登录前先完成未保存决策。

新数据库仅创建superadmin/admin123。已有库不删除旧账户或权限表，不重置密码：优先superadmin，否则按历史role降序、id升序选一个账户；validateLogin和会话均只接受该账户。旧权限表不会覆盖“登录即可编辑”的规则。账户页只改自身密码，数据库层也校验已登录及当前密码；失败保留原hash。移除明文记住密码的读入/写入，成功登录或改密后删除旧remember/lastUser/lastPassword键。SHA-256旧hash兼容保留，未做密码算法迁移。

日志目录/级别立即生效；项目默认根首次使用时固定，不迁移既有数据。数据库目录paths/database在下次启动消费，文件名固定fusevision.db；空配置使用exe/data。database/last_opened_file记录最近成功默认启动使用的绝对文件路径，显式测试路径和:memory:不写该键。目标已存在时直接复用，不覆盖或合并；目标不存在时从最近活动库（无记录时为exe/data/fusevision.db）通过SQLite VACUUM INTO建立一致副本，包含已提交WAL页，成功后同目录改名提交，保留来源。首次安装且无旧库才创建默认账户；曾记录的来源缺失或目录/迁移失败时停止并反馈，不回落新建默认库。运行中的连接不随设置保存切换；重置路径后若默认目标已存在，将使用该目标自身账户。传统视觉数据/相机目录尚无执行模块消费，仅为持久化配置。重置仅五项路径，不包含日志级别、Python和主题。设置保存反馈错误并恢复内存值，可修复存储后重试；不宣称QSettings多键写入具有数据库事务或断电原子性。

日志清理按实际活动Logger目录和basename匹配 `basename_YYYY-MM-DD`（兼容.log），日期与mtime均过期才删除，不删除当日日志或符号链接。删除失败逐项报告。

## Qt原生编辑显示状态（2026-11-17）

类别、几何、mask、UUID和visible仍由AnnotationModel/AnnotationStore持有，磁盘格式不变。列表选择按UUID对应；叠加层显示、亮度、缩放和保持视野仅为会话状态，不修改visible或dirty。未完成绘制阻止保存；保存成功后才前进下一张，末张停留，失败保留编辑与当前文件。LabelMe运行配置为兼容遗留字段，不再参与执行；LabelMe JSON读取兼容继续存在。
