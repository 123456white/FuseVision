"""Read-only documentation integrity checks, independent of historical business reports."""
from pathlib import Path
from urllib.parse import unquote
import json
import re
import zipfile

from docmodel import read, digest, encode, markdown_files, feature_inventory, issue_inventory, mindmap, catalog, walk


GENERATED = ["docs/logic/features.json", "docs/refactor/issues.json",
             "docs/logic/FuseVision-功能与代码.json", "docs/logic/FuseVision-功能与代码.xmind",
             "docs/maintenance/CATALOG.md"]
MANIFEST = "docs/maintenance/generated-manifest.json"


def slug(text):
    text = re.sub(r"<[^>]*>", "", text).replace("`", "")
    text = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)
    return re.sub(r"[^\w\-\s]", "", text.lower()).replace(" ", "-")


def without_code(text):
    return re.sub(r"(?ms)^(`{3,}|~{3,})[^\n]*\n.*?^\1[^\n]*$", "", text)


def anchors(path):
    text = without_code(read(path))
    found = set(re.findall(r'\bid=["\']([^"\']+)["\']', text))
    counts = {}
    for title in re.findall(r"^#{1,6}\s+(.+?)\s*#*\s*$", text, re.M):
        base = slug(title)
        count = counts.get(base, 0)
        counts[base] = count + 1
        found.add(base + (f"-{count}" if count else ""))
    return found


def local_links(root):
    errors, count = [], 0
    for path in [root / "AGENTS.md", *markdown_files(root)]:
        text = without_code(read(path))
        for label, target in re.findall(r"!?\[([^\]]*)\]\((<[^>]+>|[^\n)]+)\)", text):
            target = target.strip().strip("<>")
            if re.match(r"^(?:https?|mailto|app|codex):", target):
                continue
            raw, _, fragment = unquote(target).partition("#")
            line = None
            m = re.search(r":(\d+)$", raw)
            if m:
                line, raw = int(m[1]), raw[:m.start()]
            dest = path if not raw else (Path(raw) if re.match(r"^[A-Za-z]:[/\\]", raw) else (path.parent / raw).resolve())
            count += 1
            relative = path.relative_to(root).as_posix()
            if not dest.is_relative_to(root):
                errors.append(f"{relative}: 本机专属/越界链接 {target}")
            elif not dest.is_file():
                errors.append(f"{relative}: 缺少链接目标 {target}")
            elif fragment and dest.suffix == ".md" and fragment not in anchors(dest):
                errors.append(f"{relative}: 不存在标题 #{fragment}: {raw}")
            elif line is not None and not 1 <= line <= len(read(dest).splitlines()):
                errors.append(f"{relative}: 行号越界 {target}")
    return errors, count


def input_hashes(root):
    candidates = [*markdown_files(root), root / "AGENTS.md", root / "docs/maintenance/docflow.json"]
    candidates += [p for p in (root / "scripts/docs").glob("*") if p.is_file()]
    cfg = json.loads(read(root / "docs/maintenance/docflow.json"))
    candidates += [root / cfg["history"] / "features.json", root / cfg["history"] / "issues.json"]
    candidates += [root / "docs/logic" / i["image"] for i in cfg["images"]]
    import hashlib
    def source_digest(path):
        if path.suffix in {".md", ".py", ".json", ".mjs", ".ps1", ".txt"}:
            return hashlib.sha256(read(path).encode("utf-8")).hexdigest()
        return digest(path)
    return {p.relative_to(root).as_posix(): source_digest(p) for p in sorted(set(candidates), key=lambda p: p.as_posix())
            if p.relative_to(root).as_posix() not in GENERATED}


def expected(root):
    features = feature_inventory(root)
    issues = issue_inventory(root)
    return {GENERATED[0]: encode(features), GENERATED[1]: encode(issues),
            GENERATED[2]: encode(mindmap(root, features, issues)), GENERATED[4]: catalog(root)}


def validate_map(root, spec):
    errors = []
    features = {f["id"] for f in feature_inventory(root)["features"]}
    seen = set()
    for sheet in spec["sheets"]:
        nodes = list(walk(sheet["root"]))
        ids = [n["id"] for n in nodes]
        if len(set(ids)) != len(ids):
            errors.append("导图节点ID重复：" + sheet["title"])
        for node in nodes:
            seen.update(re.findall(r"\bF\d+\b", node["title"]))
            if node.get("image"):
                p = (root / "docs/logic" / node["image"]).resolve()
                if not p.is_relative_to(root / "docs/logic/screenshots") or not p.is_file():
                    errors.append("导图缺少安全的本地图像：" + node["image"])
        for relation in sheet.get("relationships", []):
            if relation["from"] not in ids or relation["to"] not in ids:
                errors.append("导图关联线端点不存在：" + sheet["title"])
    if features - seen:
        errors.append("导图漏功能：" + str(sorted(features - seen)))
    return errors


def check(root, generated=True):
    errors, links = local_links(root)
    try:
        protection = root / "docs/maintenance/archive-manifest.json"
        if protection.is_file():
            for name, sha in json.loads(read(protection))["protected_files"].items():
                if not (root / name).is_file() or digest(root / name) != sha:
                    errors.append("历史原始资料发生变化：" + name)
        values = expected(root)
        features = json.loads(values[GENERATED[0]])["features"]
        issues = json.loads(values[GENERATED[1]])
        spec = json.loads(values[GENERATED[2]])
        errors += validate_map(root, spec)
        # Check qualified symbols, not just the presence of a file.
        for item in [*features, *issues]:
            for source in item["sources"]:
                path = root / source["path"]
                if not path.is_file() or source["symbol"] not in read(path):
                    errors.append(f'{item["id"]}: 当前代码符号不存在 {source["path"]} {source["symbol"]}')
                elif not 1 <= source['line'] <= len(read(path).splitlines()) or source['symbol'] not in read(path).splitlines()[source['line'] - 1]:
                    errors.append(f'{item["id"]}: 符号与引用行不匹配 {source["path"]}:{source["line"]} {source["symbol"]}')
        if generated:
            for name, text in values.items():
                if not (root / name).is_file() or read(root / name) != text:
                    errors.append("生成物过期，请运行 sync：" + name)
            stamp = root / MANIFEST
            if not stamp.is_file():
                errors.append("缺少生成清单，请运行 sync")
            else:
                manifest = json.loads(read(stamp))
                if manifest.get("inputs") != input_hashes(root):
                    errors.append("生成输入已变化，请运行 sync")
                for name in GENERATED:
                    if not (root / name).is_file() or manifest.get("outputs", {}).get(name) != digest(root / name):
                        errors.append("生成物摘要不匹配：" + name)
            archive = root / GENERATED[3]
            if archive.is_file():
                with zipfile.ZipFile(archive) as z:
                    content = json.loads(z.read("content.json"))
                    ids = set()
                    pictures = []
                    for sheet in content:
                        def descendants(n):
                            yield n
                            for children in n.get("children", {}).values():
                                for child in children:
                                    yield from descendants(child)
                        for node in descendants(sheet["rootTopic"]):
                            ids.add(node["id"])
                            if "image" in node:
                                pictures.append(node["image"]["src"])
                    for image in pictures:
                        if image.removeprefix("xap:") not in z.namelist():
                            errors.append("XMind嵌图不存在：" + image)
                    for sheet in content:
                        for relation in sheet.get("relationships", []):
                            if relation["end1Id"] not in ids or relation["end2Id"] not in ids:
                                errors.append("XMind归档关联线端点不存在")
                    intended = sum(bool(n.get("image")) for s in spec["sheets"] for n in walk(s["root"]))
                    if len(pictures) != intended or len(content) != len(spec["sheets"]):
                        errors.append("XMind分图/图像数量与生成输入不符")
    except (ValueError, KeyError, OSError, zipfile.BadZipFile) as exc:
        errors.append(str(exc))
    return dict(passed=not errors, local_links=links, errors=errors,
                scope="文档结构/引用/生成物；不代表重新执行业务验收")
