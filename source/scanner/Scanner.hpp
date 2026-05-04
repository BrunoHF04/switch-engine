#pragma once

#include <switch.h>

#include <cstdint>

namespace Scanner {

Result detectForegroundAfterProcessList(uint64_t *out_pid, uint64_t *out_tid);

} // namespace Scanner
