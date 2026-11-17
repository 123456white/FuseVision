import importlib.util
from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET


def load(name):
    path = Path(__file__).resolve().parents[1] / "scripts/ci" / (name + ".py")
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


trace = load("verify_traceability")
package = load("package_windows")


class TraceabilityTests(unittest.TestCase):
    def setUp(self):
        self.md = "| TC-001 | scope |\n| EXEC | TC-001 | `workflow` | `PASS behavior` |\n"
        self.junit = ET.fromstring('<testsuite><testcase name="workflow" status="run"><system-out>PASS behavior</system-out></testcase><testcase name="startup-smoke" status="run"/></testsuite>')

    def test_valid(self):
        self.assertEqual(trace.verify(self.md, self.junit)["tests_passed"], 2)

    def test_unmapped_case_rejected(self):
        with self.assertRaises(ValueError):
            trace.verify(self.md + "| TC-002 | scope |\n", self.junit)

    def test_skipped_and_failed_rejected(self):
        for tag in ("skipped", "failure", "error"):
            with self.subTest(tag=tag):
                root = ET.fromstring(ET.tostring(self.junit))
                ET.SubElement(root[0], tag)
                with self.assertRaises(ValueError):
                    trace.verify(self.md, root)

    def test_missing_test_or_assertion_rejected(self):
        for text in (self.md.replace("`workflow`", "`missing`"), self.md.replace("PASS behavior", "PASS missing")):
            with self.assertRaises(ValueError):
                trace.verify(text, self.junit)

    def test_missing_smoke_or_duplicate_rejected(self):
        self.junit.remove(self.junit[1])
        with self.assertRaises(ValueError):
            trace.verify(self.md, self.junit)
        self.junit.append(ET.fromstring(ET.tostring(self.junit[0])))
        with self.assertRaises(ValueError):
            trace.verify(self.md, self.junit)


class PackageTests(unittest.TestCase):
    def test_only_runtime_files_selected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            names = ("FuseVision.exe", "Qt6Core.dll", "Qt6Widgets.dll", "platforms/qwindows.dll", "sqldrivers/qsqlite.dll", "gtest.dll", "FuseVisionStorageTests.exe", "data/account.db", "logs/app.log", "project.fvproj")
            for name in names:
                p = root / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.touch()
            selected = {p.relative_to(root).as_posix() for p in package.runtime_files(root)}
            self.assertEqual(selected, set(names[:5]))
            (root / "platforms/qwindows.dll").unlink()
            with self.assertRaises(ValueError):
                package.runtime_files(root)


if __name__ == "__main__":
    unittest.main()
