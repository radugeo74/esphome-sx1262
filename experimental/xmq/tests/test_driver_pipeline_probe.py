#!/usr/bin/env python3
"""Check chained calculations from original Techem water driver and IXML fixture."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from driver_pipeline_probe import decode, BridgeError


class DriverPipelineTests(unittest.TestCase):
    def test_water_original_fixture(self):
        driver = ROOT / "drivers" / "mkradio4.xmq"
        telegram = "2F446850200141029562A2_06702901006017030004000300000000000000000000000000000000000000000000000000"
        result = decode(driver, telegram.split("_", 1)[1])
        self.assertAlmostEqual(result["target_m3"], 0.1)
        self.assertAlmostEqual(result["total_m3"], 0.4)
        self.assertAlmostEqual(result["prev_m3"], 0.1)
        self.assertAlmostEqual(result["curr_m3"], 0.3)
        print("PASS original XMQ driver calculations:", result)

    def test_unsupported_vif_fails_closed(self):
        driver = ROOT / "drivers" / "fhkvdataiii.xmq"
        telegram = "33446850341211119480A2_0F9F292D005024040011BD08380904000000070000000000000000000000000001000000000003140E"
        with self.assertRaises(BridgeError):
            decode(driver, telegram.split("_", 1)[1])


if __name__ == "__main__":
    unittest.main()
