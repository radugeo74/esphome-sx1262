#!/usr/bin/env python3
"""Structural regression tests against original pinned XMQ definitions."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from portable_xmq import ParseError, blocks, parse, validate_driver, values


class XmqParserTests(unittest.TestCase):
    def test_original_drivers(self):
        for filename, driver_name in (("mkradio4.xmq", "mkradio4"), ("fhkvdataiii.xmq", "fhkvdataiii")):
            with self.subTest(filename=filename):
                tree = parse((ROOT / "drivers" / filename).read_text(encoding="utf-8"))
                d = validate_driver(tree)
                self.assertEqual(values(d, "name"), [driver_name])
                self.assertTrue(blocks(d, "detect"))
                fields = [field for group in blocks(d, "fields") for field in blocks(group["entries"], "field")]
                self.assertGreater(len(fields), 0)
                self.assertTrue(any(values(f["entries"], "ixml") for f in fields))
                self.assertTrue(any(values(f["entries"], "calculate") or blocks(f["entries"], "match") for f in fields))
                self.assertTrue(all(values(f["entries"], "name") for f in fields))
                print(filename, "fields:", len(fields), "ixml:", sum(bool(values(f["entries"], "ixml")) for f in fields))

    def test_repeated_values_and_multiline_string(self):
        tree = parse('driver {\n name = test\n meter_type = WaterMeter\n detect {\n mvt = TCH,95,62\n mvt = TCH,70,72\n }\n fields { field { name = test\n ixml = "a\nb" } }\n }')
        d = validate_driver(tree)
        self.assertEqual(values(blocks(d, "detect")[0]["entries"], "mvt"), ["TCH,95,62", "TCH,70,72"])

    def test_invalid_documents(self):
        for document in ('driver { name = test', 'driver { name = "unterminated}', 'driver { name = test } }'):
            with self.subTest(document=document), self.assertRaises(ParseError):
                parse(document)


if __name__ == "__main__":
    unittest.main()
