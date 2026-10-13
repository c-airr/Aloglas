#include <doctest.h>

#include <memory>

#include "core/rt_guard.h"

using namespace aloglas;

TEST_CASE("rt guard counts allocations inside an audio scope") {
    if (!rt::guard_enabled()) {
        MESSAGE("rt guard disabled in this build");
        return;
    }
    rt::set_abort_on_violation(false);
    const auto before = rt::violation_count();
    std::unique_ptr<int> p;
    {
        rt::AudioThreadScope scope;
        p = std::make_unique<int>(1);        // allocation on the "audio thread"
    }
    CHECK(*p == 1);
    CHECK(rt::violation_count() == before + 1);
    p.reset();                               // freed outside the scope: allowed
    CHECK(rt::violation_count() == before + 1);

    {
        auto p = std::make_unique<int>(2);   // outside the scope: allowed
        CHECK(*p == 2);
    }
    CHECK(rt::violation_count() == before + 1);
    rt::set_abort_on_violation(true);
}

TEST_CASE("rt guard counts frees inside an audio scope") {
    if (!rt::guard_enabled()) {
        MESSAGE("rt guard disabled in this build");
        return;
    }
    rt::set_abort_on_violation(false);
    auto p = std::make_unique<int>(3);       // allocated outside
    const auto before = rt::violation_count();
    {
        rt::AudioThreadScope scope;
        p.reset();                            // freed on the "audio thread"
        int* none = nullptr;
        delete none;                          // deleting nullptr is not a violation
    }
    CHECK(rt::violation_count() == before + 1);
    rt::set_abort_on_violation(true);
}

TEST_CASE("AudioThreadScope nests and restores") {
    CHECK_FALSE(rt::in_audio_thread());
    {
        rt::AudioThreadScope outer;
        {
            rt::AudioThreadScope inner;
            CHECK(rt::in_audio_thread());
        }
        CHECK(rt::in_audio_thread());
    }
    CHECK_FALSE(rt::in_audio_thread());
}
