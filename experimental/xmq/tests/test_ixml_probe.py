#!/usr/bin/env python3
"""Regression checks for the bounded IXML experiment, using official fixture payloads."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from ixml_probe import extract_grammar, grammar_rules, marked_fixtures, run_ixml


class IxmlProbeTests(unittest.TestCase):
    def test_original_techem_binary_captures(self):
        expected = {
            "mkradio4": [
                {"DV_prev": "0100", "DV_curr": "0300"},
            ],
            "fhkvdataiii": [
                {"DV_prev_date_raw": "9F29", "DV_prev_hca": "2D00",
                 "DV_curr_date_raw": "5024", "DV_curr_hca": "0400"},
                {"DV_prev_date_raw": "9F33", "DV_prev_hca": "4805",
                 "DV_curr_date_raw": "400C", "DV_curr_hca": "FE02"},
            ],
        }
        for name, cases in expected.items():
            path = ROOT / "drivers" / f"{name}.xmq"
            grammars = extract_grammar(path)
            self.assertEqual(len(grammars), 1)
            grammar = grammar_rules(grammars[0])
            payloads = list(marked_fixtures(path.read_text(encoding="utf-8")))
            self.assertEqual(len(payloads), len(cases))
            for payload, expected_captures in zip(payloads, cases):
                with self.subTest(name=name, payload=payload[:15]):
                    actual = run_ixml(grammar, payload)
                    for key, value in expected_captures.items():
                        self.assertEqual(actual.get(key), value, f"{name}: {key}")
                    self.assertGreaterEqual(len(actual), len(expected_captures))

    def test_unsupported_operation_fails(self):
        with self.assertRaisesRegex(ValueError, "Unsupported"):
            run_ixml({"decode": [("nonexistent",)]}, "00")

    def test_bad_payload_rejected(self):
        with self.assertRaises(ValueError):
            run_ixml({"decode": [("'AA'",)]}, "00")


if __name__ == "__main__":
    unittest.main()
