# 功能范围与证据说明

2026-11-17：登录简化为单账户；未登录可查看全部界面，登录后可修改。账户页只保留状态与改密，历史角色/权限矩阵停用；见[验证记录](VALIDATION.md)。
当前实现复核/修复日期：2026-11-17。历史分析/21张截图：2026-11-14，原文件保留。

[总入口](../README.md) · [当前主流程](CORE-FLOW.md) · [验证报告](VALIDATION.md) · [问题与产品决策](../refactor/ISSUES.md) · [截图](SCREENSHOTS.md)

## 证据状态

- 自动业务验证：QApplication、真实Widget处理器、Qt offscreen事件循环及独立文件读取断言，只代表列明场景。
- 静态确认：已追到处理器/存储，但未操作该UI或注入该故障。
- 人工未验：可见Windows桌面、原生文件对话框、完整重置重启、XMind客户端和硬件，不能由offscreen代替。
- 历史复现：runtime-ui.json、原screenshots和旧validation.json保留修复前证据，不代表当前失败。

## 当前范围

四一级模块，深度学习九页中前四页承载项目/模型/数据集/标注；后五页、数据增强和传统视觉执行仍为占位，本轮未扩展。当前核心合成样本链路与异常恢复自动回归通过，产品完整人工验收仍有边界。

新增业务回归位于 tests/workflow_regression.cpp，临时目录、内存SQLite和用户/系统scope临时INI配置完全隔离。旧capture_ui.cpp是历史采集器，不能把其旧缺陷输出当作当前结果。新截图由当前集成回归Widget::grab产生，见SCREENSHOTS.md。

## 产品决策

Q01已确认单账户，ISS-007按新需求关闭；未登录可浏览全部页面，登录统一解锁；Q02沿用明确OK/NG分类并取消旧掩码配置回退，未来模式选择界面待定；Q03矩形命名与obb/detect映射；Q04审计和备份需求；Q05用户决定采用完整原生1.0新数据标准，无需兼容旧格式。推荐方案及下一步优先级统一维护在[问题清单](../refactor/ISSUES.md)，未擅自调整任务语义。

当前原生标注入口和验证见[实施计划](../refactor/feature-native-annotation-1.md)；LabelMe运行入口已退役，JSON数据兼容保留。
