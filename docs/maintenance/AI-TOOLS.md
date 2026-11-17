# FuseVision AI工具与技能

最近核对：2026-11-17；范围：本机技能安装、插件下载与Qt文档MCP协议验证。未执行本轮C++构建或业务回归，未修改业务代码。

[维护入口](README.md) · [测试策略](../codebase/TESTING.md) · [测试追踪](../logic/TEST-TRACEABILITY.md)

## 本次新增

| 工具 | 状态及范围 | 适用工作 | 边界 |
|---|---|---|---|
| test-blindspots | 已安装至用户级Codex skills | 找出通过的测试没有证明的行为，检查真实集成边界、状态变化和断言盲区 | 使用隔离数据做小范围探查；不把未执行的风险写成缺陷 |
| cpp-benchmark-testing | 已安装至用户级Codex skills | 为图像处理、导入导出等操作设计可重复的性能测量 | 面向Google Benchmark；本次未安装该C++库、未增加构建目标、未测出性能结论 |
| Qt官方文档MCP：qt-docs | 已写入项目配置；CLI识别已启用，协议初始化、工具发现、搜索和正文读取成功 | 核对Qt API、信号槽、版本和测试辅助类文档 | 服务未提供6.10.2；其他版本结果必须标明版本并结合本机头文件/对应版本文档确认 |
| Product Design 0.1.52 | 官方市场安装命令成功、98个文件已落盘且与审查源一致，配置enabled=true；客户端列表仍报未安装/未启用，加载待确认 | 基于真实截图审查交互流程；必要时制作独立原型 | 当前不能宣称已加载；偏重浏览器/原型工作，不提供Qt Widgets原生自动化 |

技能目录为本机 `C:/Users/21012/.codex/skills/`，供其他项目复用；FuseVision的文档维护规则仍只在本项目生效。Qt文档MCP仅写入仓库 `.codex/config.toml`，保留原有Hooks配置。安装插件的CLI写入了用户级插件启用项，不改Hooks信任记录，也未发布内容或连接外部账户。

新技能应在下一轮对话被发现；若界面仍未列出，重新打开项目会话。MCP需要新会话加载后确认工具可见。Product Design需在新会话或重启客户端后检查插件列表；若仍未识别，属于未完成的客户端加载验证，不能用缓存存在代替可用性证明。

## 来源与复现记录

两个技能按完整Git提交安装，未覆盖已有技能，SKILL.md与审查源逐字节一致；各自保留上游许可证为LICENSE.upstream。

| 技能 | 上游仓库及路径 | 固定提交 | SKILL.md SHA-256 |
|---|---|---|---|
| test-blindspots | [Neeeophytee/finding-unknowns-skills](https://github.com/Neeeophytee/finding-unknowns-skills)，skills/test-blindspots | ca5696a0f08d2de6b2997fff1d6cc05c3ed587cc | 56695d48b6fd31cec838b87476e66e50c32f3f12e5a4f47d61ee7dacf2dcb45e |
| cpp-benchmark-testing | [sentenz/skills](https://github.com/sentenz/skills)，skills/cpp-benchmark-testing | 45838ccf5a3e0266b8a8f15f90d09ea66e0a1b9a | b3723ad32218b9c134ea667b383b67e4a336dedc4f13dab4de848c959f157c4a |

Product Design来源为本机已配置市场openai-api-curated，插件ID为product-design@openai-api-curated；清单版本0.1.52，CLI缓存修订1dc19589。落盘目录：`C:/Users/21012/.codex/plugins/cache/openai-api-curated/product-design/1dc19589/`。复查命令：

```powershell
$inventory = codex plugin list --available --json | ConvertFrom-Json
@($inventory.installed) + @($inventory.available) |
    Where-Object { $_.name -eq 'product-design' } |
    ConvertTo-Json -Depth 5
codex mcp get qt-docs --json
```

本轮两个技能和插件内10项技能的静态扫描均为0条模式命中；另核对了新技能全文、插件清单与辅助脚本。扫描不证明运行时绝无问题，插件功能尚未实际调用。原始扫描、安装摘要和协议结果保留在本机忽略目录 `out/skill-downloads/reports/`；仓库不依赖该临时目录构建或阅读。

## Qt文档服务的使用

来源：[Qt官方MCP说明](https://doc.qt.io/agentictools/mcp/)、[Qt官方技能仓库](https://github.com/TheQtCompanyRnD/agent-skills)、[Codex官方MCP配置](https://developers.openai.com/codex/mcp)。项目配置使用Streamable HTTP，端点为 `https://qt-docs-mcp.qt.io/mcp`，启动/调用超时分别为30/60秒。

实测服务为qt-docs-server 0.3.0，提供qt_documentation_search和qt_documentation_read。对6.10.2的QSignalSpy查询明确返回无该版本；改查6.11.0后搜索及wait成员文档读取成功。服务返回的框架版本包括6.8.7、6.11.0、6.12.0和5.15.18；这是本次响应，不是永久保证。

本轮只发送公开Qt符号查询，未上传项目源码。今后查询参数会发往Qt托管服务，应使用API名和公开技术问题。不能直接把较新版本API建议用于6.10.2，也不能因MCP文档查询成功就声称Qt界面测试通过。

## 与现有技能组合

当前已有Qt审查、CMake、规格计划、属性测试、覆盖率、变异测试、文档和XMind技能，本次没有重复安装Qt官方仓库中的13项技能。按任务选择少量技能即可：

| 任务 | 推荐组合 | 对应项目材料 |
|---|---|---|
| 检查跨功能闭环和测试盲区 | test-blindspots + property-based-testing | CORE-FLOW、FEATURE-RELATIONS、STATE-MACHINES、TEST-TRACEABILITY |
| 修改Qt/C++业务实现 | qt-cpp-review + qt-cmake-project；需要时查询qt-docs | ARCHITECTURE、CONVENTIONS、TESTING |
| 制定有证据的改进方案 | acquire-codebase-knowledge + create-specification + create-implementation-plan | ISSUES及对应规格/计划 |
| 分析图像处理性能 | cpp-benchmark-testing；需要时结合现有performance-optimization | ALGORITHMS、DATA-CONTRACTS、已有基线 |
| 梳理页面、按钮及导图 | ui-logic-xmind；插件加载后可按需用Product Design审查截图 | FEATURES、SCREENSHOTS及原始截图 |
| 维护文档 | 根AGENTS.md与maintenance规范、docflow命令 | 本目录README和STANDARD |

当前业务回归复用CTest、真实QApplication/Widget事件及QCoreApplication数据层测试；workflow回归不依赖额外Qt Test模块。不要因为某技能示例使用Jest、GoogleTest、QtTest或Google Benchmark就自动替换测试框架或增加依赖。QML专用测试技能也不能当作Widgets验收。

## 可直接使用的提示词

### 检查测试闭环

```text
请使用test-blindspots，必要时结合property-based-testing，审查FuseVision
“标注编辑→保存→切换图像→返回图库→重新打开”链路。
先读AGENTS.md、文档入口和维护规范，核对现有测试、状态表与功能关系。
复用现有CTest和真实Qt组件，在隔离临时数据上运行必要验证。
重点检查保存失败、取消、重试、退出登录、撤权、旧文件和编辑状态保留。
每项发现给出代码位置、现有断言缺口、实际复现结果及影响的F/REL/ST/TC编号；
待确认需求、未验证风险与确定缺陷分别列出，不虚报覆盖率。
本次只分析和验证，不修改业务代码；完成后同步受影响Markdown并运行docflow收尾。
```

### 制定性能基线

```text
请使用cpp-benchmark-testing分析FuseVision中图像加载、掩码处理及格式导出的
性能测量方案。先核对当前算法、构建目标和测试设施，选择有证据的候选热点。
明确样本尺寸、形状数量、测量边界、正确性断言、Release配置、重复次数、
时间/内存单位和原始结果保留方式。先提交可复现方案，不增加C++依赖或修改业务代码。
没有实际测量时不得报告加速比；同步相关文档并执行本项目维护流程。
```

### 审查页面与按钮

```text
请使用ui-logic-xmind梳理FuseVision数据集页与标注页的入口、按钮和状态变化，
以当前代码和真实界面截图为证据，对应现有P/B/F编号。
若Product Design插件已实际加载，可辅助审查截图中的操作反馈和流程缺口。
先验证截图日期和当前界面是否一致；缺少截图或原生操作能力时明确说明。
交付有代码证据的建议、更新后的Markdown和派生导图；不修改业务代码、
不发布原型，不把网页原型当作Qt原生功能已实现。
```

## 检索后未安装的候选

| 候选 | 原因 |
|---|---|
| qaskills的state-machine-test-generator | 主要是Jest/Vitest/XState及Web/API示例，技能名含空格；与当前C++ Widgets不直接匹配。状态覆盖先复用项目状态表与既有测试技能 |
| sentenz的cpp-unit-testing | 偏向GoogleTest、meta_gtest和GCC/gcov约定；当前已有真实Qt回归，直接套用会引入不必要的框架变化 |
| Qat原生Qt自动化技能 | 上游明确说明注入服务监听0.0.0.0且无鉴权，仍在早期开发；本轮仅下载审阅，没有安装、编译或注入应用 |
| Superpowers等通用合集 | 与已安装的调试、计划、TDD和验收技能大量重叠 |

以上研究源码下载到本机 `out/skill-downloads/`，不进入项目依赖。后续真正缺少原生GUI自动化时，应单独评估Windows支持、隔离运行及与现有Qt事件回归的配合；本轮安装不代表该缺口已经解决。
