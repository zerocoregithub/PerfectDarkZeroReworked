"""Synthetic source-archive checks; no game input or Windows device changes."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

sdk = Path(sys.argv.pop(1)).resolve()
spec = importlib.util.spec_from_file_location("archive_verifier", sdk / "scripts/verify_source_archive.py")
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="pdz-source-archive-test-")
        self.root = Path(self.temp.name)
        self.files = []
        names = ["CMakeLists.txt", "thirdparty/CMakeLists.txt", "LICENSE"]
        names += [f"thirdparty/{name}/source.c" for name in
                  ("cli11", "libmspack", "FFmpeg", "tomlplusplus", "simde", "xxHash",
                   "spdlog", "fmt", "utfcpp", "imgui", "sdl3")]
        for name in names:
            file = self.root / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text("synthetic test source\n", encoding="utf-8")
            self.files.append({"path": name, "sha256": hashlib.sha256(file.read_bytes()).hexdigest()})
        self.save()

    def tearDown(self):
        self.temp.cleanup()

    def save(self):
        (self.root / "SOURCE_ARCHIVE.json").write_text(
            json.dumps({"format": 1, "files": self.files}), encoding="utf-8")

    def test_valid(self):
        self.assertEqual(verifier.verify(self.root), 0)

    def test_mutation(self):
        (self.root / "LICENSE").write_text("changed", encoding="utf-8")
        with self.assertRaises(ValueError):
            verifier.verify(self.root)

    def test_duplicate(self):
        self.files.append(dict(self.files[0]))
        self.save()
        with self.assertRaises(ValueError):
            verifier.verify(self.root)

    def test_unsafe_paths(self):
        for name in ("../escape", "/absolute", "C:/absolute", "thirdparty\\escape"):
            with self.subTest(name=name):
                self.files[0]["path"] = name
                self.save()
                with self.assertRaises(ValueError):
                    verifier.verify(self.root)

    def test_missing_file(self):
        (self.root / "LICENSE").unlink()
        with self.assertRaises(OSError):
            verifier.verify(self.root)

    def test_required_root_omitted(self):
        self.files = [entry for entry in self.files if entry["path"] != "LICENSE"]
        self.save()
        with self.assertRaises(ValueError):
            verifier.verify(self.root)

    def test_required_dependency_omitted(self):
        self.files = [entry for entry in self.files if not entry["path"].startswith("thirdparty/FFmpeg/")]
        self.save()
        with self.assertRaises(ValueError):
            verifier.verify(self.root)

    def test_bad_digest(self):
        self.files[0]["sha256"] = "not-a-sha256"
        self.save()
        with self.assertRaises(ValueError):
            verifier.verify(self.root)


unittest.main()
