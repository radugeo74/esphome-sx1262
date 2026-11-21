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
# Both no-DV and DV-generated branches must report the actual stored key,
# and only after successful insertion into the original value map.
assert result.count("lab_journal->try_numeric_write(field_name_no_unit);") == 2
assert result.count("lab_journal->try_string_write(field_name_no_unit);") == 2
assert "lab_journal->try_numeric_write(fi->vname())" not in result
assert "lab_journal->try_string_write(fi->vname())" not in result
numeric = result.split("void MeterCommonImplementation::setNumericValue(FieldInfo *fi, DVEntry *dve,", 1)[1].split(
    "void MeterCommonImplementation::setNumericValue(std::string vname", 1)[0]
string = result.split("void MeterCommonImplementation::setStringValue(FieldInfo *fi, std::string v,", 1)[1].split(
    "void MeterCommonImplementation::setStringValue(std::string vname", 1)[0]
assert numeric.count("try_numeric_write(field_name_no_unit)") == 2
assert string.count("try_string_write(field_name_no_unit)") == 2
assert numeric.index("NumericField(u, v, fi);") < numeric.index("try_numeric_write(field_name_no_unit)")
assert numeric.index("NumericField(u, v, fi, *dve);") < numeric.rindex("try_numeric_write(field_name_no_unit)")
assert string.index("StringField(v, fi);") < string.index("try_string_write(field_name_no_unit)")
assert string.rindex("StringField(v, fi);") < string.rindex("try_string_write(field_name_no_unit)")
assert numeric.index("generateFieldNameNoUnit(this, dve)") < numeric.rindex("try_numeric_write(field_name_no_unit)")
assert string.index("generateFieldNameNoUnit(this, dve)") < string.rindex("try_string_write(field_name_no_unit)")
assert result.count("xmq_stage_a_register_legacy_write_observer(") == 1
assert result.count("xmq_stage_a_register_legacy_write_observer_bound(") == 1
assert result.count("lab_bound_observer(static_cast<const void *>(this),") == 1
assert result.count("const auto seq = ++lab_frame_sequence;") == 1
assert result.index("processFieldCalculators();") < result.index("const auto seq = ++lab_frame_sequence;")
assert result.count("[[maybe_unused]] thread_local unsigned long long lab_frame_sequence = 0;") == 1
assert result.index("processFieldCalculators();") < result.index(
    "lab_bound_observer(static_cast<const void *>(this),")
assert result.index("lab_bound_observer(static_cast<const void *>(this),") < result.index(
    "triggerUpdate(&t);")
assert "xmq_stage_a_register_legacy_write_observer_bound" not in original.decode()

assert result.count("xmq_runtime::dispatch_legacy_write_report(") == 1
assert result.index("processFieldCalculators();") < result.index("xmq_runtime::dispatch_legacy_write_report(")
assert result.index("xmq_runtime::dispatch_legacy_write_report(") < result.index("triggerUpdate(&t);")
assert result.count('#include "xmq_legacy_write_report.h"') == 1
assert result.count('#include "xmq_legacy_write_observer.h"') == 1
assert "summarize_legacy_writes" not in original.decode()

assert result.index("LabWriteScope lab_write_scope") < result.index("processFieldExtractors(&t);")
assert result.index("processFieldExtractors(&t);") < result.index("processContent(&t);")
assert result.index("processContent(&t);") < result.index("processFieldCalculators();")
assert result.index("processFieldCalculators();") < result.index("(void)lab_writes.counts();")
assert result.index("~LabWriteScope()") < result.index("void MeterCommonImplementation::setNumericValue")
assert "thread_local xmq_runtime::LegacyWriteJournal *lab_journal" in result
assert result.count("processFieldExtractors(&t);") == 1
assert result.count("processFieldCalculators();") == 1
assert result.count("  triggerUpdate(&t);") == 1
assert "lab_journal->try_numeric_write" not in original.decode()
assert "lab_journal->try_string_write" not in original.decode()
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

print("LEGACY_SETTER_STAGING_TEST_OK: post-commit actual field keys in both numeric/string DV and no-DV branches; source untouched")
