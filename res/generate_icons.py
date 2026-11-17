#!/usr/bin/env python
"""生成标注工具图标 PNG (32x32)，含 Light/Dark 两套主题。"""

from PIL import Image, ImageDraw
import math, os

SZ = 32
OUT = os.path.dirname(os.path.abspath(__file__))

# ── 主题色板 ──
# Light: 适合浅色背景
# Dark:  适合深色背景（颜色更亮/白色为主）
def pal(light=True):
    return {
        "brush":   ("#3a6fc5", "#5b8def") if light else ("#6ba0ff", "#8bc0ff"),
        "eraser":  ("#c55a3a", "#e07050") if light else ("#e06040", "#f08060"),
        "polygon": ("#2d9e5a", "#40c070") if light else ("#40c070", "#60e090"),
        "rect":    ("#c59a2d", "#e0b040") if light else ("#e0b040", "#f0c060"),
        "fill":    ("#8c45b0", "#a460d0") if light else ("#a460d0", "#c080e0"),
        "select":  ("#3a8fc5", "#50a0e0") if light else ("#50a0e0", "#70c0f0"),
        "save":    ("#3a6fc5", "#5b8def") if light else ("#6ba0ff", "#8bc0ff"),
        "undo":    ("#6c757d", "#8c959d") if light else ("#a0a0b0", "#c0c0d0"),
        "redo":    ("#6c757d", "#8c959d") if light else ("#a0a0b0", "#c0c0d0"),
        "fit":     ("#6c757d", "#8c959d") if light else ("#a0a0b0", "#c0c0d0"),
        "zoom1":   ("#6c757d", "#8c959d") if light else ("#a0a0b0", "#c0c0d0"),
        "back":    ("#c55555", "#e07070") if light else ("#e07070", "#f09090"),
        "navfirst":("#6c757d", "#8c959d") if light else ("#a0a0b0", "#c0c0d0"),
        "navprev": ("#6c757d", "#8c959d") if light else ("#a0a0b0", "#c0c0d0"),
        "navnext": ("#6c757d", "#8c959d") if light else ("#a0a0b0", "#c0c0d0"),
        "navlast": ("#6c757d", "#8c959d") if light else ("#a0a0b0", "#c0c0d0"),
        "labelme": ("#6c757d", "#8c959d") if light else ("#a0a0b0", "#c0c0d0"),
    }


def mk_rounded_rect(draw, xy, r, fill, **kw):
    """PIL 兼容的圆角矩形 (Pillow >= 9 可直接用 rounded_rectangle)。"""
    try:
        draw.rounded_rectangle(xy, r, fill=fill, **kw)
    except AttributeError:
        x1, y1, x2, y2 = xy
        draw.rectangle([x1+r, y1, x2-r, y2], fill=fill, **kw)
        draw.rectangle([x1, y1+r, x2, y2-r], fill=fill, **kw)
        draw.pieslice([x1, y1, x1+2*r, y1+2*r], 180, 270, fill=fill, **kw)
        draw.pieslice([x2-2*r, y1, x2, y1+2*r], 270, 360, fill=fill, **kw)
        draw.pieslice([x1, y2-2*r, x1+2*r, y2], 90, 180, fill=fill, **kw)
        draw.pieslice([x2-2*r, y2-2*r, x2, y2], 0, 90, fill=fill, **kw)


def make_icon(name, light=True):
    img = Image.new("RGBA", (SZ, SZ), (0,0,0,0))
    d = ImageDraw.Draw(img)
    p = pal(light)
    fg, fg2 = p[name]
    w = 2.5

    # ── 画笔 🖌 ──
    if name == "brush":
        # 笔杆 (斜线)
        pts = [(8,26),(7,7),(16,5),(17,24)]
        d.polygon(pts, fill=fg)
        d.line([(7,7),(16,5)], fill=fg2, width=3)
        # 笔尖
        d.polygon([(16,24),(18,27),(14,28),(12,23)], fill=fg2)
        # 笔刷鬃毛
        for i in range(3):
            y = 27 + i*1.5
            d.line([(14, int(y)), (17, int(y+1))], fill=fg2, width=2)

    # ── 橡皮 🧹 ──
    elif name == "eraser":
        # 橡皮主体
        pts = [(10,8),(22,8),(26,18),(14,18)]
        d.polygon([(10,8),(22,8),(26,18)], fill=fg)
        d.polygon([(10,8),(26,18),(14,18)], fill=fg2)
        d.polygon(pts, outline=fg, width=2)
        # 底部
        d.rectangle([12,16,24,22], fill=fg2, outline=fg, width=1)

    # ── 多边形 ⬠ ──
    elif name == "polygon":
        cx, cy, r = 16, 15, 10
        pts = []
        for i in range(6):
            a = math.pi/2 + i * math.pi/3
            pts.append((cx + r*math.cos(a), cy - r*math.sin(a)))
        d.polygon(pts, fill=fg, outline=fg2, width=2)
        # 顶点圆点
        for px, py in pts:
            d.ellipse([px-2, py-2, px+2, py+2], fill=fg2)

    # ── 矩形 ▭ ──
    elif name == "rect":
        d.rectangle([7,6,26,23], fill=fg, outline=fg2, width=2)
        # 虚线效果（四角加粗）
        d.line([(7,8),(7,13)], fill=fg2, width=3)
        d.line([(7,18),(7,23)], fill=fg2, width=3)
        d.line([(24,6),(24,11)], fill=fg2, width=3)
        d.line([(24,16),(24,23)], fill=fg2, width=3)

    # ── 填充 🪣 ──
    elif name == "fill":
        # 桶身 (梯形)
        d.polygon([(8,14),(24,14),(22,26),(10,26)], fill=fg, outline=fg2, width=2)
        # 桶把 (弧线)
        d.arc([13,5,19,15], 180, 0, fill=fg2, width=2)
        # 溢出
        d.ellipse([6,10,12,16], fill=fg2)
        d.line([(9,13),(14,14)], fill=fg, width=2)

    # ── 选择 🖱 ──
    elif name == "select":
        # 鼠标指针
        pts = [(6,4),(12,14),(10,16),(15,20),(18,17),(13,13),(22,9)]
        d.polygon(pts, fill=fg)
        d.polygon(pts, outline=fg2, width=1)

    # ── 保存 💾 ──
    elif name == "save":
        # 向下箭头 + 托盘
        d.rectangle([6,20,26,27], fill=fg, outline=fg2, width=1)
        d.polygon([(10,12),(22,12),(16,20)], fill=fg, outline=fg2, width=1)
        d.rectangle([13,7,19,15], fill=fg2)

    # ── 撤销 ↩ ──
    elif name == "undo":
        x, y = 16, 15
        d.arc([x-9,y-10,x+9,y+8], 60, 290, fill=fg2, width=3)
        # 箭头
        pts = [(x-5,y-7),(x-1,y-10),(x-7,y-11)]
        d.polygon(pts, fill=fg2)

    # ── 重做 ↪ ──
    elif name == "redo":
        x, y = 16, 15
        d.arc([x-9,y-10,x+9,y+8], 190, 420, fill=fg2, width=3)
        pts = [(x+5,y-7),(x+1,y-10),(x+7,y-11)]
        d.polygon(pts, fill=fg2)

    # ── 适应窗口 ──
    elif name == "fit":
        # 外框
        d.rectangle([3,3,29,29], outline=fg2, width=2)
        # 四角箭头
        for dx, dy in [(6,0),(26,0),(6,28),(26,28)]:
            d.line([(dx,dy),(dx,dy-5),(dx+4,dy-5),(dx,dy)], fill=fg2, width=2)
            d.line([(dx,dy),(dx,dy+5),(dx+4,dy+5),(dx,dy)], fill=fg2, width=2)
        # 内部图像示意
        d.rectangle([9,8,23,22], fill=fg)

    # ── 1:1 缩放 ──
    elif name == "zoom1":
        # 放大镜
        d.ellipse([5,4,22,21], outline=fg2, width=2)
        d.line([(19,18),(28,27)], fill=fg2, width=3)
        # "1:1" 文字用线条表示
        d.line([(9,12),(11,12)], fill=fg, width=1)
        d.line([(10,8),(10,17)], fill=fg, width=1)
        d.line([(14,8),(16,8)], fill=fg, width=1)
        d.line([(15,8),(15,17)], fill=fg, width=1)

    # ── 返回 ◀ ──
    elif name == "back":
        d.polygon([(22,6),(10,16),(22,26)], fill=fg, outline=fg2, width=1)
        d.rectangle([5,10,11,22], fill=fg2)

    # ── 导航: 第一张 |◀ ──
    elif name == "navfirst":
        d.polygon([(20,6),(8,16),(20,26)], fill=fg, outline=fg2, width=1)
        d.line([(5,8),(5,24)], fill=fg2, width=3)
        d.polygon([(28,6),(20,16),(28,26)], fill=fg2, outline=fg2, width=1)

    # ── 导航: 上一张 ◀ ──
    elif name == "navprev":
        d.polygon([(22,6),(10,16),(22,26)], fill=fg, outline=fg2, width=1)

    # ── 导航: 下一张 ▶ ──
    elif name == "navnext":
        d.polygon([(10,6),(22,16),(10,26)], fill=fg, outline=fg2, width=1)

    # ── 导航: 最后一张 ▶| ──
    elif name == "navlast":
        d.polygon([(12,6),(20,16),(12,26)], fill=fg2, outline=fg2, width=1)
        d.line([(27,8),(27,24)], fill=fg2, width=3)
        d.polygon([(8,6),(16,16),(8,26)], fill=fg, outline=fg2, width=1)

    # ── LabelMe 外部工具 ──
    elif name == "labelme":
        d.polygon([(16,4),(28,10),(28,24),(16,30),(4,24),(4,10)], fill=fg, outline=fg2, width=1)
        d.rectangle([10,13,22,19], fill=fg2)

    return img


def main():
    names = ["brush","eraser","polygon","rect","fill","select",
             "save","undo","redo","fit","zoom1","back",
             "navfirst","navprev","navnext","navlast","labelme"]
    name_map = {
        "brush": "AnnoBrush", "eraser": "AnnoEraser",
        "polygon": "AnnoPolygon", "rect": "AnnoRectangle",
        "fill": "AnnoFill", "select": "AnnoSelect",
        "save": "AnnoSave", "undo": "AnnoUndo", "redo": "AnnoRedo",
        "fit": "AnnoFit", "zoom1": "AnnoZoom1", "back": "AnnoBack",
        "navfirst": "AnnoNavFirst", "navprev": "AnnoNavPrev",
        "navnext": "AnnoNavNext", "navlast": "AnnoNavLast",
        "labelme": "AnnoLabelMe",
    }

    for n in names:
        prefix = name_map[n]
        # Light
        img = make_icon(n, light=True)
        img.save(os.path.join(OUT, f"{prefix}.png"))
        # Dark
        img_d = make_icon(n, light=False)
        img_d.save(os.path.join(OUT, f"{prefix}_D.png"))

    print(f"Generated {len(names)*2} icons in {OUT}")


if __name__ == "__main__":
    main()
