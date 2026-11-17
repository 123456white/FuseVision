"""Build editable XMind source with true UI images and explicit flow relations."""
import sys
sys.dont_write_bytecode = True
from build_analysis import ROOT, LOGIC, put
from PIL import Image, ImageDraw, ImageFont
import json

inventory=json.loads((LOGIC/'features.json').read_text(encoding='utf8'))
features=inventory['features']
issues=json.loads((ROOT/'docs/refactor/issues.json').read_text(encoding='utf8'))
runtime=json.loads((LOGIC/'evidence/runtime-ui.json').read_text(encoding='utf8'))
sheets=[]
thumb=LOGIC/'screenshots/xmind';thumb.mkdir(exist_ok=True)
for c in runtime['captures']:
    im=Image.open(LOGIC/c['file']).convert('RGB')
    im.thumbnail((520,350))
    im.save(thumb/(c['id']+'.png'))

def node(id,title,note='',children=None,status='静态代码确认',image=None,sources=None,labels=None):
    v=dict(id=id,title=title,status=status)
    if note:v['note']=note
    if children:v['children']=children
    if image:v['image']=image
    if sources:v['sources']=sources
    if labels:v['labels']=labels
    return v

def relation(a,b,title):return dict(**{'from':a,'to':b},title=title)
def sheet(title,root,relations=None):sheets.append(dict(title=title,root=root,relationships=relations or []))
def img(id,prefix):
    c=next(c for c in runtime['captures'] if c['id']==id)
    return node(prefix+'-'+id,id,'真实Qt Widget offscreen截图的等比缩小副本；原图：'+c['file']+'\n'+c['description']+'\n按钮编号与坐标见SCREENSHOTS.md / button-index.json；截图不代表所有控件已操作。',status='实际操作验证',image='screenshots/xmind/'+id+'.png')

def fn(f):
    bid='、'.join(f['buttonIds']) or '该入口未有独立截图控件编号，以页面与原文定位'
    notes='\n'.join([
        f"位置：FuseVision → {f['module']} → {f['page']} → {' / '.join(f['buttons'])}",
        '按钮编号：'+bid,'场景：'+f['scene'],'可用条件：'+f['preconditions'],
        '输入/默认/校验：'+f['inputs'],'步骤：'+'\n'.join(f'{i}. {s}' for i,s in enumerate(f['steps'],1)),
        '结果：'+f['success'],'失败/取消/边界：'+f['boundaries'],'文件/接口/算法：'+f['data'],
        '实际验证范围：'+f['runtime']])
    children=[node(f['id']+'-entry','菜单/按钮：'+' / '.join(f['buttons'])[:38],
                   '原文：'+' / '.join(f['buttons'])+'\n截图编号：'+bid+'\n'+f['preconditions']+'\n'+f['inputs'])]
    children.append(node(f['id']+'-flow','操作流程',children=[node(f['id']+f'-s{i}',f'{i}. '+s[:22]+('…' if len(s)>22 else ''),s) for i,s in enumerate(f['steps'],1)]))
    children.append(node(f['id']+'-end','结果 / 失败 / 取消',f['success']+'\n'+f['boundaries']))
    children.append(node(f['id']+'-data','数据与实现',f['data'],sources=[f"{e['path']}:{e['line']} {e['symbol']}" for e in f['sources']]))
    return node(f['id'],f['id']+' '+f['title'],notes,children,f['status'],labels=[f['page']]+f['buttonIds'])

names={'P00':'登录','P01':'共享导航','P02':'项目管理','P03':'模型管理','P04':'数据集管理','P05':'数据标注','P06':'数据检查','P07':'数据拆分','P08':'模型训练','P09':'模型预测','P10':'模型导出','P11':'传统视觉','P12':'用户管理','P13':'系统设置'}
def page(pid,fs,images,prefix):
    return node(prefix+'-'+pid,pid+' '+names[pid],children=[fn(f) for f in fs if f['page']==pid]+[img(i,prefix) for i in images])

# Overview emphasizes software and page hierarchy; detailed feature copies only in their own sheet.
module_nodes=[]
for mi,module in enumerate(dict.fromkeys(f['module'] for f in features),1):
    ps=[]
    for pid in dict.fromkeys(f['page'] for f in features if f['module']==module):
        fs=[f for f in features if f['page']==pid and f['module']==module]
        ps.append(node('ov-'+pid,pid+' '+names[pid],children=[node('ov-'+f['id'],f['id']+' '+f['title'],
          '详细见对应分图F节点备注。\n'+f['runtime'],status=f['status']) for f in fs]))
    module_nodes.append(node('ov-module'+str(mi),module,children=ps))
module_nodes += [node('ov-status','阅读图例',children=[node('ov-verified','实际操作验证','仅以runtime明确列出动作作范围；原Widget offscreen，不代表可见桌面/生产数据通过。',status='实际操作验证'),node('ov-static','静态代码确认','追踪控件到处理器与数据写入。'),node('ov-todo','待确认','Q01账号、Q02异常数据、Q03矩形映射、Q04附加需求；XMind客户端兼容。',status='待确认'),node('ov-plan','规划建议','仅docs/refactor，尚未实施。',status='规划建议')]),img('P04-dataset-imported','ov')]
sheet('01 软件总览与核心流程',node('overview','FuseVision｜功能→页面→代码','2026-11-14；当前工作区。38功能，21真实界面；读取节点备注查输入、边界、代码行号。\n标注保存存在已复现失败，不可把主链执行称为全功能验收通过。',module_nodes),[
 relation('ov-P00','ov-P02','认证成功'),relation('ov-F05','ov-F07','新建后双击'),relation('ov-F07','ov-F09','打开工程→模型'),relation('ov-F09','ov-F11','双击继续，避开通知缺口'),relation('ov-F11','ov-F13','模型上下文'),relation('ov-F13','ov-F17','导入→浏览'),relation('ov-F17','ov-F23','图像双击'),relation('ov-F23','ov-F24','完成形状→保存'),relation('ov-F24','ov-F26','重开验证失败后返回'),relation('ov-F26','ov-P04','重扫统计'),relation('ov-P04','ov-P12','共享用户/权限'),relation('ov-P04','ov-P13','共享存储设置')])

fs=[f for f in features if 5<=int(f['id'][1:])<=12]
sheet('02 项目与模型',node('pm','深度学习｜项目→模型',children=[
 page('P02',fs,['P02-new-project-dialog','P02-project-created'],'pm'),
 page('P03',fs,['P03-new-model-dialog','P03-model-created'],'pm'),
 node('pm-valid','有效名称','项目和模型：trim后^[A-Za-z0-9_]{1,64}$；同名目录冲突失败。'),
 node('pm-invalid','取消 / 校验失败','取消无创建；文件错误提示；多文件流程不保证事务。'),
 node('pm-stale','删除上下文待修','ISS-004：未广播关闭；预览与已打开目标可能不同。',status='静态代码确认'),
 node('pm-dataset','下一步：P04数据集','双击模型触发modelOpened/openDatasetRequested。')]),[
 relation('F05','pm-valid','创建前校验'),relation('pm-valid','F07','写工程并刷新'),relation('pm-valid','pm-invalid','名称/目录冲突'),relation('F07','F09','双击打开项目'),relation('F09','F11','新建通知被抑制，双击可继续'),relation('F11','pm-dataset','双击跳转'),relation('F06','F07','打开/迁移也建立正式上下文'),relation('F08','pm-stale','关闭信号缺口'),relation('F12','pm-stale','模型失效未联动')])

fs=[f for f in features if 13<=int(f['id'][1:])<=22]
sheet('03 数据集管理',node('ds','P04 数据集｜导入→维护→导出',children=[
 page('P04',fs,['P04-dataset-empty','P04-dataset-imported','P04-dataset-after-annotation'],'ds'),
 node('ds-context','前提：已选模型','双击模型设置DatasetManagement当前上下文。'),
 node('ds-no','无模型 / 取消','无模型提示；格式说明/选择文件/选择标签类型都可取消。'),
 node('ds-anno','跳转P05标注','带modelId和imagePath，编排器绑定数据源。'),
 node('ds-category','共享类别与ID','配置、内存与PNG/JSON类别语义；删除/重排有ISS-005风险。'),
 node('ds-out','外部文件','images、labels、dataset_config、COCO/YOLO/PNG导出。')]),[
 relation('ds-context','F13','单文件/多文件'),relation('ds-context','F14','根目录导入'),relation('F13','ds-no','缺上下文或取消'),relation('F14','ds-no','缺images或取消'),relation('F13','F17','重扫图库'),relation('F14','F17','重扫图库'),relation('F15','ds-category','类别维护'),relation('F16','ds-category','重排仅内存映射'),relation('ds-category','F17','标签筛选'),relation('F17','ds-anno','双击缩略图'),relation('F19','F17','统计/重扫'),relation('F17','F18','选中删除'),relation('F17','F21','有选出所选，无选出全量'),relation('F20','ds-out','写YOLO txt并保留来源'),relation('F21','ds-out','写所选格式')])

fs=[f for f in features if 23<=int(f['id'][1:])<=27]
sheet('04 数据标注与保存断点',node('an','P05 标注｜操作与数据一致性',children=[
 page('P05',fs,['P05-annotation-loaded','P05-annotation-saved','P05-annotation-reopened'],'an'),
 node('an-load','读取现有标签','已有掩码载入m_shapes，然后appendShapes进AnnotationModel；模型dirty而页面最后清m_modified。'),
 node('an-model','AnnotationModel','现代工具写入形状与自有Undo/dirty。矩形完成后形状数2。',status='实际操作验证 + 静态代码确认'),
 node('an-old','旧m_shapes / 旧Undo','列表、保存和requestRender部分路径仍使用旧集合。'),
 node('an-fail','已复现：新矩形未落盘','保存像素(500,300)=0，重开模型形状数1；见runtime-ui.json和saved-mask.png。',status='实际操作验证'),
 node('an-writefail','写失败分支','未检查mask.save/JSON write；静态确认，不等于已模拟磁盘失败。'),
 node('an-return','返回P04','按m_modified保存后返回；无SaveResult阻断。'),
 node('an-labelme','Python / QProcess / HWND','真实环境未启动，脚本路径和嵌入需确认。',status='待确认')]),[
 relation('an-load','F23','可编辑图像'),relation('F23','an-model','现代Transaction'),relation('an-model','an-old','缺统一状态消费'),relation('an-old','F24','保存旧集合'),relation('F24','an-fail','文件存在但内容不含新增'),relation('F24','an-writefail','文件写失败仍清dirty'),relation('F25','an-old','按钮Undo操作旧栈'),relation('F26','an-return','切图/返回'),relation('an-return','an-fail','可能跳过未保存更改'),relation('F27','an-labelme','外部进程共享标签')])

fs=[f for f in features if int(f['id'][1:]) in [1,2,3,4,34,35]]
sheet('05 登录权限与共享能力',node('auth','登录 / 会话 / 用户权限',children=[
 page('P00',fs,['P00-login'],'auth'),page('P01',fs,['P01-dl-empty','P14-user-readonly'],'auth'),page('P12',fs,['P12-users'],'auth'),
 node('auth-session','SessionManager','登录刷新权限后发sessionChanged；切换取消保留旧用户。'),
 node('auth-reg','Registry→Guard','Registry缓存refresh不广播；Guard以sessionChanged刷新快照。'),
 node('auth-gap','已复现：只读仍有写入口','普通用户项目canWrite=false；新建enabled=true。处理器未见对应守卫。',status='实际操作验证 + 静态代码确认'),
 node('auth-account','Q01 新增账号如何登录','添加任意username；现登录只固定映射三个名，产品模式待确认。',status='待确认')]),[
 relation('F01','auth-session','认证成功'),relation('F04','auth-session','覆盖会话'),relation('auth-session','auth-reg','刷新权限'),relation('F35','auth-reg','更新选中用户权限'),relation('auth-reg','auth-gap','授权覆盖缺口'),relation('F34','auth-account','任意用户名可新增'),relation('auth-account','F01','现有登录入口不支持')])

fs=[f for f in features if 36<=int(f['id'][1:])<=38]
sheet('06 系统设置与生效范围',node('settings','P13 系统设置',children=[
 page('P13',fs,['P13-settings'],'ss'),
 node('ss-store','SettingsManager / QSettings','setter改内存，save同步；Windows原生格式，不保证ini。'),
 node('ss-now','日志立即生效','保存重建Logger；重置未重建Logger。'),
 node('ss-restart','DB重启生效','当前SQLite连接继续使用原文件。'),
 node('ss-cache','项目根页内缓存','“所有配置立即生效”不能覆盖已存在Widget的缓存根。'),
 node('ss-pending','日志清理匹配待验证','清理仅fusevision_*.log；Logger基名无.log兼容性未实测。',status='待确认')]),[
 relation('F36','ss-store','持久化'),relation('F37','ss-store','仅重置五路径'),relation('F36','ss-now','重建Logger'),relation('F36','ss-restart','数据库路径'),relation('F36','ss-cache','业务根缓存'),relation('F38','ss-pending','过滤与命名')])

fs=[f for f in features if 28<=int(f['id'][1:])<=33]
sheet('07 占位功能与传统视觉边界',node('bound','当前未接通范围',children=[
 *[page(p,fs,[p+'-placeholder'],'bound') for p in ['P06','P07','P08','P09','P10']],
 page('P11',fs,['P11-traditional'],'bound'),
 node('bound-exists','P04已有局部能力','数据集质量提示、数据导出与独立检查页/模型导出页不同。'),
 node('bound-sdk','SDK存在不等于业务完成','OpenCV/Halcon/MVS/YOLO依赖/资源存在，未见src相机、训练或推理执行链。')]),[
 relation('F28','bound-exists','独立页仍占位'),relation('F32','bound-exists','模型导出≠数据导出'),relation('F33','bound-sdk','按钮无triggered业务处理')])

sheet('08 架构与数据关系',node('architecture','入口→页面编排→业务→算法/数据',children=[
 node('ar-entry','main / FuseVision','启动、18权限、登录、四顶层页。',sources=['src/main.cpp:105 main','src/MainWindows/FuseVision.cpp:360 initPagesAsync']),
 node('ar-dl','DeepLearningWidget','跨页信号编排，9子页；LabelMe进程。'),
 node('ar-project','Project / Model + Config','Widget交互→静态Config写fvproj/fvdl/data.yaml。'),
 node('ar-dataset','DatasetManagement','UI、导入、类别、转换、导出、统计同类5239行。'),
 node('ar-anno','Annotation / Canvas / Tools','现代模型+工具与旧集合并存；AnnotationManagement2586行。'),
 node('ar-render','算法与渲染','AnnotationRender、QImage/QPainter、几何/标签格式转换；无已接通推理器。'),
 node('ar-files','文件数据层','工程/模型JSON/YAML，images/labels/dataset_config。'),
 node('ar-db','DatabaseManager / SQLite','users、user_permissions，非视觉项目总数据库。'),
 node('ar-shared','共享基础设施','Settings、Theme、Logger、Session、Registry/Guard。'),
 node('ar-io','AnnotationIO未整合','多格式方法存在，但管理页仍自行序列化，不声称界面全格式支持。'),
 node('ar-contract','任务语义待确认','Q02异常检测分类vs掩码；Q03自由/非自由与detect/obb实际映射。',status='待确认')]),[
 relation('ar-entry','ar-dl','创建'),relation('ar-dl','ar-project','项目/模型信号'),relation('ar-project','ar-dataset','当前模型'),relation('ar-dataset','ar-anno','图像双击'),relation('ar-anno','ar-render','渲染/掩码'),relation('ar-project','ar-files','配置读写'),relation('ar-dataset','ar-files','数据处理'),relation('ar-anno','ar-files','标注保存'),relation('ar-entry','ar-db','认证/授权'),relation('ar-dataset','ar-shared','路径/权限/日志'),relation('ar-anno','ar-io','能力重叠但未接通'),relation('ar-project','ar-contract','任务配置契约')])

issue_nodes=[]
for i in issues:
    note='\n'.join([i['fact'],'机制：'+i['cause'],'影响：'+i['impact'],'范围：'+i['scope'],'验收：'+i['verify']])
    issue_nodes.append(node(i['id'],i['id']+' '+i['title'],note,status=i['status'],sources=[f"{e['path']}:{e['line']} {e['symbol']}" for e in i['sources']],labels=[i['priority']]+i['features']))
phase_nodes=[node('rf-phase1','阶段1 标注单一状态与保存','TASK-001–004；修复前固定失败fixture，收敛状态源→SaveResult→离页阻断；AC-001/002。',status='规划建议'),node('rf-phase2','阶段2 授权闭合','TASK-005–006；共享通知和所有入口/处理器守卫；AC-005。',status='规划建议'),node('rf-phase3','阶段3 上下文与类别保护','TASK-007–009；模型通知、删除上下文、已有标签阻止删/重排类别；AC-003/004。',status='规划建议'),node('rf-phase4','阶段4 表单/设置契约','TASK-010–011；描述保存、派生路径、准确生效提示；AC-006/007。',status='规划建议'),node('rf-phase5','阶段5 提取与回归','TASK-012–014；先fixture语义基准再提取文件处理/存储，注册CTest，更新文档；AC-008。',status='规划建议')]
rels=[relation('ISS-001','rf-phase1','P0'),relation('ISS-002','rf-phase1','P0'),relation('ISS-006','rf-phase2','P0'),relation('ISS-003','rf-phase3','P1'),relation('ISS-004','rf-phase3','P1'),relation('ISS-005','rf-phase3','P1'),relation('ISS-008','rf-phase4','P2'),relation('ISS-009','rf-phase4','P2'),relation('ISS-010','rf-phase5','行为稳定后提取'),relation('rf-phase1','rf-phase3','保存决策依赖'),relation('rf-phase2','rf-phase5','服务入口仍需守卫'),relation('rf-phase3','rf-phase5','状态契约稳定'),relation('rf-phase4','rf-phase5','回归门禁')]
sheet('09 问题证据与改进计划',node('refactor','证据问题→规格→Planned计划','10项证据问题、8项规格需求、5阶段14任务；业务修改未执行。ISS-007待Q01，未纳入可执行任务；不要求填补占位页。',children=[node('rf-issues','问题及证据',children=issue_nodes),node('rf-plan','后续阶段',children=phase_nodes),node('rf-decision','Q01–Q04产品意图','账号模式、异常任务、矩形命名、附加需求保留决策；不阻塞文档交付。',status='待确认')]),rels+[relation('ISS-007','rf-decision','产品选择')])

put('docs/logic/FuseVision-功能与代码.json',json.dumps(dict(project='FuseVision',date='2026-11-14',source='features.json + issues.json + runtime-ui.json',sheets=sheets),ensure_ascii=False,indent=2))

# Standalone flow illustration, clearly labelled as illustration rather than screenshot.
im=Image.new('RGB',(1760,690),'#f5f7fb');d=ImageDraw.Draw(im)
ft=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',23);small=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',18)
d.text((35,22),'FuseVision 核心流程示意图（非软件截图）',font=ft,fill='#152a4a')
boxes=[('P02 新建工程','F05 → F07 双击打开'),('P03 新建模型','F09 → F11 双击进入'),('P04 导入图像/标签','F13 → F17 打开图像'),('P05 标注/保存','F23 → F24 → F26')]
for k,(a,b) in enumerate(boxes):
    x=35+k*435;d.rounded_rectangle((x,105,x+380,225),radius=14,fill='#e1ebff',outline='#5279c4',width=2)
    d.text((x+18,124),a,font=ft,fill='#152a4a');d.text((x+18,178),b,font=small,fill='#2b4470')
    if k<3:d.line((x+383,165,x+428,165),fill='#5279c4',width=4);d.polygon([(x+428,165),(x+416,158),(x+416,172)],fill='#5279c4')
d.rounded_rectangle((470,275,850,375),radius=10,fill='#fff0cf',outline='#b98212',width=2)
d.text((488,290),'断点：新建模型通知为0',font=small,fill='#775100');d.text((488,326),'双击模型后继续验证',font=small,fill='#775100')
d.line((660,225,660,270),fill='#b98212',width=3)
d.rounded_rectangle((1010,275,1715,410),radius=10,fill='#ffe4e4',outline='#ba2929',width=2)
d.text((1030,290),'断点：模型2个形状，重开只剩1个',font=ft,fill='#9b1e1e')
d.text((1030,340),'保存文件可读，但新矩形区域像素仍为0',font=small,fill='#9b1e1e')
d.line((1515,225,1515,270),fill='#ba2929',width=3)
d.text((35,470),'已验证：原Qt Widget offscreen + 临时图像/掩码 + 内存SQLite',font=ft,fill='#152a4a')
d.text((35,525),'静态扩展：文件夹导入、类别维护、格式转换、数据导出、用户与系统设置',font=small,fill='#465674')
d.text((35,575),'占位范围：独立数据检查/拆分/训练/预测/模型导出，传统视觉主要业务',font=small,fill='#465674')
d.text((35,625),'详情：FEATURES.md、CORE-FLOW.md、SCREENSHOTS.md；问题与计划见 docs/refactor/',font=small,fill='#465674')
im.save(LOGIC/'core-flow-preview.png')
print(f'Built {len(sheets)} Chinese sheets; all {len(features)} feature IDs and {len(runtime["captures"])} UI images represented')
