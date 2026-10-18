"""Verify a source-only SDK snapshot before accepting missing Git metadata.

Integrity check, not a signature or licence certification. Never writes files.
"""
import hashlib
import json
from pathlib import Path
import re
import sys


def verify(root: Path) -> int:
    root = root.resolve(strict=True)
    manifest = json.loads((root / "SOURCE_ARCHIVE.json").read_text(encoding="utf-8-sig"))
    if manifest.get("format") != 1 or not isinstance(manifest.get("files"), list):
        raise ValueError("unsupported source archive manifest")
    seen = set()
    required = {"CMakeLists.txt", "thirdparty/CMakeLists.txt", "LICENSE"}
    for entry in manifest["files"]:
        name, expected = entry["path"], entry["sha256"]
        if not isinstance(name, str) or "\\" in name or ":" in name:
            raise ValueError("invalid archive path")
        path = Path(name)
        if name.startswith("/") or name != path.as_posix() or path.is_absolute() or any(part in (".", "..") for part in path.parts):
            raise ValueError("unsafe archive path")
        if name.casefold() in seen or not re.fullmatch(r"[0-9a-fA-F]{64}", expected):
            raise ValueError("duplicate path or invalid hash")
        seen.add(name.casefold())
        target = (root / path).resolve(strict=True)
        if not target.is_relative_to(root) or not target.is_file():
            raise ValueError("archive path escapes source tree")
        if hashlib.sha256(target.read_bytes()).hexdigest().lower() != expected.lower():
            raise ValueError(f"source mismatch: {name}")
        required.discard(name)
    if required:
        raise ValueError("archive manifest omits required root sources")
    for name in ("cli11", "libmspack", "FFmpeg", "tomlplusplus", "simde", "xxHash",
                 "spdlog", "fmt", "utfcpp", "imgui", "sdl3"):
        prefix = f"thirdparty/{name}/".casefold()
        if not any(path.startswith(prefix) for path in seen):
            raise ValueError(f"archive manifest omits required dependency: {name}")
    print(f"PASS: {len(seen)} source files verified; no Git metadata fabricated")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(verify(Path(sys.argv[1])))
    except (OSError, ValueError, KeyError, TypeError, IndexError) as error:
        print(f"Source archive verification failed: {error}", file=sys.stderr)
        sys.exit(1)
