"""Check the maintained Markdown test graph against a complete CTest JUnit run."""
import argparse
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def verify(markdown, junit):
    declared = set(re.findall(r"^\| (TC-\d+) \|", markdown, re.M))
    mappings = re.findall(r"^\| EXEC \| (TC-\d+) \| `([^`]+)` \| `([^`]+)` \|$", markdown, re.M)
    mapped = {row[0] for row in mappings}
    if not declared or declared != mapped:
        raise ValueError(f"Unmapped or unknown test IDs: {sorted(declared ^ mapped)}")
    cases = list(junit.iter("testcase"))
    if not cases or len({t.get('name') for t in cases}) != len(cases):
        raise ValueError("Empty JUnit or duplicate test names")
    for case in cases:
        if (case.get("status") not in (None, "run")
                or any(case.find(tag) is not None for tag in ("failure", "error", "skipped"))):
            raise ValueError(f"Test did not pass: {case.get('name')}")
    covered = set()
    for tc, pattern, marker in mappings:
        selected = [t for t in cases if re.fullmatch(pattern, t.get("name", ""))]
        if not selected:
            raise ValueError(f"{tc}: no executed test for {pattern}")
        for case in selected:
            if marker not in case.findtext("system-out", ""):
                raise ValueError(f"{tc}: missing assertion marker {marker}")
            covered.add(case.get("name"))
    # Also require startup deployment smoke, whose success is its process exit code.
    covered.add("startup-smoke")
    unknown = {t.get("name") for t in cases} - covered
    if unknown or not any(t.get("name") == "startup-smoke" for t in cases):
        raise ValueError(f"Tests missing from execution map or startup smoke missing: {unknown}")
    return {"tests_passed": len(cases), "traceability_ids": sorted(declared), "mapping_checks": len(mappings)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--junit", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = verify((ROOT / "docs/logic/TEST-TRACEABILITY.md").read_text(encoding="utf-8"),
                    ET.parse(args.junit).getroot())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
