#!/usr/bin/env python3
"""Generate C++ IXML subset tables from original XMQ driver grammars.

Unsupported operations fail rather than being silently skipped. Grammar
primitive definitions (-hex/-byte/-word) and >dvk metadata are handled by
the bounded runtime primitives and record mapping respectively.
"""
import argparse
import ast
import re
import sys
from pathlib import Path
from ixml_probe import extract_grammar, grammar_rules
from portable_xmq import parse, validate_driver, blocks, values

IDENT = re.compile(r"^[A-Za-z_][A-Za-z_0-9]*$")
HEX = re.compile(r"^'([A-Fa-f0-9]{2})'$")


def compile_grammar(source):
    rules = grammar_rules(source)
    output = []
    for name, options in rules.items():
        if name in ("hex", "byte", "word"):
            continue
        if not IDENT.fullmatch(name):
            raise ValueError(f"Unsupported rule name: {name}")
        entries = []
        if len(options) > 1:
            if not all(len(opt) == 1 and IDENT.fullmatch(opt[0]) for opt in options):
                raise ValueError(f"Unsupported alternatives in {name}")
            entries.append(("Choice", 0, "|".join(opt[0] for opt in options)))
        else:
            for item in options[0]:
                if item in ("byte", "word"):
                    entries.append((item.title(), 0, None))
                elif item == "byte*":
                    entries.append(("Rest", 0, None))
                elif item.startswith("@") and IDENT.fullmatch(item[1:]):
                    entries.append(("Capture", 0, item[1:]))
                elif HEX.fullmatch(item):
                    entries.append(("Literal", int(HEX.fullmatch(item)[1], 16), None))
                elif IDENT.fullmatch(item) and item in rules:
                    entries.append(("Call", 0, item))
                else:
                    raise ValueError(f"Unsupported token {item!r} in rule {name}")
        output.append((name, entries))
    if not any(name == "decode" for name, _ in output):
        raise ValueError("Missing decode rule")
    return output


def metadata(path, grammar):
    annotations = re.findall(r"(?m)^\s*(DV_[A-Za-z_0-9]+)>dvk\s*=\s*\+'([A-Fa-f0-9]+)'\s*\.", grammar)
    driver = validate_driver(parse(path.read_text(encoding="utf-8")))
    matches = []
    for group in blocks(driver, "fields"):
        for field in blocks(group["entries"], "field"):
            entries = field["entries"]
            names = values(entries, "name")
            keys = [key for m in blocks(entries, "match")
                    for key in values(m["entries"], "difvifkey")]
            if keys:
                if len(names) != 1 or len(keys) != 1:
                    raise ValueError("Ambiguous DIF/VIF mapping")
                matches.append((names[0], keys[0].upper()))
    if not annotations:
        raise ValueError(f"{path}: no IXML dvk capture annotations found")
    if not matches:
        raise ValueError(f"{path}: no field match.difvifkey definitions found")
    for name, key in annotations + matches:
        if not IDENT.fullmatch(name) or len(key) % 2 or not 4 <= len(key) <= 16:
            raise ValueError("Invalid generated mapping")
    return annotations, matches


def compile_calculations(path):
    driver = validate_driver(parse(path.read_text(encoding="utf-8")))
    result = []
    for group in blocks(driver, "fields"):
        for field in blocks(group["entries"], "field"):
            entries = field["entries"]
            name = values(entries, "name")
            expr = values(entries, "calculate")
            if not expr:
                continue
            if len(name) != 1 or len(expr) != 1:
                raise ValueError("Ambiguous calculate field")
            # Explicitly compile only an arithmetic subset; other formulas remain
            # available to the existing host-only calendar interpreter.
            if not all(ch.isalnum() or ch in "_ +*()." for ch in expr[0]):
                continue
            expression = re.sub(r"(?<![A-Za-z_0-9])1m3\b", "1", expr[0])
            try:
                tree = ast.parse(expression, mode="eval")
            except SyntaxError:
                continue
            steps = []
            def walk(node):
                if isinstance(node, ast.Constant) and type(node.value) in (float, int):
                    steps.append(("Number", None, float(node.value)))
                elif isinstance(node, ast.Name) and IDENT.fullmatch(node.id):
                    key = node.id.removesuffix("_counter").removesuffix("_m3")
                    steps.append(("Field", key, 0))
                elif isinstance(node, ast.BinOp) and isinstance(node.op, (ast.Add, ast.Mult)):
                    walk(node.left)
                    walk(node.right)
                    steps.append(("Add" if isinstance(node.op, ast.Add) else "Multiply", None, 0))
                else:
                    raise ValueError("Unsupported expression")
            try:
                walk(tree.body)
            except ValueError:
                continue
            result.append((name[0], steps))
    return result


def generate(files):
    lines = ["// Generated from pinned original XMQ definitions; do not edit.",
             "#pragma once", '#include "ixml_subset.h"', '#include "calculate_subset.h"',
             "namespace generated_ixml {"]
    for path in files:
        grammars = extract_grammar(path)
        if len(grammars) != 1:
            raise ValueError(f"Expected one IXML grammar: {path}")
        namespace = path.stem
        if not IDENT.fullmatch(namespace):
            raise ValueError("Unsafe namespace")
        parsed = compile_grammar(grammars[0])
        annotations, matches = metadata(path, grammars[0])
        calculated = compile_calculations(path)
        lines.append(f"namespace {namespace} {{")
        for index, (name, entries) in enumerate(parsed):
            lines.append(f"inline constexpr xmq_portable::Step steps_{index}[] = {{")
            for op, arg, label in entries:
                label_str = 'nullptr' if label is None else '"' + label + '"'
                lines.append(f"  {{xmq_portable::Op::{op}, {arg}, {label_str}}},")
            lines.append("};")
        lines.append("inline constexpr xmq_portable::Rule rules[] = {")
        for index, (name, entries) in enumerate(parsed):
            lines.append(f'  {{"{name}", steps_{index}, {len(entries)}}},')
        lines.append("};")
        lines.append("inline constexpr size_t count = sizeof(rules) / sizeof(rules[0]);")
        lines.append("inline constexpr xmq_portable::CaptureKey capture_keys[] = {")
        for name, key in annotations:
            lines.append(f'  {{"{name}", "{key.upper()}"}},')
        lines.append("};")
        lines.append("inline constexpr xmq_portable::FieldKey field_keys[] = {")
        for name, key in matches:
            lines.append(f'  {{"{name}", "{key}"}},')
        lines.append("};")
        for index, (fieldname, steps) in enumerate(calculated):
            lines.append(f"inline constexpr xmq_portable::CalcStep calc_steps_{index}[] = {{")
            for op, name, number in steps:
                label = 'nullptr' if name is None else '"' + name + '"'
                lines.append(f"  {{xmq_portable::CalcOp::{op}, {label}, {number}}},")
            lines.append("};")
        if calculated:
            lines.append("inline constexpr xmq_portable::CalcFormula calculations[] = {")
            for index, (fieldname, steps) in enumerate(calculated):
                lines.append(f'  {{"{fieldname}", calc_steps_{index}, {len(steps)}}},')
            lines.append("};")
            lines.append(f"inline constexpr size_t calculation_count = {len(calculated)};")
        lines.append("}")
        print(f"Generated {namespace}: {len(parsed)} rule tables", file=sys.stderr)
    lines.append("}")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+", type=Path)
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()
    result = generate(args.files)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(result)
