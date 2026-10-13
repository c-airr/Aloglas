#include "core/rt_guard.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace aloglas::rt {

namespace {
// thread_local: every thread has its own copy, so marking the audio thread
// does not affect the UI thread.
thread_local bool t_in_audio_thread = false;
std::atomic<uint64_t> g_violations{0};
std::atomic<bool> g_abort_on_violation{true};
}  // namespace

AudioThreadScope::AudioThreadScope() : previous_(t_in_audio_thread) {
    t_in_audio_thread = true;
}

AudioThreadScope::~AudioThreadScope() {
    t_in_audio_thread = previous_;
}

bool in_audio_thread() {
    return t_in_audio_thread;
}

bool guard_enabled() {
#ifdef ALOGLAS_RT_GUARD
    return true;
#else
    return false;
#endif
}

uint64_t violation_count() {
    return g_violations.load(std::memory_order_relaxed);
}

void set_abort_on_violation(bool abort_on_violation) {
    g_abort_on_violation.store(abort_on_violation, std::memory_order_relaxed);
}

#ifdef ALOGLAS_RT_GUARD
namespace {
void check(const char* what) {
    if (!t_in_audio_thread) return;
    g_violations.fetch_add(1, std::memory_order_relaxed);
    if (g_abort_on_violation.load(std::memory_order_relaxed)) {
        std::fputs("aloglas: ", stderr);
        std::fputs(what, stderr);
        std::fputs(" on the audio thread\n", stderr);
        std::abort();
    }
}

void* checked_alloc(std::size_t size) {
    check("allocation");
    if (size == 0) size = 1;
    if (void* p = std::malloc(size)) return p;
    throw std::bad_alloc();
}

void* checked_alloc_aligned(std::size_t size, std::align_val_t align) {
    check("allocation");
    if (size == 0) size = 1;
    if (void* p = _aligned_malloc(size, static_cast<std::size_t>(align))) return p;
    throw std::bad_alloc();
}

void checked_free(void* p) {
    if (!p) return;  // delete nullptr is a harmless no-op, not a violation
    check("free");
    std::free(p);
}

void checked_free_aligned(void* p) {
    if (!p) return;
    check("free");
    _aligned_free(p);
}
}  // namespace
#endif

}  // namespace aloglas::rt

#ifdef ALOGLAS_RT_GUARD
// Replacements for every global operator new/delete form. They live in the
// same .cpp as in_audio_thread() on purpose: a static library only links the
// object files something references, and the engine references this one, so
// the replacements always make it into the final executable. All forms are
// replaced so the C++ runtime's own versions are never pulled in next to ours.
using aloglas::rt::checked_alloc;
using aloglas::rt::checked_alloc_aligned;
using aloglas::rt::checked_free;
using aloglas::rt::checked_free_aligned;

void* operator new(std::size_t n) { return checked_alloc(n); }
void* operator new[](std::size_t n) { return checked_alloc(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    try { return checked_alloc(n); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept {
    try { return checked_alloc(n); } catch (...) { return nullptr; }
}
void* operator new(std::size_t n, std::align_val_t a) { return checked_alloc_aligned(n, a); }
void* operator new[](std::size_t n, std::align_val_t a) { return checked_alloc_aligned(n, a); }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
    try { return checked_alloc_aligned(n, a); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
    try { return checked_alloc_aligned(n, a); } catch (...) { return nullptr; }
}

void operator delete(void* p) noexcept { checked_free(p); }
void operator delete[](void* p) noexcept { checked_free(p); }
void operator delete(void* p, std::size_t) noexcept { checked_free(p); }
void operator delete[](void* p, std::size_t) noexcept { checked_free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { checked_free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { checked_free(p); }
void operator delete(void* p, std::align_val_t) noexcept { checked_free_aligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept { checked_free_aligned(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { checked_free_aligned(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { checked_free_aligned(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { checked_free_aligned(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { checked_free_aligned(p); }
#endif
