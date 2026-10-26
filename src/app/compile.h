#pragma once

#include <memory>

#include "app/document.h"
#include "engine/snapshot.h"

namespace aloglas {

std::unique_ptr<Snapshot> compile_snapshot(const Document& document);

}  // namespace aloglas
