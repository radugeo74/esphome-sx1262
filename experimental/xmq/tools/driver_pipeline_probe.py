#!/usr/bin/env python3
"""IXML -> synthetic DIF/VIF records -> XMQ calculate chain (host experiment).

Uses original driver field names, match.difvifkey and calculate expressions.
Currently supports unsigned 16-bit records and standard volume VIF 0x10..0x17.
Other types fail explicitly. Not yet a complete wireless M-Bus decoder.
"""
import re
from pathlib import Path
from calculate_probe import evaluate, CalculateError
from ixml_probe import extract_grammar, grammar_rules, run_ixml
from portable_xmq import blocks, parse, validate_driver, values

ANNOTATION = re.compile(r"(?m)^\s*(DV_[A-Za-z_0-9]+)>dvk\s*=\s*\+'([0-9A-Fa-f]+)'\s*\.")


class BridgeError(ValueError):
    pass


def decode(path: Path, payload: str):
    driver = validate_driver(parse(path.read_text(encoding="utf-8")))
    grammar = extract_grammar(path)
    if len(grammar) != 1:
        raise BridgeError("Only one IXML grammar currently supported")
    captures = run_ixml(grammar_rules(grammar[0]), payload)
    annotations = dict(ANNOTATION.findall(grammar[0]))
    records = {}
    for name, raw in captures.items():
        if name not in annotations:
            continue
        key = annotations[name].upper()
        binary = bytes.fromhex(raw)
        if len(binary) != 2:
            raise BridgeError(f"Unsupported data length for {name}: {len(binary)}")
        if key in records:
            raise BridgeError(f"Duplicate DIF/VIF key {key}")
        value = int.from_bytes(binary, "little")
        vif = int(key[-2:], 16)
        if not 0x10 <= vif <= 0x17:
            raise BridgeError(f"Unsupported VIF {vif:#x} for {name}")
        records[key] = {"raw": value, "m3": value * (10 ** (vif - 0x16))}

    env = {}
    computed = {}
    pending = []
    for group in blocks(driver, "fields"):
        for field in blocks(group["entries"], "field"):
            e = field["entries"]
            names = values(e, "name")
            if not names:
                raise BridgeError("Field missing name")
            fieldname = names[0]
            matches = [v for m in blocks(e, "match") for v in values(m["entries"], "difvifkey")]
            if matches:
                if len(matches) != 1:
                    raise BridgeError(f"Ambiguous match: {fieldname}")
                key = matches[0].upper()
                if key in records:
                    env[fieldname + "_counter"] = records[key]["raw"]
                    # Do not use this numeric scaling for non-volume VIFs.
                    env[fieldname + "_m3"] = records[key]["m3"]
            formulas = values(e, "calculate")
            if formulas:
                if len(formulas) != 1:
                    raise BridgeError(f"Ambiguous formula: {fieldname}")
                pending.append((fieldname, formulas[0]))
    while pending:
        next_pending = []
        progress = False
        for fieldname, expression in pending:
            try:
                result = evaluate(expression, env)
            except CalculateError as exc:
                if str(exc).startswith("Unresolved field:"):
                    next_pending.append((fieldname, expression))
                    continue
                raise BridgeError(f"{fieldname}: {exc}") from exc
            env[fieldname + "_m3"] = result
            computed[fieldname + "_m3"] = result
            progress = True
        if not progress:
            raise BridgeError("Unresolved/cyclic formula dependencies: " + ", ".join(n for n, _ in next_pending))
        pending = next_pending
    return computed
