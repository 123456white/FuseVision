"""Behavior tests for documentation automation; all mutations stay in temporary repositories."""
from pathlib import Path
from unittest.mock import patch
import json
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts/docs"))
import docflow
from docmodel import feature_inventory, issue_inventory, mindmap, write_changed, encode, digest, maintenance_lock
from doccheck import local_links, input_hashes, check, GENERATED
from docsessions import Session
from docsite import site_inputs


class DocumentationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        # Windows runners may provide TEMP through an 8.3 alias; use one canonical root.
        self.root = Path(self.temp.name).resolve()
        self.put("AGENTS.md", "# Project\n")
        self.put("src/Sample.cpp", "void Sample::save() {}\n")
        self.put("docs/logic/CORE-FLOW.md", "# Core\n")
        self.put("docs/logic/DATA-CONTRACTS.md", "# Data\n")
        for name in ["FEATURE-RELATIONS", "STATE-MACHINES", "ALGORITHMS"]:
            self.put(f"docs/logic/{name}.md", "# " + name + "\n")
        self.put("docs/codebase/ARCHITECTURE.md", "# Architecture\n")
        self.put("docs/logic/FEATURES.md", """# Features
项目：FuseVision
记录日期：2026-11-17
范围：测试样本
## F01 示例
位置：模块 / P00
入口：保存
场景：编辑
前提：登录
输入与校验：合法几何
证据状态：仅静态
按钮编号：无

1. 提交

结果：保存
失败/取消/边界：保留旧文件
数据/接口/算法：JSON
验证范围：本例仅测试文档解析
代码：[Sample::save](../../src/Sample.cpp:1)
""")
        self.put("docs/refactor/ISSUES.md", """# Issues
| [ISS-001](#iss-001) 示例 | P0 | 待验 |
<a id="iss-001"></a>
## ISS-001 示例
**状态**：待验。
**当前实现/限制**：返回错误。
**验证**：仅静态。
**未完成或边界**：未运行。
**当前代码**：[Sample::save](../../src/Sample.cpp:1)
""")
        history = "docs/archive/original"
        self.put(history + "/features.json", encode({"features": [{"id": "F01", "history_old": {"fact": "original"}}]}))
        self.put(history + "/issues.json", encode([{"id": "ISS-001", "history_old": {"status": "failed"}}]))
        self.put("docs/maintenance/docflow.json", encode(dict(project="FuseVision", features="docs/logic/FEATURES.md",
            issues="docs/refactor/ISSUES.md", history=history, images=[],
            impact_rules=[dict(paths=["src/*"], docs=["docs/logic/FEATURES.md"])])))

    def put(self, name, text):
        write_changed(self.root / name, text)

    def test_features_come_from_markdown_and_preserve_history(self):
        f = feature_inventory(self.root)["features"][0]
        self.assertEqual(f["history_old"]["fact"], "original")
        self.assertEqual(f["preconditions"], "登录")
        self.put("docs/logic/FEATURES.md", (self.root / "docs/logic/FEATURES.md").read_text(encoding="utf8").replace("结果：保存", "结果：保留结果"))
        self.assertEqual(feature_inventory(self.root)["features"][0]["success"], "保留结果")

    def test_missing_required_field_fails_instead_of_inventing_fact(self):
        p = self.root / "docs/logic/FEATURES.md"
        p.write_text(p.read_text(encoding="utf8").replace("场景：编辑", "场景："), encoding="utf8")
        with self.assertRaises(ValueError): feature_inventory(self.root)

    def test_shared_capability_has_no_invented_button(self):
        p = self.root / "docs/logic/FEATURES.md"
        p.write_text(p.read_text(encoding="utf8").replace("入口：保存", "入口：无独立按钮入口（共享状态）"), encoding="utf8")
        self.assertEqual(feature_inventory(self.root)["features"][0]["buttons"], [])

    def test_dialog_button_identifiers_are_preserved(self):
        p = self.root / 'docs/logic/FEATURES.md'
        p.write_text(p.read_text(encoding='utf8').replace('按钮编号：无', '按钮编号：B02D-01 / B02-02'), encoding='utf8')
        self.assertEqual(feature_inventory(self.root)['features'][0]['buttonIds'], ['B02D-01', 'B02-02'])

    def test_map_relationships_come_from_markdown_and_validate_endpoints(self):
        self.put('docs/logic/FEATURE-RELATIONS.md', '# Relations\n| F01 | F01 | 重复操作 |\n')
        inventory=feature_inventory(self.root)
        spec=mindmap(self.root, inventory, issue_inventory(self.root))
        self.assertEqual(spec['sheets'][0]['relationships'], [{'from':'ov-F01','to':'ov-F01','title':'重复操作'}])
        self.put('docs/logic/FEATURE-RELATIONS.md', '# Relations\n| F01 | F99 | 缺失 |\n')
        with self.assertRaises(ValueError):mindmap(self.root, inventory, issue_inventory(self.root))

    def test_duplicate_feature_id_fails(self):
        p = self.root / "docs/logic/FEATURES.md"
        p.write_text(p.read_text(encoding="utf8") + p.read_text(encoding="utf8"), encoding="utf8")
        with self.assertRaises(ValueError): feature_inventory(self.root)

    def test_issue_current_status_and_original_history_are_separate(self):
        item = issue_inventory(self.root)[0]
        self.assertEqual(item["status"], "待验")
        self.assertEqual(item["history_old"]["status"], "failed")

    def test_preexisting_dirty_untracked_files_not_attributed_to_session(self):
        session = Session(self.root, "existing-dirty")
        session.begin()
        self.assertEqual(session.status()["changed"], [])
        self.put("src/Sample.cpp", "void Sample::save() { /* later */ }\n")
        self.assertEqual(session.status()["changed"], ["src/Sample.cpp"])
        self.assertIn("docs/logic/FEATURES.md", session.status()["affected_docs"])

    def test_generation_failure_releases_repository_lock(self):
        lock=self.root / 'out/docs/generation.lock'
        with self.assertRaises(ValueError):
            with maintenance_lock(self.root):
                self.assertTrue(lock.is_dir())
                raise ValueError('export failed')
        self.assertFalse(lock.exists())
        with maintenance_lock(self.root):
            self.assertTrue(lock.is_dir())

    def test_resume_and_compact_do_not_reset_pending_baseline(self):
        s = Session(self.root, "resume"); s.begin()
        self.put("src/New.h", "new")
        self.assertFalse(s.begin()["created"])
        self.assertIn("src/New.h", s.status()["changed"])

    def test_file_deletion_is_detected(self):
        s = Session(self.root, "delete"); s.begin()
        (self.root / "src/Sample.cpp").unlink()
        self.assertIn("src/Sample.cpp", s.status()["changed"])

    def test_review_invalidated_after_additional_edit(self):
        s = Session(self.root, "review"); s.begin()
        self.put("src/New.h", "new")
        s.review("no-impact", "内部注释不改变功能", [])
        self.assertEqual(s.status()["semantic_review"], "recorded")
        self.put("src/New.h", "another")
        self.assertEqual(s.status()["semantic_review"], "pending")

    def test_updated_receipt_requires_actual_markdown_update(self):
        s = Session(self.root, "false-update"); s.begin()
        self.put("src/New.h", "new")
        with self.assertRaises(ValueError): s.review("updated", "updated", ["docs/logic/FEATURES.md"])

    def test_updated_receipt_completes_and_advances_baseline(self):
        s = Session(self.root, "complete"); s.begin()
        self.put("docs/README.md", "# Updated\n")
        s.review("updated", "同步说明", ["docs/README.md"])
        s.complete()
        self.assertEqual(s.status()["changed"], [])

    def test_cannot_complete_unreviewed_change(self):
        s = Session(self.root, "pending"); s.begin()
        self.put("src/New.h", "new")
        with self.assertRaises(RuntimeError): s.complete()

    def test_receipts_are_project_local_and_reject_outside_docs(self):
        s = Session(self.root, "../../escape"); s.begin()
        self.assertTrue(s.path.is_relative_to(self.root / "out/docs/sessions"))
        with self.assertRaises(ValueError): s.review("no-impact", "reason", ["../outside.md"])

    def test_missing_file_and_heading_links_fail(self):
        self.put("docs/README.md", "[bad](missing.md) [anchor](logic/CORE-FLOW.md#missing)\n")
        errors, _ = local_links(self.root)
        self.assertEqual(len(errors), 2)

    def test_fenced_example_links_are_not_treated_as_real_links(self):
        self.put("docs/README.md", "```md\n[example](missing.md)\n```\n")
        self.assertEqual(local_links(self.root)[0], [])

    def test_integrity_check_does_not_change_input_or_create_reports(self):
        p = self.root / "docs/logic/FEATURES.md"; before = p.read_bytes()
        result = check(self.root, generated=False)
        self.assertTrue(result["passed"], result)
        self.assertEqual(p.read_bytes(), before)
        self.assertFalse((self.root / "out").exists())

    def test_stale_mirrors_fail(self):
        self.put(GENERATED[0], "{}\n")
        result = check(self.root)
        self.assertFalse(result["passed"])
        self.assertTrue(any("过期" in e for e in result["errors"]))

    def test_moved_symbol_invalidates_old_code_line(self):
        self.put('src/Sample.cpp', '// moved\nvoid Sample::save() {}\n')
        result=check(self.root, generated=False)
        self.assertTrue(any('符号与引用行不匹配' in e for e in result['errors']))

    def test_html_refresh_tracks_linked_code_and_downloads(self):
        self.put('scripts/docs/package-lock.json', '{}\n')
        source = self.root / 'docs/logic/FEATURES.md'
        before = site_inputs(self.root, [source])
        self.assertIn('src/Sample.cpp', before)
        self.put('src/Sample.cpp', 'void Sample::save() { /* changed */ }\n')
        self.assertNotEqual(before, site_inputs(self.root, [source]))

    def test_original_evidence_tampering_fails(self):
        self.put("docs/logic/evidence/original.log", "original\n")
        name = "docs/logic/evidence/original.log"
        self.put("docs/maintenance/archive-manifest.json", encode({"protected_files": {name: digest(self.root / name)}}))
        self.put(name, "changed\n")
        result = check(self.root, generated=False)
        self.assertTrue(any("历史原始资料" in e for e in result["errors"]))

    def test_stop_requests_one_continuation_and_then_reports_unresolved(self):
        base = dict(cwd=str(self.root), session_id="stop", hook_event_name="Stop")
        git = subprocess.CompletedProcess([], 0, stdout=str(self.root), stderr="")
        with patch.object(docflow.subprocess, "run", return_value=git), patch.object(docflow, "finish", return_value={"passed": False, "reason": "pending"}):
            self.assertEqual(docflow.hook(self.root, base)["decision"], "block")
            result = docflow.hook(self.root, {**base, "stop_hook_active": True})
            self.assertNotIn("decision", result)
            self.assertIn("未完成", result["systemMessage"])

    def test_hooks_skip_other_projects(self):
        self.assertEqual(docflow.hook(self.root, dict(cwd=str(self.root.parent), hook_event_name="SessionStart")), {})

    def test_generated_changes_do_not_invalidate_semantic_receipt(self):
        s = Session(self.root, "derived"); s.begin()
        self.put("src/New.h", "new")
        s.review("no-impact", "内部注释", [])
        self.put(GENERATED[0], "{}\n")
        self.assertEqual(s.status()["semantic_review"], "recorded")


if __name__ == "__main__":
    unittest.main()
