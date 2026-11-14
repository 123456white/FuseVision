#ifndef ANNOTATIONTOOL_H
#define ANNOTATIONTOOL_H

#include <QObject>
#include <QPainterPath>
#include <QColor>
#include <QPointF>
#include <QList>
#include <QKeySequence>
#include "../AnnotationModel.h"

// =============================================================================
// AnnotationTool — 标注工具插件基类 (v3.0)
// =============================================================================
// 职责:
//   - 处理鼠标/键盘事件
//   - 通过 model->Transaction 修改数据
//   - 提供预览路径给渲染器
//   - 不直接操作 UI 组件

class AnnotationRender;  // 前向声明, 仅用于 Context

class AnnotationTool : public QObject
{
    Q_OBJECT
public:
    enum Type { Brush, Eraser, Polygon, Rectangle, Fill, Select, Custom };

    // ── 工具上下文 (每次 openImage 更新) ───────────────────────
    struct Context {
        QImage sourceImage;                 // 原图
        AnnotationModel* model   = nullptr; // 数据模型
        AnnotationRender* render = nullptr; // 渲染器
        QStringList labelColors;            // 标签颜色
        int activeLabelId   = 0;            // 当前选中标签
        int brushSize       = 12;           // 画笔尺寸
    };

    explicit AnnotationTool(Type type, QObject* parent = nullptr);
    virtual ~AnnotationTool();

    Type type() const { return m_type; }
    virtual QString name() const = 0;
    virtual QString icon() const = 0;           // Unicode 图标
    virtual QKeySequence shortcut() const = 0;
    virtual Qt::CursorShape cursor() const { return Qt::CrossCursor; }
    virtual bool showBrushSize() const { return false; }

    // ── 生命周期 ──────────────────────────────────────────────
    virtual void activated(Context& ctx);
    virtual void deactivated(Context& ctx);

    // ── 上下文管理 ─────────────────────────────────────────────
    void updateContext(Context& ctx) { m_ctx = ctx; }
    Context& context() { return m_ctx; }
    const Context& context() const { return m_ctx; }

    // ── 事件处理 (纯虚或默认实现) ──────────────────────────────
    virtual void mousePress(QPointF imagePos, Qt::MouseButton button) = 0;
    virtual void mouseMove(QPointF imagePos);       // 默认更新坐标
    virtual void mouseRelease(QPointF imagePos, Qt::MouseButton button);
    virtual void mouseDoubleClick(QPointF imagePos);
    virtual void keyPress(int key, Qt::KeyboardModifiers mods);

    // ── 状态 ──────────────────────────────────────────────────
    bool isDrawing() const { return m_drawing; }
    bool isActive() const { return m_active; }

    // ── 预览 (渲染器使用) ──────────────────────────────────────
    virtual QPainterPath previewPath() const { return {}; }
    virtual QColor previewColor() const { return Qt::yellow; }
    virtual int previewPenWidth() const { return 2; }

    // ── 取消绘制 ──────────────────────────────────────────────
    virtual void cancelDrawing();

signals:
    void requestRender();                // 请求重渲染 overlay
    void requestPreviewUpdate();         // 请求预览更新
    void shapeCreated();                 // 标注创建完成 (用于 UI 更新)
    void toolStatusChanged(const QString& status);  // 状态栏文本

protected:
    Type m_type;
    Context m_ctx;
    bool m_active = false;
    bool m_drawing = false;

    // 便捷方法: 通过 Transaction 创建一个 shape 并触发渲染
    void createShape(const AnnotationShape& shape, const QString& undoDesc);
};

#endif // ANNOTATIONTOOL_H
