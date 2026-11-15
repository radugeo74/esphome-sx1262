#!/usr/bin/env python3
"""Offline-only T1 3-of-6 single-bit candidate recovery, CRC-16/EN-13757 A.

Reads RAW radio samples from an ESPHome log or a literal hex string. No code
runs in the receiver, and no candidate is ever dispatched to meters.
Only invalid 3-of-6 symbols are corrected (one flipped bit per symbol).
A CRC failure with *all legal* symbols is NOT recoverable by this method.
"""
from __future__ import annotations

import argparse
import itertools
import re
from collections import Counter
from pathlib import Path

# Exactly the mapping in components/sx127x/wmbus_t1_length.h.
CODES = (0x16, 0x0D, 0x0E, 0x0B, 0x1C, 0x19, 0x1A, 0x13,
         0x2C, 0x25, 0x26, 0x23, 0x34, 0x31, 0x32, 0x29)
CODE_TO_NIBBLE = {code: nibble for nibble, code in enumerate(CODES)}
RAW_PATTERN = re.compile(r"SX127x T1 RAW stored (\d+)/6 .*?raw(?:64|128)=([0-9a-fA-F]+)")


def crc16_en13757(data: bytes) -> int:
    """CRC-16/EN-13757: poly 0x3D65, init 0, non-reflected, xorout FFFF."""
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x3D65 if crc & 0x8000 else 0)) & 0xFFFF
    return crc ^ 0xFFFF


def symbols(raw: bytes) -> list[int]:
    bits = "".join(f"{byte:08b}" for byte in raw)
    return [int(bits[i:i + 6], 2) for i in range(0, len(bits) - 5, 6)]


def decode_nibbles(syms: list[int]) -> bytes | None:
    if len(syms) % 2:
        return None
    try:
        digits = [CODE_TO_NIBBLE[code] for code in syms]
    except KeyError:
        return None
    return bytes((digits[i] << 4) | digits[i + 1]
                 for i in range(0, len(digits), 2))


def frame_size(l_field: int) -> int:
    if l_field < 10 or l_field > 255:
        return 0
    blocks = 1 + ((l_field - 9 + 15) // 16)
    return l_field + 1 + 2 * blocks


def validate_format_a(frame: bytes) -> bool:
    if not frame or frame_size(frame[0]) != len(frame):
        return False
    data_left = frame[0] + 1
    pos = 0
    first = True
    while data_left:
        count = min(data_left, 10 if first else 16)
        if pos + count + 2 > len(frame):
            return False
        block = frame[pos:pos + count]
        found = int.from_bytes(frame[pos + count:pos + count + 2], "big")
        if crc16_en13757(block) != found:
            return False
        pos += count + 2
        data_left -= count
        first = False
    return pos == len(frame)


def analyze(raw: bytes, max_errors: int = 2, max_tries: int = 128) -> dict:
    syms = symbols(raw)
    if len(syms) < 4 or syms[0] not in CODE_TO_NIBBLE or syms[1] not in CODE_TO_NIBBLE:
        return {"status": "bad_length_prefix"}
    l_field = (CODE_TO_NIBBLE[syms[0]] << 4) | CODE_TO_NIBBLE[syms[1]]
    expected = frame_size(l_field)
    needed_symbols = expected * 2
    if not expected:
        return {"status": "bad_length_prefix", "l_field": l_field}
    if len(syms) < needed_symbols:
        return {"status": "incomplete_capture", "l_field": l_field,
                "needed_raw_bytes": (needed_symbols * 6 + 7) // 8,
                "captured_raw_bytes": len(raw)}
    syms = syms[:needed_symbols]
    bad = [i for i, code in enumerate(syms) if code not in CODE_TO_NIBBLE]
    result = {"l_field": l_field, "invalid_symbols": len(bad),
              "invalid_positions_1based": [i + 1 for i in bad],
              "full_raw_bytes": (needed_symbols * 6 + 7) // 8}
    if len(bad) > max_errors:
        return {**result, "status": "too_many_invalid_symbols"}
    replacements = []
    for i in bad:
        candidates = [code for code in CODES if (syms[i] ^ code).bit_count() == 1]
        if not candidates:
            return {**result, "status": "no_single_bit_candidate"}
        replacements.append(candidates)
    tries = 1
    for group in replacements:
        tries *= len(group)
    if tries > max_tries:
        return {**result, "status": "search_limit", "candidate_count": tries}
    recovered = []
    for choice in itertools.product(*replacements):
        trial = list(syms)
        for index, code in zip(bad, choice):
            trial[index] = code
        decoded = decode_nibbles(trial)
        if decoded is not None and validate_format_a(decoded):
            # Strip Format A CRCs for optional independent verification.
            recovered.append({"encoded_corrections": len(bad),
                              "wire_format_a_hex": decoded.hex().upper()})
    return {**result, "status": "crc_valid" if recovered else "crc_fail",
            "tried": tries, "crc_valid_candidates": recovered[:4],
            "valid_candidate_count": len(recovered)}


def parse_log(path: Path) -> list[tuple[str, bytes]]:
    found: dict[str, bytes] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = RAW_PATTERN.search(line)
        if match:
            # Replayed samples may appear repeatedly; dedupe by RAW contents.
            raw = bytes.fromhex(match.group(2))
            found[match.group(2).upper()] = raw
    return [(str(i), raw) for i, raw in enumerate(found.values(), 1)]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--log", type=Path, help="ESPHome log with RAW stored captures")
    source.add_argument("--raw", help="RAW 3-of-6 hexadecimal bytes, after 54 3D")
    parser.add_argument("--max-errors", type=int, default=2, choices=range(0, 4))
    parser.add_argument("--max-tries", type=int, default=128)
    args = parser.parse_args()
    entries = parse_log(args.log) if args.log else [("raw", bytes.fromhex(args.raw))]
    counts: Counter[str] = Counter()
    if not entries:
        print("No RAW stored captures found (reconnect logger to replay).")
        return
    for label, raw in entries:
        result = analyze(raw, args.max_errors, args.max_tries)
        counts[result["status"]] += 1
        print(f"sample={label} bytes={len(raw)} {result}")
    print("SUMMARY:", dict(counts))
    print("NOTE: only CRC-validated Format A telegrams count as recovered;")
    print("      candidate output is offline-only, never forwarded to meters.")


if __name__ == "__main__":
    main()
