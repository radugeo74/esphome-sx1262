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
        '// Lab-only. One journal per setter-processing scope, no sensor writes.\n'
        'namespace {\n'
        'thread_local xmq_runtime::LegacyWriteJournal *lab_journal = nullptr;\n'
        'struct LabWriteScope {\n'
        '  xmq_runtime::LegacyWriteJournal &journal;\n'
        '  xmq_runtime::LegacyWriteJournal *previous;\n'
        '  explicit LabWriteScope(xmq_runtime::LegacyWriteJournal &j)\n'
        '      : journal(j), previous(lab_journal) {\n'
        '    journal.begin_telegram(); lab_journal = &journal;\n'
        '  }\n'
        '  ~LabWriteScope() { lab_journal = previous; journal.end_telegram(); }\n'
        '};\n'
        '}  // namespace'
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
        '  (void)lab_writes.counts();\n\n'
        '  // All done....'
    )
    source = replace_exact(
        source,
        'void MeterCommonImplementation::setNumericValue(FieldInfo *fi, DVEntry *dve,\n'
        '                                                Unit u, double v) {',
        'void MeterCommonImplementation::setNumericValue(FieldInfo *fi, DVEntry *dve,\n'
        '                                                Unit u, double v) {\n'
        '  if (lab_journal) lab_journal->numeric_write(fi->vname());'
    )
    source = replace_exact(
        source,
        'void MeterCommonImplementation::setStringValue(FieldInfo *fi, std::string v,\n'
        '                                               DVEntry *dve) {',
        'void MeterCommonImplementation::setStringValue(FieldInfo *fi, std::string v,\n'
        '                                               DVEntry *dve) {\n'
        '  if (lab_journal) lab_journal->string_write(fi->vname());'
    )
    if source.count('lab_journal->numeric_write(') != 1 or source.count('lab_journal->string_write(') != 1:
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
