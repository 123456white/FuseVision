# Testing Patterns

## Core Sections (Required)

### 1) Test Stack and Commands

```powershell
./CmakeBuild.ps1 -Configuration Release -NoRun -Test
```

当前CTest包含34个独立发现的GoogleTest存储用例，以及六组既有回归：data-contract-regression、module-boundaries、native-annotation-regression、workflow-regression、startup-smoke、current-issues-regression，合计40项。Release本轮全部通过；不按CTest数量推算产品覆盖率。也可在已配置环境运行 `ctest --preset windows-release --output-on-failure`。完整结果及限制见[VALIDATION](../logic/VALIDATION.md#2026-11-17-严格原生存储标准)。

只执行存储用例：`ctest --preset windows-release -R '^storage\.' --output-on-failure`。直接运行 `out/build/windows-x64-release/bin/Release/FuseVisionStorageTests.exe --gtest_filter=Storage.*` 可筛选GoogleTest；需要其独立XML时加 `--gtest_output=xml:<独立输出文件>`。一键测试脚本仍输出 `out/build/<preset>/Testing/junit.xml`，本轮归档副本见验证记录。vcpkg的tests feature提供GoogleTest 1.17.0；BUILD_TESTING默认ON，在project初始化前选择该feature，关闭时不查找/链接GoogleTest。首次测试构建需可获取该锁定依赖；应用业务目标不链接GoogleTest。

### 2) Test Layout

`tests/storage_regression.cpp`为真实存储行为的GoogleTest，目标FuseVisionStorageTests只链接Data和GTest::gtest，使用QCoreApplication，自定义main隔离用户/系统INI。CMake通过gtest_discover_tests的PRE_TEST模式发现每例，参数名采用稳定编号。覆盖类型/版本拒绝、无效保存不覆盖、模型语义校验、原生/分类目录、UTF-8/LF YAML、旧绝对引用拒绝、移动后保存、外部引用拒绝、扩展字段、旧yolo拒绝及新建完整数据集配置、设备名拒绝和日志路径；不重写既有Widget回归。

`tests/workflow_regression.cpp`是真实QApplication/Widget处理器及文件语义回归，不依赖额外Qt Test模块。CTest target为FuseVisionWorkflowTests。QTimer处理隔离测试创建的模态框；关键选择使用实际按钮click。`src/main.cpp`保留启动自检。归档中的`capture_ui.cpp`是历史分析采集器，输出的是旧缺陷观察，不可作为新回归直接重跑后宣称通过。

`tests/data_contract_regression.cpp`仅用QCoreApplication，链接FuseVisionDeepLearningData，编译时拒绝QT_WIDGETS_LIB。它验证提取后的类型、配置、undo/redo、PNG/JSON身份与几何重读、写失败保留内容及COCO配对导出。`tests/check_module_boundaries.py`检查实际include边界；`cmake/ModuleBoundaries.cmake`配置时检查禁止的直接/接口链接。`tests/native_annotation_regression.cpp`用真实Qt事件验证空格平移、缩放锚点、焦点、矩形拖放/两点击、UUID选择、保存下一张失败/重试/末图/重开及访客拒绝。外部LabelMe路径测试随入口退役。

`tests/current_issues_regression.cpp`覆盖ISS-011～014：语义非法导入保留旧配对、旋转移动/复制撤销与重开、分类目录的真实Widget预览/统计与服务导出/删除恢复。数据库测试将自身exe复制到临时runtime后启动独立子进程，验证设置目录、密码保持、WAL快照、再次迁移与失败不回落。

本轮还在同一真实Widget回归加入datasetStorage分组：合规配置读取不改写、无标记配置/缺失配置拒写、最新未知根字段保留、未知版本/文档类型/字段类型拒绝覆盖，以及修正文件后重试。模态警告由测试定时关闭，全部文件与账户为临时数据。

### 3) Test Scope Matrix

| 范围 | 自动覆盖 |
|---|---|
| 标注 | clean/dirty、矩形两次点击、选择移动/取消、形状删除、undo/redo、PNG/JSON重开、空标注、路径失败与重试、放弃/取消 |
| 六种工具 | 模型与矩形/选择处理器；画笔、单击橡皮、填充合并与多边形完成；掩码/类别/坐标round trip |
| 上下文 | 新建模型通知各一次、取消零通知、中文描述；A活动/B选中删除、取消、恢复目录失败、当前对象清理 |
| 类别 | 持久化标注下禁止删除/重排；重命名/追加保持槽位；稀疏COCO按名称映射追加；同图同类两形状的样本/类别实例数均为2 |
| 权限 | 未登录直接调用项目/模型/数据集/标注/设置处理器，前后文件字节/用户数不变；登录统一解锁、退出恢复只读；旧权限记录不影响新规则 |
| 文件传输 | 重名成对跳过、失败保持、提交前撤权、修复重试、精确配对删除与restore.json还原 |
| 导出/转换 | COCO图像与类别引用、YOLO归一化、OBB角点、有损警告、选中范围、空标注、导出失败不提交、RLE独立重读 |
| 质量检查 | 未标注坏图、非法类别、孤立标签均含具体路径 |
| 设置 | 隔离持久化重读、活动根保持、日志即时切换、不迁移项目、写失败恢复重试、取消重置、实际日志名过期匹配 |
| 存储契约 | 34个GTest独立用例与datasetStorage Widget分组；便携引用、旧格式拒绝、完整版本/字段/任务守卫、原子失败、设备名及带点日志目录 |

### 4) Mocking and Isolation Strategy

全部业务样本在QTemporaryDir；普通Widget数据库为`:memory:`，数据库迁移回归使用临时runtime和文件库；QSettings默认INI且用户/系统scope定向临时目录。测试账户使用隔离初始化数据，每例恢复超级管理员会话并清理模态驱动。失败路径通过临时目录位置放同名普通文件构造，不依赖管理员能否绕过ACL。不会删除真实工程、转换生产标签或写用户原配置。

模块检查不读用户数据。重复CTest时将FUSEVISION_WORKFLOW_EVIDENCE_DIR和FUSEVISION_ACCOUNT_EVIDENCE_DIR置空，避免改写历史截图。

### 5) Coverage and Quality Signals

先保留可复现失败再修最小路径；构建日志包含原dirty、信号、类别、COCO编号、外部PNG元数据及填充覆盖缺陷的回归失败。没有伪造覆盖率或性能提升。CTest通过不代表原生桌面、全部对话框和外部进程通过；只以明确列出的断言范围作为证据。

单账户回归还覆盖四主页面/九子页访问、只读打开项目模型与标签配置字节不变、错误密码/取消登录、旧账户拒绝、密码修改失败与成功，以及实际主窗口退出登录的取消/保存失败/重试。最新结果见验证报告2026-11-17节。

### 6) Evidence

新标准[Release日志](../logic/evidence/strict-storage-green-20261117.log)、[JUnit](../logic/evidence/strict-storage-junit-20261117.xml)和[完整CTest输出](../logic/evidence/strict-storage-ctest-20261117.log)记录40/40通过。[红阶段](../logic/evidence/strict-storage-red-20261117.log)为40项中12项预期失败：旧配置仍被接受、未生成数据集配置、路径及字段规则缺失。此前兼容策略的33/33为历史执行结果，见[存储规范历史记录](../logic/VALIDATION.md#2026-11-17-功能回归与存储规范)，不表示当前仍兼容旧数据。

当前四项修复结果见[Release日志](../logic/evidence/issue-fixes-final-release-20261117.log)；[修改前失败日志](../logic/evidence/issue-fixes-before-20261117.log)保留四项预期失败。此前原生标注结果见[native-annotation-ctest-release.log](../logic/evidence/native-annotation-ctest-release.log)及[机器校验](../logic/evidence/native-annotation-validation.json)。

历史[模块整理CTest日志](../logic/evidence/module-refactor-ctest-release.log)、[机器校验](../logic/evidence/module-refactor-validation.json)、[本轮基线](../logic/evidence/module-refactor-start-baseline.json)。历史保留[单账户日志](../logic/evidence/single-account-ctest-release.log)和[2026-11-15日志](../logic/evidence/workflow-ctest-release.log)。未执行的人工/外部验证和补验步骤集中于[验证报告](../logic/VALIDATION.md)。

Doxygen使用`./scripts/build-docs.ps1 -DoxygenPath <doxygen.exe路径>`或CMake的docs目标。文档警告使生成失败；历史HTML/XML分组校验由已归档的validate_analysis.py执行；当前文档引用与派生一致性使用`python -X utf8 scripts/docs/docflow.py check --diagrams`，两者范围不同。EXTRACT_ALL提供声明索引，WARN_IF_UNDOCUMENTED关闭，零警告不表示所有历史成员均有完整注释。C++格式遵循根.editorconfig和.clang-format；本轮未安装/运行clang-format，不做无关全量格式化。

## 文档工具回归

`python -X utf8 -m unittest discover -s tests -p test_docflow.py -v`独立检查文档工具，不加入C++业务CTest数量。覆盖基线归因、删除/新增、resume保持、review失效、证据保护、断链、过期派生与Stop续跑限制，使用临时目录，不写业务证据。功能关系与状态断言见[测试追踪](../logic/TEST-TRACEABILITY.md)。当前文档CI在`.github/workflows/docs.yml`，由构建工作流调用；远端实测状态见最新验证记录。

## 2026-11-17 全功能关系回归与CI门禁

当前CTest总数41：34个GoogleTest、6组既有业务/边界/启动回归，以及新增documentation-tools（32个Python工具断言，含6个CI工具测试）。Ninja Debug和Release均41/41通过；[追踪映射](../logic/TEST-TRACEABILITY.md#执行门禁映射)关联19个TC与24条输出核验。默认JUnit成功输出上限提高至1000000字节，避免后续业务分组断言被截断。门禁拒绝未映射追踪、缺失测试、失败/跳过、重复测试及缺少断言标记；不宣称覆盖率100%。

新增additionalFunctions真实Widget测试覆盖主题持久化、日志文本转义/1000行保留/清空/折叠、无标注类别重排失败恢复及重试、图库筛选全选。ISS-016保留Debug两项崩溃与堆栈，修复导航目标引用失效与页面析构信号回调。隔离数据规则继续适用。

```powershell
python -X utf8 scripts/ci/verify_traceability.py --junit out/build/windows-x64-release/Testing/junit.xml --output out/validation/traceability.json
python -X utf8 scripts/ci/package_windows.py --source out/build/windows-x64-release/bin/Release --archive out/delivery/FuseVision-Windows-x64.zip --revision local
python -X utf8 scripts/ci/package_windows.py --archive out/delivery/FuseVision-Windows-x64.zip --verify-only
```

打包只选应用exe、运行DLL和三类Qt插件，不携带测试exe/GTest、账户、项目、配置、日志和恢复目录。SHA256校验及解压启动在临时目录执行，PATH仅系统目录，清空外部Qt插件路径，内存数据库。GitHub Actions同时检查Ninja Debug/Release及VS2022 Debug；文档可复用工作流检查镜像、引用与Mermaid。两项作业通过才交付版本化ZIP和校验文件；v*标签才发布prerelease。详见[CI/CD](../GETTING-STARTED.md#cicd)。
