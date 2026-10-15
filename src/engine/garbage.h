// An object the audio thread is done with, on its way back to the UI thread
// to be deleted there. A function pointer instead of a virtual base keeps any
// type usable (payloads now, snapshots later) without touching it.
#pragma once

namespace aloglas {

struct Garbage {
    void (*destroy)(void*) = nullptr;
    void* object = nullptr;
};

template <typename T>
Garbage make_garbage(T* object) {
    return {[](void* p) { delete static_cast<T*>(p); }, object};
}

}  // namespace aloglas
