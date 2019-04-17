#ifndef _MARKED_PTR_
#define _MARKED_PTR_

#include <cassert>
#include <cstdint>
#include <cstddef>

namespace reclamation { namespace techniques { namespace utils {
    
template <class T, std::size_t N>
class marked_ptr {
    static constexpr uintptr_t MarkMask = (1 << N) - 1;
    T* ptr;
public:
    static constexpr std::size_t number_of_mark_bits = N;

    // Construct a marked ptr
    marked_ptr(T* p = nullptr, uintptr_t mark = 0) 
    {
        assert(mark <= MarkMask && "mark exceeds the number of bits reserved");
        assert((reinterpret_cast<uintptr_t>(p) & MarkMask) == 0 &&
            "bits reserved for masking are occupied by the pointer");
        ptr = reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(p) | mark);
    }
    
    // Set to nullptr
    void reset() { ptr = nullptr; }
    
    // Get mark bits
    uintptr_t mark() const {
        return reinterpret_cast<uintptr_t>(ptr) & MarkMask;
    }
    
    // Get underlying pointer (with mark bits stripped off).
    T* get() const {
        return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(ptr) & ~MarkMask);
    }
    
    // True if get() != nullptr || mark() != 0
    explicit operator bool() const { return ptr != nullptr; }
    
    // Get pointer with mark bits stripped off.
    T* operator->() const { return get(); }
    
    // Get reference to target of pointer.
    T& operator*() const { return *get(); }

    inline friend bool operator==(const marked_ptr& l, const marked_ptr& r) { return l.ptr == r.ptr; }
    inline friend bool operator!=(const marked_ptr& l, const marked_ptr& r) { return l.ptr != r.ptr; }
};
}}}

#endif