#!/usr/bin/env python3
"""Compare actual C++ TPL79 compact expansion and DIF/VIF volumes with
original Kamwater XMQ post-decrypted examples. No raw radio authentication.
"""
import json
import math
import subprocess
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/"experimental/xmq/tools"))
from portable_xmq import parse,validate_driver,blocks,values
# These official NOKEY examples have an ELL II header and TPL 79 at
# fixed, protocol-derived offset 10 DLL + 9 ELL bytes (not magic scan).
INDEXES=(2,4,7,8)
def main():
    if len(sys.argv)!=2: raise SystemExit("expected path to compiled C++ decoder")
    tree=validate_driver(parse(
        (ROOT/"experimental/xmq/drivers/kamwater.xmq").read_text(encoding="utf-8")))
    entries=[t["entries"] for group in blocks(tree,"tests")
             for t in blocks(group["entries"],"test")]
    assert len(entries)==14
    checks=0
    for number in INDEXES:
        entry=entries[number-1]
        assert "NOKEY" in values(entry,"args")[0]
        wire=bytes.fromhex("".join(values(entry,"telegram")[0].
                                   replace("|","").replace("_","").split()))
        assert wire[10]==0x8D and wire[19]==0x79, (
            f"kamwater #{number}: changed ELL/TPL structure")
        expected=json.loads(values(entry,"json")[0])
        result=subprocess.run(
            [sys.argv[1],"kamwater_compact",wire[19:].hex()],
            capture_output=True,text=True)
        if result.returncode:
            raise AssertionError(
                f"kamwater #{number}: existing C++ TPL79/DIFVIF rejected "
                f"original post-decrypted application (exit {result.returncode})")
        actual={}
        for line in result.stdout.splitlines():
            field,separator,value=line.partition("=")
            assert separator and field not in actual
            assert field in ("total_m3","target_m3")
            actual[field]=None if value=="null" else float(value)
        assert set(actual)=={"total_m3","target_m3"}
        for field in ("total_m3","target_m3"):
            reference=expected.get(field)
            if reference is None:
                assert actual[field] is None, (
                    f"kamwater #{number}: {field} should be absent/null")
            else:
                assert actual[field] is not None and math.isclose(
                    actual[field],reference,rel_tol=0,abs_tol=1e-6), (
                    f"kamwater #{number}: {field} differs")
            checks+=1
        print(f"PARTIAL_PASS kamwater #{number}: compact TPL79 "
              "total_m3 / target_m3")
    assert checks==8
    print("PASS compact subset: 4 original TPL79 applications, "
          "8 JSON field comparisons. Not authenticated RF/full parity.")
if __name__=="__main__":
    main()
