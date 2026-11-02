#!/usr/bin/env python3
"""Check generic arithmetic against unmodified XMQ calculate declarations."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from calculate_probe import CalculateError, evaluate
from portable_xmq import blocks, parse, validate_driver, values


def expressions(name):
    path = ROOT / "drivers" / (name + ".xmq")
    driver = validate_driver(parse(path.read_text(encoding="utf-8")))
    found = {}
    for fields in blocks(driver, "fields"):
        for field in blocks(fields["entries"], "field"):
            expr = values(field["entries"], "calculate")
            if expr:
                found[values(field["entries"], "name")[0]] = expr[0]
    return found


class CalculateTests(unittest.TestCase):
    def test_water_driver_original_expressions(self):
        expr = expressions("mkradio4")
        self.assertEqual(evaluate(expr["prev"], {"prev_raw_counter": 1}), 1)
        self.assertEqual(evaluate(expr["curr"], {"curr_raw_counter": 3}), 3)
        self.assertEqual(evaluate(expr["target"], {"prev_m3": .1}), .1)
        self.assertAlmostEqual(evaluate(expr["total"], {"prev_m3": .1, "curr_m3": .3}), .4)

    def test_heat_driver_integer_operations(self):
        expr = expressions("fhkvdataiii")
        candidates = [(key, value) for key, value in expr.items() if ">>" in value and "||" not in value and "&&" not in value]
        self.assertTrue(candidates)
        # This is a parser/evaluator smoke test, not yet a full HCA date conversion.
        self.assertEqual(evaluate("((raw >> 4counter) % 32counter)", {"raw": 0x123}), 18)

    def test_reject_unsafe_and_unsupported(self):
        for expression in ("__import__('os')", "a.b", "a[0]", "missing + 1", "1/0", "2**100"):
            with self.subTest(expression=expression), self.assertRaises(CalculateError):
                evaluate(expression, {"a": 3})

    def test_compares_and_logic(self):
        self.assertEqual(evaluate("(a <= 3counter) and (b == 2counter)", {"a": 3, "b": 2}), 1)
        self.assertEqual(evaluate("a > 4counter", {"a": 3}), 0)


if __name__ == "__main__":
    unittest.main()
