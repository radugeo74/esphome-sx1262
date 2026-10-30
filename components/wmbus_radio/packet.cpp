#include "packet.h"

#include <ctime>

#include "esphome/components/wmbus_common/meters.h"
#include "esphome/core/helpers.h"

#include "decode3of6.h"
#include "pure_dll_crc.h"
#include "pure_complete_frame.h"
#include "pure_parity_classification.h"

#define WMBUS_PREAMBLE_SIZE (3)
#define WMBUS_MODE_C_SUFIX_LEN (2)
#define WMBUS_MODE_C_PREAMBLE (0x54)
#define WMBUS_BLOCK_A_PREAMBLE (0xCD)
#define WMBUS_BLOCK_B_PREAMBLE (0x3D)

namespace esphome {
namespace wmbus_radio {
static const char *TAG = "packet";
Packet::Packet() { this->data_.reserve(WMBUS_PREAMBLE_SIZE); }

// Determine the link mode based on the first byte of the data
LinkMode Packet::link_mode() {
  if (this->link_mode_ == LinkMode::UNKNOWN)
    if (this->data_.size())
      if (this->data_[0] == WMBUS_MODE_C_PREAMBLE)
        this->link_mode_ = LinkMode::C1;
      else
        this->link_mode_ = LinkMode::T1;

  return this->link_mode_;
}

void Packet::set_rssi(int8_t rssi) { this->rssi_ = rssi; }

// Get value of L-field
uint8_t Packet::l_field() {
  switch (this->link_mode()) {
  case LinkMode::C1:
    return this->data_[2];
  case LinkMode::T1: {
    auto decoded = decode3of6(this->data_);

    if (decoded)
      return (*decoded)[0];
  }
  }
  return 0;
}

size_t Packet::expected_size() {
  if (!this->expected_size_) {
    // Format A
    //   L-field = length without CRC fields and without L (1 byte)
    // Format B
    //   L-field = length with CRC fields and without L (1 byte)
    auto l_field = this->l_field();

    // The 2 first blocks contains 25 bytes when excluding CRC and the L-field
    // The other blocks contains 16 bytes when excluding the CRC-fields
    // Less than 26 (15 + 10)
    auto nrBlocks = l_field < 26 ? 2 : (l_field - 26) / 16 + 3;

    // Add all extra fields, excluding the CRC fields + 2 CRC bytes for each
    // block
    auto nrBytes = l_field + 1 + 2 * nrBlocks;

    if (this->link_mode() != LinkMode::C1)
      this->expected_size_ = encoded_size(nrBytes);
    else if (this->data_[1] == WMBUS_BLOCK_A_PREAMBLE)
      this->expected_size_ = WMBUS_MODE_C_SUFIX_LEN + nrBytes;
    else if (this->data_[1] == WMBUS_BLOCK_B_PREAMBLE)
      this->expected_size_ = WMBUS_MODE_C_SUFIX_LEN + 1 + l_field;
  }
  ESP_LOGV(TAG, "expected_size: %zu", this->expected_size_);
  return this->expected_size_;
}

size_t Packet::rx_capacity() {
  // TODO: Remove side effects?
  auto cap = this->data_.capacity() - this->data_.size();
  this->data_.resize(this->data_.capacity());
  return cap;
}

uint8_t *Packet::rx_data_ptr() {
  return this->data_.data() + this->data_.size();
}

bool Packet::calculate_payload_size() {
  auto total_length = this->expected_size();
  this->data_.reserve(total_length);
  return total_length;
}

std::optional<Frame> Packet::convert_to_frame() {
  std::optional<Frame> frame = {};

  ESP_LOGD(TAG, "Have data from radio (%zu bytes)", this->data_.size());
  debugPayload("raw packet", this->data_);

  if (this->expected_size() == this->data_.size()) {
    if (this->link_mode() == LinkMode::T1) {
      ESP_LOGD(TAG, "Link mode: T1");
      // TODO: Remove assumption that T1 is always A
      this->frame_format_ = "A";
      auto decoded_data = decode3of6(this->data_);
      if (decoded_data)
        this->data_ = decoded_data.value();
    } else if (this->link_mode() == LinkMode::C1) {
      ESP_LOGD(TAG, "Link mode: C1");
      if (this->data_[1] == WMBUS_BLOCK_A_PREAMBLE)
        this->frame_format_ = "A";
      else if (this->data_[1] == WMBUS_BLOCK_B_PREAMBLE)
        this->frame_format_ = "B";
      this->data_.erase(this->data_.begin(),
                        this->data_.begin() + WMBUS_MODE_C_SUFIX_LEN);
    } else {
      ESP_LOGE(TAG, "unknown link mode!");
    }
  } else {
    ESP_LOGE(TAG, "expected_size: %zu NOT size: %zu", this->expected_size(),
             this->data_.size());
  }

  // Migration check: independently validate/strip the DLL CRC on a copy.
  // The existing path remains authoritative until we have on-device parity
  // across real T1/C1 frames and both DLL formats.
  const auto received_with_crc = this->data_;
  std::vector<uint8_t> pure_decoded;
  const auto pure_format = this->frame_format_ == "A"
      ? wmbus_pure::CrcFrameFormat::A : wmbus_pure::CrcFrameFormat::B;
  const bool pure_supported = this->frame_format_ == "A" || this->frame_format_ == "B";
  const auto pure_crc_status = pure_supported
      ? wmbus_pure::strip_dll_crc(received_with_crc.data(),
                                received_with_crc.size(), pure_format, pure_decoded)
      : wmbus_pure::CrcStatus::LengthMismatch;
  // Independently verify both CRC and DLL header from the original decoded
  // radio bytes. Keep this separate from the legacy frame acceptance.
  wmbus_pure::CompleteFrame pure_complete{};
  const auto complete_status = pure_supported
      ? wmbus_pure::verify_complete_frame(
            received_with_crc.data(), received_with_crc.size(),
            pure_format, pure_complete)
      : wmbus_pure::CompleteStatus::UnsupportedHeader;
  removeAnyDLLCRCs(this->data_);
  if (pure_supported) {
    if (pure_crc_status == wmbus_pure::CrcStatus::Ok) {
      if (pure_decoded != this->data_) {
        ESP_LOGW(TAG, "Pure DLL CRC mismatch with legacy (mode=%s format=%s)",
                 toString(this->link_mode_), this->frame_format_.c_str());
      } else {
        ESP_LOGD(TAG, "Pure DLL CRC parity OK (mode=%s format=%s)",
                 toString(this->link_mode_), this->frame_format_.c_str());
      }
    } else if (pure_crc_status == wmbus_pure::CrcStatus::BadCrc) {
      ESP_LOGD(TAG, "Pure DLL CRC rejected damaged frame (mode=%s format=%s, legacy_size=%zu)",
               toString(this->link_mode_), this->frame_format_.c_str(), this->data_.size());
    } else {
      ESP_LOGD(TAG, "Pure DLL structural reject status=%u (mode=%s format=%s)",
               static_cast<unsigned>(pure_crc_status),
               toString(this->link_mode_), this->frame_format_.c_str());
    }
  }
  int dummy;
  const bool legacy_accepted =
      checkWMBusFrame(this->data_, (size_t *)&dummy, &dummy, &dummy, false) ==
      FrameStatus::FullFrame;
  const bool pure_accepted = complete_status == wmbus_pure::CompleteStatus::Ok;
  const bool identical = pure_accepted && pure_complete.bytes == this->data_;
  // Aggregate reason counters, never print raw telegrams or keys.
  static uint32_t pure_checked = 0;
  static uint32_t pure_equal = 0;
  static uint32_t pure_disagree = 0;
  static uint32_t crc_legacy = 0;
  static uint32_t header_legacy = 0;
  static uint32_t structure_legacy = 0;
  static uint32_t content_diff = 0;
  static uint32_t pure_only = 0;
  const auto reason = wmbus_pure::classify_parity(
      complete_status, legacy_accepted, identical);
  ++pure_checked;
  if (reason == wmbus_pure::ParityMismatch::None) {
    ++pure_equal;
  } else {
    ++pure_disagree;
    switch (reason) {
      case wmbus_pure::ParityMismatch::BadCrcLegacyAccepted: ++crc_legacy; break;
      case wmbus_pure::ParityMismatch::InvalidHeaderLegacyAccepted: ++header_legacy; break;
      case wmbus_pure::ParityMismatch::InvalidStructureLegacyAccepted: ++structure_legacy; break;
      case wmbus_pure::ParityMismatch::ContentMismatch: ++content_diff; break;
      case wmbus_pure::ParityMismatch::PureOnlyAccepted: ++pure_only; break;
      case wmbus_pure::ParityMismatch::None: break;
    }
  }
  if (pure_checked % 100 == 1 || (reason != wmbus_pure::ParityMismatch::None &&
                                  pure_disagree <= 3)) {
    ESP_LOGI(TAG,
             "Pure radio parity checked=%u equal=%u disagree=%u crc_legacy=%u header_legacy=%u structure_legacy=%u content_diff=%u pure_only=%u pure_ok=%u legacy_ok=%u format=%s",
             static_cast<unsigned>(pure_checked),
             static_cast<unsigned>(pure_equal),
             static_cast<unsigned>(pure_disagree),
             static_cast<unsigned>(crc_legacy),
             static_cast<unsigned>(header_legacy),
             static_cast<unsigned>(structure_legacy),
             static_cast<unsigned>(content_diff),
             static_cast<unsigned>(pure_only),
             static_cast<unsigned>(pure_accepted),
             static_cast<unsigned>(legacy_accepted),
             this->frame_format_.c_str());
  }
  if (legacy_accepted) frame.emplace(this);

  // Packet lifetime is owned by Radio::loop() via std::unique_ptr.
  // Do not delete this object here: that would cause a double-free.
  return frame;
}

Frame::Frame(Packet *packet)
    : data_(std::move(packet->data_)), link_mode_(packet->link_mode_),
      rssi_(packet->rssi_), format_(packet->frame_format_) {}

std::vector<uint8_t> &Frame::data() { return this->data_; }
LinkMode Frame::link_mode() { return this->link_mode_; }
int8_t Frame::rssi() { return this->rssi_; }
std::string Frame::format() { return this->format_; }

std::vector<uint8_t> Frame::as_raw() { return this->data_; }
std::string Frame::as_hex() { return format_hex(this->data_); }
std::string Frame::as_rtlwmbus() {
  const size_t time_repr_size = sizeof("YYYY-MM-DD HH:MM:SS.00Z");
  char time_buffer[time_repr_size];
  auto t = std::time(NULL);
  std::strftime(time_buffer, time_repr_size, "%F %T.00Z", std::gmtime(&t));

  auto output = std::string{};
  output.reserve(2 + 5 + 24 + 1 + 4 + 5 + 2 * this->data_.size() + 1);

  output += linkModeName(this->link_mode_); // size 2
  output += ";1;1;";                        // size 5
  output += time_buffer;                    // size 24
  output += ';';                            // size 1
  output += std::to_string(this->rssi_);    // size up to 4
  output += ";;;0x";                        // size 5
  output += this->as_hex();                 // size 2 * frame.size()
  output += "\n";                           // size 1

  return output;
}

void Frame::mark_as_handled() { this->handlers_count_++; }
uint8_t Frame::handlers_count() { return this->handlers_count_; }

} // namespace wmbus_radio
} // namespace esphome
