#!/usr/bin/env python3
"""Fail-closed, build-only instrumentation of a COPY of legacy meters.cc.

Never edits components/wmbus_common, never changes the active firmware.
This is a source-level integration fixture for the future isolated common
decoder. The emitted translation unit is not yet linked into ESPHome.
"""
from pathlib import Path
import argparse
import hashlib

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "components/wmbus_common/meters.cc"


def replace_exact(source: str, before: str, after: str) -> str:
    occurrences = source.count(before)
    if occurrences != 1:
        raise ValueError(f"unsafe source drift: expected one anchor, found {occurrences}: {before[:70]}")
    return source.replace(before, after, 1)


def instrument(source: str) -> str:
    source = replace_exact(
        source, '#include "meters_common_implementation.h"',
        '#include "meters_common_implementation.h"\n'
        '#include "xmq_legacy_write_journal.h"\n'
        '#include "xmq_legacy_write_report.h"\n'
        '#include "xmq_legacy_write_observer.h"\n'
        '// Lab-only. One journal per setter-processing scope, no sensor writes.\n'
        'namespace {\n'
        'thread_local xmq_runtime::LegacyWriteJournal *lab_journal = nullptr;\n'
        'using LabObserver = xmq_runtime::LegacyWriteObserver;\n'
        'thread_local LabObserver lab_observer = nullptr;\n'
        'using LabBoundObserver = void (*)(const void *, unsigned long long, xmq_runtime::LegacyWriteReport) noexcept;\n'
        'thread_local LabBoundObserver lab_bound_observer = nullptr;\n'
        '[[maybe_unused]] thread_local unsigned long long lab_frame_sequence = 0;\n'
        'struct LabWriteScope {\n'
        '  xmq_runtime::LegacyWriteJournal &journal;\n'
        '  xmq_runtime::LegacyWriteJournal *previous;\n'
        '  explicit LabWriteScope(xmq_runtime::LegacyWriteJournal &j)\n'
        '      : journal(j), previous(lab_journal) {\n'
        '    journal.begin_telegram(); lab_journal = &journal;\n'
        '  }\n'
        '  ~LabWriteScope() { lab_journal = previous; journal.end_telegram(); }\n'
        '};\n'
        '}  // namespace\n'
        'extern "C" void xmq_stage_a_register_legacy_write_observer(\n'
        '    void (*callback)(xmq_runtime::LegacyWriteReport) noexcept) noexcept {\n'
        '  lab_observer = callback;\n'
        '}\n'
        'extern "C" void xmq_stage_a_register_legacy_write_observer_bound(\n'
        '    void (*callback)(const void *, unsigned long long, xmq_runtime::LegacyWriteReport) noexcept) noexcept {\n'
        '  lab_bound_observer = callback;\n'
        '}'
    )
    source = replace_exact(
        source,
        '  // Invoke standardized field extractors!\n  processFieldExtractors(&t);',
        '  // Lab scope includes extractors, proprietary processContent, calculators.\n'
        '  xmq_runtime::LegacyWriteJournal lab_writes;\n'
        '  {\n'
        '    LabWriteScope lab_write_scope(lab_writes);\n'
        '    // Invoke standardized field extractors!\n'
        '    processFieldExtractors(&t);'
    )
    source = replace_exact(
        source,
        '  processFieldCalculators();\n\n  // All done....',
        '    processFieldCalculators();\n'
        '  }\n'
        '  // The journal can be consumed here in an isolated test adapter;\n'
        '  // no values, IDs, payloads or keys are logged.\n'
        '  (void)lab_writes.counts();\n'
        '  // Isolated lab hook only: aggregate masks/counts, no IDs or values.\n'
        '  if (lab_observer) {\n'
        '    try {\n'
        '      const auto kind = driverName().str() == "fhkvdataiii"\n'
        '          ? xmq_runtime::LegacyReportDriver::Fhkvdataiii\n'
        '          : (driverName().str() == "mkradio4"\n'
        '                 ? xmq_runtime::LegacyReportDriver::Mkradio4\n'
        '                 : xmq_runtime::LegacyReportDriver::Unknown);\n'
        '      (void)xmq_runtime::dispatch_legacy_write_report(\n'
        '          lab_writes, kind, lab_observer);\n'
        '    } catch (...) { /* legacy decoding always continues */ }\n'
        '  }\n'
        '  if (lab_bound_observer) {\n'
        '    try {\n'
        '      const auto kind = driverName().str() == "fhkvdataiii"\n'
        '          ? xmq_runtime::LegacyReportDriver::Fhkvdataiii\n'
        '          : (driverName().str() == "mkradio4"\n'
        '                 ? xmq_runtime::LegacyReportDriver::Mkradio4\n'
        '                 : xmq_runtime::LegacyReportDriver::Unknown);\n'
        '      if (kind != xmq_runtime::LegacyReportDriver::Unknown) {\n'
        '        // Per-thread success-only token; never log or reuse for an earlier frame.\n'
        '        const auto seq = ++lab_frame_sequence;\n'
        '        lab_bound_observer(static_cast<const void *>(this), seq,\n'
        '                           xmq_runtime::summarize_legacy_writes(lab_writes, kind));\n'
        '      }\n'
        '    } catch (...) { /* never interrupt legacy updates */ }\n'
        '  }\n\n'
        '  // All done....'
    )
    source = replace_exact(
        source,
        'void MeterCommonImplementation::setNumericValue(FieldInfo *fi, DVEntry *dve,\n'
        '                                                Unit u, double v) {',
        'void MeterCommonImplementation::setNumericValue(FieldInfo *fi, DVEntry *dve,\n'
        '                                                Unit u, double v) {\n'
        '  // Record only AFTER the original storage operation succeeds.'
    )
    source = replace_exact(
        source,
        '        field_name_no_unit, fi->displayUnit())] = NumericField(u, v, fi);',
        '        field_name_no_unit, fi->displayUnit())] = NumericField(u, v, fi);\n'
        '    if (lab_journal) (void)lab_journal->try_numeric_write(field_name_no_unit);'
    )
    source = replace_exact(
        source,
        '        field_name_no_unit, fi->displayUnit())] = NumericField(u, v, fi, *dve);',
        '        field_name_no_unit, fi->displayUnit())] = NumericField(u, v, fi, *dve);\n'
        '    if (lab_journal) (void)lab_journal->try_numeric_write(field_name_no_unit);'
    )
    source = replace_exact(
        source,
        'void MeterCommonImplementation::setStringValue(FieldInfo *fi, std::string v,\n'
        '                                               DVEntry *dve) {',
        'void MeterCommonImplementation::setStringValue(FieldInfo *fi, std::string v,\n'
        '                                               DVEntry *dve) {\n'
        '  // Observe the committed dynamic field name, never a template name.'
    )
    source = replace_exact(
        source,
        '    std::string field_name_no_unit = fi->vname();\n'
        '    string_values_[field_name_no_unit] = StringField(v, fi);',
        '    std::string field_name_no_unit = fi->vname();\n'
        '    string_values_[field_name_no_unit] = StringField(v, fi);\n'
        '    if (lab_journal) (void)lab_journal->try_string_write(field_name_no_unit);'
    )
    source = replace_exact(
        source,
        '    field_name_no_unit = fi->generateFieldNameNoUnit(this, dve);\n'
        '    string_values_[field_name_no_unit] = StringField(v, fi);',
        '    field_name_no_unit = fi->generateFieldNameNoUnit(this, dve);\n'
        '    string_values_[field_name_no_unit] = StringField(v, fi);\n'
        '    if (lab_journal) (void)lab_journal->try_string_write(field_name_no_unit);'
    )
    if source.count('lab_journal->try_numeric_write(') != 2 or source.count('lab_journal->try_string_write(') != 2:
        raise ValueError("unexpected instrumented setter count")
    return source


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    src = SOURCE.read_text()
    result = instrument(src)
    dest = args.output.resolve()
    if dest == SOURCE.resolve() or SOURCE.resolve() in dest.parents:
        raise ValueError("refusing to overwrite production common decoder")
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(result)
    print("LEGACY_SETTER_STAGING_OK source_sha256=" + hashlib.sha256(src.encode()).hexdigest()[:12])


if __name__ == "__main__":
    main()
