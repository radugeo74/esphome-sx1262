#pragma once
// Compatibility include for existing host/ESP-IDF probe tests.
// The single maintained AES-CMAC/CCM implementation now lives in the
// native component, shared with the opt-in SX126x encrypted dispatcher.
#include "../../../../components/wmbus_native_sx126x/oms_ccm_mbedtls.h"
