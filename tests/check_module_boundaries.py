"""Keep domain and storage code usable without business widgets or Qt Widgets."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
ALLOWED = {
    "core": {"core"},
    "ui": {"core", "ui"},
    "deeplearning/domain": {"deeplearning/domain"},
    "deeplearning/model": {"core", "deeplearning/domain", "deeplearning/model"},
    "deeplearning/render": {"deeplearning/domain", "deeplearning/render"},
    "deeplearning/storage": {"core", "deeplearning/domain", "deeplearning/render", "deeplearning/storage"},
    "deeplearning/tools": {"deeplearning/domain", "deeplearning/model", "deeplearning/render", "deeplearning/tools"},
    "deeplearning/integrations": {"deeplearning/integrations"},
    "deeplearning/widgets": {"core", "ui", "deeplearning/domain", "deeplearning/model", "deeplearning/render", "deeplearning/storage", "deeplearning/tools", "deeplearning/integrations", "deeplearning/widgets"},
    "traditional": {"core", "ui", "traditional"},
    "app": {"core", "ui", "app", "deeplearning/widgets", "traditional"},
}
NO_WIDGETS = {"core", "deeplearning/domain", "deeplearning/model", "deeplearning/render", "deeplearning/storage", "deeplearning/tools", "deeplearning/integrations"}
QT_WIDGET_HEADERS = re.compile(r"^(?:QtWidgets(?:/|$)|Q(?:Widget|Application|MessageBox|Graphics\w+|PushButton|Dialog\w*|FileDialog|Label|MainWindow|ListWidget\w*|TableWidget\w*|TabBar|Splitter|ScrollArea|ComboBox|LineEdit|CheckBox|ToolBar|ToolBox|Menu|ActionGroup)$)")


def module(path):
    rel = path.relative_to(SRC).as_posix()
    return next((key for key in sorted(ALLOWED, key=len, reverse=True) if rel.startswith(key + "/")), None)


def check():
    failures = []
    for name in ["ui", "app", "deeplearning/domain", "deeplearning/storage", "traditional"]:
        if not (SRC / name).is_dir():
            failures.append(f"missing module: src/{name}")
    for path in sorted(SRC.rglob("*")):
        if path.suffix not in {".h", ".cpp"}:
            continue
        owner = module(path)
        if path == SRC / "main.cpp":
            continue  # Startup is the composition root.
        if not owner:
            failures.append(f"unclassified source: {path.relative_to(ROOT)}")
            continue
        text = path.read_text(encoding="utf-8-sig")
        for delim, include in re.findall(r'^\s*#\s*include\s*([<"])([^>"\n]+)[>"]', text, re.M):
            if owner in NO_WIDGETS and QT_WIDGET_HEADERS.match(include):
                failures.append(f"Qt Widgets dependency: {path.relative_to(ROOT)} -> {include}")
            if delim != '"':
                continue
            target = (path.parent / include).resolve()
            if not target.is_file():
                target = (SRC / include).resolve()
            if not target.is_file():
                failures.append(f"unresolved include: {path.relative_to(ROOT)} -> {include}")
                continue
            try:
                dependency = module(target)
            except ValueError:
                dependency = None
            if dependency not in ALLOWED[owner]:
                failures.append(f"forbidden dependency: {owner} -> {dependency}: {include}")
    return failures


if __name__ == "__main__":
    errors = check()
    for error in errors:
        print("FAIL " + error)
    if not errors:
        print("PASS module include boundaries (core, UI, deep learning, traditional, app)")
    sys.exit(bool(errors))
