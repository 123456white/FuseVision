# Coding Conventions

## 1. Naming Rules

类型/文件用 PascalCase，方法 camelCase，成员 m_，槽 onXxx，界面初始化 initXxx，刷新 applyXxx。目录按 core/ui/app/deeplearning/traditional 及业务职责分层。页面键仅描述操作，当前授权不再依赖历史数据库角色矩阵。

## 2. Formatting and Documentation

.editorconfig 定义 UTF-8、LF、4 空格及文件末尾换行。.clang-format 提供 Microsoft 基础格式、Allman 花括号和 120 列，供编辑器/clang-format 使用；本轮没有批量格式化全部历史代码。Doxygen 不是格式化器或静态缺陷检查器。

公共头以 @file/@ingroup 标记模块，关键公共类型和接口写 @brief；涉及写入的接口说明输入编号、失败返回值和旧数据保持规则。QObject/Widget 注释明确所属线程、parent 所有权与借用指针。注释写可由代码核对的行为，不把占位能力写为已实现，不在每个访问器重复显而易见内容。

Doxygen 当前生成完整声明索引并校验已写注释；EXTRACT_ALL=YES、WARN_IF_UNDOCUMENTED=NO，不把零警告等同全部成员完整注释。源码与关键接口持续补注释，避免模板化填充。

## 3. Includes and Module Boundaries

同目录可包含 Foo.h，跨目录使用从 src 根开始的完整职责路径，例如 deeplearning/domain/AnnotationShape.h。公共头直接包含所需值类型，Widget 指针优先前向声明，不依赖上层头文件的偶然传递包含。Core/领域/状态/存储/工具不得包含业务 Widget。Qt Gui 的 QImage、QPainter、QUndoStack 不等于 Qt Widgets。

CMake PUBLIC 仅用于公开接口需要的依赖；应用组合根对业务页 PRIVATE 链接。第三方 SDK 的 include/link 要求集中于 FuseVisionVisionSdk，并由 Traditional 私有消费，不全局注入 Core/Data。边界同时通过配置检查和源码 include 检查验证。

## 4. Errors, Authorization and Logging

底层使用 bool+错误字符串或结构化结果，不创建弹窗。UI 在实际修改前调用 WriteAccess::require；AccessControl 本身只读会话、不含 QMessageBox。文件服务的 Allowed 回调来自调用方，不引用 SessionManager 或数据集页面。只有提交成功才清 dirty、更新成功统计和日志。

QSaveFile 提交单文件；多文件配对依赖暂存/备份/回滚，不宣称断电事务。失败返回具体路径与原因，保留编辑态和可重试信息。Logger 的文件日志与 LogMonitor 的显式界面消息各有用途，不以写文件日志代替用户反馈。

## 5. Testing Conventions

行为测试使用临时目录、合成图像、内存 SQLite 和临时 INI，不覆盖真实配置或数据。界面业务回归使用实际 Widget 与模态事件驱动；数据目标仅用 QCoreApplication，禁止链接 Qt Widgets。目录移动靠编译/include 检查验证，新增行为只为真实跨模块风险增加测试，不为纯移动逐行复刻测试。

格式、文档、编译、自动业务和人工桌面验证分开报告；启动成功不表示业务闭环验收完成。

## 6. Evidence

[格式配置](../../.clang-format)、[Doxygen](../Doxyfile)、[WriteAccess](../../src/ui/WriteAccess.h)、[AnnotationStore](../../src/deeplearning/storage/AnnotationStore.h)、[CMake 边界](../../cmake/ModuleBoundaries.cmake)、[测试范围](TESTING.md)。
