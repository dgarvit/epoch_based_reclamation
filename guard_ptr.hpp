#ifndef _GUARD_PTR_
#define _GUARD_PTR_

#include <utility>

namespace reclamation { namespace techniques { namespace utils {

template <class T, class MarkedPtr, class Derived>
class guard_ptr {
public:
    // Get underlying pointer
    T* get() const { return ptr.get(); }

    // Get mark bits
    uintptr_t mark() const { return ptr.mark(); }

    operator MarkedPtr() const { return ptr; }

    // True if get() != nullptr || mark() != 0
    explicit operator bool() const { return static_cast<bool>(ptr); }

    // Get pointer with mark bits stripped off. Undefined if target has been reclaimed.
    T* operator->() const { return ptr.get(); }

    // Get reference to target of pointer. Undefined if target has been reclaimed.
    T& operator*() const { return *ptr; }

    // Swap two guards
    void swap(Derived& g) 
    {
        std::swap(ptr, g.ptr);
        self().do_swap(g);
    }

    ~guard_ptr() { self().reset(); }

protected:
    guard_ptr(const MarkedPtr& p = MarkedPtr{}) : ptr(p) {}
    MarkedPtr ptr;

    void do_swap(Derived& g) {} // empty dummy

private:
    Derived& self() { return static_cast<Derived&>(*this); }
};
}}}

namespace reclamation {

// Helper function to acquire a `guard_ptr` without having to define the guard_ptr instance beforehand.
template <typename ConcurrentPtr>
auto acquire_guard(ConcurrentPtr& p, std::memory_order order = std::memory_order_seq_cst)
{
        typename ConcurrentPtr::guard_ptr guard;
        guard.acquire(p, order);
        return guard;
}

}

#endif
