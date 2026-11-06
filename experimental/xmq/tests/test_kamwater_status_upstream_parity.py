#!/usr/bin/env python3
"""Stage B: compare original Kamwater status/duration JSON to actual C++.
Upstream supplies already-clear NOKEY applications; not RF security parity.
"""
import json
import subprocess
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/"experimental/xmq/tools"))
from portable_xmq import parse,validate_driver,blocks,values
INDEXES=(1,3,5,6,11,12)
OLD=("status","current_status","time_dry","time_reversed",
     "time_leaking","time_bursting")
MODERN=("status","time_dry","time_reversed","time_leaking",
        "time_bursting","time_flow_above_q4")
def main():
    if len(sys.argv)!=2: raise SystemExit("expected C++ CLI binary path")
    source=(ROOT/"experimental/xmq/drivers/kamwater.xmq").read_text(encoding="utf-8")
    d=validate_driver(parse(source))
    cases=[t["entries"] for g in blocks(d,"tests")
           for t in blocks(g["entries"],"test")]
    assert len(cases)==14
    total=0
    for i in INDEXES:
        case=cases[i-1]
        assert "NOKEY" in values(case,"args")[0]
        telegram=values(case,"telegram")[0]
        application="".join(telegram.split("_")[-1].replace("|","").split())
        if i==3:
            # Original Kamwater #3 carries the reviewed clear-application
            # prefix 46 88 78 ahead of the standard DIF/VIF records.
            # Do not normalize arbitrary prefixes or relax the RF parser.
            assert application.startswith("468878")
            application=application[6:]
        expected=json.loads(values(case,"json")[0])
        result=subprocess.run([sys.argv[1],"kamwater_status",application],
                              capture_output=True,text=True)
        if result.returncode:
            raise AssertionError(f"kamwater #{i}: status parser rejected "
                                 f"original clear application (exit {result.returncode})")
        actual={}
        for line in result.stdout.splitlines():
            key,sep,val=line.partition("=")
            assert sep and key not in actual
            actual[key]=val
        fields=OLD if i in (1,3) else MODERN+(
            ("time_high_ambient_temperature",) if i==12
            else ("time_low_ambient_temperature",))
        assert set(actual)==set(fields), (
            f"kamwater #{i}: unexpected status field shape {sorted(actual)}")
        for field in fields:
            assert field in expected, f"kamwater #{i}: upstream field {field} missing"
            assert actual[field]==expected[field], (
                f"kamwater #{i}: {field} differs "
                f"native={actual[field]!r} upstream={expected[field]!r}")
            total+=1
        print(f"PARTIAL_PASS kamwater #{i}: {len(fields)} original "
              "status/duration fields")
    assert total==40
    print(f"PASS 6 Kamwater original status/duration applications; 
          f"{total} extra JSON field comparisons. No RF publication.")
if __name__=="__main__": main()
