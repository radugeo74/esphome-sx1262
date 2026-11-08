#!/usr/bin/env python3
"""Fail CI on security evidence catalog drift; never infer model-wide RF key policy."""
from pathlib import Path
import json
import re
import sys

ROOT=Path(__file__).resolve().parents[3]
BASE=ROOT/"experimental/xmq"
sys.path.insert(0,str(BASE/"tools"))
from native_driver_specs import GENERATED, PORTABLE, CONFIGURED_NATIVE, SECURE_ONLY

GROUPS={
    "GENERATED":{d.name for d in GENERATED},
    "PORTABLE":set(PORTABLE),
    "CONFIGURED_NATIVE":set(CONFIGURED_NATIVE),
    "SECURE_ONLY":set(SECURE_ONLY),
}
ROUTES={
    "minomess":["TPL_AES_CBC_MODE5"],
    "kamwater":["ELL_AES_CTR","TPL_AES_CCM_MODE10"],
    "apator162":["TPL_AES_CBC_MODE5"],
    "mkradio4a":["TPL_AES_CBC_MODE5"],
    "topaseskr":["TPL_AES_CBC_MODE5"],
    "lansenth":["TPL_AES_CBC_MODE5"],
    "lansensm":["TPL_AES_CBC_MODE5"],
}
CLEAR={
    "GENERATED":"manufacturer_specific",
    "PORTABLE":"clear_only",
    "CONFIGURED_NATIVE":"configured_clear",
    "SECURE_ONLY":"none",
}
MVT=re.compile(r"\bmvt\s*=\s*([A-Z]{3})\s*,\s*([0-9a-fA-F]{2})\s*,\s*([0-9a-fA-F]{2})")
FIXTURE=re.compile(r"\bargs\s*=\s*'([^']+)'[\s\S]*?\btelegram\s*=\s*([a-fA-F0-9_]+)")

def verify():
    data=json.loads((BASE/"security_profile_catalog.json").read_text(encoding="utf-8"))
    doc=(BASE/"SECURITY_PROFILE_CATALOG.md").read_text(encoding="utf-8")
    assert data["schema_version"]==1
    assert data["oms_catalog"]=="https://oms-group.org/en/certified-products/"
    records=data["drivers"]
    expected=set().union(*GROUPS.values())
    assert len(records)==len(expected)==32
    assert {r["driver"] for r in records}==expected
    assert len({r["driver"] for r in records})==32
    doc_rows=re.findall(r"^\| \[\x60([a-z0-9]+)\x60\]\(drivers/[^)]+\.xmq\) \|",doc,re.M)
    assert len(doc_rows)==32 and set(doc_rows)==expected,doc_rows
    total=0
    for record in records:
        name=record["driver"]
        assert set(record)=={"driver","group","mvt","total_tests","nokey_tests",
                            "clear_route","keyed_routes","oms_certified_sku","evidence_note"}
        group=record["group"]
        assert group in GROUPS and name in GROUPS[group],name
        assert record["clear_route"]==CLEAR[group],name
        assert record["keyed_routes"]==ROUTES.get(name,[]),name
        assert record["oms_certified_sku"] is None, (
            "Exact certified SKU and certificate evidence required before setting OMS match",name)
        assert record["evidence_note"] and "|" not in record["evidence_note"]
        source=(BASE/"drivers"/f"{name}.xmq").read_text(encoding="utf-8")
        sigs=[f"{m[1]}/{m[2].upper()}/{m[3].upper()}" for m in MVT.finditer(source)]
        cases=list(FIXTURE.finditer(source))
        assert sigs and cases,name
        no_key=sum("NOKEY" in m[1].split() for m in cases)
        assert record["mvt"]==sigs,(name,"MVT changed")
        assert record["total_tests"]==len(cases),(name,"XMQ fixtures changed")
        assert record["nokey_tests"]==no_key,(name,"NOKEY fixture count changed")
        row=next(l for l in doc.splitlines() if l.startswith(f"| [\x60{name}\x60]"))
        assert record["group"] in row and f"{len(cases)} ({no_key} \x60NOKEY\x60)" in row,name
        assert all(f"\x60{sig}\x60" in row for sig in sigs),name
        assert record["evidence_note"] in doc,name
        assert not any(x in record for x in ("always_unencrypted",
                   "always_encrypted","keyless_on_air","model_wide_requires_key")),name
        total+=len(cases)
    src=(ROOT/"components/wmbus_native_sx126x/xmq_batch2_secure_dispatch.h").read_text()
    for token in ("decode_minomess_mode5(","decode_apator162_mode5(",
                  "decode_mkradio4a_mode5(","decode_topaseskr_mode5(",
                  "decode_lansen_mode5(",
                  "kamwater::ell_ctr_mbedtls::decrypt_ell_ii(","oms_ccm_idf_probe"):
        assert token in src,token
    security=(ROOT/"components/wmbus_native_sx126x/tpl_security_probe.h").read_text()
    assert "& 0x1F" in security and "encrypted_blocks == 0" in security
    assert "Mode 7" in doc and "Mode 13" in doc and "NOKEY" in doc
    print(f"PASS security catalog: {len(records)} drivers, {total} upstream XMQ fixtures, "
          "MVT/capability parity and no unverified model-wide key or OMS certificate claims")

if __name__=="__main__":
    verify()
