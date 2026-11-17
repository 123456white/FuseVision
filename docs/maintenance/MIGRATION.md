# 文档架构与自更新实施记录

日期：2026-11-17。范围：项目文档、文档工具、项目级Codex配置及独立文档CI；不修改软件业务实现。

## 目录与维护关系

| 原位置/方式 | 当前位置/方式 | 原因 |
|---|---|---|
| docs/DOCUMENTATION-STANDARD.md | [maintenance/STANDARD.md](STANDARD.md) | 规范与维护操作集中管理，根入口保持简短 |
| docs/logic/tools中的七个旧文件 | [archive](../archive/README.md)下同名原字节文件 | 含固定分析/旧任务基线，不作为当前执行入口 |
| MD、功能/问题JSON与导图分别维护 | MD维护当前说明→docflow生成镜像与导图 | 减少重复维护与旧内容回流 |
| 无独立功能关系/状态/测试追踪 | [关系](../logic/FEATURE-RELATIONS.md)、[状态](../logic/STATE-MACHINES.md)、[追踪](../logic/TEST-TRACEABILITY.md)、[算法](../logic/ALGORITHMS.md) | 连接功能依赖、业务不变量与已有真实测试 |
| 无当前通用校验入口 | scripts/docs/docflow.py check | 与旧业务验收分离，不重写原始报告 |
| HTML仅有API索引 | MD业务阅读站＋保留Doxygen索引 | 业务与API分工；生成在out |

功能/契约/架构等现有稳定路径保留；原始需求、截图、日志与基线不搬动。迁移前四份JSON/XMind与旧工具保留在归档，123份原始资料摘要受保护，详见[归档清单](archive-manifest.json)。

## 当前实现

已实现会话内容基线、路径影响提示、Markdown解析生成、JSON镜像、含原图XMind、自动目录、只读结构检查、离线HTML与本机阅读服务，以及语义核对收据。新增源改动会使旧核对失效，退出检查最多触发一次继续，未解决时明确保留未完成状态。

根AGENTS.md与.codex配置只属于本项目；没有修改全局配置、信任数据库或运行绕过信任的命令。原始证据不作为自更新输出。独立文档CI只检查和构建，不自动提交内容。

## 验证记录

本轮在本地实际执行，详细机器结果位于out/docs/maintenance-verification.json（本机构建产物，不替代历史业务报告）：

| 验证 | 结果与范围 |
|---|---|
| python -X utf8 -m unittest discover -s tests -p test_docflow.py -v | 26项工具行为测试通过；修改均在临时目录；覆盖历史字段、弹窗按钮编号、MD关联线、代码行号漂移、基线/核对失效、锁释放与失败收尾 |
| sync --html及再次生成 | 38项功能字段与迁移前JSON全部一致（含history字段）；10项问题以当前MD为准；连续原样生成不改输出摘要/修改时间 |
| check --diagrams | 478处本地引用、当前功能/问题符号及6张Mermaid语法检查通过；无MD或原始证据写入 |
| XMind归档结构 | 9张可编辑分图、26个原始截图引用、9条关联线；content.json、嵌图路径和端点检查通过 |
| HTML与本机HTTP | 30份MD生成阅读页；连同代码预览共69个HTML文件、1372处本地链接/锚点检查通过；实际HTTP获取正文、Mermaid模块、源预览、PNG与XMind成功 |
| Windows Hooks适配 | 用配置中的commandWindows从项目根及docs/logic分别传入SessionStart、两次PostToolUse及Stop；8次真实进程调用返回合法JSON且符合预期。属于合成协议测试，不是Codex真实事件验收 |
| 原始内容保护 | 对比任务前摘要，108份已有非文档文件未变；123份受保护需求/证据/图片/归档原字节不变 |

首次真实生成中发现并修复：共享能力无按钮被误拒绝、问题字段不统一、弹窗B02D/B03D编号遗漏、过期代码行号、XMind包漏声明JSZip运行依赖、Node环境Mermaid标签净化缺DOM，以及浏览器模块路径和代码预览缓存依赖。没有通过修改旧证据或虚构入口消除错误。

依赖在out中隔离安装，锁定Markdown、Mermaid、xmind-generator及其适配依赖JSZip/JSDOM；没有更新全局环境。本任务手动会话docs-architecture-20261117从真实任务前基线导入并标记来源，随后按相同review/finish流程收尾；未冒称本会话已自动触发Hooks。

真实Codex客户端的Hooks审查/信任和新会话自动触发尚未验证；远端CI尚未执行。HTML已验证本机HTTP和链接结构，未进行浏览器视觉验收；软件业务测试、原生窗口操作、XMind桌面客户端、本轮Doxygen重生成不属于本次文档工具验收。

## 取舍与限制

- 自动化识别改动范围和生成可确定内容，业务语义仍需AI/开发者对照代码维护；review不是机器证明。
- 保留兼容JSON和旧导图快照，不通过删除证据实现表面“唯一源”。
- 不把整个Git脏工作区当作本次改动；初始化时有真实任务前基线，恢复/压缩不中途重置。
- 完整图表通过本机HTTP服务阅读；直接file打开时浏览器可能限制ES模块。
- 路径映射只提供候选；产品决策、人工交互和算法性能仍按对应专题处理。
