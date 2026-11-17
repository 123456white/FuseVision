---
goal: Qt 原生连续标注工作区，退役 LabelMe 进程入口
version: 1
date_created: 2026-11-17
last_updated: 2026-11-17
owner: FuseVision
status: Completed
tags: [feature, qt, annotation, recovery]
---

# Introduction

![Status: Completed](https://img.shields.io/badge/status-Completed-brightgreen)

用户已选择“一切以推荐及参考软件为准”。参考 MVTec Deep Learning Tool 26.03 的连续工作、缩放定位和视野保持原则，在现有 Qt Widgets 编辑器上实施；不声称完整复刻参考产品。既有工作区已快照至 `out/native-annotation-baseline.zip`，哈希见 [本轮基线](../logic/evidence/native-annotation-start-baseline.json)。

## 1. Requirements & Constraints

- **REQ-001**: 取消 LabelMe/Python 启动、环境选择、原生窗口嵌入及部署；保留 LabelMe rectangle/polygon JSON 读取兼容。
- **REQ-002**: 连续矩形支持拖放和既有两次点击；多边形显示已放置边链；工具和类别不因保存或切图重置。
- **REQ-003**: 中键及空格左拖正确结束；失焦恢复未提交笔画/拖动；缩放围绕鼠标；亮度与对比度不重置视野。
- **REQ-004**: 同模型默认保持视野，可取消；增加保存并下一张，保存失败或绘制未完成不离开，末图保存后停留。
- **REQ-005**: 画布/列表采用模型 UUID 对应选择，拖动不清列表；可见性显示开关不改数据，选中控制点保持屏幕尺寸。
- **SEC-001**: 未登录可查看；写处理器仍检查会话；文件结果只通过既有 AnnotationStore 成功后清理修改状态。
- **CON-001**: 不修改真实数据、用户配置、凭据、任务命名及 detect/obb 映射，不引入 AI 标注或训练服务，不提交或发布。

## 2. Implementation Steps

### Implementation Phase 1

- **GOAL-001**: 建立旧行为失败证据并修复 Qt 输入和连续工作流。

| Task | Description | Completed | Date |
|---|---|---|---|
| TASK-001 | tests/native_annotation_regression.cpp 先记录悬停、空格平移、拖放矩形及旧入口失败，旧流程保持通过 | ✅ | 2026-11-17 |
| TASK-002 | AnnotationCanvas 修复鼠标/键盘/失焦、锚点缩放、视野及笔刷轮廓；AnnotationManagement 同步选择并加入保存下一张 | ✅ | 2026-11-17 |
| TASK-003 | RectTool/PolygonTool 保持原操作并补连续预览；SelectTool 使用模型命中规则，修复旋转框命中与角点收缩 | ✅ | 2026-11-17 |
| TASK-004 | DeepLearningWidget 删除外部进程与窗口嵌入，CMake 退役脚本部署及旧路径测试 | ✅ | 2026-11-17 |

### Implementation Phase 2

- **GOAL-002**: 在隔离数据上验证保存恢复并维护当前文档与导图。

| Task | Description | Completed | Date |
|---|---|---|---|
| TASK-005 | Release 编译；五项 CTest；新增画布事件、选择、视野、保存下一张失败/恢复/重开及访客拒绝断言 | ✅ | 2026-11-17 |
| TASK-006 | 更新 CORE-FLOW、DATA-CONTRACTS、TESTING、ARCHITECTURE、功能 JSON/XMind 和 Doxygen，历史证据保留原文 | ✅ | 2026-11-17 |

## 3. Alternatives

- **ALT-001**: 新建第二套标注状态或切换 QML 会扩大迁移风险；保留 AnnotationModel 单一权威模型和现有 Qt Widgets 画布。
- **ALT-002**: 继续嵌入 Python 与本次明确需求冲突；自研适配从构建/源码退役，第三方 sdk 和用户旧配置不删除。

## 4. Dependencies

- **DEP-001**: 现有 Qt 6 Widgets/Gui、AnnotationModel、AnnotationStore 和 DatasetFileService；不增加运行时包。
- **DEP-002**: Doxygen 1.18.0 与已安装 XMind JSON 导出工具，仅文档构建使用。

## 5. Files

- **FILE-001**: `src/deeplearning/widgets/AnnotationCanvas.*`、`AnnotationManagement.*`、`DeepLearningWidget.*`。
- **FILE-002**: `src/deeplearning/tools/{RectTool,PolygonTool,SelectTool,AnnotationTool}.*`、`model/AnnotationModel.*`。
- **FILE-003**: `tests/native_annotation_regression.cpp`、根和深度学习 CMake、`cmake/DeployRuntime.cmake`、现有 docs。

## 6. Testing

Release编译成功，五项CTest全部通过；原15组工作流、新增24项行为断言与截图保存检查通过。Doxygen 1.18.0零警告；当前文档/历史保留机器校验19项通过。可见桌面与人工手感不包含在本次自动验收内。详见[验证报告](../logic/VALIDATION.md)。

- **TEST-001**: QApplication 向真实画布发送鼠标、滚轮、按键和失焦事件；覆盖两种松开顺序及缩放锚点。
- **TEST-002**: 临时工程、两张合成图片、内存 SQLite、隔离 INI；验证 UUID 选择、每拖动一个撤销、失败不前进及恢复后重开。
- **TEST-003**: 原 15 组 workflow、数据契约、模块边界、启动自检继续通过；新增截图明确标记 offscreen。

## 7. Risks & Assumptions

- **RISK-001**: offscreen 事件测试和截图不能替代可见 Windows 桌面、高 DPI、触控板或人工手感验收；需人工连续标注补验。
- **RISK-002**: 本轮不实现参考软件的 AI 辅助、掩码裁切拆分、多选整体变换和旋转角度拖动；已有旋转框读取、移动、角点编辑和导出继续保留。
- **ASSUMPTION-001**: 视野偏好仅本次编辑会话，按同模型连续图片使用，跨模型和清空上下文回到适应窗口；不写入项目格式。

## 8. Related Specifications / Further Reading

[核心流程](../logic/CORE-FLOW.md) · [数据契约](../logic/DATA-CONTRACTS.md) · [验证](../logic/VALIDATION.md) · [原工作流规格](spec-process-workflow-consistency.md)。参考资料：MVTec 官网 Deep Learning Tool 产品页及 26.03 release notes；本机没有参考软件可做像素或操作对照。
