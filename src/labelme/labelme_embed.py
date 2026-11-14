#!/usr/bin/env python
"""labelme 嵌入模式：在 C++ 程序传入的容器中运行，输出窗口句柄供 SetParent 使用。
用法: python labelme_embed.py <image_dir> [current_file] [--labels label1,...] [--output output_dir]
"""
import argparse
import sys
import os.path as osp

_script_dir = osp.dirname(osp.abspath(__file__))
sys.path.insert(0, _script_dir)
sys.path.insert(0, osp.normpath(osp.join(_script_dir, '../../sdk/labelme')))

from PyQt5 import QtCore, QtWidgets
from labelme.app import MainWindow
from labelme import __appname__
from labelme.utils import newIcon


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image_dir", help="图像目录")
    parser.add_argument("current_file", nargs="?", help="初始定位图像")
    parser.add_argument("--labels", help="逗号分隔标签")
    parser.add_argument("--output", help="标注 JSON 输出目录")
    args = parser.parse_args()

    config_overrides = {}
    if args.labels:
        config_overrides["labels"] = [
            s.strip() for s in args.labels.split(",") if s.strip()
        ]

    app = QtWidgets.QApplication(sys.argv)
    app.setStyle("Fusion")
    app.setApplicationName(__appname__)
    app.setWindowIcon(newIcon("icon"))

    win = MainWindow(
        filename=args.image_dir,
        config_overrides=config_overrides,
        output_dir=args.output,
    )

    # 嵌入模式：移除所有窗口装饰，由 C++ 侧通过 SetParent 嵌入
    win.setWindowFlags(QtCore.Qt.Window | QtCore.Qt.FramelessWindowHint)
    # 去除所有内部最小尺寸限制，允许缩放以适应嵌入容器
    win.setMinimumSize(0, 0)
    win.centralWidget().setMinimumSize(0, 0)
    # 遍历所有子控件，移除最小尺寸约束
    for widget in win.findChildren(QtWidgets.QWidget):
        widget.setMinimumSize(0, 0)
    win.show()

    # 输出 HWND 供 C++ SetParent 使用（务必在 show() 之后）
    sys.stdout.write(f"HWND:{int(win.winId())}\n")
    sys.stdout.flush()

    if args.current_file:
        def select_current():
            items = win._docks.file_list.findItems(
                args.current_file, QtCore.Qt.MatchExactly
            )
            if items:
                item = items[0]
                win._docks.file_list.setCurrentItem(item)
                win._docks.file_list.scrollToItem(item)
                win.fileSelectionChanged()
        QtCore.QTimer.singleShot(400, select_current)

    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
