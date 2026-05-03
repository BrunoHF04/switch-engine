#pragma once

#include <switch.h>

#include <cstdint>

namespace seng::mod {

/** Primeira busca u32 no processo alvo; grava results.bin no SD. */
Result runFirstScanU32(uint64_t pid, uint32_t value, uint64_t *out_total_hits);

} // namespace seng::mod
