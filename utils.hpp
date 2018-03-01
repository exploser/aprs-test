#pragma once
#include <cstdlib>
#include <cstdint>
#include <memory>

#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)
#define DEFER(x) std::shared_ptr<void> CONCAT(_defer___, __COUNTER__)(nullptr, [&](auto) { x; });

namespace utils
{
static int8_t lut[] = {
#include "sine.txt"
};

const size_t lut_size = sizeof(lut) / sizeof(*lut);
}