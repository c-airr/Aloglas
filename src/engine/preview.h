// What the UI hands the engine to preview a file. Allocated by the UI, owned
// by the engine while it plays, handed back through the garbage queue and
// deleted by the UI: the audio thread never frees memory (spec §4.2).
#pragma once

#include <memory>

#include "samples/sample.h"

namespace aloglas {

struct PreviewPayload {
    std::shared_ptr<const Sample> sample;  // keeps the sample alive while it plays
    float gain = 0.5012f;                  // -6 dB
};

}  // namespace aloglas
