#pragma once

#include <switch.h>

#include <cstdint>

#include "seng_ipc.hpp"

namespace seng::mod {

Result runFirstScan(uint64_t pid, const seng::ScanParams &params, uint64_t *out_total_hits);

} // namespace seng::mod
