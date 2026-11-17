#include "FillTool.h"
#include "deeplearning/render/AnnotationRender.h"
#include <QPainter>
#include <QVector>

FillTool::FillTool(QObject* parent)
    : AnnotationTool(Fill, parent)
{}

void FillTool::mousePress(QPointF p, Qt::MouseButton button)
{
    if (button != Qt::LeftButton) return;
    int sx = (int)p.x(), sy = (int)p.y();
    if (sx < 0 || sy < 0 ||
        sx >= m_ctx.sourceImage.width() ||
        sy >= m_ctx.sourceImage.height())
        return;

    floodFill(sx, sy, m_ctx.activeLabelId);
}

void FillTool::floodFill(int seedX, int seedY, int fillLabelId)
{
    if (!m_ctx.model) return;

    int w = m_ctx.sourceImage.width();
    int h = m_ctx.sourceImage.height();
    QColor seedColor = m_ctx.sourceImage.pixelColor(seedX, seedY);
    int sr = seedColor.red(), sg = seedColor.green(), sb = seedColor.blue();

    // 构建已标注区域掩码
    AnnotationModel::Transaction tx(m_ctx.model, QString::fromUtf8("填充区域"));

    QImage occupied = buildOccupiedMask(tx.shapes());

    if(occupied.constScanLine(seedY)[seedX]) return;

    // 扫描线 flood fill
    QImage fillMask(w, h, QImage::Format_Grayscale8);
    fillMask.fill(0);
    const int tolerance = 32;
    int tolSq = tolerance * tolerance * 3;
    QVector<bool> visited(w * h, false);
    QStack<QPair<int,int>> stack;
    stack.push({seedX, seedY});

    while (!stack.isEmpty()) {
        auto [x, y] = stack.pop();
        if(visited[x+y*w] || occupied.constScanLine(y)[x]) continue;

        // 左扩展
        int left = x;
        while (left > 0) {
            int idx = (left - 1) + y * w;
            if (visited[idx] || occupied.scanLine(y)[left - 1] != 0) break;
            QColor c = m_ctx.sourceImage.pixelColor(left - 1, y);
            int dr = c.red() - sr, dg = c.green() - sg, db = c.blue() - sb;
            if (dr * dr + dg * dg + db * db > tolSq) break;
            left--;
        }

        // 右扩展
        int right = x;
        while (right < w - 1) {
            int idx = (right + 1) + y * w;
            if (visited[idx] || occupied.scanLine(y)[right + 1] != 0) break;
            QColor c = m_ctx.sourceImage.pixelColor(right + 1, y);
            int dr = c.red() - sr, dg = c.green() - sg, db = c.blue() - sb;
            if (dr * dr + dg * dg + db * db > tolSq) break;
            right++;
        }

        uchar* fl = fillMask.scanLine(y);
        for (int fx = left; fx <= right; fx++) {
            fl[fx] = 255;
            visited[fx + y * w] = true;
        }

        // 上下扫描
        for (int ny = y - 1; ny <= y + 1; ny += 2) {
            if (ny < 0 || ny >= h) continue;
            for (int nx = left; nx <= right; nx++) {
                int nidx = nx + ny * w;
                if (visited[nidx] || occupied.scanLine(ny)[nx] != 0) continue;
                QColor c = m_ctx.sourceImage.pixelColor(nx, ny);
                int dr = c.red() - sr, dg = c.green() - sg, db = c.blue() - sb;
                if (dr * dr + dg * dg + db * db <= tolSq) {
                    stack.push({nx, ny});
                }
            }
        }
    }

    // 合并到已有同标签图层
    bool merged = false;
    for (auto& s : tx.shapes()) {
        if (s.visible && s.type == AnnotationShape::BrushMask &&
            s.labelId == fillLabelId) {
            for(int y=0;y<h;++y) for(int x=0;x<w;++x)
                if(fillMask.constScanLine(y)[x]) s.maskLayer.scanLine(y)[x]=255;
            merged = true;
            break;
        }
    }
    if (!merged) {
        AnnotationShape shape;
        shape.type = AnnotationShape::BrushMask;
        shape.labelId = fillLabelId;
        shape.maskLayer = fillMask;
        tx.shapes().append(shape);
    }

    emit shapeCreated();
    emit requestRender();
}

QImage FillTool::buildOccupiedMask(const QList<AnnotationShape>& shapes) const
{
    AnnotationRender render;render.setSourceImage(m_ctx.sourceImage);
    return render.compileMask(shapes,1);
}
