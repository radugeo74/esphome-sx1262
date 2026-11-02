#!/usr/bin/env python3
"""Validate typed records from the unmodified Techem heating driver."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from driver_pipeline_probe import extract_records, interpret_record, BridgeError


class HeatingRecordsTests(unittest.TestCase):
    def test_original_version94_fixture(self):
        path = ROOT / "drivers" / "fhkvdataiii.xmq"
        payload = "0F9F292D005024040011BD08380904000000070000000000000000000000000001000000000003140E"
        records = extract_records(path, payload)
        self.assertEqual(records["426E"]["scaled"], 45)
        self.assertEqual(records["026E"]["scaled"], 4)
        self.assertAlmostEqual(records["0265"]["scaled"], 22.37)
        self.assertAlmostEqual(records["025D"]["scaled"], 23.60)
        self.assertEqual(records["02FD3A"]["scaled"], 0x299F)
        self.assertEqual(records["42FD3A"]["scaled"], 0x2450)

    def test_unknown_vif_not_silently_mapped(self):
        with self.assertRaises(BridgeError):
            interpret_record("0201", 100)


if __name__ == "__main__":
    unittest.main()
