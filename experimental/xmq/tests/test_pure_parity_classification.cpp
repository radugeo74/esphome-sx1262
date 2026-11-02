#include <cassert>
#include "../../../components/wmbus_radio/pure_parity_classification.h"
using wmbus_pure::CompleteStatus;
using wmbus_pure::ParityMismatch;
using wmbus_pure::classify_parity;
int main() {
  assert(classify_parity(CompleteStatus::Ok, true, true) == ParityMismatch::None);
  assert(classify_parity(CompleteStatus::Ok, true, false) == ParityMismatch::ContentMismatch);
  assert(classify_parity(CompleteStatus::Ok, false, false) == ParityMismatch::PureOnlyAccepted);
  assert(classify_parity(CompleteStatus::BadCrc, true, false) == ParityMismatch::BadCrcLegacyAccepted);
  assert(classify_parity(CompleteStatus::UnsupportedHeader, true, false) == ParityMismatch::InvalidHeaderLegacyAccepted);
  assert(classify_parity(CompleteStatus::TooShort, true, false) == ParityMismatch::InvalidStructureLegacyAccepted);
  assert(classify_parity(CompleteStatus::InvalidLength, true, false) == ParityMismatch::InvalidStructureLegacyAccepted);
  assert(classify_parity(CompleteStatus::BadCrc, false, false) == ParityMismatch::None);
}
