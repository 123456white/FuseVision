# 当前算法与格式处理说明

最近核对：2026-11-17；范围：当前存储、渲染和格式处理边界的静态说明。本次几何与格式处理回归已执行；没有性能实验。

[数据契约](DATA-CONTRACTS.md) 定义具体字段、编号和兼容规则，本页只解释处理目的、假设与限制，避免复制字段表。

| 能力 | 输入→处理→输出 | 核心约束/失败模式 | 代码与验证 |
|---|---|---|---|
| 类别掩码合成 | 形状/画笔层与类别→像素合成→类别PNG | 图像尺寸、可见性、类别槽位必须有效；背景与二值编码见契约；标准PNG本身不能恢复实例身份 | [AnnotationRender](../../src/deeplearning/render/AnnotationRender.cpp)、TC-002/008 |
| 原生标注重读 | PNG/JSON及原生元数据→一致性检查→形状模型 | 外部修改标准像素/annotations后不能盲信旧元数据；普通掩码只恢复可表达的信息 | [AnnotationStore](../../src/deeplearning/storage/AnnotationStore.cpp)、外部修改与round-trip回归 |
| 框/轮廓到YOLO | 像素几何与真实图像尺寸→归一化→detect/segment/obb | 不能猜测640尺寸；非法坐标拒绝；轮廓、旋转或孔洞到较弱格式可能有损 | [DatasetFileService](../../src/deeplearning/storage/DatasetFileService.cpp)、TC-008 |
| COCO类别与RLE | 外部类别/标签→编号映射或非压缩RLE→内部形状/导出 | 稀疏外部编号不是数组索引；RLE顺序与长度校验；压缩字符串RLE不在支持范围 | [DatasetManagement](../../src/deeplearning/widgets/DatasetManagement.cpp)、[DatasetFileService](../../src/deeplearning/storage/DatasetFileService.cpp)、TC-007/008 |
| 分类布局 | train/val/test下OK/NG图像→目录类别/split→图库、预览及分类导出 | 类别固定；同名异类不冲突；必须有明确classification配置且与模型布局匹配；旧mask回退拒绝 | TC-016 |
| 旋转移动与复制 | 图像位移→bbox及显式中心同步平移→相同屏幕位移 | 空中心保持隐式哨兵；撤销/重做及存储保持中心 | TC-014 |
| 图像坐标交互 | 窗口事件→画布/图像变换→形状编辑或视野变化 | 缩放锚点、平移结束、失焦取消和选择身份必须稳定；显示变化不等于标签变化 | [AnnotationCanvas](../../src/deeplearning/widgets/AnnotationCanvas.cpp)、[SelectTool](../../src/deeplearning/tools/SelectTool.cpp)、TC-003 |

没有独立测量复杂度、吞吐量或大图内存峰值，不填写估计值冒充基准。当前能力主要是确定性图像/几何/格式处理；若以后接入训练或推理，应另补数据版本、模型版本、随机种子、超参数、评估协议和失败样本，而不是把依赖SDK列成已接入算法。
