#!/usr/bin/env python3
"""Read-only staging contract for proprietary legacy setter integration."""
import hashlib
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE))
from stage_legacy_setter_instrumentation import SOURCE, instrument, main  # noqa: E402

original = SOURCE.read_bytes()
baseline_hash = hashlib.sha256(original).hexdigest()
result = instrument(original.decode())
assert result != original.decode()
assert result.count("LabWriteScope lab_write_scope(lab_writes);") == 1
assert result.count("lab_journal->numeric_write(fi->vname());") == 1
assert result.count("lab_journal->string_write(fi->vname());") == 1
assert result.index("LabWriteScope lab_write_scope") < result.index("processFieldExtractors(&t);")
assert result.index("processFieldExtractors(&t);") < result.index("processContent(&t);")
assert result.index("processContent(&t);") < result.index("processFieldCalculators();")
assert result.index("processFieldCalculators();") < result.index("(void)lab_writes.counts();")
assert result.index("~LabWriteScope()") < result.index("void MeterCommonImplementation::setNumericValue")
assert "thread_local xmq_runtime::LegacyWriteJournal *lab_journal" in result
assert result.count("processFieldExtractors(&t);") == 1
assert result.count("processFieldCalculators();") == 1
assert result.count("  triggerUpdate(&t);") == 1
assert "lab_journal->numeric_write" not in original.decode()
assert "lab_journal->string_write" not in original.decode()
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest() == baseline_hash

for fragment in (
    '#include "meters_common_implementation.h"',
    '  processFieldCalculators();\n\n  // All done....',
    'void MeterCommonImplementation::setNumericValue(FieldInfo *fi, DVEntry *dve,',
    'void MeterCommonImplementation::setStringValue(FieldInfo *fi, std::string v,',
):
    try:
        instrument(original.decode().replace(fragment, "SOURCE_DRIFT", 1))
    except ValueError:
        pass
    else:
        raise AssertionError("staging did not fail closed on source drift")

with TemporaryDirectory() as temp:
    out = Path(temp) / "meters-instrumented.cc"
    old_argv = sys.argv
    try:
        sys.argv = ["stage_legacy_setter_instrumentation.py", "--output", str(out)]
        main()
    finally:
        sys.argv = old_argv
    assert out.read_text() == result
    assert hashlib.sha256(SOURCE.read_bytes()).hexdigest() == baseline_hash

print("LEGACY_SETTER_STAGING_TEST_OK: both real setter sites + RAII per-frame scope; active components unchanged")
