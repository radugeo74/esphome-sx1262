"""Offline regression tests for raw_t1_recover_offline.py; no device needed."""
import unittest

from raw_t1_recover_offline import CODES, analyze, crc16_en13757, validate_format_a


def format_a(data: bytes) -> bytes:
    output = bytearray()
    pos = 0
    while pos < len(data):
        count = min(len(data) - pos, 10 if pos == 0 else 16)
        block = data[pos:pos + count]
        output.extend(block)
        output.extend(crc16_en13757(block).to_bytes(2, "big"))
        pos += count
    return bytes(output)


def encode_t1(frame: bytes) -> bytes:
    bits = "".join(f"{CODES[nibble]:06b}"
                   for value in frame for nibble in (value >> 4, value & 15))
    return int(bits.ljust((len(bits) + 7) // 8 * 8, "0"), 2).to_bytes(
        (len(bits) + 7) // 8, "big")


class TestOfflineRecovery(unittest.TestCase):
    def setUp(self):
        self.payload = bytes.fromhex(
            "2F446850953841229572A2063E35DC03F0161A000000060B0B0C0F08080B0A"
            "0A080808090908080A09090A08070A0A08")
        # Synthetic valid T1 Format A packet with real link-layer CRCs.
        self.frame = format_a(self.payload)
        self.raw = encode_t1(self.frame)

    def test_crc_vector_from_ti(self):
        self.assertEqual(crc16_en13757(bytes.fromhex("0F44AE0C785634120107")), 0x4447)
        self.assertEqual(crc16_en13757(bytes.fromhex("780B13436587")), 0x1E6D)

    def test_valid_frame(self):
        self.assertTrue(validate_format_a(self.frame))
        self.assertEqual(analyze(self.raw)["status"], "crc_valid")

    def test_single_bad_symbol_recovered(self):
        # Flip one bit of an encoded symbol until it becomes invalid.
        bits = list("".join(f"{b:08b}" for b in self.raw))
        found = False
        for pos in range(20 * 6, 22 * 6):
            flipped = bits.copy()
            flipped[pos] = "0" if flipped[pos] == "1" else "1"
            corrupted = int("".join(flipped), 2).to_bytes(len(self.raw), "big")
            if analyze(corrupted)["status"] == "crc_valid" and analyze(corrupted)["invalid_symbols"] == 1:
                found = True
                break
        self.assertTrue(found)

    def test_incomplete_capture(self):
        self.assertEqual(analyze(self.raw[:12])["status"], "incomplete_capture")

    def test_crc_rejection(self):
        corrupted = bytearray(self.frame)
        corrupted[-1] ^= 1
        self.assertFalse(validate_format_a(bytes(corrupted)))


if __name__ == "__main__":
    unittest.main()
