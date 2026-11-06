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
REFERENCE_CASE=7
# The TPL79 data CRC for #7 references the immediately preceding full
# telegram #6, whose measurement values differ from the current compact.
# A reference frame must be separately validated before being trusted in
# production; the original NOKEY application is a host-only parity fixture.
EXPECTED_CRC=0x8054
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
        command=[sys.argv[1],"kamwater_compact",wire[19:].hex()]
        if number==REFERENCE_CASE:
            previous_full=values(entries[5],"telegram")[0]
            assert previous_full.count("_")==1
            previous_application=bytes.fromhex(previous_full.split("_",1)[1])
            assert wire[22] | (wire[23]<<8)==EXPECTED_CRC
            # Regression proof: old current-record CRC interpretation
            # rejects, but the actual referenced-full-frame CRC matches.
            old_check=subprocess.run(command,capture_output=True,text=True)
            assert old_check.returncode==4 and not old_check.stdout
            command=[sys.argv[1],"kamwater_compact_ref",
                     wire[19:].hex(),previous_application.hex()]
        result=subprocess.run(command,capture_output=True,text=True)
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
    # Test the SAME compiled C++ compact expansion and status parser on
    # three original legacy-status compact fixtures. Unknown formats,
    # reconstructed CRC failures or malformed records remain fail-closed.
    status_fields=("status","current_status","time_dry","time_reversed",
                   "time_leaking","time_bursting")
    status_checks=0
    for number in (2,4,8):
        entry=entries[number-1]
        assert "NOKEY" in values(entry,"args")[0]
        telegram=values(entry,"telegram")[0]
        wire=bytes.fromhex("".join(telegram.replace("|","").replace("_","").split()))
        assert wire[10]==0x8D and wire[19]==0x79
        expected=json.loads(values(entry,"json")[0])
        command=[sys.argv[1],"kamwater_compact_status",wire[19:].hex()]
        result=subprocess.run(command,capture_output=True,text=True)
        assert result.returncode==0, (
            f"kamwater #{number}: native compact status rejected original "
            f"fixture (exit {result.returncode})")
        actual={}
        for line in result.stdout.splitlines():
            key,sep,value=line.partition("=")
            assert sep and key not in actual
            actual[key]=value
        assert set(actual)==set(status_fields), (
            f"kamwater #{number}: unexpected compact status output")
        for field in status_fields:
            assert actual[field]==expected[field], (
                f"kamwater #{number} {field}: compact native "
                f"{actual[field]!r} != source {expected[field]!r}")
            status_checks+=1
        # A changed reconstructed data CRC must reject before parsing.
        altered=bytearray(wire[19:])
        altered[3]^=1
        rejected=subprocess.run(
            [sys.argv[1],"kamwater_compact_status",altered.hex()],
            capture_output=True,text=True)
        assert rejected.returncode!=0 and not rejected.stdout
        print(f"PARTIAL_PASS kamwater #{number}: 6 source compact "
              "status/duration fields with TPL79 CRC gate")
    assert status_checks==18
    assert checks==8
    print("PASS compact subset: 4 original TPL79 volume applications "
          "and 3 status applications; 26 original JSON field comparisons. "
          "#7 verified against preceding full application #6. "
          "Not authenticated RF/full parity.")
if __name__=="__main__":
    main()
