#!/usr/bin/env python3
"""Calendar regression against original Techem XMQ date fields."""
import sys
import unittest
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from calendar_probe import compute_dates, month_offset, CalendarError


class CalendarTests(unittest.TestCase):
    def test_original_techem_94(self):
        result = compute_dates(ROOT / "drivers" / "fhkvdataiii.xmq",
                               {"previous_date_raw_counter": 0x299F,
                                "current_date_raw_counter": 0x2450})
        self.assertEqual(result, {"previous_date": "2020-12-31",
                                  "current_date": "2021-02-05"})

    def test_original_techem_01(self):
        result = compute_dates(ROOT / "drivers" / "fhkvdataiii.xmq",
                               {"previous_date_raw_counter": 0x339F,
                                "current_date_raw_counter": 0x0C40})
        self.assertEqual(result, {"previous_date": "2025-12-31",
                                  "current_date": "2026-06-04"})

    def test_calendar_month_not_fixed_seconds(self):
        self.assertEqual(month_offset(date(2024, 1, 31), 1), date(2024, 2, 29))
        self.assertEqual(month_offset(date(2025, 12, 31), 2), date(2026, 2, 28))

    def test_reject_bad_offset(self):
        with self.assertRaises(CalendarError):
            month_offset(date(2025, 1, 1), 1.5)


if __name__ == "__main__":
    unittest.main()
