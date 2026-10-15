// Lock-free single-producer / single-consumer ring buffer.
//
// One thread only pushes (the UI), one thread only pops (the audio thread).
// With exactly one writer per index we need no mutex: each side owns one
// counter and only reads the other side's counter.
//
// The counters grow forever (size_t wraps after ~1.8e19 pushes, which never
// happens in practice) and the slot is `index & (Capacity - 1)`. Because
// Capacity is a power of two, `head - tail` is the number of items even after
// the counters wrap, and the queue can use all Capacity slots.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace aloglas {

template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
    // Items are copied byte-wise between threads; anything with a destructor
    // or allocation inside would break the "no allocation on the audio
    // thread" rule anyway.
    static_assert(std::is_trivially_copyable_v<T>, "SpscQueue items must be trivially copyable");

public:
    // Producer thread only. Returns false when the queue is full.
    bool push(const T& item) {
        const std::size_t head = head_.load(std::memory_order_relaxed);  // we own head
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        if (head - tail == Capacity) return false;
        slots_[head & (Capacity - 1)] = item;
        // release: the slot write above becomes visible before the new head.
        // The consumer reads head with acquire, so once it sees head+1 it is
        // guaranteed to also see the item.
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    // Producer thread only: true when the consumer has taken everything
    // pushed so far.
    bool drained() const {
        return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_relaxed);
    }

    // Consumer thread only. Returns false when the queue is empty.
    bool pop(T& out) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);  // we own tail
        const std::size_t head = head_.load(std::memory_order_acquire);
        if (head == tail) return false;
        out = slots_[tail & (Capacity - 1)];
        // release: we finished reading the slot before the producer may reuse it.
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

private:
    // Each counter sits on its own 64-byte cache line. If they shared one,
    // every push would invalidate the consumer's cached line and vice versa
    // ("false sharing"), slowing both threads down for no reason.
    alignas(64) std::atomic<std::size_t> head_{0};  // written by the producer
    alignas(64) std::atomic<std::size_t> tail_{0};  // written by the consumer
    alignas(64) std::array<T, Capacity> slots_{};
};

}  // namespace aloglas
