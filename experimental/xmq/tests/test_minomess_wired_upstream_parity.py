#!/usr/bin/env python3
"""Compare complete wired M-Bus Minomess #1/#2 with upstream XMQ JSON.

C++ decoder verifies M-Bus 68 frame length/checksum and uses the existing
Minomess clear application. Gateway RSSI/actuality remain labelled as such.
Not a wireless T1/C1 RF acceptance test.
"""
import json
import math
import subprocess
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/"experimental/xmq/tools"))
from portable_xmq import parse,validate_driver,blocks,values

FIELDS=("total_m3","target_m3","meter_date","target_date","status","rssi_dbm")
def decode(binary,frame,configured_id=None):
    args=[binary,"minomess_wired_configured" if configured_id is not None
          else "minomess_wired",frame.hex()]
    if configured_id is not None:
        args.append(configured_id)
    return subprocess.run(args,capture_output=True,text=True)

def check_bad(binary,frame,configured_id=None):
    result=decode(binary,frame,configured_id)
    assert result.returncode!=0 and not result.stdout, (
        "invalid wired frame unexpectedly accepted")

def main():
    if len(sys.argv)!=2: raise SystemExit("expected compiled native CLI")
    d=validate_driver(parse(
        (ROOT/"experimental/xmq/drivers/minomess.xmq").read_text(
            encoding="utf-8")))
    entries=[t["entries"] for g in blocks(d,"tests")
             for t in blocks(g["entries"],"test")]
    assert len(entries)==6
    comparisons=0
    for index in (1,2):
        entry=entries[index-1]
        assert "NOKEY" in values(entry,"args")[0]
        telegram=values(entry,"telegram")[0]
        assert "_" not in telegram
        wire=bytearray(bytes.fromhex(telegram))
        expected=json.loads(values(entry,"json")[0])
        configured_id=expected["id"]
        assert isinstance(configured_id,str) and len(configured_id)==8
        assert configured_id.isdecimal()
        result=decode(sys.argv[1],wire,configured_id)
        if result.returncode:
            raise AssertionError(
                f"minomess #{index}: C++ M-Bus decoder rejected original "
                f"complete frame, exit {result.returncode}")
        actual={}
        for line in result.stdout.splitlines():
            key,sep,val=line.partition("=")
            assert sep and key not in actual
            actual[key]=val
        fields=FIELDS+(("actuality_duration_s",) if index==2 else ())
        assert set(actual)==set(fields), (
            f"minomess #{index}: missing or unexpected output {sorted(actual)}")
        for key in fields:
            reference=expected[key]
            value=actual[key]
            if isinstance(reference,float):
                assert math.isclose(float(value),reference,rel_tol=0,
                                    abs_tol=1e-6), (
                    f"minomess #{index} {key}: numeric mismatch")
            else:
                assert str(reference)==value, (
                    f"minomess #{index} {key}: value mismatch")
            comparisons+=1
        # Independent configured identity is mandatory, including leading
        # zeroes; the public wire address is not a YAML authorization.
        wrong_id=("0" if configured_id[0]!="0" else "1")+configured_id[1:]
        check_bad(sys.argv[1],wire,wrong_id)
        check_bad(sys.argv[1],wire,configured_id[:-1])
        check_bad(sys.argv[1],wire,configured_id+"0")
        damaged=wire.copy()
        damaged[7]^=1  # changed meter ID, recomputed valid wired checksum
        damaged[-2]=sum(damaged[4:-2])&255
        check_bad(sys.argv[1],damaged,configured_id)
        # Length and checksum are independently verified, not assumed.
        assert wire[0]==wire[3]==0x68 and wire[-1]==0x16
        assert wire[1]==wire[2]==len(wire)-6
        assert sum(wire[4:-2])&255==wire[-2]
        damaged=wire.copy()
        damaged[22]^=1  # application mutation, checksum untouched
        check_bad(sys.argv[1],damaged)
        damaged=wire.copy()
        damaged[1]^=1
        check_bad(sys.argv[1],damaged)
        damaged=wire.copy()
        damaged[-1]=0
        check_bad(sys.argv[1],damaged)
        damaged=wire.copy()
        damaged[11]^=1  # wrong manufacturer, re-sum envelope
        damaged[-2]=sum(damaged[4:-2])&255
        check_bad(sys.argv[1],damaged)
        # Changing gateway RSSI while recomputing checksum must NOT be
        # interpreted as meter-owned; test only the correct original.
        print(f"PARTIAL_PASS minomess wired #{index}: "
              f"{len(fields)} original JSON fields and M-Bus integrity")
    assert comparisons==13
    print("PASS original Minomess wired #1/#2: 13 JSON fields, "
          "length/checksum/meter identity negative checks. "
          "No wireless T1/C1 authorization.")

if __name__=="__main__":main()
