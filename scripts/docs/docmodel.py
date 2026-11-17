"""Read current Markdown and build deterministic mirrors; never infer business facts."""
from pathlib import Path
from contextlib import contextmanager
import hashlib
import json
import os
import re
import tempfile
import time


def read(path):
    return Path(path).read_text(encoding="utf-8-sig")


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_changed(path, value):
    path = Path(path)
    data = value.encode("utf-8") if isinstance(value, str) else value
    if path.is_file() and path.read_bytes() == data:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix='.' + path.name + '.', suffix='.docflow-tmp', dir=path.parent)
    temporary = Path(name)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)
    return True


@contextmanager
def maintenance_lock(root):
    folder = root / 'out/docs'
    folder.mkdir(parents=True, exist_ok=True)
    lock = folder / 'generation.lock'
    deadline = time.monotonic() + 60
    while True:
        try:
            lock.mkdir()
            break
        except FileExistsError:
            if time.monotonic() >= deadline:
                raise RuntimeError('另一文档生成任务仍持有out/docs/generation.lock；不会覆盖并发写入，请确认进程后重试')
            time.sleep(.1)
    try:
        yield
    finally:
        lock.rmdir()


def encode(value):
    return json.dumps(value, ensure_ascii=False, indent=2) + "\n"


def config(root):
    return json.loads(read(root / "docs/maintenance/docflow.json"))


def field(block, label):
    found = re.search(r"^" + re.escape(label) + r"：(.*)$", block, re.M)
    if not found or not found[1].strip():
        raise ValueError(f"缺少非空字段 {label}")
    return found[1].strip()


def sources(block):
    return [dict(path=path, symbol=symbol, line=int(line)) for symbol, path, line in
            re.findall(r"\[([^\]]+)\]\((?:\.\./)+([^):]+):(\d+)\)", block)]


def feature_inventory(root):
    cfg = config(root)
    text = read(root / cfg["features"])
    old = json.loads(read(root / cfg["history"] / "features.json"))
    histories = {f["id"]: {k: v for k, v in f.items() if k.startswith("history_")}
                 for f in old["features"]}
    features = []
    for match in re.finditer(r"(?ms)^## (F\d+) ([^\n]+)\n(.*?)(?=^## F\d+ |\Z)", text):
        identifier, title, block = match.groups()
        location = re.fullmatch(r"(.+?) / (P\d+)", field(block, "位置"))
        if not location:
            raise ValueError(f"{identifier}: 位置须为 模块 / P编号")
        entry = field(block, "入口")
        f = dict(id=identifier, module=location[1], page=location[2], title=title.strip(),
                 buttons=[] if entry.startswith("无独立按钮入口") else entry.split(" / "), scene=field(block, "场景"),
                 preconditions=field(block, "前提"), inputs=field(block, "输入与校验"),
                 steps=re.findall(r"^\d+\. (.+)$", block, re.M), success=field(block, "结果"),
                 boundaries=field(block, "失败/取消/边界"), data=field(block, "数据/接口/算法"),
                 sources=sources(block), runtime=field(block, "验证范围"),
                 status=field(block, "证据状态"),
                 buttonIds=re.findall(r"\bB\d+[A-Z]?-\d+\b", field(block, "按钮编号")))
        if not f["steps"] or not f["sources"]:
            raise ValueError(f"{identifier}: 缺少步骤或代码证据")
        f.update(histories.get(identifier, {}))
        features.append(f)
    if not features or len({f["id"] for f in features}) != len(features):
        raise ValueError("功能编号为空或重复")
    return dict(project=field(text, "项目"), date=field(text, "记录日期"),
                scope=field(text, "范围"), features=features)


def issue_inventory(root):
    cfg = config(root)
    text = read(root / cfg["issues"])
    old = {i["id"]: i for i in json.loads(read(root / cfg["history"] / "issues.json"))}
    priority = dict(re.findall(r"\| \[(ISS-\d+)\].*?\| (P\d) \|", text))
    result = []
    for m in re.finditer(r"(?ms)^## (ISS-\d+) ([^\n]+)\n(.*?)(?=^## |\Z)", text):
        identifier, title, block = m.groups()
        def bold(label):
            return field(block.replace("**" + label + "**", label), label).rstrip("。")
        issue = dict(old.get(identifier, {}))
        issue.update(id=identifier, title=title, priority=priority[identifier],
                     status=bold("状态"), fact=bold("当前实现/限制"), verify=bold("验证"),
                     limitations=bold("未完成或边界"), sources=sources(block))
        if not issue["sources"]:
            raise ValueError(f"{identifier}: 缺少当前代码引用")
        # Historical attributes are retained for compatibility, never refreshed from code.
        result.append(issue)
    if not result or len({i["id"] for i in result}) != len(result):
        raise ValueError("问题编号为空或重复")
    return result


def walk(node):
    yield node
    for child in node.get("children", []):
        yield from walk(child)


def feature_node(f, prefix=""):
    identifier = prefix + f["id"]
    note = "\n".join(["场景：" + f["scene"], "前提：" + f["preconditions"], "输入：" + f["inputs"],
                       "结果：" + f["success"], "失败/取消：" + f["boundaries"],
                       "数据：" + f["data"], "原记录验证范围：" + f["runtime"]])
    return dict(id=identifier, title=f["id"] + " " + f["title"], note=note,
                status=f["status"], labels=[f["page"]] + f["buttonIds"],
                sources=[f'{s["path"]}:{s["line"]} {s["symbol"]}' for s in f["sources"]],
                children=[dict(id=identifier + "-step" + str(n), title=f"{n}. {step[:30]}", note=step)
                          for n, step in enumerate(f["steps"], 1)])


def mindmap(root, inventory, issues):
    cfg = config(root)
    features = inventory["features"]
    sheets = []
    def add(title, identifier, children, note="", relations=None):
        sheets.append(dict(title=title, root=dict(id=identifier, title=title, note=note,
                                                  children=children), relationships=relations or []))
    modules = []
    for n, module in enumerate(dict.fromkeys(f["module"] for f in features)):
        fs = [f for f in features if f["module"] == module]
        pages = [dict(id="overview-" + p, title=p, children=[feature_node(f, "ov-") for f in fs if f["page"] == p])
                 for p in dict.fromkeys(f["page"] for f in fs)]
        modules.append(dict(id=f"module-{n}", title=module, children=pages))
    pairs = re.findall(r'^\| (F\d+) \| (F\d+) \| ([^|\n]+?) \|\s*$',
                       read(root / 'docs/logic/FEATURE-RELATIONS.md'), re.M)
    ids = {f["id"] for f in features}
    if any(a not in ids or b not in ids for a, b, _ in pairs):
        raise ValueError('导图主流程关联引用了不存在的功能编号')
    add("01 软件总览与核心流程", "overview", modules, read(root / "docs/logic/CORE-FLOW.md"),
        [dict(**{"from": "ov-" + a, "to": "ov-" + b}, title=t) for a, b, t in pairs if a in ids and b in ids])
    groups = [("02 项目与模型", ["P02", "P03"]), ("03 数据集管理", ["P04"]),
              ("04 Qt原生标注与保存", ["P05"]), ("05 单账户与共享能力", ["P00", "P01", "P12"]),
              ("06 系统设置与生效范围", ["P13"]), ("07 占位功能与传统视觉", ["P06", "P07", "P08", "P09", "P10", "P11"])]
    for n, (title, pages) in enumerate(groups, 1):
        fs = [f for f in features if f["page"] in pages]
        add(title, f"sheet-{n}", [feature_node(f) for f in fs],
            "内容从当前Markdown生成。验证状态沿用原记录，不表示生成时重新操作软件。")
    topics = [("架构", "codebase/ARCHITECTURE.md"), ("数据契约", "logic/DATA-CONTRACTS.md"),
              ("功能关系", "logic/FEATURE-RELATIONS.md"), ("状态迁移", "logic/STATE-MACHINES.md"),
              ("算法", "logic/ALGORITHMS.md")]
    add("08 架构、关系与状态", "architecture", [dict(id=f"architecture-{n}", title=t,
        note=read(root / "docs" / p)) for n, (t, p) in enumerate(topics)])
    add("09 问题证据与改进状态", "issues", [dict(id=i["id"], title=i["id"] + " " + i["title"],
        status=i["status"], note="\n".join([i["fact"], "原记录验证：" + i["verify"], "边界：" + i["limitations"]]),
        sources=[f'{s["path"]}:{s["line"]} {s["symbol"]}' for s in i["sources"]]) for i in issues])
    for image in cfg["images"]:
        node = {k: v for k, v in image.items() if k != "sheet"}
        node["id"] = "image-" + node["id"]
        node["note"] = "原始截图与采集范围保留；不是本次生成时重新截图。\n" + node.get("note", "")
        sheets[image["sheet"]]["root"]["children"].append(node)
    return dict(project=inventory["project"], date=inventory["date"],
                source="Markdown → docflow；生成不改变历史验证结论", sheets=sheets)


def markdown_files(root):
    return sorted((p for p in (root / "docs").rglob("*.md")
                   if "archive" not in p.relative_to(root / "docs").parts), key=lambda p: p.as_posix())


def catalog(root):
    lines = ["# 当前文档目录", "", "<!-- generated: scripts/docs/docflow.py sync；勿手工编辑 -->", "",
             "只列当前维护文档；原始证据与归档不计入当前说明。", ""]
    for p in markdown_files(root):
        if p.name == "CATALOG.md":
            continue
        title = re.search(r"^# (.+)$", read(p), re.M)
        import os
        relative = os.path.relpath(p, root / "docs/maintenance").replace("\\", "/")
        lines.append(f'- [{title[1] if title else p.stem}]({relative})')
    return "\n".join(lines) + "\n"
