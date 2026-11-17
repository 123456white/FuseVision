#include "AnnotationTool.h"

AnnotationTool::AnnotationTool(Type type, QObject* parent)
    : QObject(parent), m_type(type)
{}

AnnotationTool::~AnnotationTool() {}

void AnnotationTool::activated(Context& ctx)
{
    m_active = true;
    m_ctx = ctx;
}

void AnnotationTool::deactivated(Context& ctx)
{
    Q_UNUSED(ctx);
    cancelDrawing();
    m_active = false;
}

void AnnotationTool::mouseMove(QPointF imagePos)
{
    Q_UNUSED(imagePos);
}

void AnnotationTool::mouseRelease(QPointF imagePos, Qt::MouseButton button)
{
    Q_UNUSED(imagePos);
    Q_UNUSED(button);
}

void AnnotationTool::mouseDoubleClick(QPointF imagePos)
{
    Q_UNUSED(imagePos);
}

void AnnotationTool::keyPress(int key, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(key);
    Q_UNUSED(mods);
}

void AnnotationTool::createShape(const AnnotationShape& shape, const QString& undoDesc)
{
    if (!m_ctx.model) return;

    AnnotationModel::Transaction tx(m_ctx.model, undoDesc);
    tx.shapes().append(shape);
    // 析构自动 commit, 触发 dataChanged → 渲染
    emit shapeCreated();
    emit requestRender();
}

void AnnotationTool::cancelDrawing()
{
    m_drawing = false;
}
