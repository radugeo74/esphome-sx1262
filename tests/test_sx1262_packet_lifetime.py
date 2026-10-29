#!/usr/bin/env python3
"""Source-contract regression for SX1262 packet ownership.

These checks are structural, not a substitute for an ESP-IDF build or hardware
runtime test. They guard the lifetime fixes most likely to regress.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPONENT = (ROOT / "components/wmbus_radio/component.cpp").read_text()
PACKET = (ROOT / "components/wmbus_radio/packet.cpp").read_text()


class PacketOwnershipContract(unittest.TestCase):
    def test_queue_success_transfers_ownership_only_after_send(self):
        self.assertRegex(COMPONENT,
            r"if\s*\(xQueueSend\([^;]+==\s*pdTRUE\)\s*\{[^}]*packet\.release\(\)")
        self.assertIn("auto packet = std::make_unique<Packet>();", COMPONENT)

    def test_receiver_failure_retains_raii_cleanup(self):
        self.assertIn('std::unique_ptr<Packet> packet(packet_ptr);', COMPONENT)
        self.assertIn('if (!frame)', COMPONENT)
        self.assertNotIn('delete packet', PACKET)

    def test_validation_uses_correct_output_types(self):
        self.assertIn("size_t frame_length = 0;", PACKET)
        self.assertNotIn("(size_t *)&dummy", PACKET)

    def test_isr_wakeup_flag_initialization(self):
        self.assertIn("BaseType_t xHigherPriorityTaskWoken = pdFALSE;", COMPONENT)
        self.assertIn("if (arg == nullptr || *arg == nullptr)", COMPONENT)


if __name__ == "__main__":
    unittest.main()
