import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("check_pkg", ROOT / "scripts" / "check_pkg.py")
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
CONTENT_ID = "IV0000-BREW00001_00-PS4DOWNLOADMGR01"

class TestPkgPreflight(unittest.TestCase):
    def make_pkg(self, content_id=CONTENT_ID, magic=b"\x7fCNT", declared_size=0x2000):
        f = tempfile.NamedTemporaryFile(suffix=".pkg", delete=False)
        p = Path(f.name)
        h = bytearray(0x2000)
        h[:4] = magic
        h[0x40:0x64] = content_id.encode("ascii")
        struct.pack_into(">I", h, 0x10, 1)
        struct.pack_into(">I", h, 0x18, 0x1000)
        struct.pack_into(">Q", h, 0x430, declared_size)
        f.write(h)
        f.close()
        self.addCleanup(p.unlink)
        return p
    def test_good_header(self):
        self.assertEqual(mod.inspect(self.make_pkg(), CONTENT_ID), [])
    def test_invalid_magic(self):
        self.assertIn("Not a PS4 PKG", str(mod.inspect(self.make_pkg(magic=b"BAD!"))))
    def test_invalid_id(self):
        self.assertIn("Invalid Content ID", str(mod.inspect(self.make_pkg(content_id="X" * 36))))
    def test_mismatch(self):
        self.assertIn("Content ID mismatch", str(mod.inspect(self.make_pkg(), "IV0000-BREW00002_00-PS4DOWNLOADMGR01")))
    def test_truncated(self):
        self.assertIn("Truncated", str(mod.inspect(self.make_pkg(declared_size=0x4000))))

if __name__ == "__main__":
    unittest.main()
