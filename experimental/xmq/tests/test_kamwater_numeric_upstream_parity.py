#!/usr/bin/env python3
"""Stage B: compare actual Kamwater C++ numeric fields with original JSON.

Uses original NOKEY already-clear applications; compact case #7 requires the
preceding, separately validated full application (#6) as TPL79 CRC reference.
No live RF, user meters, encryption keys or firmware dispatch.
"""
import json
import math
import subprocess
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/"experimental/xmq/tools"))
from portable_xmq import parse,validate_driver,blocks,values
FULL=(1,3,5,6)
COMPACT=(2,4,7,8)
def run(command,number):
    result=subprocess.run(command,capture_output=True,text=True)
    if result.returncode:
        raise AssertionError(
            f"kamwater #{number}: native numeric decoder returned "
            f"{result.returncode}; {result.stderr[:200]}")
    actual={}
    for line in result.stdout.splitlines():
        field,sep,value=line.partition("=")
        if not sep or field in actual:
            raise AssertionError(f"kamwater #{number}: malformed native output")
        actual[field]=value if field in ("target_date","accoustic_noise") else float(value)
    return actual

def main():
    if len(sys.argv)!=2: raise SystemExit("Expected C++ CLI binary path")
    data=(ROOT/"experimental/xmq/drivers/kamwater.xmq").read_text(encoding="utf-8")
    d=validate_driver(parse(data))
    cases=[t["entries"] for group in blocks(d,"tests")
           for t in blocks(group["entries"],"test")]
    assert len(cases)==14
    comparisons=0
    for index in FULL+COMPACT:
        case=cases[index-1]
        assert "NOKEY" in values(case,"args")[0]
        telegram=values(case,"telegram")[0]
        raw=bytes.fromhex("".join(telegram.replace("|","").replace("_","").split()))
        expected=json.loads(values(case,"json")[0])
        if index in FULL:
            assert telegram.count("_")==1
            application=telegram.split("_",1)[1].replace("|","")
            command=[sys.argv[1],"kamwater_numeric",application]
        else:
            assert raw[10]==0x8D and raw[19]==0x79
            command=[sys.argv[1],"kamwater_numeric_compact",raw[19:].hex()]
            if index==7:
                reference=values(cases[5],"telegram")[0]
                assert reference.count("_")==1
                prior_full=bytes.fromhex(reference.split("_",1)[1])
                command=[sys.argv[1],"kamwater_numeric_compact_ref",
                         raw[19:].hex(),prior_full.hex()]
        actual=run(command,index)
        wanted={k:v for k,v in expected.items() if
                k.endswith("_m3h") or k.endswith("_c") or\n                k in ("target_date","accoustic_noise")}
        if not wanted:
            raise AssertionError(f"kamwater #{index}: no numeric expected fields")
        if set(actual)!=set(wanted):
            raise AssertionError(
                f"kamwater #{index}: extra/missing actual fields "
                f"{sorted(set(actual)^set(wanted))}")
        for field,reference in wanted.items():
            measured=actual[field]
            if field in ("target_date","accoustic_noise"):
                assert measured==reference, (
                    f"kamwater #{index}: {field} mismatch")
            else:
                assert isinstance(measured,float) and math.isclose(
                    measured,reference,rel_tol=0,abs_tol=1e-6), (
                    f"kamwater #{index}: {field} native={measured} "
                    f"reference={reference}")
            comparisons+=1
        print(f"PARTIAL_PASS kamwater #{index}: {len(wanted)} "
              "numeric/date fields from original JSON")
    assert comparisons==34, f"unexpected comparison count {comparisons}"
    print("PASS Kamwater numeric subset: 8 original applications, "
          "34 real C++/XMQ comparisons. No radio/full-output parity.")

if __name__=="__main__":main()
