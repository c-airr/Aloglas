// Real-time guard: catches memory allocation/freeing on the audio thread.
//
// The audio callback has a few milliseconds to fill each buffer. malloc and
// free can take a lock inside the C runtime and wait for another thread, so a
// single allocation can occasionally blow that deadline and cause a click.
// The rule is therefore absolute: no new/delete inside Engine::render().
//
// When built with ALOGLAS_RT_GUARD (debug builds), the global operator
// new/delete are replaced. While the current thread is inside an
// AudioThreadScope, every allocation or free is a violation: it is counted
// and, by default, the program aborts with a message so the bug is found
// immediately instead of as a rare crackle.
#pragma once

#include <cstdint>

namespace aloglas::rt {

// Marks the current thread as "audio" for the lifetime of this object.
// Nestable: the previous state is restored on destruction.
class AudioThreadScope {
public:
    AudioThreadScope();
    ~AudioThreadScope();
    AudioThreadScope(const AudioThreadScope&) = delete;
    AudioThreadScope& operator=(const AudioThreadScope&) = delete;

private:
    bool previous_;
};

bool in_audio_thread();

// True when this build replaces operator new/delete (ALOGLAS_RT_GUARD).
bool guard_enabled();

// Number of allocations/frees seen inside an AudioThreadScope so far.
uint64_t violation_count();

// Default true. Tests turn it off to count violations instead of dying.
void set_abort_on_violation(bool abort_on_violation);

}  // namespace aloglas::rt
