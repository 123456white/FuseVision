"""Create a runtime-only Windows ZIP and smoke-test an isolated extraction."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile


def runtime_files(source):
    files = [source / "FuseVision.exe"]
    files += [p for p in source.glob("*.dll") if not p.name.lower().startswith(("gtest", "gmock"))]
    for folder in ("platforms", "sqldrivers", "imageformats"):
        files += list((source / folder).glob("*.dll"))
    for required in ("FuseVision.exe", "Qt6Core.dll", "Qt6Widgets.dll", "platforms/qwindows.dll", "sqldrivers/qsqlite.dll"):
        if not (source / required).is_file():
            raise ValueError(f"Missing runtime file: {required}")
    return sorted(files)


def verify_package(archive):
    with tempfile.TemporaryDirectory(prefix="fusevision-package-") as temp:
        dest = Path(temp)
        with zipfile.ZipFile(archive) as package:
            for name in package.namelist():
                path = Path(name)
                if path.is_absolute() or ".." in path.parts or ":" in name or "\\" in name:
                    raise ValueError(f"Unsafe archive entry: {name}")
            package.extractall(dest)
        env = os.environ.copy()
        system_root = next((value for key, value in env.items() if key.upper() == "SYSTEMROOT"), None)
        if not system_root:
            raise ValueError("Windows SYSTEMROOT is required for isolated runtime verification")
        env.update(PATH=str(Path(system_root) / "System32"), QT_PLUGIN_PATH="",
                   QT_QPA_PLATFORM_PLUGIN_PATH="", QT_QPA_PLATFORM="offscreen", FUSEVISION_STARTUP_SMOKE_TEST="1")
        subprocess.run([str(dest / "FuseVision.exe")], cwd=dest, env=env, check=True, timeout=45,
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        logs = list((dest / "startup-smoke/logs").glob("fusevision_*"))
        if not any("Startup smoke test completed successfully." in p.read_text(encoding="utf-8") for p in logs):
            raise ValueError("Package startup did not produce the success marker")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--revision", default="local")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    if not args.verify_only:
        if not args.source:
            parser.error("--source is required to build a package")
        args.archive.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(args.archive, "w", zipfile.ZIP_DEFLATED) as package:
            for path in runtime_files(args.source):
                package.write(path, path.relative_to(args.source).as_posix())
            package.writestr("build-info.json", json.dumps({"revision": args.revision, "configuration": "Release"}) + "\n")
        digest = hashlib.sha256(args.archive.read_bytes()).hexdigest()
        args.archive.with_suffix(".zip.sha256").write_text(f"{digest}  {args.archive.name}\n", encoding="ascii")
    else:
        expected = args.archive.with_suffix(".zip.sha256").read_text(encoding="ascii").split()[0]
        if hashlib.sha256(args.archive.read_bytes()).hexdigest() != expected:
            raise ValueError("Package SHA256 mismatch")
    verify_package(args.archive)
    print(f"PASS verified isolated package: {args.archive}")


if __name__ == "__main__":
    main()
