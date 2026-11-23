#pragma once
// wmbus_common/util.h defines legacy logging macro error(...).
// The independent XMQ reader has a Parser::error(...) method. Keep
// legacy macro visible to the caller, but not while parsing XMQ headers.
#ifdef error
#pragma push_macro("error")
#undef error
#define XMQ_SHADOW_RESTORE_LEGACY_ERROR_MACRO
#endif

#include "xmq_shadow_diagnostic.h"

#ifdef XMQ_SHADOW_RESTORE_LEGACY_ERROR_MACRO
#pragma pop_macro("error")
#undef XMQ_SHADOW_RESTORE_LEGACY_ERROR_MACRO
#endif
