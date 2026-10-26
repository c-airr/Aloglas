// A CommandSink that remembers every command instead of playing it.
#pragma once

#include <vector>

#include "engine/commands.h"
#include "engine/preview.h"
#include "engine/snapshot.h"

namespace aloglas::test {

struct RecordingSink : CommandSink {
    RecordingSink() = default;
    RecordingSink(const RecordingSink&) = delete;
    RecordingSink& operator=(const RecordingSink&) = delete;
    // Owns pointer payloads it recorded, as the engine would.
    ~RecordingSink() override {
        for (const Command& c : sent) {
            delete c.preview;
            delete c.snapshot;
        }
    }
    bool send(const Command& command) override {
        sent.push_back(command);
        return true;
    }
    std::vector<Command> sent;
};

}  // namespace aloglas::test
