#!/usr/bin/env python3
"""Check ORIGINAL Kamwater #13/#14 TPL error flag propagation in C++.

Only the explicitly demonstrated unencrypted short TPL7A status byte 02
is recognized. No admission of a radio telegram and no sensor output.
"""
import json
import subprocess
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/"experimental/xmq/tools"))
from portable_xmq import parse,validate_driver,blocks,values

def invoke(binary,clear):
    return subprocess.run([binary,"kamwater_tpl_status",clear.hex()],
                          capture_output=True,text=True)

def main():
    if len(sys.argv)!=2: raise SystemExit("expected compiled native CLI")
    path=ROOT/"experimental/xmq/drivers/kamwater.xmq"
    parsed=validate_driver(parse(path.read_text(encoding="utf-8")))
    entries=[t["entries"] for g in blocks(parsed,"tests")
             for t in blocks(g["entries"],"test")]
    assert len(entries)==14
    checks=0
    for index in (13,14):
        e=entries[index-1]
        assert "NOKEY" in values(e,"args")[0]
        chunks=values(e,"telegram")[0].split("_")
        assert len(chunks)==4
        tpl=bytes.fromhex(chunks[2]+chunks[3])
        assert tpl[:5]==bytes.fromhex("7a44020000")
        reference=json.loads(values(e,"json")[0])
        result=invoke(sys.argv[1],tpl)
        assert result.returncode==0, (
            f"kamwater #{index}: native TPL status parser rejected "
            f"original fixture (exit {result.returncode})")
        actual={}
        for line in result.stdout.splitlines():
            key,sep,value=line.partition("=")
            assert sep and key not in actual
            actual[key]=value
        expected_keys=("status",) if index==13 else (
            "status","time_dry","time_reversed","time_leaking",
            "time_bursting","time_flow_above_q4",
            "time_low_ambient_temperature")
        assert set(actual)==set(expected_keys), (
            f"kamwater #{index}: incorrect output fields {sorted(actual)}")
        for key in expected_keys:
            assert actual[key]==reference[key], (
                f"kamwater #{index}: {key} differed from original JSON")
            checks+=1
        if index==13:
            # Source #13 contains a storage-one target volume, but no
            # current total. Reuse the bounded shared DIF/VIF parser after
            # the reviewed, clear short-TPL ERROR classification.
            volume=subprocess.run(
                [sys.argv[1],"kamwater_tpl_target",tpl.hex()],
                capture_output=True,text=True)
            assert volume.returncode==0, (
                "kamwater #13: target-only volume rejected")
            assert volume.stdout.strip()==(
                f"target_m3={reference['target_m3']:.9f}"), (
                "kamwater #13 target_m3 differs from original source")
            checks+=1
            bad_volume=bytearray(tpl)
            bad_volume.pop()  # malformed four-byte target, not a new value
            rejected=subprocess.run(
                [sys.argv[1],"kamwater_tpl_target",bad_volume.hex()],
                capture_output=True,text=True)
            assert rejected.returncode!=0 and not rejected.stdout
        # Explicit regression: other TPL status values, changed config,
        # wrong CI and truncated payloads are never treated as ERROR.
        for bad in (
            bytes([tpl[0],tpl[1],0x00])+tpl[3:],
            tpl[:3]+b"\x01"+tpl[4:],
            b"\x78"+tpl[1:],
            tpl[:5],
        ):
            response=invoke(sys.argv[1],bad)
            assert response.returncode!=0 and not response.stdout, (
                f"kamwater #{index}: unexpected acceptance of invalid TPL")
        print(f"PARTIAL_PASS kamwater #{index}: "
              f"{len(expected_keys)} original TPL status/alarms")
    assert checks==9
    print("PASS Kamwater #13/#14 TPL partial JSON parity: "
          "9 fields (including #13 target-only volume); malformed "
          "configs fail closed; not RF parity.")

if __name__=="__main__":
    main()
