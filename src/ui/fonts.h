// Fonts compiled into the executable (no font files to ship next to the .exe).
#pragma once

#include <span>

namespace aloglas {

std::span<const unsigned char> inter_regular();
std::span<const unsigned char> inter_semibold();

}  // namespace aloglas
