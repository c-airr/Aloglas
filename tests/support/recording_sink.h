// A CommandSink that remembers every command instead of playing it.
#pragma once

#include <vector>

#include "engine/commands.h"

namespace aloglas::test {

struct RecordingSink : CommandSink {
    bool send(const Command& command) override {
        sent.push_back(command);
        return true;
    }
    std::vector<Command> sent;
};

}  // namespace aloglas::test
