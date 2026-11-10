#!/usr/bin/env python3
"""Compile source-backed Lansen Pulse C++ record selectors and status text.

Genuine XMQ source compilation: record keys, output names, manufacturer
status bit and fixture-approved text statuses are extracted, not duplicated
as manually maintained C++ tables. Security and RF gates are independent.
Unsupported record shapes, TPL changes or semantics fail closed.
"""
import json
from pathlib import Path

from portable_xmq import parse, validate_driver, blocks, values


def build(path: Path):
    driver=validate_driver(parse(path.read_text(encoding="utf-8")))
    if path.stem!="lansenpu" or values(driver,"name")!=["lansenpu"]:
        raise ValueError("lansenpu: wrong XMQ driver")
    fields=[f["entries"] for group in blocks(driver,"fields")
            for f in blocks(group["entries"],"field")]
    if len(fields)!=3:
        raise ValueError("lansenpu: changed field inventory")
    status=fields[0]
    if (values(status,"name")!=["status"] or
            values(status,"quantity")!=["Flags"] or
            values(status,"attributes")!=["STATUS,INCLUDE_TPL_STATUS"]):
        raise ValueError("lansenpu: changed XMQ status contract")
    records=[]
    for field in fields[1:]:
        name=values(field,"name")
        quantity=values(field,"quantity")
        matches=blocks(field,"match")
        if (len(name)!=1 or len(matches)!=1 or
                quantity!=["Dimensionless"] or values(field,"calculate")):
            raise ValueError("lansenpu: unknown pulse record formula")
        match=matches[0]["entries"]
        key=values(match,"difvifkey")
        if (len(key)!=1 or len(match)!=1 or
                len(bytes.fromhex(key[0])) not in (3,4)):
            raise ValueError("lansenpu: unsupported record selector")
        raw=bytes.fromhex(key[0])
        if raw[0]&0x0F!=0x0E:
            raise ValueError("lansenpu: only 12-digit BCD DIF supported")
        records.append((name[0]+"_counter",raw,6))
    if len({name for name,_,_ in records})!=2:
        raise ValueError("lansenpu: ambiguous output fields")
    status_bits=blocks(driver,"mfct_tpl_status_bits")
    if len(status_bits)!=1:
        raise ValueError("lansenpu: missing manufacturer status")
    s=status_bits[0]["entries"]
    maps=blocks(s,"map")
    flags=blocks(driver,"flags")
    if (values(s,"mask_bits")!=["0xe0"] or
            values(s,"default_message")!=["OK"] or len(maps)!=1 or
            values(maps[0]["entries"],"name")!=["SABOTAGE_ENCLOSURE"] or
            values(maps[0]["entries"],"test")!=["set"] or
            len(flags)!=1):
        raise ValueError("lansenpu: unreviewed manufacturer error mapping")
    raw_mask=int(values(maps[0]["entries"],"value")[0],0)
    if raw_mask==0 or raw_mask&0xE0!=raw_mask:
        raise ValueError("lansenpu: unsupported manufacturer status mask")
    original_tests=blocks(driver,"tests")
    if len(original_tests)!=1:
        raise ValueError("lansenpu: missing fixture tests")
    samples=blocks(original_tests[0]["entries"],"test")
    if not samples:
        raise ValueError("lansenpu: missing fixture statuses")
    statuses={}
    seen_records=set()
    for sample in samples:
        t=sample["entries"]
        telegram, results, args=(values(t,key) for key in
                                 ("telegram","json","args"))
        if len(telegram)!=1 or len(results)!=1 or len(args)!=1:
            raise ValueError("lansenpu: incomplete fixture")
        frame=bytes.fromhex(telegram[0].replace("_","").replace("|",""))
        expected=json.loads(results[0])
        if len(frame)!=frame[0]+1 or frame[10]!=0x7A:
            raise ValueError("lansenpu: invalid original transport sample")
        meter_id="".join(f"{b:02X}" for b in reversed(frame[4:8]))
        if (expected.get("driver")!="lansenpu" or
                expected.get("id")!=meter_id or
                args[0].split()!=[
                    expected.get("name"),"lansenpu",meter_id,"NOKEY"
                ]):
            raise ValueError("lansenpu: unexpected source ID/driver")
        status_byte=frame[12]
        expected_text=expected.get("status")
        if not isinstance(expected_text,str) or not expected_text:
            raise ValueError("lansenpu: missing status")
        parts=[]
        if status_byte&0x08:parts.append("PERMANENT_ERROR")
        if status_byte&0x04:parts.append("POWER_LOW")
        if status_byte&raw_mask:parts.append("SABOTAGE_ENCLOSURE")
        if status_byte & ~(0x08|0x04|raw_mask):
            raise ValueError("lansenpu: unknown status semantics")
        if expected_text!=(" ".join(parts) if parts else "OK"):
            raise ValueError("lansenpu: source status text mismatch")
        if status_byte in statuses and statuses[status_byte]!=expected_text:
            raise ValueError("lansenpu: conflicting original status")
        statuses[status_byte]=expected_text
        data=frame[15:]
        pos=0
        actual={}
        while pos<len(data):
            if data[pos]==0x2F:
                pos+=1
                continue
            choices=[(name,raw,width) for name,raw,width in records
                     if data[pos:pos+len(raw)]==raw]
            if len(choices)!=1:
                raise ValueError("lansenpu: unrecognized DIF/VIF fixture")
            name,raw,width=choices[0]
            pos+=len(raw)
            if name in actual or pos+width>len(data):
                raise ValueError("lansenpu: duplicate/truncated source counter")
            value=data[pos:pos+width]
            count=0
            for byte in reversed(value):
                lo,hi=byte&15,byte>>4
                if lo>9 or hi>9:
                    raise ValueError("lansenpu: non-BCD count in source")
                count=count*100+hi*10+lo
            actual[name]=count
            seen_records.add(name)
            pos+=width
        keys={"_","media","driver","name","id","status","timestamp"}|set(actual)
        if set(expected)!=keys or any(expected[k]!=v for k,v in actual.items()):
            raise ValueError("lansenpu: source JSON parity mismatch")
    if seen_records!={name for name,_,_ in records}:
        raise ValueError("lansenpu: untested pulse field")
    return records,statuses,raw_mask


def generate(driver_dir: Path):
    rec,states,sabotage=build(driver_dir/"lansenpu.xmq")
    output=[
        "// Generated from original Lansen Pulse XMQ declarations and fixtures.",
        "// Only source-verified TPL statuses; no implicit RF/decryption claims.",
        "#pragma once",
        "#include <cstddef>",
        "#include <cstdint>",
        "namespace xmq_generated_pulse {",
        "struct Field {const char*name;uint8_t key[4];uint8_t key_len;uint8_t bcd_bytes;};",
        "struct KnownStatus {uint8_t raw;const char*text;};",
        "namespace lansenpu {",
        "inline constexpr Field fields[]={",
    ]
    for name,raw,span in rec:
        encoded=",".join(f"0x{x:02X}" for x in list(raw)+[0]*(4-len(raw)))
        output.append(f'  {{"{name}",{{{encoded}}},{len(raw)},{span}}},')
    output.extend([
        "};",
        f"inline constexpr uint8_t sabotage_bit=0x{sabotage:02X};",
        "inline constexpr KnownStatus statuses[]={",
    ])
    for status,string in sorted(states.items()):
        output.append(f'  {{0x{status:02X},{json.dumps(string)}}},')
    output.extend([
        "};",
        "inline const char*status_text(uint8_t raw){",
        " for(const auto&item:statuses)if(item.raw==raw)return item.text;",
        " return nullptr;}",
        "inline int find_field(const uint8_t*data,size_t n){",
        " if(!data)return -1;",
        " int result=-1;",
        " for(size_t i=0;i<sizeof(fields)/sizeof(fields[0]);++i){",
        "  const Field&f=fields[i];",
        "  if(n<f.key_len+f.bcd_bytes)continue;",
        "  bool match=true;",
        "  for(size_t j=0;j<f.key_len;++j)if(data[j]!=f.key[j])match=false;",
        "  if(match){if(result>=0)return -2;result=static_cast<int>(i);}",
        " }",
        " return result;}",
        "}",
        "}",
    ])
    return "\n".join(output)+"\n"


if __name__=="__main__":
    import argparse
    p=argparse.ArgumentParser()
    p.add_argument("--drivers",type=Path,required=True)
    p.add_argument("--out",type=Path,required=True)
    a=p.parse_args()
    a.out.write_text(generate(a.drivers),encoding="utf-8")
