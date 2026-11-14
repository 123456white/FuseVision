#!/usr/bin/env python
"""labelme 启动器：加载整个图像目录、标签列表、已有标注，并定位到当前图像。
用法: python run_labelme.py <image_dir> [current_file] [--labels label1,...] [--output output_dir]
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
    parser.add_argument("image_dir", help="图像目录（labelme 将导入此目录下所有图像）")
    parser.add_argument("current_file", nargs="?", help="初始定位的图像路径")
    parser.add_argument("--labels", help="逗号分隔的标签列表，如 cat,dog,bird")
    parser.add_argument("--output", help="标注 JSON 输出目录（也用于读取已有标注）")
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
    win.show()
    win.raise_()

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
