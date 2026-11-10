#!/usr/bin/env python3
"""Bounded hex-IXML compiler for public Vario451 CI=A2 example.

The original grammar consumes ASCII hex nibbles. All three energy formulas,
DIF/VIF annotations and the original complete fixture are validated. Only
the fixture-proven TCH/27/C3 profile is emitted, not untested TCH/27/04.
"""
import argparse
import json
import math
import re
from pathlib import Path
from generate_driver_registry import manufacturer_code
from ixml_probe import extract_grammar, grammar_rules
from portable_xmq import blocks, parse, validate_driver, values


def layout(grammar):
    rules = grammar_rules(grammar)
    if not re.search(r"-hex\s*=\s*\['A'-'F';\s*'0'-'9'\]\s*\.", grammar):
        raise ValueError("vario451: changed hex primitive")
    events = []
    def expand(name, stack):
        if name in stack or len(stack) >= 12:
            raise ValueError("vario451: recursive IXML")
        options = rules.get(name)
        if options is None or len(options) != 1:
            raise ValueError("vario451: unsupported grammar choice")
        for token in options[0]:
            if token == "hex":
                events.append(("hex",name,""))
            elif token == "hex*":
                events.append(("rest",name,""))
            elif token in ("@DV_PREV","@DV_CURR"):
                events.append(("capture",name,token[1:]))
            elif token.lstrip("-") in rules and token.lstrip("-") != "hex":
                expand(token.lstrip("-"),stack+(name,))
            else:
                raise ValueError(f"vario451: unsupported IXML token {token}")
    expand("decode",())
    if not events or events[-1][0] != "rest" or sum(k=="rest" for k,_,_ in events) != 1:
        raise ValueError("vario451: rest must be terminal")
    position = 0
    captures = {}
    for kind,owner,name in events[:-1]:
        if kind == "hex":
            position += 1
        elif kind == "capture":
            if name in captures or position < 4 or position%2:
                raise ValueError("vario451: invalid capture")
            if (name,owner) not in (("DV_PREV","prev_val"),("DV_CURR","curr_val")):
                raise ValueError("vario451: misplaced capture")
            captures[name] = (position-4)//2
        else:
            raise ValueError("vario451: nonterminal rest")
    if captures != {"DV_PREV":3,"DV_CURR":7} or position != 18:
        raise ValueError("vario451: unreviewed nibble layout")
    return captures["DV_PREV"],captures["DV_CURR"]


def scaling(fields):
    formulas={}
    for field in fields:
        data=field["entries"]
        exp=values(data,"calculate")
        if exp:
            names=values(data,"name")
            if len(names)!=1 or len(exp)!=1 or names[0] in formulas:
                raise ValueError("vario451: ambiguous formula")
            formulas[names[0]]=exp[0].strip()
    forms={"total":r"\(\s*prev_raw_counter\s*\+\s*curr_raw_counter\s*\)",
           "current":r"curr_raw_counter","previous":r"prev_raw_counter"}
    if set(formulas)!=set(forms):
        raise ValueError("vario451: missing or extra formula")
    divs=[]
    for key,expr in forms.items():
        match=re.fullmatch(
            rf"{expr}\s*/\s*([0-9]+(?:\.[0-9]+)?)\s+counter\s*\*\s*1\s+kwh",
            formulas[key])
        if not match:
            raise ValueError(f"vario451: unsupported {key} formula")
        number=float(match[1])
        if not math.isfinite(number) or number<=0:
            raise ValueError("vario451: bad divisor")
        divs.append(number)
    if len(set(divs))!=1:
        raise ValueError("vario451: mismatched conversion factors")
    return divs[0]


def generate(path: Path):
    if path.stem != "vario451":
        raise ValueError("vario451: wrong XMQ file name")
    tree=validate_driver(parse(path.read_text(encoding="utf-8")))
    if values(tree,"name")!=["vario451"]:
        raise ValueError("vario451: wrong XMQ driver name")
    grammars=extract_grammar(path)
    if len(grammars)!=1:
        raise ValueError("vario451: expected one grammar")
    grammar=grammars[0]
    prev,curr=layout(grammar)
    annotations=dict(re.findall(
        r"(?m)^\s*(DV_[A-Za-z_0-9]+)>dvk\s*=\s*\+'([0-9A-Fa-f]+)'\s*\.",
        grammar))
    if annotations!={"DV_PREV":"027E","DV_CURR":"027F"}:
        raise ValueError("vario451: changed capture mappings")
    groups=blocks(tree,"fields")
    if len(groups)!=1:
        raise ValueError("vario451: duplicate field groups")
    fields=blocks(groups[0]["entries"],"field")
    by_name={}
    for f in fields:
        data=f["entries"]
        names=values(data,"name")
        if len(names)!=1 or names[0] in by_name:
            raise ValueError("vario451: duplicated field")
        by_name[names[0]]=data
    for name,key in (("prev_raw","027E"),("curr_raw","027F")):
        match=blocks(by_name.get(name,[]),"match")
        if len(match)!=1 or values(match[0]["entries"],"difvifkey")!=[key]:
            raise ValueError("vario451: changed raw DIF/VIF mapping")
        if (values(by_name[name],"dif_signedness")!=["Unsigned"] or
            values(by_name[name],"vif_scaling")!=["None"]):
            raise ValueError("vario451: raw meter unit changed")
    divisor=scaling(fields)
    detect=blocks(tree,"detect")
    if len(detect)!=1 or sorted(values(detect[0]["entries"],"mvt")) != ["TCH,27,04","TCH,27,C3"]:
        raise ValueError("vario451: original detect models changed")
    tests=blocks(tree,"tests")
    if len(tests)!=1:
        raise ValueError("vario451: missing tests")
    fixtures=blocks(tests[0]["entries"],"test")
    if len(fixtures)!=1:
        raise ValueError("vario451: original fixture set changed")
    test=fixtures[0]["entries"]
    telegram=values(test,"telegram"); results=values(test,"json")
    if len(telegram)!=1 or len(results)!=1:
        raise ValueError("vario451: missing fixture data")
    f=bytes.fromhex(telegram[0].replace("_","").replace("|",""))
    expected=json.loads(results[0])
    maker=manufacturer_code("TCH")
    if (len(f)!=56 or f[0]!=55 or f[1]!=0x44 or
        f[2]|f[3]<<8 != maker or f[8]!=0x27 or f[9]!=0xC3 or
        f[10]!=0xA2 or expected.get("driver")!="vario451"):
        raise ValueError("vario451: wrong original RF profile")
    actual_id="".join(f"{b:02X}" for b in reversed(f[4:8]))
    if actual_id!=expected.get("id"):
        raise ValueError("vario451: fixture ID drift")
    payload=f[11:]
    vprev=int.from_bytes(payload[prev:prev+2],"little")
    vcurr=int.from_bytes(payload[curr:curr+2],"little")
    expected_values={"total_kwh":(vprev+vcurr)/divisor,
                     "current_kwh":vcurr/divisor,
                     "previous_kwh":vprev/divisor}
    if any(k not in expected or abs(expected[k]-v)>0.000001
           for k,v in expected_values.items()):
        raise ValueError("vario451: XMQ JSON arithmetic mismatch")
    return "\n".join([
        "// Generated from original GPL-3.0-or-later vario451.xmq.",
        "// Only the original TCH/27/C3 CI=A2 public fixture is proven.",
        "#pragma once","#include <cstddef>","#include <cstdint>",
        "namespace xmq_generated_vario451 {",
        f"inline constexpr uint16_t manufacturer = 0x{maker:04X};",
        "inline constexpr uint8_t version = 0x27;",
        "inline constexpr uint8_t device_type = 0xC3;",
        "inline constexpr uint8_t ci = 0xA2;",
        f"inline constexpr size_t previous_payload_offset = {prev};",
        f"inline constexpr size_t current_payload_offset = {curr};",
        f"inline constexpr double kwh_divisor = {divisor:.12g};",
        "}",""])


if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("--xmq",type=Path,
                    default=Path(__file__).resolve().parents[1]/"drivers"/"vario451.xmq")
    ap.add_argument("--out",required=True,type=Path)
    args=ap.parse_args()
    args.out.write_text(generate(args.xmq),encoding="utf-8")
