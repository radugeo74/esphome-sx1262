#!/usr/bin/env python3
"""Dependency-free structural reader for wmbusmeters XMQ driver definitions.

Experimental subset: nested blocks, repeated keys, bare values and quoted
multiline strings. Does not evaluate IXML, XPath, calculations or XMQ escapes.
"""
import argparse
import json
from pathlib import Path


class ParseError(ValueError):
    pass


def tokens(source):
    i, line, n = 0, 1, len(source)
    while i < n:
        c = source[i]
        if c == "\n":
            yield ("NL", "\n", line)
            line += 1
            i += 1
        elif c.isspace():
            i += 1
        elif source.startswith("//", i) or c == "#":
            while i < n and source[i] != "\n":
                i += 1
        elif c in "{}=,":
            yield (c, c, line)
            i += 1
        elif c in "'\\"":
            quote, begin, buf = c, line, []
            i += 1
            while i < n:
                c = source[i]
                if c == quote:
                    i += 1
                    break
                if c == "\\\\" and i + 1 < n:
                    buf.append(source[i:i+2])
                    i += 2
                    continue
                if c == "\n":
                    line += 1
                buf.append(c)
                i += 1
            else:
                raise ParseError(f"Unclosed string at line {begin}")
            yield ("STRING", "".join(buf), begin)
        else:
            start = i
            while i < n and not source[i].isspace() and source[i] not in "{}=,\\"'":
                if source.startswith("//", i):
                    break
                i += 1
            if i == start:
                raise ParseError(f"Unexpected character {source[i]!r} at line {line}")
            yield ("WORD", source[start:i], line)
    yield ("EOF", "", line)


def parse(source):
    ts = list(tokens(source))
    pos = 0

    def skip_newlines():
        nonlocal pos
        while ts[pos][0] == "NL":
            pos += 1

    def entries(end=False):
        nonlocal pos
        result = []
        while True:
            skip_newlines()
            kind, value, lineno = ts[pos]
            if kind == "EOF":
                if end:
                    raise ParseError(f"Missing closing brace before line {lineno}")
                return result
            if kind == "}":
                if not end:
                    raise ParseError(f"Unexpected closing brace at line {lineno}")
                pos += 1
                return result
            if kind != "WORD":
                raise ParseError(f"Expected key or block at line {lineno}, got {kind}")
            key = value
            pos += 1
            skip_newlines()
            kind = ts[pos][0]
            if kind == "{":
                pos += 1
                result.append({"block": key, "entries": entries(True)})
            elif kind == "=":
                pos += 1
                parts = []
                while ts[pos][0] not in ("NL", "EOF", "}"):
                    k, v, ln = ts[pos]
                    if k not in ("WORD", "STRING", ","):
                        raise ParseError(f"Unexpected {k} in value at line {ln}")
                    parts.append(v)
                    pos += 1
                result.append({"key": key, "value": "".join(parts) if len(parts) > 1 else (parts[0] if parts else "")})
            else:
                raise ParseError(f"Expected '=' or '{{' after {key} at line {lineno}")
    return entries()


def blocks(entries, name):
    return [e for e in entries if e.get("block") == name]


def values(entries, key):
    return [e["value"] for e in entries if e.get("key") == key]


def validate_driver(tree):
    ds = blocks(tree, "driver")
    if len(ds) != 1:
        raise ParseError(f"Expected one driver block, found {len(ds)}")
    d = ds[0]["entries"]
    if not values(d, "name") or not values(d, "meter_type"):
        raise ParseError("Driver is missing name or meter_type")
    return d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="+", type=Path)
    ap.add_argument("--out", type=Path)
    args = ap.parse_args()
    result = {}
    for path in args.paths:
        tree = parse(path.read_text(encoding="utf-8"))
        d = validate_driver(tree)
        count = sum(len(blocks(f["entries"], "field")) for fields in blocks(d, "fields") for f in [fields])
        ixml = sum(bool(values(field["entries"], "ixml")) for fields in blocks(d, "fields") for field in blocks(fields["entries"], "field"))
        print(f"{path.name}: driver={values(d,'name')[0]} fields={count} ixml_fields={ixml}")
        result[path.name] = tree
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
