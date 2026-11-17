# External Integrations

## 1. Integration Inventory

| 集成 | 入口与模块 | 当前状态 |
|---|---|---|
| SQLite / QSQLITE | core/DatabaseManager | 单账户认证与自身改密，旧权限表保留但停用 |
| 本地文件 | core/AtomicFile、RecoveryStore、deeplearning/storage | 原子单文件、配对传输与恢复移动，临时样本已验证 |
| LabelMe JSON | deeplearning/storage/AnnotationStore | 仅数据读取兼容rectangle/polygon；Python进程、嵌入与部署已退役 |
| Windows DWM | app/FuseVision | 平台专有；offscreen 不验证原生窗口效果 |
| OpenCV / Halcon / MVS | FuseVisionVisionSdk → Traditional | 构建/链接要求集中于传统模块；尚无算法或相机执行引擎 |
| Doxygen | docs/Doxyfile、cmake/Documentation.cmake | 本机 1.18.0，生成分组 HTML/XML；不参与运行时 |
| GitHub Actions | .github/workflows/build.yml | 配置存在；本轮未查询远端运行结果 |

## 2. Data Stores and Process Boundaries

项目、模型、图像和标签仍为本地文件；SQLite 仅账户数据，QSettings 存路径/主题等偏好。没有云对象存储、业务 HTTP API 或训练服务。业务格式及零基/一基编号见[数据契约](../logic/DATA-CONTRACTS.md)。

LabelMe自研适配及路径解析已从源码和CMake退役；旧源码保留在本轮基线快照。第三方sdk和历史labelmePythonPath配置不删除，但应用不再读取该配置或启动外部标注器。

## 3. Credentials

users.password_hash 沿用既有 SHA-256 保持旧密码兼容。新库只创建一个管理账户；历史其他账户不再认证。登录/改密成功清理旧记住字段，不自动填充或持久化密码。没有在此次结构整理中迁移密码算法或新增账号恢复机制。

## 4. Failure and Recovery

AnnotationStore 返回保存结果，失败不清编辑状态；文件服务按组暂存/回滚并报告失败源。多文件与 QSettings 不是断电事务。标注在同一Qt进程内执行，未提交绘制、未保存和保存失败由原生工作区处理。

## 5. Observability

Logger/spdlog 提供控制台/文件，LogMonitor 提供接入的界面消息。未实现远程追踪、指标或崩溃服务。Doxygen 的日志仅用于文档生成错误；编译与业务测试结果另行记录。

## 6. Evidence

[部署](../../cmake/DeployRuntime.cmake)、[原生标注事件测试](../../tests/native_annotation_regression.cpp)、[数据库](../../src/core/DatabaseManager.cpp)、[文档目标](../../cmake/Documentation.cmake)、[验证](../logic/VALIDATION.md)。


Qt动态图像插件依赖由cmake/DeployPluginDependencies.cmake扫描vcpkg运行库并复制，Windows系统DLL排除，未解析依赖使构建失败。仅复制插件本体不足以支持TIFF/WebP；startup-smoke与解压交付包均实际验证五种格式读写（ISS-017）。
