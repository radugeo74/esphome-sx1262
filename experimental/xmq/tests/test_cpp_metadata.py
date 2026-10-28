#!/usr/bin/env python3
"""Regression checks for original XMQ capture-to-DIF/VIF metadata."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from generate_ixml_cpp import generate, metadata, compile_calculations
from ixml_probe import extract_grammar


class MetadataTests(unittest.TestCase):
    def test_original_driver_mappings(self):
        for name, expected in (
            ("mkradio4", {"DV_prev": "4215", "DV_curr": "0215"}),
            ("fhkvdataiii", {"DV_prev_hca": "426E", "DV_curr_hca": "026E"}),
        ):
            with self.subTest(name=name):
                path = ROOT / "drivers" / (name + ".xmq")
                annotations, matches = metadata(path, extract_grammar(path)[0])
                actual = dict(annotations)
                for capture, key in expected.items():
                    self.assertEqual(actual[capture].upper(), key)
                self.assertTrue(matches)

    def test_water_calculate_dependency_chain(self):
        formulas = compile_calculations(ROOT / "drivers" / "mkradio4.xmq")
        self.assertEqual([name for name, _ in formulas],
                         ["prev", "curr", "target", "total"])
        self.assertEqual(formulas[0][1][0], ("Field", "prev_raw", 0))
        self.assertIn(("Number", None, 1.0), formulas[0][1])
        self.assertEqual(formulas[-1][1][-1][0], "Add")

    def test_heat_numeric_calculations(self):
        formulas = compile_calculations(ROOT / "drivers" / "fhkvdataiii.xmq")
        names = [name for name, _ in formulas]
        self.assertEqual(names, ["previous_day", "previous_month", "previous_year",
                                 "current_day", "current_month", "current_year"])
        ops = {step[0] for _, steps in formulas for step in steps}
        self.assertTrue({"Modulo", "ShiftRight", "LessEqual", "Less", "Equal",
                         "And", "Or"}.issubset(ops))

    def test_generated_tables(self):
        header = generate([ROOT / "drivers" / "mkradio4.xmq",
                           ROOT / "drivers" / "fhkvdataiii.xmq"])
        self.assertIn('{"DV_prev", "4215"}', header)
        self.assertIn('{"previous_date_raw", "02FD3A"}', header)


if __name__ == "__main__":
    unittest.main()
