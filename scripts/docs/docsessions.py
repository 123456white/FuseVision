"""Per-session content baselines and explicit semantic review receipts, stored only in out/."""
from contextlib import contextmanager
from pathlib import Path
import fnmatch
import hashlib
import json
import os
import time

from docmodel import read, encode, digest, write_changed
from doccheck import GENERATED, MANIFEST


ROOTS = ("src", "tests", "scripts", "cmake", ".github", ".codex", "docs")


def snapshot(root):
    files = [p for p in root.iterdir() if p.is_file()]
    for name in ROOTS:
        folder = root / name
        if folder.is_dir():
            files.extend(p for p in folder.rglob("*") if p.is_file())
    values = {}
    for p in files:
        relative = p.relative_to(root).as_posix()
        if relative in GENERATED or relative == MANIFEST or any(part in p.parts for part in ("__pycache__", "node_modules")):
            continue
        if relative.startswith(("docs/archive/", "docs/logic/evidence/", "docs/logic/screenshots/")):
            continue
        values[relative] = digest(p)
    return values


def changed(before, after):
    return sorted(p for p in before.keys() | after.keys() if before.get(p) != after.get(p))


def signature(values):
    return hashlib.sha256(json.dumps(values, sort_keys=True).encode()).hexdigest()


class Session:
    def __init__(self, root, identifier):
        if not identifier or len(identifier) > 200:
            raise ValueError("缺少有效 session ID")
        self.root = root
        self.identifier = identifier
        key = hashlib.sha256(identifier.encode()).hexdigest()[:32]
        self.folder = root / "out/docs/sessions" / key
        self.path = self.folder / "state.json"

    @contextmanager
    def locked(self):
        self.folder.mkdir(parents=True, exist_ok=True)
        lock = self.folder / "lock"
        deadline = time.monotonic() + 5
        while True:
            try:
                lock.mkdir()
                break
            except FileExistsError:
                if time.monotonic() >= deadline:
                    raise RuntimeError("本会话文档状态正被其他钩子更新，请重试；不会覆盖并发写入")
                time.sleep(0.05)
        try:
            yield
        finally:
            lock.rmdir()

    def load(self):
        if not self.path.is_file():
            raise RuntimeError("没有会话基线；请在修改前运行 begin。事后 begin 无法证明首次改动归属")
        return json.loads(read(self.path))

    def begin(self):
        with self.locked():
            if self.path.is_file():
                return {"created": False, "session": self.identifier}
            write_changed(self.path, encode(dict(session=self.identifier, baseline=snapshot(self.root),
                                                receipt=None, last_notice=None)))
        return {"created": True, "session": self.identifier}

    def status(self):
        data = self.load()
        now = snapshot(self.root)
        changes = changed(data["baseline"], now)
        rules = json.loads(read(self.root / "docs/maintenance/docflow.json"))["impact_rules"]
        affected = sorted({doc for p in changes for rule in rules
                           if any(fnmatch.fnmatchcase(p, pattern) for pattern in rule["paths"])
                           for doc in rule["docs"]})
        receipt = data.get("receipt")
        valid = bool(receipt and receipt["signature"] == signature(now))
        return dict(session=self.identifier, changed=changes, affected_docs=affected,
                    semantic_review="recorded" if valid else ("pending" if changes else "not-needed"),
                    receipt=receipt if valid else None, signature=signature(now))

    def notice(self):
        with self.locked():
            status = self.status()
            data = self.load()
            if not status["changed"] or data.get("last_notice") == status["signature"]:
                return None
            data["last_notice"] = status["signature"]
            write_changed(self.path, encode(data))
            return status

    def review(self, decision, reason, docs):
        if not reason.strip():
            raise ValueError("必须记录具体核对理由")
        for name in docs:
            p = (self.root / name).resolve()
            if not p.is_relative_to(self.root / "docs") or not p.is_file():
                raise ValueError("review只能引用存在的本项目文档：" + name)
        with self.locked():
            data = self.load()
            now = snapshot(self.root)
            changes = changed(data["baseline"], now)
            if decision == "updated" and not any(p.startswith("docs/") and p.endswith(".md") for p in changes):
                raise ValueError("updated需要实际修改Markdown；无说明影响请使用no-impact并写理由")
            if decision == "updated" and not docs:
                raise ValueError("updated需要通过--docs列出已核对专题")
            data["receipt"] = dict(decision=decision, reason=reason.strip(), docs=docs,
                                   signature=signature(now), evidence="AI/开发者语义核对记录，不是自动证明")
            write_changed(self.path, encode(data))
            return data["receipt"]

    def complete(self):
        with self.locked():
            status = self.status()
            if status["semantic_review"] == "pending":
                raise RuntimeError("尚未记录文档语义核对，不能推进会话基线")
            data = self.load()
            data["last_completed"] = data.get("receipt")
            data.update(baseline=snapshot(self.root), receipt=None, last_notice=None)
            write_changed(self.path, encode(data))
        return status
