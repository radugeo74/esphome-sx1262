#!/usr/bin/env python3
"""Compile fixture-backed full clear profiles for Supercal 739 and LSE/01/08.

All original XMQ JSON measurement keys, model/field declarations and every
on-air DIF/VIF record are checked. Fails closed on input changes; no
decryption, interpolation or unverified model admission.
"""
import argparse
import calendar
import json
from pathlib import Path

from generate_driver_registry import manufacturer_code
from portable_xmq import parse, validate_driver, blocks, values
from native_driver_specs import PORTABLE

# (public XMQ value, original record start, exact DIF/DIFE/VIF/VIFE, byte width)
RECORDS = {
    "supercal": (
        ("meter_datetime",15,"046D",4),
        ("target_date",21,"820A6C",2),
        ("total_kwh",26,"0406",4),
        ("target_kwh",32,"840A06",4),
        ("total_m3",39,"0414",4),
        ("target_m3",45,"840A14",4),
        ("flow_temperature_c",52,"0259",2),
        ("return_temperature_c",56,"025D",2),
        ("volume_flow_m3h",60,"023B",2),
        ("power_kw",64,"032C",3),
    ),
    "lse_08": (
        ("set_date",15,"82046C",2),
        ("consumption_at_set_date_hca",20,"8B046E",3),
        ("device_datetime",26,"046D",4),
        ("duration_since_readout_h",32,"02FDAC7E",2),
        ("model_version",38,"01FD0C",1),
    ),
}
XMQ_FIELDS = {
    "supercal": {
        ("total","Energy"): {"measurement_type":"Instantaneous","vif_range":"AnyEnergyVIF"},
        ("total","Volume"): {"measurement_type":"Instantaneous","vif_range":"Volume"},
        ("target","Energy"): {"measurement_type":"Instantaneous","vif_range":"AnyEnergyVIF","storage_nr":"20"},
        ("target","Volume"): {"measurement_type":"Instantaneous","vif_range":"Volume","storage_nr":"20"},
        ("target","PointInTime"): {"measurement_type":"Instantaneous","vif_range":"Date","storage_nr":"20"},
        ("power","Power"): {"measurement_type":"Instantaneous","vif_range":"AnyPowerVIF"},
    },
    "lse_08": {
        ("status","Flags"): {"difvifkey":"01FD73"},
        ("current_consumption","HCA"): {"measurement_type":"Instantaneous","vif_range":"HeatCostAllocation"},
        ("set","PointInTime"): {"measurement_type":"Instantaneous","vif_range":"Date","storage_nr":"8"},
        ("consumption_at_set_date","HCA"): {"measurement_type":"Instantaneous","vif_range":"HeatCostAllocation","storage_nr":"8"},
        ("device","PointInTime"): {"measurement_type":"Instantaneous","vif_range":"DateTime"},
        ("duration_since_readout","Time"): {"measurement_type":"Instantaneous","vif_range":"DurationSinceReadout","add_combinable":"FutureValue"},
        ("model_version","Text"): {"measurement_type":"Instantaneous","vif_range":"ModelVersion"},
    },
}
LIBRARIES = {
    "supercal": ("meter_datetime","flow_temperature_c","return_temperature_c","volume_flow_m3h"),
    "lse_08": (),
}
EXPECTED_JSON = {
    "supercal": {
        "total_kwh":296,"target_kwh":0,
        "total_m3":44.26,"target_m3":0,
        "flow_temperature_c":40.67,"return_temperature_c":31.14,
        "volume_flow_m3h":0.285,"power_kw":3.14,
        "target_date":"2127-01-01","meter_datetime":"2023-03-04 18:54",
    },
    "lse_08": {
        "set_date":"2003-01-31","consumption_at_set_date_hca":321,
        "device_datetime":"2003-02-15 14:26",
        "duration_since_readout_h":2.489167,
        "model_version":"01","status":"TPL_STS_128",
    },
}
EXPECTED_MVT={"supercal":("SON","1B","04"),"lse_08":("LSE","01","08")}
EXPECTED_MEDIA={"supercal":"heat","lse_08":"heat cost allocation"}
EXPECTED_NAME={"supercal":"sonsupercal","lse_08":"HCA2"}
EXPECTED_ID={"supercal":"89508019","lse_08":"04998541"}
EXPECTED_CONTROL={"supercal":0x44,"lse_08":0x44}
EXPECTED_STATUS={"supercal":0x00,"lse_08":0x80}


def calendar_text(b, datetime):
    raw=int.from_bytes(b,"little")
    d0=((raw>>16)&255) if datetime else raw&255
    d1=((raw>>24)&255) if datetime else (raw>>8)&255
    year=2000+(d0>>5)+((d1>>4)<<3)
    month=d1&15
    day=d0&31
    if not 1<=month<=12 or not 1<=day<=calendar.monthrange(year,month)[1]:
        raise ValueError("invalid calendar in upstream fixture")
    if not datetime:
        return f"{year:04d}-{month:02d}-{day:02d}"
    minute=raw&63
    hour=(raw>>8)&31
    if hour>=24 or minute>=60:
        raise ValueError("invalid upstream datetime")
    return f"{year:04d}-{month:02d}-{day:02d} {hour:02d}:{minute:02d}"


def check(path):
    name=path.stem
    if name not in RECORDS or name not in PORTABLE:
        raise ValueError("unreviewed clear driver")
    tree=validate_driver(parse(path.read_text(encoding="utf-8")))
    if values(tree,"name")!=[name]:
        raise ValueError(f"{name}: incorrect source name")
    detect=blocks(tree,"detect")
    if len(detect)!=1:
        raise ValueError(f"{name}: unsupported detect shape")
    mvt=values(detect[0]["entries"],"mvt")
    if len(mvt)!=1 or tuple(x.strip().upper() for x in mvt[0].split(","))!=EXPECTED_MVT[name]:
        raise ValueError(f"{name}: changed MVT")
    groups=blocks(tree,"fields")
    if len(groups)!=1:
        raise ValueError(f"{name}: expected one fields group")
    field_map={}
    for fld in blocks(groups[0]["entries"],"field"):
        entries=fld["entries"]
        n=values(entries,"name")
        q=values(entries,"quantity")
        if len(n)!=1 or len(q)!=1 or (n[0],q[0]) in field_map:
            raise ValueError(f"{name}: duplicate or incomplete field")
        matches=blocks(entries,"match")
        if len(matches)!=1:
            raise ValueError(f"{name}: missing match for {n}")
        field_map[(n[0],q[0])]={
            key:values(matches[0]["entries"],key)[0]
            for key in ("measurement_type","vif_range","storage_nr","difvifkey","add_combinable")
            if values(matches[0]["entries"],key)
        }
    if field_map!=XMQ_FIELDS[name]:
        raise ValueError(f"{name}: changed XMQ field selectors")
    libs=[w.strip() for lib in blocks(tree,"library")
          for used in values(lib["entries"],"use") for w in used.split(",")]
    if tuple(libs)!=LIBRARIES[name]:
        raise ValueError(f"{name}: library mapping changed")
    test_groups=blocks(tree,"tests")
    if len(test_groups)!=1:
        raise ValueError(f"{name}: missing source test")
    fixtures=blocks(test_groups[0]["entries"],"test")
    if len(fixtures)!=1:
        raise ValueError(f"{name}: expected exactly one full public fixture")
    source=fixtures[0]["entries"]
    telegram=values(source,"telegram")
    result=values(source,"json")
    args=values(source,"args")
    if len(telegram)!=1 or len(result)!=1 or len(args)!=1:
        raise ValueError(f"{name}: incomplete source fixture")
    frame=bytes.fromhex(telegram[0].replace("_","").replace("|",""))
    expected=json.loads(result[0])
    keys=set(EXPECTED_JSON[name])|{"_","media","driver","name","id","timestamp"}
    if (set(expected)!=keys or expected["_"]!="telegram" or
            expected["driver"]!=name or expected["media"]!=EXPECTED_MEDIA[name] or
            expected["name"]!=EXPECTED_NAME[name] or
            expected["id"]!=EXPECTED_ID[name] or
            expected["timestamp"]!="1111-11-11T11:11:11Z" or
            args[0].split()!=[EXPECTED_NAME[name],name,EXPECTED_ID[name],"NOKEY"]):
        raise ValueError(f"{name}: incomplete/changed original JSON metadata")
    for field,value in EXPECTED_JSON[name].items():
        if field not in expected or (
            isinstance(value,(int,float)) and
            abs(expected[field]-value)>0.0000001
        ) or (isinstance(value,str) and expected[field]!=value):
            raise ValueError(f"{name}: original XMQ field changed {field}")
    m,v,t=EXPECTED_MVT[name]
    manufacturer=manufacturer_code(m)
    if (len(frame)!=frame[0]+1 or frame[1]!=EXPECTED_CONTROL[name] or
            (frame[2]|frame[3]<<8)!=manufacturer or
            frame[8]!=int(v,16) or frame[9]!=int(t,16) or frame[10]!=0x7A or
            frame[12]!=EXPECTED_STATUS[name] or frame[13:15]!=bytes(2)):
        raise ValueError(f"{name}: wrong radio/security header")
    frame_id="".join(f"{byte:02X}" for byte in reversed(frame[4:8]))
    if frame_id!=EXPECTED_ID[name]:
        raise ValueError(f"{name}: mismatched DLL ID")
    last=15
    payloads={}
    for field,offset,key,width in RECORDS[name]:
        prefix=bytes.fromhex(key)
        if offset!=last or frame[offset:offset+len(prefix)]!=prefix:
            raise ValueError(f"{name}: unexpected {field} record offset/prefix")
        all_offsets=[i for i in range(15,len(frame)-len(prefix)-width+1)
                     if frame[i:i+len(prefix)]==prefix]
        if all_offsets!=[offset]:
            raise ValueError(f"{name}: ambiguous original {field} prefix")
        from_byte=offset+len(prefix)
        payloads[field]=frame[from_byte:from_byte+width]
        last=from_byte+width
    if last!=len(frame):
        raise ValueError(f"{name}: unexpected unhandled fixture record")
    if name=="supercal":
        raw=lambda field: int.from_bytes(payloads[field],"little")
        measured={
            "meter_datetime":calendar_text(payloads["meter_datetime"],True),
            "target_date":calendar_text(payloads["target_date"],False),
            "total_kwh":raw("total_kwh"),
            "target_kwh":raw("target_kwh"),
            "total_m3":raw("total_m3")/100,
            "target_m3":raw("target_m3")/100,
            "flow_temperature_c":raw("flow_temperature_c")/100,
            "return_temperature_c":raw("return_temperature_c")/100,
            "volume_flow_m3h":raw("volume_flow_m3h")/1000,
            "power_kw":raw("power_kw")/100,
        }
    else:
        hca=payloads["consumption_at_set_date_hca"]
        if any((byte&15)>9 or byte>>4>9 for byte in hca):
            raise ValueError("lse_08: invalid BCD history")
        measured={
            "set_date":calendar_text(payloads["set_date"],False),
            "consumption_at_set_date_hca":sum(
                ((byte>>4)*10+(byte&15))*100**i for i,byte in enumerate(hca)),
            "device_datetime":calendar_text(payloads["device_datetime"],True),
            "duration_since_readout_h":
                int.from_bytes(payloads["duration_since_readout_h"],"little")/3600,
            "model_version":payloads["model_version"].hex().upper(),
            "status":"TPL_STS_128",
        }
    for field,expected_value in EXPECTED_JSON[name].items():
        actual=measured[field]
        if (isinstance(actual,str) and actual!=expected_value) or (
            not isinstance(actual,str) and abs(actual-expected_value)>0.0000005):
            raise ValueError(f"{name}: derived {field} differs from original XMQ")
    return manufacturer, int(v,16), int(t,16), len(frame)


def generate(driver_dir: Path):
    lines=[
        "// Source-generated from reviewed full Supercal and LSE08 XMQ fixtures.",
        "// Incoming DLL CRC, configured ID and plaintext security are checked separately.",
        "#pragma once", "#include <cstddef>", "#include <cstdint>",
        "namespace xmq_generated_pair {",
        "struct Rule {size_t offset; uint8_t key[4]; uint8_t key_len,value_len;};",
        "inline bool match(const uint8_t *f,size_t n,const Rule&r) {",
        " if(!f||r.offset>n||n-r.offset<size_t(r.key_len)+r.value_len)return false;",
        " for(size_t i=0;i<r.key_len;++i)if(f[r.offset+i]!=r.key[i])return false;",
        " return true;}",
    ]
    for name in RECORDS:
        maker,version,kind,n=check(driver_dir/f"{name}.xmq")
        lines+=[
            f"namespace {name} {{",
            f"inline constexpr uint16_t maker=0x{maker:04X};",
            f"inline constexpr uint8_t version=0x{version:02X};",
            f"inline constexpr uint8_t type=0x{kind:02X};",
            f"inline constexpr size_t verified_length={n};",
            "inline constexpr Rule rules[]={",
        ]
        for field,offset,key,width in RECORDS[name]:
            packed=list(bytes.fromhex(key))
            padded=",".join(f"0x{v:02X}" for v in packed+[0]*(4-len(packed)))
            lines.append(f" {{ {offset},{{{padded}}},{len(packed)},{width}}}, // {field}")
        lines+=["};","}"]
    lines+=["}",""]
    return "\n".join(lines)


if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("--drivers",type=Path,default=Path(__file__).resolve().parents[1]/"drivers")
    ap.add_argument("--out",required=True,type=Path)
    args=ap.parse_args()
    args.out.write_text(generate(args.drivers),encoding="utf-8")
