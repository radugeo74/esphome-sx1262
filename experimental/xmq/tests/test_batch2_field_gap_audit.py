#!/usr/bin/env python3
"""Audit the EXACT original XMQ functional JSON fields vs separately tested
native C++ subcontracts. This is an inventory/coverage claim guard, not an
additional decode run and NOT full transport or complete-driver acceptance.

Covered sets must be backed by one of the already compiled C++ fixture tests.
Adding a new mapping here requires updating/running that test first.
Never count a deliberately invalid authenticated telegram as decoded output.
"""
import json
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/"experimental/xmq/tools"))
from portable_xmq import parse,validate_driver,blocks,values
META={"_","driver","name","id","media","timestamp",
      "current_status_deprecated_by"}
STATUS_OLD={"status","current_status","time_dry","time_reversed",
            "time_leaking","time_bursting"}
STATUS_MODERN={"status","time_dry","time_reversed","time_leaking",
               "time_bursting","time_flow_above_q4"}
NUMERIC_SUFFIXES=("_m3h","_c")
def source_cases(driver):
    p=ROOT/"experimental/xmq/drivers"/(driver+".xmq")
    obj=validate_driver(parse(p.read_text(encoding="utf-8")))
    return [json.loads(values(t["entries"],"json")[0])
            for g in blocks(obj,"tests")
            for t in blocks(g["entries"],"test")]

def tested_fields(driver,index,reference):
    # Contract: test_minomess_wired_upstream_parity.py + C++ wired adapter
    if driver=="minomess":
        if index in (1,2):
            return {"total_m3","target_m3","meter_date","target_date",
                    "status","rssi_dbm"} | (
                {"actuality_duration_s"} if index==2 else set())
        # test_batch2_clear_fixture_parity.py + C++ prefix decoder
        # #3 has no target_m3 JSON key; its negative/absence check in the
        # other test is useful, but cannot count as a source JSON field.
        return {"total_m3","meter_date","target_date","status"} | ({"target_m3"} if index!=3 else set())
    if driver=="apator162":
        # test_apator162_upstream_volume_parity.py covers #1..18.
        # test_apator162_upstream_cbc_mode5.cpp covers #19; its original
        # mode-5 CBC 2F2F plaintext marker is NOT authentication.
        return {"total_m3"}
    if driver!="kamwater":
        raise AssertionError("Unknown driver")
    if index==9:
        # test_kamwater_upstream_ccm_openssl.cpp REJECTS original bad tag.
        return set()
    fields=set()
    # test_batch2_clear_fixture_parity.py, compact variant of same.
    if index in (1,2,4,5,6,7,8):
        fields.add("total_m3")
        # Upstream Kamwater #4 has no target_m3 JSON key. An absence
        # check must not be reported as comparison of an existing field.
        if index!=4:
            fields.add("target_m3")
    # Original #3 volume is now compared directly by the clear-application
    # C++ CLI and original XMQ JSON in the primary fixture parity test.
    if index==3:
        fields.add("total_m3")
    # test_kamwater_upstream_ccm_openssl.cpp, original valid tag #10
    if index==10:
        fields.update(("total_m3","target_m3","accoustic_noise"))
    # Source Kamwater #3 now has 6 strict old-style status/duration field
    # comparisons against the SAME native status_semantics C++ CLI as #1.
    # The historical 46 88 78 clear-prefix is stripped only in the test.
    if index==3:
        fields.update(STATUS_OLD)
    # test_kamwater_status_upstream_parity.py
    if index==1:
        fields.update(STATUS_OLD)
    if index in (5,6,11,12):
        fields.update(STATUS_MODERN)
        fields.add("time_high_ambient_temperature" if index==12
                   else "time_low_ambient_temperature")
    # test_kamwater_numeric_upstream_parity.py matches exact output keys
    if index<=8:
        fields.update(key for key in reference if
                      key.endswith(NUMERIC_SUFFIXES) or
                      key in {"target_date","accoustic_noise"})
    # test_kamwater_tpl_status_upstream_parity.py
    if index in (13,14):
        fields.add("status")
    if index==14:
        fields.update(STATUS_MODERN)
        fields.add("time_low_ambient_temperature")
    return fields

def main():
    output={}
    total_expected=0
    total_compared=0
    positives=0
    for driver,case_count in (("minomess",6),("kamwater",14),
                              ("apator162",19)):
        cases=source_cases(driver)
        assert len(cases)==case_count
        rows=[]
        for index,reference in enumerate(cases,1):
            expected=set(reference)-META
            compared=tested_fields(driver,index,reference)
            # Fail if mapped tests no longer have expected source fields.
            ungrounded=compared-expected
            if ungrounded:
                raise AssertionError(
                    f"{driver} #{index}: nonexistent tested fields {ungrounded}")
            missing=expected-compared
            if compared: positives+=1
            total_expected+=len(expected)
            total_compared+=len(compared)
            rows.append({"case":index,
                         "expected":len(expected),
                         "compared_partial":len(compared),
                         "not_compared":sorted(missing),
                         "negative_security_only":driver=="kamwater"
                                                   and index==9})
        output[driver]=rows
    assert sum(map(len,output.values()))==39
    # Snapshot of *unique functional fields* derived from XMQ expected JSON.
    # NOT the cumulative number of assertions across overlapping test suites.
    assert (total_expected,total_compared,positives)==(181,150,38)
    assert output["kamwater"][8]["compared_partial"]==0
    assert output["kamwater"][8]["negative_security_only"]
    assert output["kamwater"][8]["not_compared"]==[
        "accoustic_noise","status","target_m3","total_m3"]
    summary={"source_examples":39,"positive_partial_examples":positives,
             "negative_only_examples":1,"functional_fields":total_expected,
             "unique_fields_compared":total_compared,
             "unique_fields_not_compared":total_expected-total_compared,
             "full_driver_parity_approved":False,
             "radio_or_firmware_enablement_approved":False,
             "drivers":output}
    if "--json" in sys.argv:
        print(json.dumps(summary,indent=2,sort_keys=True))
    else:
        print("BATCH2 FIELD AUDIT: 39 original fixtures, 38 with positive "
              "partial field evidence, one negative-only security fixture")
        for driver,rows in output.items():
            expected=sum(row["expected"] for row in rows)
            compared=sum(row["compared_partial"] for row in rows)
            print(f"{driver}: {compared}/{expected} unique functional "
                  "JSON fields with partial C++ comparisons")
            for row in rows:
                if row["not_compared"]:
                    print(f"  #{row['case']}: NOT_COMPARED "
                          f"{','.join(row['not_compared'])}")
        print(f"TOTAL {total_compared}/{total_expected} unique functional "
              "fields compared; 0 FULL DRIVER PARITY approvals. "
              "Firmware and RF publication remain BLOCKED.")
if __name__=="__main__":
    main()
