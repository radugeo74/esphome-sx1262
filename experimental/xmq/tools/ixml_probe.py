#!/usr/bin/env python3
"""Bounded IXML extraction experiment. NOT a complete IXML interpreter.

Reads the original embedded IXML grammar and evaluates sequence, alternatives,
hex literals, named rules, byte/word, capture names and trailing byte*.
Only tests fixtures marking the manufacturer payload with '_'.
"""
import argparse
import json
import re
from pathlib import Path
from portable_xmq import blocks, parse, validate_driver, values

ROOT = Path(__file__).resolve().parents[1]
RULE = re.compile(r"(?ms)^\s*(-?[A-Za-z_][A-Za-z_0-9]*)\s*=\s*(.*?)\.")
TOKEN = re.compile(r"^'([0-9A-Fa-f]+)'$")


def extract_grammar(driver_path):
    d = validate_driver(parse(driver_path.read_text(encoding="utf-8")))
    grammars = []
    for fs in blocks(d, "fields"):
        for field in blocks(fs["entries"], "field"):
            grammar = values(field["entries"], "ixml")
            if grammar:
                grammars.extend(grammar)
    if not grammars:
        raise ValueError(f"No IXML definitions in {driver_path}")
    return grammars


def grammar_rules(grammar):
    rules = {}
    for name, expression in RULE.findall(grammar):
        if name.startswith("-"):
            name = name[1:]
        if name in rules:
            raise ValueError("Duplicate rule: " + name)
        rules[name] = [tuple(x.strip() for x in part.split(",") if x.strip())
                       for part in expression.strip().split("|")]
    if "decode" not in rules:
        raise ValueError("Missing decode entrypoint")
    return rules


def run_ixml(rules, payload):
    raw = bytes.fromhex(payload)
    def sequence(parts, offset, captures, depth):
        if depth > 32:
            raise ValueError("IXML recursion depth exceeded")
        current = dict(captures)
        for piece in parts:
            if piece.startswith("@"):
                if "_last" not in current:
                    return None
                current[piece[1:]] = current["_last"]
                continue
            if piece.endswith("*") and piece[:-1] == "byte":
                current["_last"] = raw[offset:]
                offset = len(raw)
                continue
            lit = TOKEN.fullmatch(piece)
            if lit:
                expected = bytes.fromhex(lit.group(1))
                if raw[offset:offset+len(expected)] != expected:
                    return None
                offset += len(expected)
            elif piece in ("byte", "word"):
                size = 1 if piece == "byte" else 2
                if offset + size > len(raw):
                    return None
                current["_last"] = raw[offset:offset+size]
                offset += size
            elif piece in rules:
                nested = rule(piece, offset, current, depth + 1)
                if nested is None:
                    return None
                offset, current = nested
            else:
                raise ValueError(f"Unsupported IXML token: {piece}")
        return offset, current

    def rule(name, offset, captures, depth):
        for option in rules[name]:
            matched = sequence(option, offset, captures, depth)
            if matched is not None:
                return matched
        return None

    result = rule("decode", 0, {}, 0)
    if result is None:
        raise ValueError("Payload did not match selected IXML grammar")
    consumed, captures = result
    if consumed != len(raw):
        raise ValueError(f"IXML did not consume payload: {consumed}/{len(raw)}")
    return {key: value.hex().upper() for key, value in captures.items() if not key.startswith("_")}


def marked_fixtures(source):
    # Official test telegrams with explicit '_' separating transport headers and payload.
    for body in re.findall(r"(?ms)^\s*test\s*\{(.*?)^\s*\}", source):
        match = re.search(r"(?m)^\s*telegram\s*=\s*(\S+)", body)
        if match and "_" in match.group(1):
            yield match.group(1).split("_", 1)[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()
    report = {}
    for path in sorted((ROOT / "drivers").glob("*.xmq")):
        grammars = extract_grammar(path)
        if len(grammars) != 1:
            raise ValueError(f"Expected one IXML grammar: {path}")
        rules = grammar_rules(grammars[0])
        fixtures = list(marked_fixtures(path.read_text(encoding="utf-8")))
        if not fixtures:
            raise ValueError(f"No marked fixtures: {path}")
        results = [run_ixml(rules, raw) for raw in fixtures]
        report[path.stem] = {"rule_count": len(rules), "fixtures": results}
        print(path.name, "rules", len(rules), "fixture results", results, flush=True)
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    main()
