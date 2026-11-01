import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
import sonic_heroes


def make_xbe(entry=0x10000, debug=False, title=0x4F48534E, build=4531):
    header = bytearray(0x200)
    header[0:4] = b"XBEH"
    struct.pack_into("<I", header, 0x104, 0x10000)  # base
    struct.pack_into("<I", header, 0x108, 0x200)    # header size
    struct.pack_into("<I", header, 0x10C, 0x1000)   # image size
    struct.pack_into("<I", header, 0x118, 0x10180)  # certificate
    struct.pack_into("<I", header, 0x11C, 1)        # section count
    struct.pack_into("<I", header, 0x120, 0x10100)  # section headers
    struct.pack_into("<I", header, 0x124, 0)        # init flags
    if debug:
        struct.pack_into("<I", header, 0x128, entry ^ sonic_heroes.DEBUG_ENTRY_KEY)
        struct.pack_into("<I", header, 0x158, entry ^ sonic_heroes.DEBUG_THUNK_KEY)
    else:
        struct.pack_into("<I", header, 0x128, entry ^ sonic_heroes.RETAIL_ENTRY_KEY)
        struct.pack_into("<I", header, 0x158, entry ^ sonic_heroes.RETAIL_THUNK_KEY)
    struct.pack_into("<I", header, 0x12C, 0)        # tls
    struct.pack_into("<I", header, 0x160, 1)        # library count
    struct.pack_into("<I", header, 0x164, 0x10140)  # libraries
    struct.pack_into("<I", header, 0x188, title)    # title id at certificate + 8
    # section header
    struct.pack_into("<I", header, 0x100, 4)        # executable
    struct.pack_into("<I", header, 0x104, 0x10000)
    struct.pack_into("<I", header, 0x108, 0x1000)
    struct.pack_into("<I", header, 0x10C, 0x200)
    struct.pack_into("<I", header, 0x110, 0x1000)
    struct.pack_into("<I", header, 0x114, 0x10190)  # name address
    header[0x190:0x196] = b".text\0"
    # library
    header[0x140:0x144] = b"D3D8"
    struct.pack_into("<HHHH", header, 0x148, 4, 0, build, 0)
    return bytes(header)


class XbeParserTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def write(self, data, name="default.xbe"):
        path = self.root / name
        path.write_bytes(data)
        return path

    def test_retail_entry_and_sections(self):
        xbe = sonic_heroes.Xbe(self.write(make_xbe()))
        self.assertEqual(xbe.entry, 0x10000)
        self.assertEqual(xbe.title_id, 0x4F48534E)
        self.assertEqual(xbe.debug, False)
        self.assertEqual([s["name"] for s in xbe.sections], [".text"])
        self.assertEqual(xbe.executable_sections(), [".text"])
        self.assertEqual(xbe.library_builds(), [4531])
        self.assertEqual(xbe.choose_xdk(), (4531, True))

    def test_debug_entry_key(self):
        xbe = sonic_heroes.Xbe(self.write(make_xbe(debug=True)))
        self.assertEqual(xbe.entry, 0x10000)
        self.assertEqual(xbe.debug, True)

    def test_rejects_non_xbe(self):
        path = self.write(b"not an xbe" + bytes(0x200))
        with self.assertRaises(ValueError):
            sonic_heroes.Xbe(path)

    def test_generated_config(self):
        xbe = sonic_heroes.Xbe(self.write(make_xbe()))
        out = self.root / "game.toml"
        xdk, exact = sonic_heroes.write_config(xbe, out)
        self.assertEqual(xdk, 4531)
        self.assertTrue(exact)
        text = out.read_text()
        self.assertIn("entry = 0x10000", text)
        self.assertIn("title = 0x4F48534E", text)
        self.assertIn("xdk = 4531", text)
        self.assertIn('code_sections = [".text"]', text)
        self.assertIn("[symbols]", text)
        self.assertIn("[data]", text)

    def test_xdk_override_is_not_exact(self):
        xbe = sonic_heroes.Xbe(self.write(make_xbe()))
        out = self.root / "game.toml"
        xdk, exact = sonic_heroes.write_config(xbe, out, 4721)
        self.assertEqual(xdk, 4721)
        self.assertFalse(exact)
        self.assertIn("xdk = 4721", out.read_text())

    def test_partitions_are_clamped(self):
        xbe = sonic_heroes.Xbe(self.write(make_xbe()))
        self.assertGreaterEqual(xbe.partitions(), 8)
        self.assertLessEqual(xbe.partitions(), 64)


if __name__ == "__main__":
    unittest.main()
