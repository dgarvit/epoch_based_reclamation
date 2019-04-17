#ifndef _CONCURRENT_PTR_
#define _CONCURRENT_PTR_

#include "marked_ptr.hpp"

#include <atomic>

namespace reclamation { namespace techniques { namespace utils {
    
// T must be derived from enable_concurrent_ptr<T>. D is deleter
template <class T, std::size_t N, template <class, class MarkedPtr> class GuardPtr>
class concurrent_ptr {
public:
    using marked_ptr = utils::marked_ptr<T, N>;
    using guard_ptr = GuardPtr<T, marked_ptr>;
    
    concurrent_ptr(const marked_ptr& p = marked_ptr()) : ptr(p) {}
    concurrent_ptr(const concurrent_ptr&) = delete;
    concurrent_ptr& operator=(const concurrent_ptr&) = delete;
    concurrent_ptr(concurrent_ptr&&) = delete;
    concurrent_ptr& operator=(concurrent_ptr&&) = delete;

    // Atomic load that does not guard target from being reclaimed
    marked_ptr load(std::memory_order order = std::memory_order_seq_cst) const {
        return ptr.load(order);
    }
    
    // Atomic store
    void store(const marked_ptr& src, std::memory_order order = std::memory_order_seq_cst) {
        ptr.store(src, order);
    }
    
    // store from guard's source
    void store(const guard_ptr& src, std::memory_order order = std::memory_order_seq_cst) {
        ptr.store(src.get(), order);
    }

    // CAS functions
    bool compare_exchange_weak(marked_ptr& old, marked_ptr desired, std::memory_order order = std::memory_order_seq_cst)
    {
        return ptr.compare_exchange_weak(old, desired, order);
    }

    bool compare_exchange_weak(marked_ptr& old, marked_ptr desired, std::memory_order order = std::memory_order_seq_cst) volatile
    {
        return ptr.compare_exchange_weak(old, desired, order);
    }

    bool compare_exchange_weak(marked_ptr& old, marked_ptr desired, std::memory_order success, std::memory_order failure)
    {
        return ptr.compare_exchange_weak(old, desired, success, failure);
    }

    bool compare_exchange_weak(marked_ptr& old, marked_ptr desired, std::memory_order success, std::memory_order failure) volatile
    {
        return ptr.compare_exchange_weak(old, desired, success, failure);
    }

    bool compare_exchange_strong(marked_ptr& old, marked_ptr desired, std::memory_order order = std::memory_order_seq_cst)
    {
        return ptr.compare_exchange_strong(old, desired, order);
    }

    bool compare_exchange_strong(marked_ptr& old, marked_ptr desired, std::memory_order order = std::memory_order_seq_cst) volatile
    {
        return ptr.compare_exchange_strong(old, desired, order);
    }

    bool compare_exchange_strong(marked_ptr& old, marked_ptr desired, std::memory_order success, std::memory_order failure)
    {
        return ptr.compare_exchange_strong(old, desired, success, failure);
    }

    bool compare_exchange_strong(marked_ptr& old, marked_ptr desired, std::memory_order success, std::memory_order failure) volatile
    {
        return ptr.compare_exchange_strong(old, desired, success, failure);
    }

private:
    std::atomic<marked_ptr> ptr;
};
}}}

#endif
