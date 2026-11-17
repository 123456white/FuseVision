#!/usr/bin/env python3
"""FuseVision project-only doc generation/checks and Codex lifecycle adapters."""
from pathlib import Path
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from docmodel import read, encode, write_changed, digest, markdown_files, maintenance_lock
from doccheck import check, expected, input_hashes, GENERATED, MANIFEST
from docsessions import Session

ROOT = Path(__file__).resolve().parents[2]


def run_node(root, action, input_path, output=None):
    command = ["node", str(root / "scripts/docs/assets.mjs"), str(root), action, str(input_path)]
    if output: command.append(str(output))
    result = subprocess.run(command, cwd=root, capture_output=True, text=True, encoding="utf-8", timeout=90)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() + "\n缺少依赖时先运行 scripts/docs/setup.ps1")
    return result.stdout.strip()


def sync(root, html=False):
    with maintenance_lock(root):
        return sync_locked(root, html)


def sync_locked(root, html=False):
    (root / "out").mkdir(exist_ok=True)
    values = expected(root)  # Validate all current source records before replacing any mirrors.
    changes = [name for name, text in values.items() if not (root / name).is_file() or read(root / name) != text]
    for name in changes:
        write_changed(root / name, values[name])
    before = json.loads(read(root / MANIFEST)) if (root / MANIFEST).is_file() else {}
    inputs = input_hashes(root)
    map_path = root / GENERATED[2]
    archive = root / GENERATED[3]
    if before.get("inputs") != inputs or not archive.is_file() or GENERATED[2] in changes or before.get("outputs", {}).get(GENERATED[3]) != digest(archive):
        fd, name = tempfile.mkstemp(suffix=".xmind", dir=root / "out")
        os.close(fd)
        temporary = Path(name)
        temporary.unlink()  # Official exporter uses exclusive creation.
        try:
            run_node(root, "xmind", map_path, temporary)
            temporary.replace(archive)
        finally:
            temporary.unlink(missing_ok=True)
        changes.append(GENERATED[3])
    manifest = dict(format=1, tool="scripts/docs/docflow.py", inputs=inputs,
                    outputs={name: digest(root / name) for name in GENERATED})
    write_changed(root / MANIFEST, encode(manifest))
    result = dict(updated=changes)
    if html:
        from docsite import build
        result["html"] = build(root)
    return result


def diagrams(root):
    blocks = [dict(file=p.relative_to(root).as_posix(), code=m[1]) for p in markdown_files(root)
              for m in re.finditer(r"(?ms)^```mermaid\s*\n(.*?)^```\s*$", read(p))]
    path = root / "out/docs/diagram-input.json"
    write_changed(path, encode(blocks))
    return run_node(root, "mermaid", path)


def finish(root, session, html=True):
    generation = sync(root, html=html)
    report = check(root)
    if report['passed']:
        report['diagrams'] = diagrams(root)
    if not report["passed"]:
        return dict(passed=False, check=report, generation=generation)
    status = session.status()
    if status["semantic_review"] == "pending":
        return dict(passed=False, status=status, reason="需要AI/开发者核对受影响文档，并记录review；构建成功不替代语义核对")
    previous = session.complete()
    return dict(passed=True, review=previous.get("receipt"), generation=generation, check=report)


def hook(root, payload):
    cwd = Path(payload.get("cwd", str(root))).resolve()
    if not cwd.is_relative_to(root):
        return {}
    # A nested/foreign Git project must not inherit FuseVision's automation.
    git = subprocess.run(["git", "-C", str(cwd), "rev-parse", "--show-toplevel"], capture_output=True, text=True)
    if git.returncode or Path(git.stdout.strip()).resolve() != root:
        return {}
    event = payload.get("hook_event_name", "")
    session = Session(root, payload.get("session_id", ""))
    context = ("FuseVision文档维护：读取AGENTS.md、docs/maintenance/STANDARD.md和README.md。"
               "改动后更新相关Markdown，工具负责派生JSON/XMind/HTML，不更新历史验收结论。"
               f"当前会话ID={session.identifier}。结束前运行docflow.py review记录具体核对理由。")
    if event == "SessionStart":
        session.begin()  # Resume/compact never discards pending work.
        return {"hookSpecificOutput": {"hookEventName": event, "additionalContext": context}}
    if event == "PostToolUse":
        if not session.path.exists():
            return {"hookSpecificOutput": {"hookEventName": event, "additionalContext":
                "文档会话基线缺失。先运行docflow.py begin；首次工具改动归属需人工核对，不能声称已自动追踪。"}}
        status = session.notice()
        if not status: return {}
        message = "本会话有文件内容变化；请核对文档：" + ", ".join(status["affected_docs"])
        message += "。无需更新时通过review --decision no-impact记录原因；不要只改日期。"
        return {"hookSpecificOutput": {"hookEventName": event, "additionalContext": message}}
    if event == "Stop":
        try:
            result = finish(root, session)
        except Exception as exc:
            result = dict(passed=False, reason=str(exc))
        write_changed(session.folder / "last-stop.json", encode(result))
        if result["passed"]:
            if (result.get("review") or {}).get("decision") == "deferred":
                return {"systemMessage": "文档仍有待办：" + result["review"]["reason"] + "；交付中必须说明，不能写全部同步完成。"}
            return {}
        reason = "文档收尾未完成：" + result.get("reason", "; ".join(result.get("check", {}).get("errors", [])))
        reason += f"。会话 {session.identifier}；检查status，更新相关MD或记录具体no-impact/deferred理由后运行sync --html、check与review。"
        if payload.get("stop_hook_active"):
            return {"systemMessage": reason + "。为避免循环不再自动续跑；交付须明确未完成项。"}
        return {"decision": "block", "reason": reason}
    return {}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("sync"); p.add_argument("--html", action="store_true")
    p = sub.add_parser("check"); p.add_argument("--diagrams", action="store_true")
    sub.add_parser("html")
    p = sub.add_parser("serve"); p.add_argument("--port", type=int, default=8765)
    for name in ("begin", "status", "finish", "review"):
        p = sub.add_parser(name)
        p.add_argument("--session", required=True)
        if name == "review":
            p.add_argument("--decision", choices=["updated", "no-impact", "deferred"], required=True)
            p.add_argument("--reason", required=True)
            p.add_argument("--docs", nargs="*", default=[])
    sub.add_parser("hook")
    args = parser.parse_args()
    root = args.root.resolve()
    if not (root / "docs/maintenance/docflow.json").is_file():
        raise ValueError("不是已配置的FuseVision文档根目录")
    if args.command == "hook":
        try:
            payload = json.loads(sys.stdin.read().lstrip("\ufeff"))
            output = hook(root, payload)
        except Exception as exc:
            # Hook failures must remain visible and must not claim documentation is current.
            output = {"systemMessage": "FuseVision文档钩子失败：" + str(exc) + "；请按AGENTS.md手工执行维护流程。"}
        print(encode(output), end="")
        return 0
    if args.command == "sync": result = sync(root, args.html)
    elif args.command == "check":
        result = check(root)
        if args.diagrams: result["diagrams"] = diagrams(root)
    elif args.command == "html":
        from docsite import build
        with maintenance_lock(root):
            result = build(root)
    elif args.command == "serve":
        from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
        from functools import partial
        from docsite import build
        with maintenance_lock(root):
            build(root)
        print(f"文档阅读入口：http://127.0.0.1:{args.port}/ （Ctrl+C停止，仅本机监听）", flush=True)
        ThreadingHTTPServer(("127.0.0.1", args.port), partial(SimpleHTTPRequestHandler,
            directory=str(root / "out/docs/site"))).serve_forever()
        return 0
    else:
        session = Session(root, args.session)
        if args.command == "begin": result = session.begin()
        elif args.command == "status": result = session.status()
        elif args.command == "review": result = session.review(args.decision, args.reason, args.docs)
        else: result = finish(root, session)
    print(encode(result), end="")
    return 0 if result.get("passed", True) else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, RuntimeError, subprocess.SubprocessError) as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
