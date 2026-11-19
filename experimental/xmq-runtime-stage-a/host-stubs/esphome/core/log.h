#pragma once
// Host-only logging declarations for syntax-checking the actual legacy
// wmbus_common translation unit. NEVER included by ESPHome firmware.
// Nothing is emitted and no runtime log semantics are asserted here.
// C-style variadic functions match the call syntax used in util.h.
static inline void esph_log_d(const char *, const char *, ...) noexcept {}
static inline void esph_log_v(const char *, const char *, ...) noexcept {}
static inline void esph_log_i(const char *, const char *, ...) noexcept {}
static inline void esph_log_w(const char *, const char *, ...) noexcept {}
static inline void esph_log_e(const char *, const char *, ...) noexcept {}
