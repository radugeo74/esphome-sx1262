#include "xmq_legacy_write_journal.h"
#include <iostream>
#include <stdexcept>
#include <string>

static void require(bool yes, const char *reason) {
  if (!yes) throw std::runtime_error(reason);
}
template<typename F> static bool throws(F f) {
  try { f(); } catch (const std::exception &) { return true; }
  return false;
}

int main() {
  try {
    xmq_runtime::LegacyWriteJournal journal(4, 6);
    require(throws([&] { journal.numeric_write("current_hca"); }),
            "write outside telegram was accepted");
    journal.begin_telegram();
    journal.numeric_write("current_hca");
    journal.numeric_write("previous_hca");
    journal.numeric_write("current_hca"); // Same value or repeat write still counts.
    journal.string_write("current_date");
    require(journal.wrote_numeric("current_hca") &&
                journal.wrote_numeric("previous_hca") &&
                journal.wrote_string("current_date") &&
                !journal.wrote_numeric("temp_room_c"),
            "write-provenance within frame is incorrect");
    require(journal.counts().numeric_writes == 3 &&
                journal.counts().distinct_numeric == 2 &&
                journal.counts().string_writes == 1,
            "write counts are incorrect");
    journal.end_telegram();
    require(throws([&] { journal.numeric_write("stale"); }),
            "write after end was accepted");
    journal.begin_telegram();
    require(!journal.wrote_numeric("current_hca") &&
                !journal.wrote_string("current_date"),
            "prior telegram writes leaked into next frame");
    journal.numeric_write("current_hca");
    require(journal.wrote_numeric("current_hca"), "current write missing");
    require(throws([&] { journal.numeric_write(""); }),
            "empty field accepted");
    require(throws([&] { journal.numeric_write(std::string(97, 'x')); }),
            "oversized field accepted");
    journal.numeric_write("a");
    journal.numeric_write("b");
    journal.string_write("c");
    require(throws([&] { journal.string_write("over_capacity"); }),
            "distinct field bound ignored");
    journal.end_telegram();

    xmq_runtime::LegacyWriteJournal write_bound(8, 1);
    write_bound.begin_telegram();
    write_bound.numeric_write("x");
    require(throws([&] { write_bound.numeric_write("x"); }),
            "total write bound ignored");
    require(throws([&] { xmq_runtime::LegacyWriteJournal invalid(0, 1); }),
            "zero field capacity accepted");

    xmq_runtime::LegacyWriteJournal safe(1, 1);
    require(!safe.try_numeric_write("out_of_frame") && safe.dropped_writes() == 1,
            "fail-open recorder must absorb writes outside a telegram");
    safe.begin_telegram();
    require(safe.dropped_writes() == 0, "dropped counter leaked across frames");
    require(safe.try_numeric_write("current_hca") &&
                safe.wrote_numeric("current_hca"),
            "fail-open recorder dropped a valid write");
    require(!safe.try_string_write("date") &&
                !safe.try_numeric_write("current_hca") &&
                safe.dropped_writes() == 2,
            "fail-open recorder must absorb field and write limits");
    safe.end_telegram();
    require(!safe.try_numeric_write("after_frame") &&
                safe.dropped_writes() == 3,
            "closed frame must not throw into legacy decoder");

    std::cout << "LEGACY_WRITE_JOURNAL_OK: isolated frame lifecycle, duplicate writes, "
                 "unchanged-value writes, numeric/string separation and bounds\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
