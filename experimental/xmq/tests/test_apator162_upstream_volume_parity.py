#!/usr/bin/env python3
"""Compare host-only Apator162 XMQ IXML grammar decoder with upstream JSON.

Only original XMQ NOKEY already-clear manufacturer applications are eligible.
The 19th upstream encrypted fixture remains NOT_RUN, not a false PASS.
No personal captures/keys and no firmware integration.
"""
import json
import math
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT / "experimental/xmq/tools"))
from portable_xmq import parse, validate_driver, blocks, values

def fixtures():
    xmq = ROOT / "experimental/xmq/drivers/apator162.xmq"
    d = validate_driver(parse(xmq.read_text(encoding="utf-8")))
    return [entry["entries"] for group in blocks(d,"tests")
            for entry in blocks(group["entries"],"test")]

def main():
    if len(sys.argv)!=2: raise SystemExit("usage: script <compiled-cli>")
    binary = sys.argv[1]
    samples = fixtures()
    assert len(samples)==19
    passed=0
    for index,case in enumerate(samples,1):
        args = values(case,"args")[0]
        telegram = values(case,"telegram")[0]
        expected = json.loads(values(case,"json")[0])
        assert expected["driver"]=="apator162"
        if index==19:
            assert "NOKEY" not in args, "Expected keyed final fixture changed"
            # Encrypted application must never be treated as clear and published.
            frame = bytes.fromhex("".join(telegram.replace("|","").replace("_","").split()))
            assert frame[10]==0x7A
            payload = frame[15:]
            result = subprocess.run([binary,"apator162",payload.hex()],
                                    capture_output=True,text=True)
            assert result.returncode!=0 and not result.stdout
            print("UNSUPPORTED apator162 #19: encrypted; not decoded as clear")
            continue
        assert "NOKEY" in args
        frame = bytes.fromhex("".join(telegram.replace("|","").replace("_","").split()))
        assert frame[10]==0x7A, f"apator162 #{index}: unexpected TPL CI"
        # The original NOKEY fixture already supplies an unencrypted
        # manufacturer application after the 4-byte TPL-7A short header.
        # Skip only OMS 2F padding immediately preceding the 0F IXML data.
        offset=15
        while offset<len(frame) and frame[offset]==0x2F:
            offset+=1
        assert offset<len(frame) and frame[offset]==0x0F, (
            f"apator162 #{index}: unsupported manufacturer marker")
        result = subprocess.run([binary,"apator162",frame[offset:].hex()],
                                capture_output=True,text=True)
        if result.returncode!=0:
            raise AssertionError(
                f"apator162 #{index}: C++ grammar decoder rejected original "
                f"already-clear application (code {result.returncode})")
        output=result.stdout.strip().splitlines()
        assert len(output)==1 and output[0].startswith("total_m3="), (
            f"apator162 #{index}: wrong output contract")
        actual=float(output[0].partition("=")[2])
        reference=expected["total_m3"]
        assert math.isclose(actual,reference,rel_tol=0,abs_tol=1e-6), (
            f"apator162 #{index}: got {actual}, expected {reference}")
        passed+=1
        print(f"PARTIAL_PASS apator162 #{index}: total_m3")
    assert passed==18
    print("PASS Apator162 upstream clear-field parity: 18/18 original "
          "NOKEY applications total_m3; #19 encrypted NOT_RUN. "
          "No full-frame authentication or firmware authorization.")

if __name__=="__main__":
    main()
