#!/usr/bin/env python3
"""Generate Lansen Pulse and Supercom587 descriptors for common XMQ interpreter.

This is a build-time XMQ compiler backend, imported into
generate_xmq_full_profiles.py. Only tested unencrypted on-air profiles
are promoted. Unknown source semantics or source fixture drift stop build.
"""
import json
from pathlib import Path

from portable_xmq import parse, validate_driver, blocks, values
from generate_driver_registry import manufacturer_code
from generate_pulse_fields import build as pulse_source
from generate_xmq_full_profiles import one, one_block


def models(driver):
    declared=[]
    for source in values(one_block(driver,"detect"),"mvt"):
        parts=[x.strip() for x in source.split(",")]
        if len(parts)!=3:
            raise ValueError("invalid source MVT")
        declared.append((manufacturer_code(parts[0].upper()),
                         int(parts[1],16),int(parts[2],16)))
    if not declared or len(declared)!=len(set(declared)):
        raise ValueError("missing/ambiguous source MVT")
    return declared


def frame_and_json(sample, name, declared):
    args=one(sample,"args").split()
    frame=bytes.fromhex(one(sample,"telegram").replace("_","").replace("|",""))
    expected=json.loads(one(sample,"json"))
    if len(frame)<19 or frame[0]+1!=len(frame) or frame[1]!=0x44 or frame[10]!=0x7A:
        raise ValueError(f"{name}: bad original DLL/TPL")
    mvt=(frame[2] | frame[3]<<8,frame[8],frame[9])
    ident="".join(f"{b:02X}" for b in reversed(frame[4:8]))
    if (mvt not in declared or len(args)!=4 or
        args!=[expected.get("name"),name,ident,"NOKEY"] or
        expected.get("id")!=ident or expected.get("driver")!=name or
        expected.get("_")!="telegram" or
        expected.get("timestamp")!="1111-11-11T11:11:11Z"):
        raise ValueError(f"{name}: altered source JSON/identity")
    cfg=int.from_bytes(frame[13:15],"little")
    if cfg & 0x1f:
        raise ValueError(f"{name}: encrypted NOKEY source is not clear RF")
    return frame,expected,mvt,cfg


def compile_lansen(path):
    xmq=validate_driver(parse(path.read_text(encoding="utf-8")))
    if one(xmq,"name")!="lansenpu" or one(xmq,"meter_type")!="PulseCounter":
        raise ValueError("unexpected pulse definition")
    declared=models(xmq)
    rules,states,sabotage=pulse_source(path)
    if len(rules)!=2 or any(r[2]!=6 for r in rules):
        raise ValueError("pulse source record inventory changed")
    cases=blocks(one_block(xmq,"tests"),"test")
    if len(cases)!=3:
        raise ValueError("pulse source tests changed")
    variants=[]
    for source in cases:
        f,e,mvt,cfg=frame_and_json(source["entries"],"lansenpu",declared)
        if f[15:17]!=b"\x2f\x2f" or f[12] not in states:
            raise ValueError("unsupported pulse leading markers/status")
        if len(f) not in (27,36) or cfg not in (0,0x20):
            raise ValueError("unsupported pulse source layout")
        mask=0
        offset=17
        while offset<len(f):
            found=[(index,rule) for index,rule in enumerate(rules)
                   if f[offset:offset+len(rule[1])]==rule[1]]
            if len(found)!=1:
                raise ValueError("unsupported pulse record key")
            index,rule=found[0]
            if mask&(1<<index):
                raise ValueError("duplicate original pulse record")
            mask|=1<<index
            offset+=len(rule[1])
            if len(f)-offset<6:
                raise ValueError("truncated BCD source")
            raw=f[offset:offset+6]
            if any((b&15)>9 or (b>>4)>9 for b in raw):
                raise ValueError("invalid pulse source BCD")
            value=sum(((b>>4)*10+(b&15))*100**i for i,b in enumerate(raw))
            if e.get(rule[0])!=value:
                raise ValueError("pulse original JSON value disagreement")
            offset+=6
        if offset!=len(f):
            raise ValueError("pulse unparsed suffix")
        expected={"_","driver","name","id","timestamp","status","media"}
        expected|={r[0] for idx,r in enumerate(rules) if mask&(1<<idx)}
        if set(e)!=expected or e["status"]!=states[f[12]]:
            raise ValueError("pulse original JSON keys changed")
        if (mvt,cfg,len(f),mask,f[12]) in variants:
            raise ValueError("duplicate source pulse variant")
        variants.append((*mvt,cfg,len(f),mask,f[12]))
    return rules,states,sabotage,variants


def compile_supercom(path):
    xmq=validate_driver(parse(path.read_text(encoding="utf-8")))
    if one(xmq,"name")!="supercom587" or one(xmq,"meter_type")!="WaterMeter":
        raise ValueError("unexpected supercom source")
    declared=models(xmq)
    library=blocks(xmq,"library")
    uses=[a.strip() for section in library
          for group in values(section["entries"],"use")
          for a in group.split(",")]
    if sorted(uses)!=["software_version","total_m3"]:
        raise ValueError("changed source library inputs")
    fields=[field["entries"] for group in blocks(xmq,"fields")
            for field in blocks(group["entries"],"field")]
    if len(fields)!=1 or one(fields[0],"name")!="status" or (
        one(fields[0],"quantity")!="Flags") or (
        one(fields[0],"attributes")!="STATUS,INCLUDE_TPL_STATUS"):
        raise ValueError("changed supercom field semantics")
    selector=one_block(fields[0],"match")
    if (values(selector,"measurement_type")!=["Instantaneous"] or
        values(selector,"vif_range")!=["ErrorFlags"] or len(selector)!=2):
        raise ValueError("unhandled supercom selector")
    lookup=one_block(fields[0],"lookup")
    if (values(lookup,"name")!=["ERROR_FLAGS"] or
        values(lookup,"map_type")!=["BitToString"] or
        values(lookup,"mask_bits")!=["0x000f"] or
        values(lookup,"default_message")!=["OK"] or
        blocks(lookup,"map")):
        raise ValueError("unreviewed supercom error lookup")
    cases=blocks(one_block(xmq,"tests"),"test")
    if len(cases)!=2:
        raise ValueError("supercom source examples changed")
    descriptions=[
        ("total_m3",b"\x0c\x13",4,"Bcd8",.001),
        ("software_version",b"\x0b\xfd\x0f",3,"BcdVersion6",1.0),
        ("status",b"\x02\xfd\x17",2,"Unsigned16",1.0),
    ]
    signatures=None
    used_models=set()
    for source in cases:
        frame,e,mvt,cfg=frame_and_json(source["entries"],"supercom587",declared)
        if (len(frame)!=163 or cfg!=0 or frame[12]!=0 or
            e.get("status")!="OK" or e.get("media")!=
            ("warm water" if frame[9]==6 else "water")):
            raise ValueError("supercom unsupported mode/status")
        anchors=[]
        for field,key,size,op,scale in descriptions:
            offsets=[i for i in range(15,len(frame)-size-len(key)+1)
                     if frame[i:i+len(key)]==key]
            if len(offsets)!=1:
                raise ValueError(f"ambiguous supercom source {field}")
            offset=offsets[0]
            buf=frame[offset+len(key):offset+len(key)+size]
            if any((b&15)>9 or (b>>4)>9 for b in buf) and op!="Unsigned16":
                raise ValueError("unreviewed non-BCD source")
            if op=="Bcd8":
                decoded=sum(((b>>4)*10+(b&15))*100**i
                            for i,b in enumerate(buf))*scale
            elif op=="BcdVersion6":
                decoded="".join(f"{b:02X}" for b in reversed(buf))
            else:
                decoded=int.from_bytes(buf,"little")
            if e.get(field)!=decoded:
                # ErrorFlags is a status field, represented as a string.
                if not (field=="status" and decoded==0 and e["status"]=="OK"):
                    raise ValueError("supercom source JSON conversion mismatch")
            anchors.append(offset)
        if anchors !=[15,123,158] or frame[21:23]!=b"\x42\x6c" or (
            frame[25:27]!=b"\x4c\x13"):
            raise ValueError("supercom verified auxiliary record keys changed")
        expected={"_","media","driver","name","id","timestamp",
                  "total_m3","software_version","status"}
        if set(e)!=expected:
            raise ValueError("new supercom fields need compiler review")
        if signatures is None:
            signatures=anchors
        elif signatures!=anchors:
            raise ValueError("supercom sparse source offsets changed")
        used_models.add((*mvt,cfg))
    if used_models!={(*m,0) for m in declared}:
        raise ValueError("supercom sources do not cover model variants")
    return descriptions,signatures,sorted(used_models),[(21,b"\x42\x6c"),
                                                          (25,b"\x4c\x13")]


def generate(path:Path):
    rec,statuses,sabotage,variants=compile_lansen(path/"lansenpu.xmq")
    superrules,offsets,models,guards=compile_supercom(path/"supercom587.xmq")
    out=[
        "// Source-compiled XMQ Lansen Pulse and Sontex Supercom587 profiles.",
        "// Shared native interpreter; fail closed on unsupported statuses.",
        "struct PulseVariant {uint16_t maker;uint8_t ver,medium;",
        " uint16_t config,length,fields_mask;uint8_t tpl;};",
        "struct PulseProfile {const Rule*rules;size_t rule_count;",
        " const PulseVariant*variants;size_t variant_count;",
        " const State*states;size_t state_count;uint8_t sabotage;};",
        "namespace lansenpu {",
        "inline constexpr Rule rules[]={",
    ]
    for field,key,width in rec:
        padded=list(key)+[0]*(5-len(key))
        if len(padded)!=5:raise ValueError("pulse record key too long")
        out.append(f' {{{json.dumps(field)},{{{",".join(f"0x{b:02X}" for b in padded)}}},'
                   f'{len(key)},{width},Op::Bcd12,1.0}},')
    out.extend(["};","inline constexpr PulseVariant variants[]={"])
    for maker,ver,medium,cfg,n,mask,tpl in variants:
        out.append(f" {{0x{maker:04X},0x{ver:02X},0x{medium:02X},"
                   f"0x{cfg:04X},{n},{mask},0x{tpl:02X}}},")
    out.extend(["};","inline constexpr State states[]={"])
    for tpl,txt in sorted(statuses.items()):
        out.append(f" {{0x{tpl:02X},0,{json.dumps(txt)}}},")
    out.extend([
        "};",
        "inline constexpr PulseProfile profile={rules,sizeof(rules)/sizeof(rules[0]),",
        " variants,sizeof(variants)/sizeof(variants[0]),",
        " states,sizeof(states)/sizeof(states[0]),",
        f" 0x{sabotage:02X}}};",
        "}",
        "struct SparseGuard {uint16_t at;uint8_t key[3];uint8_t length;};",
        "struct SparseProfile {const Model*models;size_t model_count;",
        " const Rule*rules;size_t rule_count;",
        " const uint16_t*offsets;size_t length;",
        " const SparseGuard*guards;size_t guard_count;",
        " const char*status_ok;};",
        "namespace supercom587 {",
        "inline constexpr Model models[]={",
    ])
    for maker,ver,medium,cfg in models:
        out.append(f" {{0x{maker:04X},0x{ver:02X},0x{medium:02X},"
                   f"0x{cfg:04X}}},")
    out.extend(["};","inline constexpr Rule rules[]={"])
    for name,key,width,op,scale in superrules:
        pad=list(key)+[0]*(5-len(key))
        out.append(f' {{{json.dumps(name)},{{{",".join(f"0x{b:02X}" for b in pad)}}},'
                   f'{len(key)},{width},Op::{op},{scale:.10g}}},')
    out.extend(["};","inline constexpr uint16_t offsets[]={"])
    for at in offsets: out.append(f" {at},")
    out.extend(["};","inline constexpr SparseGuard guards[]={"])
    for at,word in guards:
        padded=list(word)+[0]*(3-len(word))
        out.append(f' {{{at},{{{",".join(f"0x{b:02X}" for b in padded)}}},{len(word)}}},')
    out.extend(["};","inline constexpr SparseProfile profile={",
                " models,sizeof(models)/sizeof(models[0]),",
                " rules,sizeof(rules)/sizeof(rules[0]),",
                ' offsets,163,guards,sizeof(guards)/sizeof(guards[0]),"OK"',
                "};","}"])
    return "\n".join(out)+"\n"
