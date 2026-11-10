#!/usr/bin/env python3
"""Compile source-XMQ iPERL/Munia into the SAME common full-profile C++ ABI.

Only verified *on-air* genuinely clear TPL frames are promoted to firmware.
Original XMQ examples already decrypted upstream are verified offline but
cannot become no-key RF permissions. No on-device XMQ interpreter.
"""
import json
from pathlib import Path

from portable_xmq import blocks, parse, validate_driver, values
from generate_driver_registry import manufacturer_code
from generate_xmq_full_profiles import one, one_block, source_fields

SOURCES=("iperl","munia")
FORMATS={
    # exact DIF/VIF semantics, not a literal key pinned to the fixture
    "Volume":(b"\x04\x13",4,"Unsigned32",0.001),
    "VolumeFlow":(b"\x02\x3b",2,"Unsigned16",0.001),
    "ExternalTemperature":(b"\x0a\x66",2,"Bcd4",0.1),
    "RelativeHumidity":(b"\x0a\xfb\x1a",2,"Bcd4",0.1),
    "ErrorFlags":(b"\x02\xfd\x97\x1d",2,"Unsigned16",1.0),
}
SUFFIX={
    "Volume":"_m3","Flow":"_m3h",
    "Temperature":"_c","RelativeHumidity":"_rh",
}
FIXTURE_META={"iperl":("water",3,2),"munia":("room sensor",2,2)}
SUPPORTED_TPL_FLAGS={0x02:"ERROR",0x04:"POWER_LOW",0x08:"PERMANENT_ERROR",
                     0x10:"TEMPORARY_ERROR",0x01:"BUSY"}


def parse_records(data):
    """Bounded original key/value records, no unknown formats or silent skip."""
    records=[]
    pos=15
    while pos<len(data):
        if data[pos]==0x2F:
            pos+=1
            continue
        possibilities=[(name,key,width,op,scale)
                       for name,(key,width,op,scale) in FORMATS.items()
                       if data[pos:pos+len(key)]==key]
        if len(possibilities)!=1:
            raise ValueError(f"source record at {pos} has unsupported DIF/VIF")
        name,key,width,op,scale=possibilities[0]
        if pos+len(key)+width>len(data):
            raise ValueError("truncated original record")
        value=data[pos+len(key):pos+len(key)+width]
        records.append((name,key,width,op,scale,value,pos))
        pos+=len(key)+width
    return records


def scaled(op,scale,data):
    if op=="Unsigned32":
        return int.from_bytes(data,"little")*scale
    if op=="Unsigned16":
        return int.from_bytes(data,"little")*scale
    if op=="Bcd4":
        if len(data)!=2 or any((n&15)>9 or (n>>4)>9 for n in data):
            raise ValueError("invalid source BCD")
        return sum(((n>>4)*10+(n&15))*100**i for i,n in enumerate(data))*scale
    raise ValueError("unsupported native value operation")


def standard_status(tpl):
    if tpl & ~0x1F:
        raise ValueError("uninterpreted TPL status in source example")
    labels=[title for bit,title in SUPPORTED_TPL_FLAGS.items() if tpl&bit]
    return " ".join(labels) if labels else "OK"


def compile_one(path):
    driver=validate_driver(parse(path.read_text(encoding="utf-8")))
    name=one(driver,"name")
    if name not in SOURCES or path.stem!=name:
        raise ValueError("unexpected source XMQ identity")
    detect=one_block(driver,"detect")
    models=[]
    for mvt in values(detect,"mvt"):
        parts=[s.strip().upper() for s in mvt.split(",")]
        if len(parts)!=3:
            raise ValueError("unexpected MVT")
        models.append((manufacturer_code(parts[0]),
                       int(parts[1],16),int(parts[2],16)))
    if len(models)!=len(set(models)) or not models:
        raise ValueError("bad XMQ detect")
    fields=source_fields(driver)
    if len(fields)!=3:
        raise ValueError("source added fields: require compiler review")
    status=[f for f in fields if f["name"]=="status"]
    if len(status)!=1 or status[0]["quantity"]!="Flags" or (
        status[0]["attrs"]!="STATUS,INCLUDE_TPL_STATUS"):
        raise ValueError("changed XMQ status contract")
    error_match=bool(status[0]["selectors"])
    if name=="munia":
        lookup=status[0]["mapping"]
        if (status[0]["selectors"]!={"difvifkey":"02FD971D"} or
                not lookup or lookup["name"]!="ERROR_FLAGS" or
                lookup["type"]!="BitToString" or lookup["mask"]!=65535 or
                lookup["default"]!="OK" or lookup["maps"]):
            raise ValueError("unreviewed Munia ERROR_FLAGS semantics")
    elif status[0]["selectors"] or status[0]["mapping"]:
        raise ValueError("changed iPERL status expression")
    source_rules={}
    for field in fields:
        if field is status[0]:
            continue
        if field["calcs"] or field["mapping"] or field["attrs"]:
            raise ValueError("unsupported source calculation/lookup")
        selector=field["selectors"]
        if set(selector)!={"measurement_type","vif_range"} or (
            selector["measurement_type"]!="Instantaneous"):
            raise ValueError("changed selector")
        vif=selector["vif_range"]
        expected_quantity={
            "Volume":"Volume","VolumeFlow":"Flow",
            "ExternalTemperature":"Temperature","RelativeHumidity":"RelativeHumidity"
        }.get(vif)
        if not expected_quantity or field["quantity"]!=expected_quantity:
            raise ValueError("incompatible source units")
        key,width,op,scale=FORMATS[vif]
        suffix=SUFFIX[field["quantity"]]
        source_rules[vif]=(field["name"]+suffix,key,width,op,scale)
    if len(source_rules)!=2:
        raise ValueError("duplicate or missing source quantity")
    if name=="munia":
        source_rules["ErrorFlags"]=("error_flags",
                                     *FORMATS["ErrorFlags"])
    samples=[x["entries"] for x in blocks(one_block(driver,"tests"),"test")]
    _,count,clear_count=FIXTURE_META[name]
    if len(samples)!=count:
        raise ValueError("upstream source fixture inventory changed")
    source_ids=set()
    safe_models=set()
    safe_statuses={}
    safe_keys=None
    safe_lengths=set()
    layouts=set()
    source_values=set()
    for entry in samples:
        frame=bytes.fromhex(one(entry,"telegram").replace("_","").replace("|",""))
        expect=json.loads(one(entry,"json"))
        args=one(entry,"args").split()
        meter_id="".join(f"{b:02X}" for b in reversed(frame[4:8]))
        mvt=(frame[2]|frame[3]<<8,frame[8],frame[9])
        media,count,_=FIXTURE_META[name]
        if (len(frame)<20 or frame[0]+1!=len(frame) or frame[1]!=0x44
                or frame[10]!=0x7A or mvt not in models or
                len(args)!=4 or args!=[expect.get("name"),name,meter_id,"NOKEY"]
                or expect.get("_")!="telegram" or expect.get("media")!=media
                or expect.get("driver")!=name or expect.get("id")!=meter_id
                or expect.get("timestamp")!="1111-11-11T11:11:11Z"):
            raise ValueError("original XMQ fixture identity changed")
        source_ids.add(meter_id)
        raw=parse_records(frame)
        extracted={}
        for range_name,key,width,op,scale,payload,position in raw:
            if range_name not in source_rules:
                raise ValueError("source record without matching XMQ field")
            output,key_expect,size_expect,operation,unit_scale=source_rules[range_name]
            if key!=key_expect or width!=size_expect or op!=operation or scale!=unit_scale:
                raise ValueError("field source definition/record mismatch")
            if output in extracted:
                raise ValueError("duplicate source application field")
            extracted[output]=scaled(op,scale,payload)
        if set(extracted)!={r[0] for r in source_rules.values()}:
            raise ValueError("missing original source record")
        tpl=frame[12]
        config=int.from_bytes(frame[13:15],"little")
        status_text=standard_status(tpl)
        if name=="munia":
            if extracted["error_flags"]!=0:
                raise ValueError("unreviewed Munia error bits")
            status_text="OK" if tpl==0 else status_text
            # No source evidence for nonzero manufacturer flags.
            if status_text!="OK":
                raise ValueError("unknown Munia status")
            extracted.pop("error_flags")
        expected_keys={"_","media","driver","name","id","timestamp","status"}|set(extracted)
        if set(expect)!=expected_keys:
            raise ValueError(f"unhandled source field changes: {set(expect)^expected_keys}")
        if any(abs(expect[key]-val)>1e-8 for key,val in extracted.items()):
            raise ValueError("source XMQ data not equal to original JSON")
        if expect["status"]!=status_text:
            raise ValueError("source TPL status semantics changed")
        if name=="iperl":
            # Explicit upstream says the first sample is ALREADY decrypted:
            # 0x0010 encodes N=1. Never authorize it for on-air no-key decode.
            comments=values(entry,"comment")
            if config & 0x001F:
                if ("after decryption" not in " ".join(comments) or
                        frame[15:17]!=bytes.fromhex("2F2F")):
                    raise ValueError("encrypted fixture without provenance")
                continue
            if config!=0 or frame[15]==0x2F:
                raise ValueError("unreviewed clear iPERL layout")
        else:
            if config!=0 or tpl!=0 or frame[15:17]!=bytes.fromhex("2F2F"):
                raise ValueError("unreviewed Munia clear layout")
            if (frame[32:]!=b"\x2f"*(len(frame)-32) or
                    len(frame)!=47):
                raise ValueError("unreviewed Munia padding")
        record_keys=tuple((kind,key) for kind,key,*_ in raw)
        if safe_keys is None:
            safe_keys=record_keys
        elif safe_keys!=record_keys:
            raise ValueError("inconsistent source record ordering")
        safe_models.add((*mvt,config))
        safe_lengths.add(len(frame))
        safe_statuses[(tpl,0)]=status_text
        first_pos=raw[0][-1]
        end=raw[-1][-1]+len(raw[-1][1])+raw[-1][2]
        layouts.add((first_pos-15,len(frame)-end))
    if len(safe_models)==0 or len(safe_lengths)!=1 or len(layouts)!=1:
        raise ValueError("no uniform clear on-air profile")
    if len(safe_statuses)!=(2 if name=="iperl" else 1):
        raise ValueError("unexpected source-approved statuses")
    if len(source_ids)!=(2 if name=="iperl" else 2):
        raise ValueError("source ID inventory drift")
    prefix,suffix=next(iter(layouts))
    if name=="iperl" and (prefix or suffix or next(iter(safe_lengths))!=25):
        raise ValueError("iPERL clear layout unsupported")
    if name=="munia" and (prefix,suffix)!=(2,15):
        raise ValueError("Munia frame padding unsupported")
    # The XMQ matching rules define field identity. Source example record
    # order is required so an unreviewed positional layout cannot slip in.
    ordered=[source_rules[kind] for kind,_ in safe_keys]
    if len(ordered) not in (2,3):
        raise ValueError("unreviewed source record count")
    return {"name":name,"models":safe_models,"rules":ordered,
            "states":safe_statuses,"length":next(iter(safe_lengths)),
            "leading":prefix,"trailing":suffix,
            "error_index":2 if name=="munia" else 255}


def emit(profile):
    name=profile["name"]
    result=[f"namespace {name} {{","inline constexpr Model models[]={"]
    for maker,version,medium,config in sorted(profile["models"]):
        result.append(f" {{{maker},0x{version:02X},0x{medium:02X},0x{config:04X}}},")
    result.extend(["};","inline constexpr Rule rules[]={"])
    for label,key,width,operator,scale in profile["rules"]:
        data=list(key)+[0]*(5-len(key))
        result.append(
            f" {{{json.dumps(label)},{{{','.join(f'0x{b:02X}' for b in data)}}},"
            f"{len(key)},{width},Op::{operator},{scale:.10g}}},")
    result.extend(["};","inline constexpr State states[]={"])
    for (tpl,error),message in sorted(profile["states"].items()):
        result.append(f" {{0x{tpl:02X},0x{error:04X},{json.dumps(message)}}},")
    result.extend([
        "};",
        "inline constexpr Lookup lookup[]={ {0,\"\"} };",
        "inline constexpr Profile profile={",
        " models,sizeof(models)/sizeof(models[0]),",
        " rules,sizeof(rules)/sizeof(rules[0]),",
        " states,sizeof(states)/sizeof(states[0]),",
        " lookup,0,",
        f" {profile['length']},false,",
        f" {profile['error_index']},255,255,255,\"\",",
        f" {profile['leading']},{profile['trailing']},true",
        "};",
        "}",
    ])
    return result


def generate(path:Path):
    return "\n".join(line for name in SOURCES for line in emit(
        compile_one(path/f"{name}.xmq")))+"\n"
