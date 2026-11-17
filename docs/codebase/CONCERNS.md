# Codebase Concerns

## Core Sections (Required)

### 1) Top Risks (Prioritized)

2026-11-15已修复的ISS-001–006、008主要实现与009/010限定范围见[问题清单](../refactor/ISSUES.md)。旧标注双状态、新建通知被屏蔽和写处理器缺守卫不再作为当前未修复结论。

当前P0是可见桌面主链/登录/退出登录/关闭与原生标注连续操作补验；P1是任务/审计产品决策、断电恢复和恢复目录产品管理。P2为真正分区导入、压缩RLE等能力边界及基于实测的性能改造。

### 2) Technical Debt

AnnotationStore和DatasetFileService承接必要持久化/传输职责；DatasetManagement仍有COCO编排与UI，不追求本轮全类拆分。AnnotationIO保留既有用途。删除采用恢复目录，尚无自动还原/保留期UI。类别删除/重排采用保守拒绝，没有迁移服务。

2026-11-17已拆开core/ui/app/deeplearning/traditional，并把领域类型、模型、存储、渲染与工具放入无Widgets的数据目标；移除了存储/模型反向包含业务Widget及核心授权弹窗依赖。include和CMake直接链接有自动约束。仍有业务Widget承担较多编排、共享单例和同步文件处理；这些是后续有行为基准时逐步提取的范围。当前无通用插件容器或传统算法执行引擎，不能把目录整理解释为所有依赖均已消除。

### 3) Security Concerns

2026-11-17改为单账户，访客可读、登录可写。用户增删/角色/权限矩阵已移除；不再读取或保存明文记住密码，成功登录/改密清理旧记住字段。无盐摘要和默认初始密码保持兼容，密码算法迁移未实施。历史账户/权限表保留但不参与授权，ISS-007按新需求关闭而非补上任意用户名入口。

### 4) Performance and Scaling Concerns

导入/扫描/转换仍同步处理；没有大型真实数据计时，不能按文件行数断言性能差或贸然加线程。图库processEvents与延迟加载保留generation检查，后续先测量并明确取消/中断需求。

### 5) Fragile/High-Churn Areas

文件对提交依赖备份/回滚，普通写失败测试通过但不能保证断电原子性。QSettings多键存储不等于数据库事务。外部编辑PNG/JSON元数据过期已有检测，第三方工具兼容仍需代表性生产副本补验。

LabelMe进程、适配脚本及部署已在原生标注阶段退役；现保留已有JSON数据兼容。Qt原生事件/保存恢复已自动验证，真实大图手感与高DPI仍需补验。Doxygen生成了声明索引并补充关键边界注释，历史成员文档覆盖率未统计；本机未找到clang-format，新增规则未作全库格式重写。

### 6) `[ASK USER]` Questions

Q01已明确单账户、未登录只读；仍待确认Q02异常分类/掩码主契约；Q03矩形名称与task映射；Q04角色人数、审计和备份。推荐方案见ISSUES.md。依赖决策的具体改动暂停，独立修复已完成。

### 7) Evidence

[当前自动验证](../logic/VALIDATION.md)、[业务回归](../../tests/workflow_regression.cpp)、[历史运行观察](../logic/evidence/runtime-ui.json)。
