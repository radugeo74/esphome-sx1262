#!/usr/bin/env python3
"""Bounded source-XMQ compiler for five fixture-validated native clear drivers.

Reviewed on-air record keys and value widths stay pinned here. Original XMQ
field selectors, model signatures and ALL upstream examples are independently
validated. No key guessing, no plaintext fallback, no generic XMQ evaluation.
"""
import argparse
import json
from pathlib import Path
from portable_xmq import parse, validate_driver, values, blocks
from generate_driver_registry import manufacturer_code
from native_driver_specs import PORTABLE

# XMQ field / verified radio DIF-VIF prefix / payload bytes /
# XMQ match key / exact expected XMQ match value.
SPECS = {
 "munia": (
  ("current_temperature","0A66",2,"vif_range","ExternalTemperature"),
  ("current_relative_humidity","0AFB1A",2,"vif_range","RelativeHumidity"),
  ("status","02FD971D",2,"difvifkey","02FD971D")),
 "cma12w": (
  ("current_temperature","0265",2,"vif_range","ExternalTemperature"),
  ("average_temperature_1h","4265",2,"storage_nr","1"),
  ("battery","02FD1B",2,"vif_range","DigitalInput"),
  ("software_version","0DFD0F05",5,"library","software_version")),
 "lansendw": (
  ("status","02FD1B",2,"vif_range","DigitalInput"),
  ("error_flags","02FD971D",2,"vif_range","ErrorFlags"),
  ("a","0EFD3A",6,"vif_range","Dimensionless"),
  ("b","8E40FD3A",6,"subunit_nr","1")),
 "lansenpu": (
  ("a","0EFD3A",6,"difvifkey","0EFD3A"),
  ("b","8E40FD3A",6,"difvifkey","8E40FD3A")),
 "supercom587": (
  ("total_m3","0C13",4,"library","total_m3"),
  ("software_version","0BFD0F",3,"library","software_version"),
  ("status","02FD17",2,"vif_range","ErrorFlags")),
 "sontex868": (
  ("current_consumption","036E",3,"vif_range","HeatCostAllocation"),
  ("set_date","426C",2,"vif_range","Date"),
  ("consumption_at_set_date","436E",3,"vif_range","HeatCostAllocation"),
  ("current_temp","0259",2,"vif_range","FlowTemperature"),
  ("current_room_temp","0265",2,"vif_range","ExternalTemperature"),
  ("max_temp","1259",2,"vif_range","FlowTemperature"),
  ("max_temp_previous_period","5259",2,"vif_range","FlowTemperature"),
  ("device","046D",4,"vif_range","DateTime")),
 "maddalena": (
  ("total_m3","0413",4,"library","total_m3"),
  ("target_m3","4413",4,"library","target_m3"),
  ("target_date","426C",2,"library","target_date"),
  ("meter_datetime","046D",4,"library","meter_datetime"),
  ("fabrication_no","0E78",6,"library","fabrication_no"),
  ("status","04FD17",4,"vif_range","ErrorFlags")),
}
COUNTS={"munia":2,"cma12w":2,"lansendw":2,"lansenpu":3,"supercom587":2,
        "sontex868":1,"maddalena":1}
EXTRA_OFFSETS = {
    "sontex868": (21,26,30,126,130,134,35,15),
    "maddalena": (17,44,50,23,36,29),
}
EXTRA_JSON = {
    "sontex868": {
        "current_consumption_hca":119,
        "set_date":"2127-07-01",
        "consumption_at_set_date_hca":102,
        "current_temp_c":27.33,"current_room_temp_c":12.4,
        "max_temp_c":27.33,"max_temp_previous_period_c":0,
        "device_datetime":"2020-10-31 10:04",
    },
    "maddalena": {
        "total_m3":0.167,"target_m3":0,
        "target_date":"2000-01-01",
        "fabrication_no":"FF0124018699",
        "meter_datetime":"2024-11-22 12:14",
        "status":"OK",
    },
}



def check(path, rules):
    name=path.stem
    if name not in PORTABLE: raise ValueError(f"{name}: not native portable")
    d=validate_driver(parse(path.read_text(encoding="utf-8")))
    if values(d,"name")!=[name]: raise ValueError(f"{name}: name changed")
    detection=blocks(d,"detect")
    if len(detection)!=1: raise ValueError(f"{name}: expected one detect")
    variants=[]
    for mvt in values(detection[0]["entries"],"mvt"):
        parts=[part.strip().upper() for part in mvt.split(",")]
        if len(parts)!=3: raise ValueError(f"{name}: invalid mvt")
        variants.append((manufacturer_code(parts[0]),int(parts[1],16),int(parts[2],16)))
    if not variants or len(set(variants))!=len(variants):
        raise ValueError(f"{name}: invalid MVT variants")
    field_map={}
    for fs in blocks(d,"fields"):
        for f in blocks(fs["entries"],"field"):
            names=values(f["entries"],"name")
            if len(names)!=1 or names[0] in field_map:
                raise ValueError(f"{name}: ambiguous field")
            field_map[names[0]]=f["entries"]
    libraries=[part.strip() for lib in blocks(d,"library")
               for use in values(lib["entries"],"use") for part in use.split(",")]
    for field,key,width,selector,expected in rules:
        if len(key)%2 or len(key)>8 or width not in (2,3,4,5,6):
            raise ValueError(f"{name}: unsupported record shape")
        if selector=="library":
            if expected not in libraries: raise ValueError(f"{name}: missing library {expected}")
        else:
            matches=blocks(field_map.get(field,[]),"match")
            if len(matches)!=1 or values(matches[0]["entries"],selector)!=[expected]:
                raise ValueError(f"{name}: selector mismatch {field}.{selector}")
    tests=blocks(d,"tests")
    if len(tests)!=1: raise ValueError(f"{name}: missing tests")
    samples=blocks(tests[0]["entries"],"test")
    if len(samples)!=COUNTS[name]: raise ValueError(f"{name}: fixtures changed")
    used=set()
    seen_variants=set()
    offsets={}
    for sample in samples:
        s=sample["entries"]
        telegram=values(s,"telegram")
        if len(telegram)!=1 or len(values(s,"json"))!=1:
            raise ValueError(f"{name}: incomplete fixture")
        frame=bytes.fromhex(telegram[0].replace("_","").replace("|",""))
        if len(frame)<15 or frame[0]+1!=len(frame) or frame[10]!=0x7A:
            raise ValueError(f"{name}: malformed/non-CI7A original fixture")
        mvt=(frame[2]|(frame[3]<<8),frame[8],frame[9])
        if mvt not in variants: raise ValueError(f"{name}: unrecognized fixture MVT")
        seen_variants.add(mvt)
        positions={}
        for field,key,width,_,_ in rules:
            prefix=bytes.fromhex(key)
            matches=[i for i in range(15,len(frame))
                     if frame[i:i+len(prefix)]==prefix and
                     i+len(prefix)+width<=len(frame)]
            if matches:
                used.add(field)
            if name=="supercom587" or name in EXTRA_OFFSETS:
                if len(matches)!=1:
                    raise ValueError(f"{name}: missing/duplicate original {field} record")
                positions[field]=matches[0]
        if name=="supercom587":
            # All original source JSON keys are accounted for. The timestamp
            # is a test harness sentinel, not on-air device date/time.
            expected=json.loads(values(s,"json")[0])
            keys={"_","media","driver","name","id","total_m3",
                  "software_version","status","timestamp"}
            if (set(expected)!=keys or expected["_"]!="telegram" or
                    expected["driver"]!="supercom587"):
                raise ValueError("supercom587: unsupported upstream output fields")
            arguments=values(s,"args")
            if len(arguments)!=1:
                raise ValueError("supercom587: missing test arguments")
            parts=arguments[0].split()
            if (len(parts)!=4 or parts[1:]!=["supercom587",expected["id"],"NOKEY"] or
                    parts[0]!=expected["name"]):
                raise ValueError("supercom587: fixture name/driver/ID mismatch")
            meter_id="".join(f"{byte:02X}" for byte in reversed(frame[4:8]))
            if (meter_id!=expected["id"] or
                    expected["timestamp"]!="1111-11-11T11:11:11Z"):
                raise ValueError("supercom587: fixture ID/timestamp drift")
            expected_media={0x06:"warm water",0x07:"water"}[frame[9]]
            if expected["media"]!=expected_media:
                raise ValueError("supercom587: fixture medium mismatch")
            if (len(frame)!=163 or frame[1]!=0x44 or
                    frame[12:15]!=bytes(3) or
                    [positions.get(field) for field,_,_,_,_ in rules]!=[15,123,158] or
                    frame[21:23]!=bytes.fromhex("426C") or
                    frame[25:27]!=bytes.fromhex("4C13")):
                raise ValueError("supercom587: unreviewed radio layout")
            raw_volume=frame[17:21]
            raw_version=frame[126:129]
            flags=frame[161:163]
            if any((byte&15)>9 or (byte>>4)>9 for byte in raw_volume+raw_version):
                raise ValueError("supercom587: invalid original BCD")
            volume=sum(((byte>>4)*10+(byte&15))*100**i
                       for i,byte in enumerate(raw_volume))/1000.0
            software="".join(f"{byte:02X}" for byte in reversed(raw_version))
            if (abs(expected["total_m3"]-volume)>1e-7 or
                    expected["software_version"]!=software or
                    expected["status"]!="OK" or flags!=bytes(2)):
                raise ValueError("supercom587: XMQ fixture value mismatch")
            for name_of_field,position in positions.items():
                if name_of_field in offsets and offsets[name_of_field]!=position:
                    raise ValueError("supercom587: inconsistent fixture offsets")
                offsets[name_of_field]=position
        if name in EXTRA_OFFSETS:
            expected=json.loads(values(s,"json")[0])
            fields=EXTRA_JSON[name]
            # An original upstream JSON with a newly added field must cause
            # a generator failure until that field is implemented.
            expected_keys=set(fields)|{"_","media","driver","name","id","timestamp"}
            if (set(expected)!=expected_keys or expected["driver"]!=name or
                    expected["_"]!="telegram"):
                raise ValueError(f"{name}: changed original XMQ output schema")
            if any(expected[field]!=value for field,value in fields.items()):
                raise ValueError(f"{name}: source XMQ values changed")
            if expected["timestamp"]!="1111-11-11T11:11:11Z":
                raise ValueError(f"{name}: source timestamp is not test sentinel")
            name_of_sample=values(s,"args")
            if len(name_of_sample)!=1:
                raise ValueError(f"{name}: no original invocation")
            parts=name_of_sample[0].split()
            if len(parts)!=4 or parts!=[expected["name"],name,expected["id"],"NOKEY"]:
                raise ValueError(f"{name}: original invocation changed")
            actual_id="".join(f"{byte:02X}" for byte in reversed(frame[4:8]))
            if actual_id!=expected["id"]:
                raise ValueError(f"{name}: sample DLL identity changed")
            if tuple(positions[field] for field,_,_,_,_ in rules)!=EXTRA_OFFSETS[name]:
                raise ValueError(f"{name}: source record positions changed")
            if name=="sontex868":
                if (len(frame)!=176 or frame[1]!=0x46 or frame[9]!=0x08 or
                        frame[12:15]!=bytes(3) or
                        frame[160:163]!=bytes.fromhex("0BFD0F")):
                    raise ValueError("sontex868: unreviewed radio layout")
            if name=="maddalena":
                if (len(frame)!=79 or frame[1]!=0x44 or frame[9]!=0x07 or
                        frame[12:15]!=bytes.fromhex("000020") or
                        frame[15:17]!=bytes.fromhex("2F2F")):
                    raise ValueError("maddalena: unreviewed radio layout")
            offsets=positions
    # A declared MVT can have no public fixture; do not invent RF evidence.
    if used!={r[0] for r in rules}:
        raise ValueError(f"{name}: fixture/schema evidence mismatch")
    if name=="supercom587" and seen_variants!=set(variants):
        raise ValueError("supercom587: incomplete public model fixture coverage")
    return variants,offsets


def generate(driver_dir: Path):
    lines=[
     "// Generated by generate_clear_profiles.py; do not edit.",
     "// Use only after existing native CRC, ID, security and status validation.",
     "#pragma once","#include <cstdint>","#include <cstddef>",
     "namespace xmq_generated_clear {",
     "struct Model {uint16_t maker; uint8_t version,type;};",
     "struct Rule {const char*name; uint8_t key[4]; uint8_t key_len,value_len;};",
     "inline bool match_model(uint16_t m,uint8_t v,uint8_t t,const Model*p,size_t n){",
     "  for(size_t i=0;i<n;++i)if(p[i].maker==m&&p[i].version==v&&p[i].type==t)return true;",
     "  return false;}",
     "inline bool match_rule(const uint8_t*b,size_t n,const Rule&r){",
     "  if(!b||n<size_t(r.key_len)+r.value_len)return false;",
     "  for(size_t i=0;i<r.key_len;++i)if(b[i]!=r.key[i])return false;",
     "  return true;}",
     "inline int find_rule(const uint8_t*b,size_t n,const Rule*r,size_t count){",
     "  int found=-1;for(size_t i=0;i<count;++i)if(match_rule(b,n,r[i])){",
     "    if(found!=-1)return -2;",
     "    found=static_cast<int>(i);}",
     "  return found;}",
    ]
    for name,rules in SPECS.items():
        variants,offsets=check(driver_dir/f"{name}.xmq",rules)
        lines.append(f"namespace {name} {{")
        lines.append("inline constexpr Model models[]={")
        for maker,version,kind in variants:
            lines.append(f"  {{0x{maker:04X},0x{version:02X},0x{kind:02X}}},")
        lines.append("};")
        lines.append("inline constexpr Rule rules[]={")
        for field,key,width,_,_ in rules:
            b=list(bytes.fromhex(key))
            padded=",".join(f"0x{v:02X}" for v in b+[0]*(4-len(b)))
            lines.append(f'  {{"{field}",{{{padded}}},{len(b)},{width}}},')
        lines.append("};")
        if name=="supercom587" or name in EXTRA_OFFSETS:
            lines.append(f"inline constexpr size_t verified_frame_bytes = {163 if name=='supercom587' else 176 if name=='sontex868' else 79};")
            lines.append("inline constexpr size_t offsets[] = {")
            for field,_,_,_,_ in rules:
                lines.append(f"  {offsets[field]}, // {field}")
            lines.append("};")
        lines.append("}")
    lines.append("}")
    return "\n".join(lines)+"\n"


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--out",required=True,type=Path)
    parser.add_argument("--drivers",type=Path,default=Path(__file__).resolve().parents[1]/"drivers")
    args=parser.parse_args()
    args.out.write_text(generate(args.drivers),encoding="utf-8")
