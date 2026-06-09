# 深度学习一体化平台完整设计文档（最终版）

> **项目归属**：本文档是 [FuseVision](../README.md) 桌面应用的深度学习模块设计方案。
> FuseVision 是 Qt6 C++ 智能机器视觉一体化平台，集成深度学习（YOLO）、传统图像处理（Halcon/OpenCV）、工业相机采集与用户权限管理。
>
> **已实现的基础设施**（可直接复用）：
> - `Logger` / `LogMonitor`：日志记录与底部可折叠面板
> - `SettingsManager`：QSettings 持久化，已含 `dlDataPath/dlModelPath/dlDatasetPath`
> - `PermissionGuard`：8 个 DL 权限点已注册（`深度学习.项目管理` ~ `深度学习.模型导出`）
> - `SessionManager`：用户登录会话管理（信号驱动）
> - `ThemeManager`：亮色/暗色主题自动切换
> - `DatabaseManager`：SQLite 用户 & 权限 CRUD
>
> **已集成的 SDK**（位于 `sdk/` 目录）：
> - `labelme/`：Python 标注工具（通过 QProcess 调用）
> - `halcon/`：传统视觉算法库（可选）
> - `MVS/`：海康工业相机 SDK
>
> **代码位置**：`src/DeepLearningWidget/`（静态库 `FuseVisionDeepLearning`）。当前实现进度：
> - ✅ `DeepLearningWidget`：8 标签页框架 + LogMonitor + PermissionGuard 权限控制
> - ✅ `ProjectConfig`：`.fvproj` JSON 创建/加载/保存/扫描
> - ✅ `ProjectManagement`：卡片式项目浏览 + 新建/打开项目 + 步骤引导条 + 状态栏联动
> - 🆕 `ModelManagement` ~ `ModelExport`（Tab 1~7）：占位页面，待实现

---

## 目录

1. 总体架构与核心工作流
2. 全局目录结构与配置文件
3. 项目管理模块（界面严格按用户设计）
4. 模型管理模块（界面严格按用户设计）
5. 数据集管理模块（优化界面）
6. 数据标注模块（集成 labelme）
7. 数据拆分模块（直观界面）
8. 模型训练模块（多版本 + FastAPI 后端）
9. 模型预测模块
10. 模型导出模块
11. 状态机与全局校验机制
12. 日志与底部状态栏
13. 技术实现建议
14. 后续扩展方向

---

## 一、总体架构与核心工作流

本平台采用 **单项目-多模型-单模型单数据集** 的核心逻辑。每个项目下可包含多个模型，每个模型绑定一个独立的数据集（包括原始图像、标注、拆分）。工作流顺序如下：

```
新建/打开项目
    ↓
[模型管理] 解锁 → 创建或打开模型（选定模型后 status.model = true）
    ↓
[数据集管理] 解锁 → 导入图像、定义标签类别 (data_imported = true)
    ↓
[数据标注] 解锁 → 使用 labelme 进行像素级标注 (annotated = true)
    ↓
[数据拆分] 解锁 → 划分训练/验证/测试集 (split_done = true)
    ↓
[模型训练] 解锁 → 支持多版本训练，调用 FastAPI 后端 (trained = true)
    ↓
[模型预测] 和 [模型导出] 同时解锁
```

所有功能按钮的锁定/解锁基于当前模型在项目配置文件中的状态字段，鼠标悬停显示提示。

---

## 二、全局目录结构与配置文件

系统预设三个全局根目录（可在设置中修改）：

| 根目录 | 用途 | 示例路径 |
|--------|------|----------|
| `data_root` | 存放原始数据（图像、视频等） | `D:\...\dl_data` |
| `dataset_root` | 存放标注、拆分后的数据集 | `D:\...\dl_datasets` |
| `model_root` | 存放模型文件（.fvdl 及权重） | `D:\...\dl_models` |

### 2.1 项目目录结构示例（项目名：ProjectA）

```
dl_data/ProjectA/                     # 原始数据（用户放入或导入）
dl_datasets/ProjectA/                 # 项目下的数据集根
│   ├── model1_dataset/               # 模型1绑定的数据集
│   │   ├── images/                   # 图像（可软链接或复制）
│   │   ├── annotations/              # labelme 输出的 JSON 标注文件
│   │   ├── splits/                   # 拆分索引文件（train.txt, val.txt, test.txt）
│   │   └── dataset_config.json       # 类别、实例统计等元信息
│   └── model2_dataset/
dl_models/ProjectA/                   # 项目下的模型根
│   ├── model1.fvdl
│   ├── model1_checkpoints/           # 多版本权重目录
│   │   ├── 20250609_120000/          # 版本1（时间戳）
│   │   │   ├── best.pt
│   │   │   ├── best.onnx
│   │   │   └── best.engine
│   │   └── 20250610_150000/
│   └── model2.fvdl
```

### 2.2 项目配置文件 `.fvproj`

保存在 `dl_data/ProjectA/project.fvproj`，JSON 格式：

```json
{
  "project_name": "ProjectA",
  "description": "工业缺陷检测项目",
  "created_time": "2026-06-09T10:00:00",
  "modified_time": "2026-06-09T15:30:00",
  "paths": {
    "data_root": "D:/.../dl_data/ProjectA",
    "dataset_root": "D:/.../dl_datasets/ProjectA",
    "model_root": "D:/.../dl_models/ProjectA"
  },
  "current_model_id": "model1",
  "models": {
    "model1": {
      "model_file": "dl_models/ProjectA/model1.fvdl",
      "dataset_folder": "model1_dataset",
      "status": {
        "data_imported": false,
        "annotated": false,
        "split_done": false,
        "trained": false
      }
    }
  }
}
```

### 2.3 模型文件格式 `.fvdl`

JSON 格式：

```json
{
  "format_version": "1.0",
  "model_name": "MaskRCNN_缺陷",
  "model_type": "实例分割",
  "description": "工业缺陷检测",
  "created_time": "2026-06-09 10:00:00",
  "modified_time": "2026-06-09 10:00:00",
  "project_ref": "ProjectA",
  "dataset_ref": "model1_dataset",
  "architecture": {
    "backbone": "resnet50",
    "neck": "FPN"
  },
  "training_versions": [
    {
      "version_id": "20250609_120000",
      "weights_pt": "checkpoints/20250609_120000/best.pt",
      "weights_onnx": "checkpoints/20250609_120000/best.onnx",
      "weights_engine": "checkpoints/20250609_120000/best.engine",
      "training_date": "2026-06-09 12:00:00",
      "config": {
        "optimizer": "AdamW",
        "lr": 0.001,
        "batch_size": 8,
        "epochs": 100,
        "attention_mechanism": "SE"
      },
      "metrics": {
        "mAP": 0.85,
        "loss": 0.23
      }
    }
  ],
  "current_version": "20250609_120000",
  "inference_config": {
    "confidence_threshold": 0.5,
    "nms_threshold": 0.4
  }
}
```

---

## 三、项目管理模块 ✅ 已实现

### 3.1 界面布局（实际实现）

> **核心类**：`ProjectConfig`（纯静态工具类），`ProjectManagement`（QWidget，含 `StepGuideBar` + `ProjectCard` 内部类）

- 主窗口使用 `QSplitter` 左右分栏，`setStretchFactor(0,1) + setStretchFactor(1,5)` 比例 **1:5**。
- **左侧栏**（`m_leftSplitter`，上下 1:9）：
  - **左上栏**：新建项目（蓝色 `accentPrimary` 主按钮）、打开项目（描边按钮），图标使用 `NewProject.png` / `OpenProject.png`（暗色主题自动切换 `_D.png` 后缀）。
  - **左下栏**：
    - 模型列表标题 + `QListWidget`（最大高度 150px），显示当前项目下的所有模型 ID，单击可切换。
    - 项目创建/修改时间（`yyyy-MM-dd hh:mm` 格式）。
    - 项目描述（打开或单击预览时显示，带 `bgTertiary` 圆角背景块，无描述时自动隐藏）。
    - 分隔线。
    - **底部弹簧（`addStretch`）** + 逻辑说明文字（`"单项目管多模型..."`），沉底显示。
- **右侧主体**（`m_rightContainer`）：
  - `QScrollArea` + `QGridLayout` 卡片网格：扫描 `dl_data/` 下所有包含 `project.fvproj` 的文件夹。
  - 每个 `ProjectCard`（200x210 固定尺寸，上方 160px 图标 + 下方 40px 名称），三态：默认 `bgSecondary + borderLight`、悬停 `bgHover + accentPrimary 边框`、选中 `bgSelected + accentPrimary 2px 边框`。
  - 列数动态自适应：`cols = max(1, (viewportWidth + spacing) / (200 + spacing))`，`resizeEvent` 触发 `relayoutCards()` 重排。
  - 空状态提示：无项目时显示 `"暂无项目，请点击左上角新建项目创建第一个项目"`。
  - 底部 `StepGuideBar`：6 步横向圆点 + 名称 + 连接线，已完成为绿色，当前步为蓝色高亮，未到达为灰色。

### 3.2 组件架构

| 类 | 职责 | 关键方法 |
|----|------|---------|
| `ProjectConfig` | 纯静态工具，`.fvproj` JSON 读写 | `createProject()`, `loadProject()`, `saveProject()`, `scanProjects()`, `isValidProjectName()` |
| `ProjectCard` | 项目卡片（QFrame 子类） | `setSelected()`, `refreshTheme()`, signal `clicked`/`doubleClicked` |
| `StepGuideBar` | 底部 6 步引导条 | `setCurrentStep()`, `setStepCompleted()`, `refreshTheme()` |
| `ProjectManagement` | 标签页主容器 | `refreshCards()`, `relayoutCards()`, `applyTheme()`, signal `projectOpened`/`projectPreviewed`/`projectClosed` |

### 3.3 新建项目流程

1. 点击"新建项目" → 弹出 `QDialog`（`QFormLayout`）：
   - **项目名称**（`QLineEdit` *必填）：占位提示 `"仅允许英文字母、数字和下划线"`，校验正则为 `^[A-Za-z0-9_]+$`，长度 1~64。
   - **项目描述**（`QTextEdit` 可选）：`maxHeight=80`，占位提示 `"可选，简要描述项目用途"`。
   - 两个输入框均适配 `ThemePalette`（`surfaceWhite` 底色、`borderMedium` 边框、`accentPrimary` 聚焦边框）。
2. 调用 `ProjectConfig::createProject(name, desc, dlDataPath, dlDatasetPath, dlModelPath)`：
   - 在三个全局根目录下创建同名子文件夹。
   - 生成 `project.fvproj`（含 `description` 字段）。
3. 成功后弹出提示并 `refreshCards()` 刷新卡片网格。

### 3.4 打开项目流程

1. 点击"打开项目" → `QFileDialog` 选择 `project.fvproj` 文件。
2. `ProjectConfig::loadProject()` 解析 JSON，填充 `FvprojInfo` 结构体。
3. 校验 `dataRoot` / `datasetRoot` / `modelRoot` 是否存在；若缺失则 `QMessageBox::question` 询问是否继续。
4. 更新 `m_currentProject` + `m_hasProject = true` → `updateLeftPanel()` + `updateStepGuide()`。
5. `emit projectOpened(name, path)` → `DeepLearningWidget::onProjectOpened` → `dlProjectChanged` 信号 → `FuseVision` 状态栏更新 + **自动跳转至模型管理标签页（Tab 1）**。

### 3.5 单击预览 vs 双击打开

| 行为 | 单击选中卡片 | 双击卡片 / 打开按钮 |
|------|-------------|-------------------|
| `m_hasProject` | `false`（预览态，存储在 `m_previewProject`） | `true`（打开态，存储在 `m_currentProject`） |
| 左侧面板更新 | 时间 + 描述 + 模型列表 | 同左 |
| `projectPreviewed` 信号 | 发射 → 状态栏显示项目名 | — |
| `projectOpened` 信号 | 不发射 | 发射 → 自动跳模型管理 |
| 步骤引导条 | 停留在步骤 0 | 步骤 0 完成 → 指向步骤 1 |

### 3.6 明暗主题适配

- 所有硬编码颜色已替换为 `ThemePalette` 成员（`bgPrimary` / `bgSecondary` / `textPrimary` / `accentPrimary` 等）。
- `connect(&ThemeManager::instance(), &ThemeManager::themeChanged, ...)` 驱动 `applyTheme()` 全量重刷。
- PNG 图标自动切换暗色版（`NewProject_D.png` 等暗色后缀资源）。

### 3.7 卡片网格自适应列数

```cpp
// ProjectManagement::relayoutCards()
int availWidth = viewport->width() - layoutMargins.left() - layoutMargins.right();
const int cols = qMax(1, (availWidth + spacing) / (200 + spacing));
for (int i = 0; i < m_cards.size(); ++i)
    m_cardLayout->addWidget(m_cards[i], i / cols, i % cols);
```
`resizeEvent` 中调用 `relayoutCards()`，窗口缩放时实时重排。

### 3.8 状态栏联动

信号链路：
```
ProjectCard::clicked -> onCardClicked -> emit projectPreviewed(name)
  -> DeepLearningWidget -> emit dlProjectChanged(name)
    -> FuseVision::setProjectName(0, name) -> m_dlProjectLabel + m_statusBar.update()
```

---

## 四、模型管理模块（界面严格按用户设计，不作改动）

### 4.1 界面布局

- 最外层 widget 分两栏：左侧边栏、模型管理主体，比例 **1:5**。
- 模型左侧边栏分上下两栏，比例 **1:9**。
  - **左上栏**：新建模型、打开模型两个圆角按钮（左图标右文字），资源 `Newfile`/`Openfile`。
  - **左下栏**：显示当前项目下所有模型列表（模型名称、类型），点击可切换当前模型。
- **右侧主体**：
  - 显示模型图像（结构图或示例预测图），并可点击进入数据集管理。
  - 下方显示模型详细信息。

### 4.2 新建模型对话框

- 布局：垂直方向分三栏，比例 **3:1:1**。
  - **第一栏**：左栏为深度学习类型（带图标+文字：实例分割、异常检测、物体检测(非自由矩形)、物体检测(自由矩形)）；右栏为该类型对应的简介描述（动态更新）。
  - **第二栏**：左栏为“模型名称”输入框；右栏为“模型文件路径”输入框 + 浏览按钮（限定 `.fvdl`，默认定位到 `dl_models/当前项目名/`）。
  - **第三栏**：创建模型、取消按钮。
- 创建逻辑：
  - 生成 `.fvdl` 文件，写入模型类型、名称、创建时间等。
  - 自动在 `dl_datasets/当前项目名/` 下创建以模型名称命名的数据集文件夹（如 `model1_dataset`），生成初始 `dataset_config.json`（空类别）。
  - 更新项目的 `.fvproj`，添加该模型记录，设置 `status.data_imported=false`，并将 `current_model_id` 指向新模型。
  - 关闭对话框后刷新模型列表及右侧信息。

### 4.3 模型信息显示

- **未选定模型时**：显示“未选定模型，可点击新建模型按钮创建新项目或打开已有项目，或选择右侧显示的模型。”
- **选定模型时**（从 `.fvdl` 和项目状态读取）：
  - 模型文件：`dl_models/项目名/xxx.fvdl`
  - 图像所在目录：`dl_data/项目名`（原始数据）或 `dl_datasets/项目名/模型名_dataset/images`
  - 标签类别：`xx (总数)：class1, class2, ...`
  - 实例个数：`xxx 图，xxx 实例`（从 `dataset_config.json` 统计）
  - 创建时间/修改时间：`yyyy-mm-dd hh:mm / yyyy-mm-dd hh:mm`

### 4.4 打开模型

- 通过文件选择器选择一个 `.fvdl` 文件。
- 校验该文件是否属于当前项目（比较 `project_ref`）。
- 加载模型信息，更新当前模型 ID，刷新锁定状态及右侧信息。

---

## 五、数据集管理模块（优化界面设计）

### 5.1 整体布局

- 主窗口左右可调分栏（默认比例 **1:5**）。
- 左侧为属性面板（四个固定面板），右侧为图像画廊。
- 底部固定横向引导栏。

### 5.2 左侧属性侧边栏

#### 面板1：当前绑定模型信息区（顶部固定）

- 展示：绑定模型名称、任务类型（带图标）。
- 关联数据集目录完整路径（可点击复制）。
- 数据集全局状态标签：🟡 未导入图像 / 🟢 已导入未标注 / 🔵 已标注未拆分 / 🟣 拆分完成。

#### 面板2：图像信息面板

- 无选中图像时置灰，提示“请选择右侧预览图像”。
- 选中画廊任意图像后刷新内容：
  - 图像文件名
  - 本地绝对路径
  - 图像分辨率（宽 × 高）
  - 标签实例数量（当前图标注实例总数）
- 配套功能按钮：
  - 🔄 刷新单图（重新读取标注文件）
  - 📂 打开图像所在文件夹

#### 面板3：拆分映射面板（数据集拆分管理）

- **未拆分时**：显示“⚡ 尚未拆分数据集” + 提示文案，按钮“前往拆分页”。
- **已拆分时**：显示条形图（训练/验证/测试比例及数量），按钮“查看拆分详情”、“重置拆分记录”。
- 查看拆分详情 → 跳转【数据拆分】模块。
- 重置拆分 → 清空 `splits/` 目录，重置 `split_done=false`。

#### 面板4：标签类别管理（可折叠）

- 列表展示所有标签（名称、颜色、实例数）。
- 支持添加、修改、删除（锁定状态：仅在数据集导入后、标注前可编辑）。
- 快速导入：从 labelme labels.txt 或 COCO json 导入。

### 5.3 右侧图像画廊主体区

#### 顶部工具栏

- **导入图像**按钮（核心入口，未导入时高亮引导）。
  - 弹窗支持多选本地图片 / 文件夹批量导入。
  - 自动过滤非图片文件（png/jpg/bmp/tif）。
  - 导入完成自动写入数据集目录，置 `data_imported = true`，解锁【数据标注】模块。
  - 批量刷新画廊。
- **数据集统计面板**：显示总图像数、已标注图像数、实例总数、标签类别总数。
- **视图切换**：网格视图 / 列表视图。
- **筛选**：全部 / 未标注 / 已标注。

#### 画廊网格

- 缩略图统一尺寸（如 200x200），缩略图下方显示文件名（名称区域高度比例 4:1）。
- 状态叠加标识：
  - 已标注图像：右上角绿色勾选标志 ✓
  - 未标注图像：右上角灰色圆圈 ●
- 交互：
  - **单击缩略图**：左侧属性面板刷新对应图像元数据。
  - **双击缩略图**：跳转【数据标注】模块，直接打开当前图像进行掩码标注（若未导入图像则禁用）。
  - **右键菜单**：删除当前图像、打开文件目录、复制图像路径、重新标注。

### 5.4 底部引导步骤栏

- 横向展示完整工作流步骤：📥 数据集导入 → 🏷️ 标签定义 → ✏️ 像素级标注 → 🔀 数据集拆分。
- 未完成步骤置灰锁定，鼠标悬停显示阻断提示文案（如“请先导入图像”）。
- 当前步骤高亮，已完成步骤显示绿色对勾。
- 全局进度条：显示当前阶段完成百分比（如 3/4 步骤完成）。

---

## 六、数据标注模块（集成 labelme）

### 6.1 启动方式

- 在数据集管理画廊中双击任意图像 → 调用外部 labelme 进程。
- 传递参数：
  - `--image`：当前图像绝对路径
  - `--output`：该模型数据集的 `annotations/` 目录
  - `--labels`：从 `dataset_config.json` 生成的 labelme 标签文件（labels.txt）
- 支持批量标注：选中多张图像 → 右键“批量标注” → 依次打开 labelme（用户手动切换图像）。

### 6.2 状态同步

- 软件使用文件系统监视器（如 QFileSystemWatcher）监听 `annotations/` 目录变化。
- 标注完成后（JSON 文件生成或修改），自动：
  - 刷新画廊中对应图像的标注状态标识。
  - 更新统计信息：已标注图像数、实例总数。
  - 若所有图像均已标注（`annotated_count == image_count`），则自动设置 `annotated = true`，解锁【数据拆分】模块，并在底部引导栏标记“标注完成”。

### 6.3 辅助功能

- 支持导入外部 labelme 格式标注：拖放 JSON 文件到画廊，自动复制到 `annotations/`。
- 提供标注一致性检查：如缺失标注文件、标签不在类别列表中等，给出警告。

---

## 七、数据拆分模块（直观界面设计）

### 7.1 独立窗口布局

采用模态对话框或独立窗口，布局为上下结构。

#### 顶部控制区

- **拆分比例设置**：三个滑动条联动（总和100%），实时显示百分比。默认 70% / 15% / 15%。
- **高级选项**（可折叠）：
  - 随机种子（整数输入，默认 42）
  - 按类别均衡采样（checkbox，启用后避免某些类在某个 split 中缺失）
  - 保持图像原始目录结构（checkbox）
- **操作按钮**：
  - **一键拆分**（主按钮，绿色）
  - **重置拆分**（清空现有拆分，红色边框，需要二次确认）
  - **取消**（关闭窗口）

#### 底部预览区

- **拆分结果条形图**：横向堆叠条形图，不同颜色代表训练/验证/测试，鼠标悬停显示具体数量。
- **数据集摘要表格**：显示每个 split 的图像数量、标注实例数量、各类别分布（可折叠展开）。
- **文件列表预览**：可展开查看每个 split 的前20个图像路径。

### 7.2 拆分逻辑

- 基于图像级随机划分。
- 若启用“按类别均衡采样”，则使用分层采样（按图像中主要类别或实例平衡）。
- 生成文件：`splits/train.txt`, `val.txt`, `test.txt`，每行图像相对路径（相对于数据集根目录）。
- 拆分完成后：
  - 更新项目 `.fvproj` 中当前模型的 `status.split_done = true`。
  - 关闭窗口，刷新数据集管理左侧拆分面板（显示新的统计和条形图）。
  - 解锁【模型训练】模块。

### 7.3 校验与提示

- 若未完成标注（`annotated = false`），拆分按钮灰显，悬停提示“请先完成所有图像标注”。
- 若已有拆分记录，点击“重置拆分”弹出确认对话框，重置后 `split_done = false`。

---

## 八、模型训练模块（多版本 + FastAPI 后端）

### 8.1 独立窗口/可停靠面板

布局：左侧参数配置区（可折叠分类），右侧训练监控区（日志 + 实时指标图表）。

#### 左侧：参数配置区（使用折叠面板或标签页）

**基础参数**（展开）：
- 训练轮数（整数输入，滑动条辅助）
- 批次大小（下拉：2,4,8,16,32）
- 学习率（科学计数法输入）
- 优化器（下拉：SGD, Adam, AdamW, RMSprop）
- 学习率调度器（下拉：StepLR, CosineAnnealing, ReduceLROnPlateau）

**模型架构参数**（根据任务类型动态显示）：
- 主干网络（下拉：ResNet50, ResNet101, EfficientNet-B0…）
- 注意力机制（下拉：None, SE, CBAM, Transformer）
- 颈部（下拉：FPN, PAN, BiFPN）
- 检测头特定参数（如 anchor 尺寸、NMS 阈值等）

**训练策略与增强**（折叠面板）：
- 数据增强（多选框：随机翻转、旋转、色彩抖动、马赛克…）
- 损失函数权重（文本输入，如 `1.0,0.5`）
- 早停（checkbox + patience 轮数）
- 混合精度训练（checkbox）
- GPU 选择（多选框，显示可用 GPU）

**版本信息**：自动生成版本 ID（时间戳），显示在顶部。

#### 右侧：训练监控区

- **日志窗口**：实时显示后端返回的训练日志（可复制、清空）。
- **指标图表**：动态绘制 loss、mAP、准确率等曲线（使用 QtChart 或 matplotlib 嵌入）。
- **进度信息**：当前 epoch / 总 epoch，进度条，剩余时间估算。
- **控制按钮**：开始训练、停止训练（优雅中断）、导出训练配置（JSON）。

### 8.2 多版本管理

- 每次训练自动创建新版本，版本 ID = `YYYYMMDD_HHMMSS`。
- 权重保存路径：`dl_models/项目名/模型名_checkpoints/版本ID/best.pt`
- 训练完成后后端自动转换 ONNX 和 TensorRT（使用 torch.onnx 和 TensorRT Python API）。
- 更新 `.fvdl` 文件：在 `training_versions` 数组中追加记录，并将 `current_version` 指向最新版本，设置 `trained = true`。

### 8.3 后端交互

- 前端收集参数 → POST 请求到 FastAPI `/train`。
- 请求体示例：
```json
{
  "project_name": "ProjectA",
  "model_name": "model1",
  "version_id": "20250609_120000",
  "fvdl_path": "dl_models/ProjectA/model1.fvdl",
  "dataset_path": "dl_datasets/ProjectA/model1_dataset",
  "train_params": { ... }
}
```
- 后端异步执行训练，通过 WebSocket 推送日志和指标。
- 训练完成后自动执行 ONNX 和 TensorRT 转换，并更新 `.fvdl`。
- 前端收到完成信号后，刷新模型信息，解锁【模型预测】和【模型导出】。

### 8.4 状态更新

- 训练开始 → 禁用开始按钮，显示“训练中...”，停止按钮可用。
- 训练完成/中断 → 恢复按钮状态，若成功则设置 `trained = true`。

---

## 九、模型预测模块

### 9.1 独立窗口/面板

布局：左右分栏（比例 1:2），左侧配置，右侧结果展示。

#### 左侧配置区

- **模型版本选择**：下拉框（展示所有 `training_versions` 的时间戳，默认最新）。
- **推理引擎选择**：按钮组（PT / ONNX / TensorRT），根据所选版本是否存在对应文件动态启用。
- **图像源选择**：
  - 单张图片（文件选择器）
  - 文件夹批量预测（目录选择器）
  - 使用测试集（从 `splits/test.txt` 读取）
- **后处理参数**：
  - 置信度阈值（滑动条 0~1）
  - NMS 阈值（滑动条 0~1）
  - 最大检测数（整数输入）
- **运行预测**按钮（主按钮）

#### 右侧结果展示区（标签页）

- **可视化页**：显示原图 + 叠加掩码/检测框，支持缩放、平移、保存结果图。
- **详情页**：表格形式显示每个检测对象的类别、置信度、坐标。
- **批量结果页**（仅批量预测时显示）：缩略图网格，单击查看详细结果。

### 9.2 自建图像集预测

- 用户可通过“导入图像”按钮添加任意图像（不进入数据集）。
- 预测结果可导出为 JSON / COCO 格式，或保存带标注的图像。

### 9.3 后端交互

- 发送预测请求到 `/predict`，传入模型路径、引擎类型、图像路径列表。
- 返回结果（框、掩码、类别等），前端渲染。

---

## 十、模型导出模块

### 10.1 独立对话框

布局：垂直布局，简洁明了。

- **选择模型版本**：下拉框（所有训练版本）。
- **导出格式**：多选框或按钮组（PyTorch(.pt), ONNX(.onnx), TensorRT(.engine), TorchScript(.ts), OpenVINO(.xml/.bin)）。
  - 若所选版本缺少某种格式，自动提示“将先进行转换”。
- **导出路径**：显示默认路径（`.../checkpoints/版本号/export/`），支持浏览自定义。
- **高级选项**（可折叠）：是否压缩、是否包含元数据、输入尺寸覆盖等。
- **导出按钮** + **取消按钮**

### 10.2 导出流程

1. 校验所选版本是否存在对应权重（.pt 必须存在）。
2. 对于需要转换的格式，调用后端 `/export` 接口进行转换（若已有则直接复制）。
3. 导出完成后弹出成功提示，并提供“打开文件夹”按钮。
4. 支持一键导出所有格式（checkbox 全选）。

---

## 十一、状态机与全局校验机制

### 11.1 状态存储

- 每个模型的状态字段存储在 `.fvproj` 的 `models[model_id].status` 中：
  - `data_imported`：是否已导入图像
  - `annotated`：是否已完成标注
  - `split_done`：是否已拆分
  - `trained`：是否至少训练过一次
- 全局状态管理器监听这些字段的变化，并通知所有相关 UI 更新（按钮锁定、步骤引导条等）。

### 11.2 操作前校验

- 任何关键操作（如进入数据集管理、训练、预测）都会先检查当前模型对应的状态。
- 不满足条件时：
  - 按钮显示为禁用（灰显）状态。
  - 鼠标悬停显示具体原因，例如：“请先在数据集管理中导入图像并定义标签类别。”
- 若尝试通过其他方式触发（如快捷键），弹出模态提示框。

### 11.3 界面刷新

- **项目切换** → 重新加载 `.fvproj` → 刷新模型列表、当前模型、所有模块的锁定状态。
- **模型切换** → 更新当前模型 ID → 重新加载该模型的数据集状态 → 刷新数据集管理画廊、拆分面板等。
- **数据集导入/标注/拆分完成** → 自动更新状态并通知模型管理模块（更新 `.fvdl` 中的 `dataset_ref` 统计）。

---

## 十二、日志与底部状态栏

### 12.1 底部状态栏

- `FuseVision` 主窗口 `QStatusBar` 包含 `m_dlProjectLabel`（`QLabel`），初始显示 `"深度学习项目: 无"`。
- 项目单击/双击通过 `projectPreviewed` / `projectOpened` → `dlProjectChanged` 信号触发 `setProjectName(0, name)`，强制 `update()` label 和 statusBar。
- `m_traditionalProjectLabel` 对称支持传统视觉项目。
- 当前模型名称待 ModelManagement 实现后联动。

### 12.2 日志系统

- 所有操作（新建项目、打开项目等）通过 `Logger::info()` 写入日志。
- `DeepLearningWidget` 底部 `LogMonitor` 面板实时显示最新日志条目（支持 INFO / WARNING / ERROR 级别着色）。
- 日志面板可折叠、清空、导出。

---

## 十三、技术实现方案（针对 FuseVision Qt6 C++ 项目）

### 13.1 前端框架

**确定选型：Qt6 C++ (Widgets)**，与 FuseVision 主框架保持一致。

| 组件 | 实现方式 | 说明 |
|------|---------|------|
| 主容器 | `DeepLearningWidget`（`QWidget`，8 标签页） | 已在 `src/DeepLearningWidget/` 实现框架 |
| 项目管理卡片 | `QScrollArea` + `QGridLayout` 自适应列数卡片 | 已实现 `ProjectCard` + `ProjectManagement::relayoutCards()` |
| 图像画廊 | `QListWidget`（IconMode）/ `QTableWidget` | 缩略图懒加载 + 状态叠加 |
| 图表绘制 | `QtCharts`（`QLineSeries`） | 训练 loss/mAP 实时曲线 |
| 标签页锁定 | `PermissionGuard::canRead()` | 8 个权限点已注册，控制 Tab 可见性 |

**关键复用**：
- 状态栏项目名：`FuseVision::setProjectName(0, name)` → `m_dlProjectLabel` 实时更新
- 信号链路：`projectPreviewed` / `projectOpened` → `dlProjectChanged` → 状态栏
- 底部日志面板：`LogMonitor`（最大 1000 行、级别着色、可折叠）
- 配置读写：`SettingsManager`（`dlDataPath/dlModelPath/dlDatasetPath`）
- 主题适配：`ThemeManager` 信号驱动，全局自动切换 QSS

### 13.2 后端服务（Python）

**FastAPI + Uvicorn**，作为独立 Python 子进程运行。

- **启动方式**：`QProcess::start("python", {"-m", "uvicorn", "backend.main:app", "--port", "8000"})`
- **生命周期**：`DeepLearningWidget` 构造时启动，析构时 `terminate()` + `waitForFinished()`
- **健康检查**：启动后轮询 `GET /health`（超时 5s），失败则弹出警告
- **Python 环境**：建议使用项目内 `sdk/yolo/` 下的 `venv` 或 Conda 环境

### 13.3 通信协议

| 操作 | 协议 | 端点 | Qt 实现 |
|------|------|------|---------|
| 启动训练 | HTTP POST | `/train` | `QNetworkAccessManager::post()` |
| 训练日志流 | WebSocket | `/ws/train/{task_id}` | `QWebSocket` |
| 执行预测 | HTTP POST | `/predict` | `QNetworkAccessManager` |
| 模型导出 | HTTP POST | `/export` | `QNetworkAccessManager` |
| JSON 配置读写 | 本地文件 I/O | `.fvproj` / `.fvdl` | `QFile` + `QJsonDocument` |

### 13.4 深度学习库

- **PyTorch 2.0+**：训练与推理核心（Python 后端）
- **ONNX Runtime**：跨平台推理加速
- **TensorRT**：NVIDIA GPU 推理优化（需独立安装）
- **Ultralytics**：YOLO 系列模型训练框架（`sdk/yolo/` 目录）

### 13.5 数据存储

- 项目配置 `.fvproj` / 模型配置 `.fvdl`：JSON 格式，`QJsonDocument` 读写
- 标注文件：labelme JSON 格式（由 `sdk/labelme/` 生成）
- 拆分索引：纯文本 `train.txt / val.txt / test.txt`
- 日志：spdlog 统一输出到文件 + LogMonitor 面板
- 用户/权限数据：SQLite（`DatabaseManager`，已有 schema）

### 13.6 外部进程管理

| 进程 | 启动方式 | 管理 |
|------|---------|------|
| FastAPI 后端 | `QProcess`（Python 子进程） | 随 DL 模块启停 |
| labelme 标注 | `QProcess::startDetached()` | 独立窗口，通过 `QFileSystemWatcher` 监听标注输出 |
| 训练脚本 | 通过后端 API 间接触发 | 后端管理 GPU 进程 |

### 13.7 C++ 源码组织

```
src/DeepLearningWidget/
├── CMakeLists.txt                    # 静态库 FuseVisionDeepLearning
├── DeepLearningWidget.h/cpp          # 顶层容器（8标签页 + LogMonitor）✅已实现
├── ProjectManagement.h/cpp           # 标签1：项目管理（卡片视图 + .fvproj 解析）✅已实现
├── ModelManagement.h/cpp             # 标签2：模型管理（.fvdl 创建/切换）🆕待实现
├── DatasetManagement.h/cpp           # 标签3：数据集管理（画廊 + 标签类别）🆕待实现
├── DataAnnotation.h/cpp              # 标签4：数据标注（labelme 进程调度）🆕待实现
├── DataSplit.h/cpp                   # 标签5：数据拆分（比例配置 + 分层采样）🆕待实现
├── ModelTraining.h/cpp               # 标签6：模型训练（参数配置 + WebSocket 监控）🆕待实现
├── ModelPredict.h/cpp                # 标签7：模型预测（推理引擎选择 + 结果渲染）🆕待实现
├── ModelExport.h/cpp                 # 标签8：模型导出（格式转换调度）🆕待实现
├── ProjectConfig.h/cpp               # .fvproj JSON 读写工具类 ✅已实现
├── ProjectCard.h/cpp                 # 项目卡片组件（QFrame 子类）✅已实现
├── StepGuideBar.h/cpp                # 底部步骤引导条组件 ✅已实现
└── DatasetConfig.h/cpp               # dataset_config.json 读写 + 统计 🆕待实现
```

---

## 十四、后续扩展方向

- **多用户与协作**：通过 Git 管理项目配置文件，实现团队协作。
- **云端训练**：添加远程训练后端配置，支持 SSH 提交训练任务。
- **超参数搜索**：集成 Optuna，自动生成多个训练版本并比较。
- **更多数据增强**：集成 Albumentations 库，提供丰富的可视化增强预览。
- **模型 Zoo**：内置预训练模型下载和管理。

---

## 十五、与 FuseVision 现有架构的集成映射

### 15.1 信号与事件流

```
SessionManager::sessionChanged
    ↓
FuseVision::onSessionChanged → refreshStatusBar（更新用户信息）
    ↓
DeepLearningWidget::applyPermissions
    ↓
各子页面 PermissionGuard::canRead → 按钮/标签页启用/禁用
    ↓
用户操作（新建项目/导入数据/开始训练）→ Logger::info → LogMonitor 面板刷新
    ↓
SettingsManager → dlDataPath/dlModelPath/dlDatasetPath 读取 → 子页面路径初始化
```

### 15.2 权限点对应关系

| 权限键（已注册） | DeepLearningWidget Tab | 锁定条件 |
|-----------------|----------------------|---------|
| `深度学习.项目管理` | Tab 0（项目管理） | 需登录 `canRead` |
| `深度学习.模型管理` | Tab 1（模型管理） | `project_opened == true` |
| `深度学习.数据集管理` | Tab 2（数据集管理） | `status.model == true` |
| `深度学习.数据标注` | Tab 3（数据标注） | `status.data_imported == true` |
| `深度学习.数据拆分` | Tab 4（数据拆分） | `status.annotated == true` |
| `深度学习.模型训练` | Tab 5（模型训练） | `status.split_done == true` |
| `深度学习.模型预测` | Tab 6（模型预测） | `status.trained == true` |
| `深度学习.模型导出` | Tab 7（模型导出） | `status.trained == true` |

> **双重锁定机制**：`PermissionGuard::canRead` 控制 Tab 可见性（权限层面），`status` 字段控制操作按钮可用性（业务状态层面）。

### 15.3 与传统视觉模块的协作

- **共享 SettingsManager**：DL 和传统视觉的存储路径统一管理
- **共享 LogMonitor**：两个模块各自的 LogMonitor 实例，独立日志流
- **FuseVision 状态栏**：`m_dlProjectLabel` 显示当前 DL 项目名，`m_traditionalProjectLabel` 显示传统视觉项目名
- **相机采集 → DL 预测**（未来）：传统视觉模块的实时图像流可送入 DL 模块进行在线推理

### 15.4 构建系统集成

```cmake
# src/DeepLearningWidget/CMakeLists.txt
add_library(FuseVisionDeepLearning STATIC
    DeepLearningWidget.h/cpp      # ✅已有
    ProjectManagement.h/cpp       # ✅已实现
    ModelManagement.h/cpp         # 🆕待实现
    DatasetManagement.h/cpp       # 🆕待实现
    DataAnnotation.h/cpp          # 🆕待实现
    DataSplit.h/cpp               # 🆕待实现
    ModelTraining.h/cpp           # 🆕待实现
    ModelPredict.h/cpp            # 🆕待实现
    ModelExport.h/cpp             # 🆕待实现
    ProjectConfig.h/cpp           # ✅已实现
    ProjectCard.h/cpp             # ✅已实现
    StepGuideBar.h/cpp            # ✅已实现
    DatasetConfig.h/cpp           # 🆕待实现
)
target_link_libraries(FuseVisionDeepLearning PUBLIC Qt6::Widgets FuseVisionCore)
```

---

## 十六、分阶段开发路线图

### 阶段 0：基础设施验证 ✅ 已完成

- [x] `DeepLearningWidget` 8 标签页框架 + `QStackedWidget`
- [x] `LogMonitor` 底部可折叠日志面板
- [x] `PermissionGuard` 8 个 DL 权限点注册
- [x] `SettingsManager` 含 `dlDataPath/dlModelPath/dlDatasetPath`
- [x] `FuseVision` 主窗口集成 DL 模块
- [x] `ProjectManagement.h/cpp` 文件已创建（空壳）

### 阶段 1：项目与模型管理 ✅ 已完成

- [x] `ProjectConfig` 工具类：`.fvproj` JSON 解析与序列化
- [x] `ProjectCard` + `StepGuideBar`：卡片组件 + 步骤引导条
- [x] `ProjectManagement`：卡片式项目浏览 + 新建/打开项目对话框 + 项目描述
- [x] 单击预览 vs 双击打开：`projectPreviewed` / `projectOpened` 双信号
- [x] 状态栏项目名联动（`FuseVision::setProjectName`）
- [x] 打开项目后自动跳转模型管理标签页（Tab 1）
- [x] 明暗主题全面适配（`ThemePalette` 驱动 + 暗色图标自动切换）
- [x] 卡片网格自适应列数（`resizeEvent` + `relayoutCards()`）
- [ ] `ModelManagement`：模型列表 + 新建模型对话框（4 种任务类型）
- [ ] `QSettings` 持久化最近打开的项目列表

### 阶段 2：数据集与标注（第 3-4 周）

- [ ] `DatasetManagement`：图像画廊（网格/列表视图）+ 导入/筛选
- [ ] `DatasetConfig`：`dataset_config.json` 读写 + 类别管理
- [ ] `DataAnnotation`：`QProcess` 启动 labelme + `QFileSystemWatcher` 监听标注输出
- [ ] 画廊标注状态叠加标识（已标注 ✓ / 未标注 ○）
- [ ] 底部引导步骤栏（步骤 1→4 可视化）

### 阶段 3：数据拆分（第 5 周）

- [ ] `DataSplit`：滑动条联动比例（默认 70/15/15）+ 分层采样
- [ ] `splits/train.txt val.txt test.txt` 生成逻辑
- [ ] 拆分结果条形图 + 摘要表格
- [ ] 状态联动：`split_done=true` → 解锁训练

### 阶段 4：训练后端 + 前端（第 6-8 周）

- [ ] FastAPI 后端项目搭建（`sdk/yolo/backend/`）
- [ ] `ModelTraining`：参数配置面板 + `QProcess` 启动 FastAPI
- [ ] WebSocket 实时日志流 → `LogMonitor` 面板渲染
- [ ] `QtCharts` 实时 loss/mAP 曲线
- [ ] 多版本管理：时间戳版本 ID + `.fvdl` 追加记录
- [ ] 训练完成自动 ONNX / TensorRT 转换

### 阶段 5：预测与导出（第 9-10 周）

- [ ] `ModelPredict`：引擎选择（PT/ONNX/TensorRT）+ 可视化结果叠加
- [ ] `ModelExport`：多格式导出 + 转换调度
- [ ] 批量预测结果缩略图网格
- [ ] JSON/COCO 结果导出

### 阶段 6：打磨与集成（第 11-12 周）

- [ ] 全局状态机：`status` 字段驱动的全链路锁定/解锁
- [ ] 悬停阻断提示（`QToolTip`）
- [ ] 操作前校验 + 模态提示框
- [ ] 异常恢复：后端崩溃自动重启，进度恢复
- [ ] 性能优化：画廊缩略图异步加载，大项目延迟渲染

---