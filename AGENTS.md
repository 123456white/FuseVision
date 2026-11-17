# FuseVision 项目执行规则

适用范围：本仓库及子目录。用户明确指令优先于本文件；本规则不修改其他项目或全局 Codex 配置。

## 开始任务

- 先读 [文档总入口](docs/README.md) 和 [文档维护规范](docs/maintenance/STANDARD.md)，再按任务阅读相关专题；无需每次通读所有历史资料。
- 修改前检查工作区已有改动，保护用户未提交工作。不得 reset、覆盖或清理无关内容，不将已有改动全部归入本次任务。
- 以当前代码、配置和适用的运行证据核实实现。旧分析、旧行号、计划、SDK或按钮存在都不能单独证明当前功能已实现或通过。
- 构建运行见 [启动指南](docs/GETTING-STARTED.md)，源码边界和惯例见 [架构](docs/codebase/ARCHITECTURE.md)、[代码惯例](docs/codebase/CONVENTIONS.md)。

## 会话自动维护

- Hooks已启用时读取其中给出的会话ID，沿用自动内容基线；无Hooks时必须在修改前运行 `python -X utf8 scripts/docs/docflow.py begin --session <任务ID>`。会话开始前已有改动不能当成本次新增改动。
- 修改完成后运行 `sync --html`、`check --diagrams`，再用 `review --session <ID> --decision updated|no-impact|deferred --reason <具体理由>` 记录语义核对；updated还需 `--docs` 指明对应专题。手动模式最后运行finish。完整命令见 [维护操作](docs/maintenance/README.md)。
- 工具只生成派生产物、检查遗漏和提示，业务说明由AI对照代码更新；不能以JSON/HTML生成成功替代实际更新。后续源文件变化使旧review失效。
- Hooks配置只属于本项目，首次须在Codex审查信任。不得修改信任数据库或绕过审核；未生效时使用手动流程，不声称钩子已经运行。

## 修改与文档同步

- 完成一组有业务含义的改动后，按文档规范的变更映射核对影响；本次任务结束前同步必要说明。无需每次工具调用后重写文档。
- 页面、按钮与操作更新 `docs/logic/FEATURES.md`；跨页状态与异常恢复更新 `CORE-FLOW.md`；格式、编号、坐标、存储与配置生效规则更新 `DATA-CONTRACTS.md`。
- 模块与依赖变化更新 `docs/codebase/` 对应专题；测试策略更新 `TESTING.md`；新运行结果引用到 `docs/logic/VALIDATION.md`；缺陷与进展更新 `docs/refactor/` 对应记录。
- 同步受影响的功能/问题 JSON、导图节点与当前代码引用，保留已有 F/P/B/ISS 编号和历史字段。业务内容只改MD；运行docflow.py sync --html生成JSON与XMind，勿手工双向维护。
- 项目说明统一放在 `docs/`；复用现有专题，避免新增重复总结。根 `AGENTS.md` 仅作规则入口；完整规范只维护一份。
- Markdown维护说明；HTML/XML为生成物，不手工编辑。源码Doxygen注释、导图结构JSON和真实媒体各守其用途；输出放 `out/docs/`。
- 无文档影响时，不为满足规则修改日期或生成空报告；交付中说明具体原因。只做文档的任务不得借机修改业务代码。

## 证据与验证

- 区分计划、占位、已实现、已退役，并另列静态、自动、人工、历史和未验证状态。已有测试不等于已执行；offscreen/启动/API文档生成不等于完整产品验收。
- 验证使用临时数据、隔离数据库和配置，不删除真实项目或覆盖用户设置。新增结果使用独立文件，保留历史日志、截图、基线及原始需求。
- 旧分析/导图/采集工具已归档到 `docs/archive/`，不得作为当前入口执行或恢复旧快照。
- 当前只读校验入口为 `python -X utf8 scripts/docs/docflow.py check --diagrams`；旧validate_analysis.py已归档，不得覆盖历史报告。
- 纯文档修改核对链接、路径、符号与内容即可；业务变更按 `docs/codebase/TESTING.md` 运行必要验证。遇阻说明原因和补验方式，不编造通过结果。
- 当前专题与历史资料冲突时先核查，不恢复旧角色矩阵、已退役入口或过期文件路径。新增产品决策明确记录依据。

## 交付前

- 核对本次涉及的说明、JSON、图、编号及代码引用是否一致；生成物受影响时核实其来源与结果。
- 交付主要文件、实际修改、已执行验证与限制；未运行不写通过。
- 在用户已授权范围内继续完成必要的同步与检查，无需对常规步骤反复确认；不将本文件当作额外业务重构、发布或全局配置的授权。
