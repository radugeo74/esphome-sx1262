#!/usr/bin/env python3
"""Execute the exact instrumented legacy numeric/string setter bodies on host.

Uses synthetic C++ stand-ins for FieldInfo, DVEntry and the storage maps.
This does NOT instantiate the full MeterCommonImplementation decoder.
"""
from pathlib import Path
import subprocess
import tempfile

from stage_legacy_setter_instrumentation import SOURCE, instrument

HERE = Path(__file__).resolve().parent
STAGE = HERE.parent


def extract_method(src: str, signature: str) -> str:
    begin = src.find(signature)
    if begin < 0 or src.find(signature, begin + 1) != -1:
        raise ValueError("missing or ambiguous legacy setter implementation: " + signature)
    opening = src.find("{", begin)
    if opening < 0:
        raise ValueError("setter has no body")
    nesting = 0
    for pos in range(opening, len(src)):
        char = src[pos]
        if char == "{":
            nesting += 1
        elif char == "}":
            nesting -= 1
            if nesting == 0:
                return src[begin:pos + 1]
    raise ValueError("unterminated setter")


def main() -> None:
    staged = instrument(SOURCE.read_text())
    numeric = extract_method(
        staged,
        "void MeterCommonImplementation::setNumericValue(FieldInfo *fi, DVEntry *dve,",
    )
    string = extract_method(
        staged,
        "void MeterCommonImplementation::setStringValue(FieldInfo *fi, std::string v,",
    )
    if numeric.count("try_numeric_write(field_name_no_unit)") != 2:
        raise ValueError("numeric setter instrumentation drift")
    if string.count("try_string_write(field_name_no_unit)") != 2:
        raise ValueError("string setter instrumentation drift")

    # Synthetic types implement precisely the APIs referenced by the two
    # verbatim method definitions; no firmware or legacy source is modified.
    header = r"""
#include <map>
#include <string>
#include <utility>
#include <iostream>
#include <stdexcept>
#include "xmq_legacy_write_journal.h"
#include "xmq_legacy_write_report.h"

enum class Unit { Unknown, Unitless };
struct DVEntry {};
struct MeterCommonImplementation;
struct FieldInfo {
  std::string field;
  std::string vname() const { return field; }
  Unit displayUnit() const { return Unit::Unitless; }
  std::string generateFieldNameNoUnit(MeterCommonImplementation *, DVEntry *) const {
    return field + "_dv";
  }
};
struct NumericField {
  Unit unit;
  double value;
  FieldInfo *info;
  NumericField() : unit(Unit::Unknown), value(0), info(nullptr) {}
  NumericField(Unit u, double v, FieldInfo *fi) : unit(u), value(v), info(fi) {}
  NumericField(Unit u, double v, FieldInfo *fi, DVEntry &) :
      unit(u), value(v), info(fi) {}
};
struct StringField {
  std::string value;
  FieldInfo *info;
  StringField() : info(nullptr) {}
  StringField(std::string v, FieldInfo *fi) : value(v), info(fi) {}
};
struct MeterCommonImplementation {
  std::map<std::pair<std::string, Unit>, NumericField> numeric_values_;
  std::map<std::string, StringField> string_values_;
  void setNumericValue(FieldInfo *fi, DVEntry *dve, Unit u, double v);
  void setStringValue(FieldInfo *fi, std::string v, DVEntry *dve);
};

thread_local xmq_runtime::LegacyWriteJournal *lab_journal = nullptr;
static void require(bool yes, const char *msg) {
  if (!yes) throw std::runtime_error(msg);
}
"""
    tests = r"""
int main() {
  try {
    MeterCommonImplementation meter;
    FieldInfo heat{"current_hca"};
    FieldInfo water{"total_m3"};
    FieldInfo date{"previous_date"};
    DVEntry entry{};
    xmq_runtime::LegacyWriteJournal journal(12, 20);
    lab_journal = &journal;
    journal.begin_telegram();
    meter.setNumericValue(&heat, nullptr, Unit::Unitless, 42);
    meter.setNumericValue(&heat, &entry, Unit::Unitless, 43);
    meter.setNumericValue(&water, &entry, Unit::Unitless, 91.3);
    meter.setStringValue(&date, "synthetic", nullptr);
    meter.setStringValue(&date, "synthetic2", &entry);
    require(journal.wrote_numeric("current_hca"), "no-DV numeric key missing");
    require(journal.wrote_numeric("current_hca_dv"), "generated DV numeric key missing");
    require(journal.wrote_numeric("total_m3_dv"), "generated water DV key missing");
    require(!journal.wrote_numeric("total_m3"), "template was falsely attributed");
    require(journal.wrote_string("previous_date"), "no-DV text key missing");
    require(journal.wrote_string("previous_date_dv"), "generated DV text key missing");
    require(meter.numeric_values_.count({"current_hca_dv", Unit::Unitless}) == 1,
            "generated numeric key not actually stored");
    require(meter.string_values_.count("previous_date_dv") == 1,
            "generated text key not actually stored");
    require(journal.counts().numeric_writes == 3 &&
            journal.counts().string_writes == 2 &&
            journal.dropped_writes() == 0, "incorrect write counts");
    journal.end_telegram();
    journal.begin_telegram();
    meter.setNumericValue(&heat, nullptr, Unit::Unitless, 42); // unchanged
    require(journal.counts().numeric_writes == 1 &&
            journal.wrote_numeric("current_hca") &&
            !journal.wrote_numeric("current_hca_dv") &&
            !journal.wrote_string("previous_date"),
            "previous frame leaked or unchanged write missed");
    journal.end_telegram();
    lab_journal = nullptr;
    meter.setNumericValue(&heat, nullptr, Unit::Unitless, 44);
    require(journal.counts().numeric_writes == 1,
            "unregistered writer still changed journal");
    std::cout << "LEGACY_SETTER_EXECUTION_OK exact setter bodies, committed DV keys, "
                 "both value types, frame isolation and disabled observer\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
"""
    with tempfile.TemporaryDirectory(prefix="xmq-setters-") as directory:
        src = Path(directory) / "exact-setters.cpp"
        binary = Path(directory) / "exact-setters"
        src.write_text(header + "\n" + numeric + "\n" + string + "\n" + tests)
        subprocess.run(
            ["g++", "-std=c++17", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
             "-pedantic", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
             "-I", str(STAGE), str(src), "-o", str(binary)],
            check=True,
        )
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
