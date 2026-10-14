#include "ui/fonts.h"

namespace aloglas {

namespace {
// #embed (C23, available in clang as a C++ extension) pastes the bytes of a
// file into an array initializer at compile time. The paths are relative to
// this source file. Inter is licensed under the OFL (assets/fonts/OFL.txt).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
constexpr unsigned char kInterRegular[] = {
#embed "../../assets/fonts/Inter-Regular.ttf"
};
constexpr unsigned char kInterSemiBold[] = {
#embed "../../assets/fonts/Inter-SemiBold.ttf"
};
#pragma clang diagnostic pop
}  // namespace

std::span<const unsigned char> inter_regular() {
    return kInterRegular;
}

std::span<const unsigned char> inter_semibold() {
    return kInterSemiBold;
}

}  // namespace aloglas
