#!/usr/bin/env python3
"""Upstream C-comment parsing regression. No private meter payloads."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental/xmq/tools"))
from portable_xmq import parse, validate_driver, values, ParseError

source = """/* comment with fake driver {\n   name = not_a_real_driver\n*/
driver {
  name = sample // inline comment
  meter_type = WaterMeter
  /* comment in a block
     fields { bad = value } */
  note = "literal /* this is part of the value */ end"
  test = 1 /* trailing comment */ 
}
"""
data = validate_driver(parse(source))
assert values(data, "name") == ["sample"]
assert values(data, "note") == ["literal /* this is part of the value */ end"]
assert values(data, "test") == ["1"]

for broken in ("/* unclosed", "driver { name = sample\n /* unclosed"):
    try:
        parse(broken)
    except ParseError as err:
        assert "Unclosed block comment" in str(err)
    else:
        raise AssertionError("Unclosed upstream block comment accepted")
print("PASS: upstream C-style comments are transparent outside quoted values")
