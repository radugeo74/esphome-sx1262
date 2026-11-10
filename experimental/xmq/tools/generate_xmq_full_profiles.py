#!/usr/bin/env python3
"""Compile the entire supported CMA12w/LansenDW XMQ *application* profiles.

The upstream XMQ field declarations, matcher selectors, lookup maps, library
uses, model signatures and full original JSON fixtures are the source of
descriptors. Unsupported concepts fail during generation. The runtime is
a single C++ interpreter for a small set of bounded standard record types.
No dynamic XMQ on ESP32; radio CRC, configured ID and encryption are separate.
"""
import argparse
import json
from pathlib import Path
from portable_xmq import blocks, parse, validate_driver, values
from generate_driver_registry import manufacturer_code

DRIVERS = ("cma12w", "lansendw")
SUFFIX = {
    "Temperature": "_c",
    "Dimensionless": "_counter",
}
RECORD_FORMAT_BYTES = {2: 2, 14: 6}
KNOWN_CLASSES = {
    "ExternalTemperature": (b"\x65", 2, "Signed16", 0.01),
    "DigitalInput": (b"\xfd\x1b", 2, "Unsigned16", 1.0),
    "ErrorFlags": (b"\xfd\x97\x1d", 2, "Unsigned16", 1.0),
    "Dimensionless": (b"\xfd\x3a", 14, "Bcd12", 1.0),
}
LIBRARIES = {
    "software_version": (b"\xfd\x0f", 13, "Version3", 1.0),
}

def one(entries, key, required=True):
    found = values(entries, key)
    if len(found) != 1:
        if not required and not found:
            return None
        raise ValueError(f"expected one {key}, got {found}")
    return found[0]

def one_block(entries, key):
    found = blocks(entries, key)
    if len(found) != 1:
        raise ValueError(f"expected one {key} block, got {len(found)}")
    return found[0]["entries"]

def source_fields(driver):
    raw = [f["entries"] for section in blocks(driver, "fields")
           for f in blocks(section["entries"], "field")]
    if not raw:
        raise ValueError("no source fields")
    fields = []
    for field in raw:
        name = one(field, "name")
        quantity = one(field, "quantity")
        lookups = blocks(field, "lookup")
        if len(lookups)>1:
            raise ValueError("ambiguous source lookup")
        mapping = None
        if lookups:
            spec = lookups[0]["entries"]
            maps = blocks(spec, "map")
            mapped = []
            for entry in maps:
                valueset = entry["entries"]
                mapped.append((one(valueset, "name"),
                               int(one(valueset,"value"),0),
                               one(valueset, "test").lower()))
            mapping = {
                "name": one(spec,"name"),
                "type": one(spec,"map_type"),
                "mask": int(one(spec,"mask_bits"),0),
                "default": one(spec,"default_message",False),
                "maps": mapped,
            }
        match = blocks(field,"match")
        if len(match)>1:
            raise ValueError("ambiguous source match")
        selectors = {}
        if match:
            data = match[0]["entries"]
            for key in ("measurement_type","vif_range","storage_nr","subunit_nr"):
                s = one(data,key,False)
                if s is not None:
                    selectors[key] = s
            if len(selectors) != sum(1 for e in data if "key" in e):
                raise ValueError(f"unsupported selector keys in {name}")
        fields.append({"name":name, "quantity":quantity,
                       "selectors":selectors, "mapping":mapping,
                       "attrs":one(field,"attributes",False),
                       "calcs":values(field,"calculate")})
    if len({f["name"] for f in fields})!=len(fields):
        raise ValueError("duplicate field name")
    return fields

def records(frame):
    """Read only the bounded record types represented by the original tests."""
    out=[]
    i=15
    trailer=False
    while i<len(frame):
        if frame[i]==0x2F:
            i+=1
            continue
        if frame[i]==0x0F and i+1==len(frame):
            trailer=True
            i+=1
            continue
        start=i
        dif=frame[i];i+=1
        storage=(dif>>6)&1
        subunit=0
        if dif&0x80:
            while True:
                if i>=len(frame):
                    raise ValueError("truncated DIFE")
                dife=frame[i];i+=1
                storage|=(dife&15)<<1
                subunit=(dife>>6)&1
                if not dife&0x80:
                    break
        if i>=len(frame):
            raise ValueError("missing VIF")
        vif=frame[i];i+=1
        vifs=bytes([vif])
        while vif&0x80:
            if i>=len(frame):
                raise ValueError("truncated VIFE")
            vif=frame[i];i+=1
            vifs+=bytes([vif])
        fmt=dif&15
        if fmt in RECORD_FORMAT_BYTES:
            n=RECORD_FORMAT_BYTES[fmt]
        elif fmt==13:
            if i>=len(frame):
                raise ValueError("missing LVAR")
            n=frame[i]
            i+=1
            if n>12:
                raise ValueError("unsupported variable length")
        else:
            raise ValueError(f"unsupported DIF {dif:02X}")
        key=frame[start:i]
        if i+n>len(frame):
            raise ValueError("truncated source record")
        data=frame[i:i+n]
        i+=n
        out.append({"key":key, "value":data, "fmt":fmt,
                    "vif":vifs, "storage":storage,"subunit":subunit})
    return out,trailer

def class_of(record):
    for kind,(vif,fmt,operation,scale) in KNOWN_CLASSES.items():
        if record["vif"]==vif and record["fmt"]==fmt:
            return kind,operation,scale
    for kind,(vif,fmt,operation,scale) in LIBRARIES.items():
        if record["vif"]==vif and record["fmt"]==fmt:
            return kind,operation,scale
    raise ValueError(f"unhandled VIF {record['vif'].hex()}")

def checked_value(record,operation,scale):
    buf=record["value"]
    if operation=="Signed16":
        return int.from_bytes(buf,"little",signed=True)*scale
    if operation=="Unsigned16":
        return int.from_bytes(buf,"little")
    if operation=="Bcd12":
        if len(buf)!=6 or any((b&15)>9 or (b>>4)>9 for b in buf):
            raise ValueError("invalid 12 digit BCD")
        return sum(((b>>4)*10+(b&15))*100**i for i,b in enumerate(buf))
    if operation=="Version3":
        try:
            value=buf.decode("ascii")
        except UnicodeDecodeError as err:
            raise ValueError("non-ASCII software version") from err
        if len(value)!=5 or value[1]!="." or value[3]!="." or any(
                not value[i].isdigit() for i in (0,2,4)):
            raise ValueError("unsupported software version")
        return value[4]+"."+value[2]+"."+value[0]
    raise ValueError("unsupported XMQ value operation")

def compile_one(path):
    name=path.stem
    if name not in DRIVERS:
        raise ValueError("unreviewed driver")
    source=validate_driver(parse(path.read_text(encoding="utf-8")))
    if one(source,"name")!=name:
        raise ValueError("XMQ identity mismatch")
    detections = one_block(source,"detect")
    model=[]
    for mvt in values(detections,"mvt"):
        parts=[s.strip().upper() for s in mvt.split(",")]
        if len(parts)!=3:
            raise ValueError("invalid MVT")
        model.append((manufacturer_code(parts[0]),int(parts[1],16),int(parts[2],16)))
    if len(set(model))!=len(model):
        raise ValueError("duplicate MVT")
    fields=source_fields(source)
    libraries=[v.strip() for b in blocks(source,"library") for q in
               values(b["entries"],"use") for v in q.split(",")]
    if len(set(libraries))!=len(libraries) or any(x not in LIBRARIES for x in libraries):
        raise ValueError("unsupported XMQ library")
    fields_by_name={f["name"]:f for f in fields}
    standalone=[]
    for field in fields:
        sel=field["selectors"]
        if field["calcs"]:
            raise ValueError("XMQ calculation not yet implemented for this profile")
        if not sel:
            if (field["name"]!="status" or field["quantity"]!="Flags" or
                    field["attrs"]!="STATUS,INCLUDE_TPL_STATUS" or field["mapping"]):
                raise ValueError("unsupported field without match")
            standalone.append(field["name"])
            continue
        if sel.get("measurement_type")!="Instantaneous":
            raise ValueError("unsupported source function")
        if field["quantity"]=="Flags" and field["attrs"]!="STATUS,INCLUDE_TPL_STATUS":
            raise ValueError("changed Flags semantics")
        if field["quantity"] not in ("Flags","Text","Temperature","Dimensionless"):
            raise ValueError("unrecognized source quantity")
        if field["mapping"] and field["mapping"]["mask"]!=65535:
            raise ValueError("unsupported lookup bit mask")
    samples=[t["entries"] for t in blocks(one_block(source,"tests"),"test")]
    if len(samples)<2:
        raise ValueError("not enough original XMQ fixtures")
    rules={}
    states={}
    mappings={}
    model_cfg=set()
    frame_len=set()
    trailer_values=set()
    allowed_modes=set()
    expected_fields={}
    for s in samples:
        frame=bytes.fromhex(one(s,"telegram").replace("_","").replace("|",""))
        expected=json.loads(one(s,"json"))
        args=one(s,"args").split()
        if (len(frame)<17 or len(frame)!=frame[0]+1 or frame[1]!=0x44
                or frame[10]!=0x7A or
                (frame[2]|frame[3]<<8,frame[8],frame[9]) not in model):
            raise ValueError("unreviewed original transport frame")
        actual_id="".join(f"{x:02X}" for x in reversed(frame[4:8]))
        if (len(args)!=4 or args!=[expected.get("name"),name,actual_id,"NOKEY"]
                or expected.get("id")!=actual_id
                or expected.get("driver")!=name
                or expected.get("_")!="telegram"
                or expected.get("timestamp")!="1111-11-11T11:11:11Z"):
            raise ValueError("fixture/JSON identity drift")
        config=int.from_bytes(frame[13:15],"little")
        # This is source evidence, not an AES or ELL decryptor.
        if config & 0x001f:
            raise ValueError("encrypted fixture does not authorize plaintext")
        model_cfg.add((frame[2]|frame[3]<<8,frame[8],frame[9],config))
        frame_len.add(len(frame))
        record_list,trailer=records(frame)
        trailer_values.add(trailer)
        observed={}
        decoded_values={}
        for r in record_list:
            range_key,operation,scale=class_of(r)
            if range_key in LIBRARIES:
                if range_key not in libraries:
                    raise ValueError("library source mismatch")
                source_field=range_key
            else:
                choices=[]
                for field in fields:
                    sel=field["selectors"]
                    if sel.get("vif_range")!=range_key:
                        continue
                    if int(sel.get("storage_nr","0"))!=r["storage"]:
                        continue
                    if int(sel.get("subunit_nr","0"))!=r["subunit"]:
                        continue
                    choices.append(field)
                if len(choices)!=1:
                    raise ValueError(f"ambiguous source match for {r['key'].hex()}")
                source_field=choices[0]["name"]
            if source_field in observed:
                raise ValueError("duplicate field in source telegram")
            observed[source_field]=1
            display_name=(source_field+SUFFIX.get(
                fields_by_name[source_field]["quantity"],"")
                if source_field in fields_by_name else source_field)
            value=checked_value(r,operation,scale)
            rule=(display_name,r["key"],len(r["value"]),operation,scale,
                  source_field,range_key,r["storage"],r["subunit"])
            if source_field in rules and rules[source_field]!=rule:
                raise ValueError(f"inconsistent source record {source_field}")
            rules[source_field]=rule
            decoded_values[display_name]=value
            if source_field=="battery":
                lookup=fields_by_name[source_field]["mapping"]
                if not lookup or lookup["type"]!="BitToString" or lookup["maps"]:
                    raise ValueError("unhandled source battery lookup")
                decoded_values[source_field]=lookup["name"]+f"_{value:X}"
            if source_field=="status" and name=="lansendw":
                lookup=fields_by_name[source_field]["mapping"]
                if not lookup or lookup["type"]!="IndexToString":
                    raise ValueError("unhandled source state lookup")
                matches=[title for title,mask,test in lookup["maps"]
                         if test=="set" and value==mask]
                if len(matches)!=1:
                    raise ValueError("missing door/window lookup")
                decoded_values["status"]=matches[0]
                mappings[value]=matches[0]
        if len(observed)!=len(fields)-len(standalone)+len(libraries):
            raise ValueError("partial source XMQ record")
        expected_keys={"_","media","driver","name","id","timestamp",
                       "status"}
        expected_keys|={k for k in decoded_values if k!="status"}
        if name=="lansendw":
            err_val=decoded_values["error_flags"]
            error_base=fields_by_name["error_flags"]["mapping"]
            if (error_base["name"]!="ERROR_FLAGS" or
                    error_base["type"]!="BitToString" or
                    error_base["maps"] or error_base["default"]!="OK"):
                raise ValueError("unreviewed error lookup")
            # The generic OMS status bits (0x08 permanent error, unknown
            # upper status bits) yield the original XMQ JSON strings.
            tpl=frame[12]
            flags=[]
            if err_val: flags.append(f"{error_base['name']}_{err_val:X}")
            if tpl&8: flags.append("PERMANENT_ERROR")
            if tpl&4: flags.append("POWER_LOW")
            if tpl&~0x0C: flags.append(f"TPL_STS_{tpl&~0x0C}")
            error_text=" ".join(flags) if flags else "OK"
            if expected.get("error_flags")!=error_text:
                raise ValueError("upstream status semantics changed")
            decoded_values["error_flags"]=error_text
            status_key=(tpl,err_val)
            if status_key in states and states[status_key]!=error_text:
                raise ValueError("conflicting fixture status")
            states[status_key]=error_text
        else:
            tpl=frame[12]
            if tpl!=0 or expected.get("status")!="OK":
                raise ValueError("unknown CMA TPL status")
            states[(tpl,0)]="OK"
            lookup=fields_by_name["battery"]["mapping"]
            if not lookup or lookup["name"]!="BATTERY" or lookup["maps"]:
                raise ValueError("unreviewed battery bit definitions")
        if any(expected.get(k)!=v for k,v in decoded_values.items()):
            raise ValueError("source record value != original JSON")
        if set(expected)!=expected_keys:
            raise ValueError(f"unhandled upstream JSON output: {set(expected)^expected_keys}")
        if name=="lansendw" and expected.get("status")!=decoded_values["status"]:
            raise ValueError("door state mismatch")
        allowed_modes.add(config)
    if len(frame_len)!=1 or len(trailer_values)!=1:
        raise ValueError("mixed source frame layouts not supported")
    expected_record_count=len(fields)-len(standalone)+len(libraries)
    if len(rules)!=expected_record_count:
        raise ValueError("not all source match rules compiled")
    for f in fields:
        if f["selectors"] and f["name"] not in rules:
            raise ValueError(f"XMQ field missing record evidence: {f['name']}")
    if name=="cma12w" and set(libraries)!={"software_version"}:
        raise ValueError("software_version library required")
    if name=="lansendw" and libraries:
        raise ValueError("unexpected library")
    if any(f["quantity"]=="Flags" and f["name"]=="status" for f in fields):
        status_name="status"
    else:
        status_name="error_flags"
    return {"name":name,"models":model_cfg,"fields":fields,"rules":rules,
            "states":states,"mappings":mappings,"length":next(iter(frame_len)),
            "trailer":next(iter(trailer_values)), "status_name":status_name,
            "modes":allowed_modes}

def emit(profile):
    """Native data only. Parsing and transformations live in common C++."""
    name=profile["name"]
    lines=[f"namespace {name} {{",
           "inline constexpr Model models[]={"]
    for maker,ver,medium,cfg in sorted(profile["models"]):
        lines.append(f" {{{maker},0x{ver:02X},0x{medium:02X},0x{cfg:04X}}},")
    lines.extend(["};","inline constexpr Rule rules[]={"])
    for key,data in profile["rules"].items():
        field,keybytes,width,op,scale,*_=data
        raw=list(keybytes)+[0]*(5-len(keybytes))
        if len(raw)!=5:
            raise ValueError("record prefix larger than compiler boundary")
        literal=",".join(f"0x{n:02X}" for n in raw)
        lines.append(
            f' {{{json.dumps(field)},{{{literal}}},{len(keybytes)},{width},'
            f"Op::{op},{scale:.10g}}},")
    lines.extend(["};","inline constexpr State states[]={"])
    for (tpl,err),label in sorted(profile["states"].items()):
        lines.append(f" {{0x{tpl:02X},0x{err:04X},{json.dumps(label)}}},")
    lines.append("};")
    mappings=profile["mappings"]
    if not mappings:
        mappings={0:""}  # harmless placeholder: explicit map count remains 0
    lines.append("inline constexpr Lookup lookup[]={")
    for raw,label in sorted(mappings.items()):
        lines.append(f" {{{raw},{json.dumps(label)}}},")
    lines.extend([
        "};",
        "inline constexpr Profile profile={",
        f" models,sizeof(models)/sizeof(models[0]),"
        f"rules,sizeof(rules)/sizeof(rules[0]),"
        f"states,sizeof(states)/sizeof(states[0]),"
        f"lookup,{len(profile['mappings'])},"
        f"{profile['length']},"
        f"{'true' if profile['trailer'] else 'false'},",
        f"{list(profile['rules']).index('error_flags') if 'error_flags' in profile['rules'] else 255},",
        f"{list(profile['rules']).index('status') if profile['name']=='lansendw' else 255},",
        f"{list(profile['rules']).index('battery') if 'battery' in profile['rules'] else 255},",
        f"{list(profile['rules']).index('software_version') if 'software_version' in profile['rules'] else 255},",
        f'{json.dumps(next((f["mapping"]["name"] for f in profile["fields"] if f["name"]=="battery"), "") if profile["name"]=="cma12w" else "")}',
        "};",
        "}",
    ])
    return lines

def generate(path:Path):
    lines=[
        "// Compiled from XMQ source match/lookup/library and full original JSON.",
        "// Generated at build time; no dynamic XMQ evaluator in firmware.",
        "#pragma once",
        "#include <cstdint>",
        "#include <cstddef>",
        "namespace xmq_compiled_profiles {",
        "enum class Op : uint8_t { Signed16, Unsigned16, Bcd12, Version3 };",
        "struct Model {uint16_t maker;uint8_t ver,medium;uint16_t config;};",
        "struct Rule {const char*name;uint8_t key[5];uint8_t key_len,bytes;Op op;double scale;};",
        "struct State {uint8_t tpl;uint16_t error;const char*text;};",
        "struct Lookup {uint16_t raw;const char*text;};",
        "struct Profile {",
        " const Model *models;size_t model_count;",
        " const Rule *rules;size_t rule_count;",
        " const State *states;size_t state_count;",
        " const Lookup *lookup;size_t lookup_count;",
        " size_t verified_length;bool trailing_0f;",
        " uint8_t error_index,lookup_index,battery_index,version_index;",
        " const char* battery_prefix;",
        "};",
    ]
    for name in DRIVERS:
        lines.extend(emit(compile_one(path/f"{name}.xmq")))
    lines.append("}")
    return "\n".join(lines)+"\n"

if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("--drivers",type=Path,required=True)
    ap.add_argument("--out",type=Path,required=True)
    args=ap.parse_args()
    args.out.write_text(generate(args.drivers),encoding="utf-8")
