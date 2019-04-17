#ifndef _EPOCH_BASED_
#define _EPOCH_BASED_

#include <algorithm>

#include "allocation_tracker.hpp"
#include "concurrent_ptr.hpp"
#include "deletable_object.hpp"
#include "guard_ptr.hpp"
#include "port.hpp"
#include "thread_block_list.hpp"

namespace reclamation { namespace techniques {

template <std::size_t UpdateThreshold>
class epoch_based {
    template <class T, class MarkedPtr>
    class guard_ptr;

public:
    template <class T, std::size_t N = 0, class Deleter = std::default_delete<T>>
    class enable_concurrent_ptr;

    class region_guard {};

    template <class T, std::size_t N = T::number_of_mark_bits>
    using concurrent_ptr = utils::concurrent_ptr<T, N, guard_ptr>;

    ALLOCATION_TRACKER;

private:
    static constexpr unsigned number_epochs = 3;

    struct thread_data;
    struct thread_control_block;

    static std::atomic<unsigned> global_epoch;
    static utils::thread_block_list<thread_control_block> global_thread_block_list;
    static thread_data& local_thread_data();

    ALLOCATION_TRACKING_FUNCTIONS;
};

template <std::size_t UpdateThreshold>
template <class T, std::size_t N, class Deleter>
class epoch_based<UpdateThreshold>::enable_concurrent_ptr : private utils::deletable_object_impl<T, Deleter>, private utils::tracked_object<epoch_based> {
public:
    static constexpr std::size_t number_of_mark_bits = N;

protected:
    enable_concurrent_ptr() = default;
    enable_concurrent_ptr(const enable_concurrent_ptr&) = default;
    enable_concurrent_ptr(enable_concurrent_ptr&&) = default;
    enable_concurrent_ptr& operator=(const enable_concurrent_ptr&) = default;
    enable_concurrent_ptr& operator=(enable_concurrent_ptr&&) = default;
    ~enable_concurrent_ptr() = default;

private:
    friend utils::deletable_object_impl<T, Deleter>;

    template <class, class>
    friend class guard_ptr;
};

template <std::size_t UpdateThreshold>
template <class T, class MarkedPtr>
class epoch_based<UpdateThreshold>::guard_ptr : public utils::guard_ptr<T, MarkedPtr, guard_ptr<T, MarkedPtr>> {
    using base = utils::guard_ptr<T, MarkedPtr, guard_ptr>;
    using Deleter = typename T::Deleter;
public:
    // Guard a marked ptr.
    guard_ptr(const MarkedPtr& p = MarkedPtr()) ;
    explicit guard_ptr(const guard_ptr& p) ;
    guard_ptr(guard_ptr&& p) ;

    guard_ptr& operator=(const guard_ptr& p) ;
    guard_ptr& operator=(guard_ptr&& p) ;

    // Atomically take snapshot of p, and *if* it points to unreclaimed object, acquire shared ownership of it.
    void acquire(const concurrent_ptr<T>& p, std::memory_order order = std::memory_order_seq_cst) ;

    // Like acquire, but quit early if a snapshot != expected.
    bool acquire_if_equal(const concurrent_ptr<T>& p,
                                                const MarkedPtr& expected,
                                                std::memory_order order = std::memory_order_seq_cst) ;

    // Release ownership. Postcondition: get() == nullptr.
    void reset() ;

    // Reset. Deleter d will be applied some time after all owners release their ownership.
    void reclaim(Deleter d = Deleter()) ;
};

template <std::size_t UpdateThreshold>
template <class T, class MarkedPtr>
epoch_based<UpdateThreshold>::guard_ptr<T, MarkedPtr>::guard_ptr(const MarkedPtr& p) : base(p) {
    if (this->ptr)
        local_thread_data().enter_critical();
}

template <std::size_t UpdateThreshold>
template <class T, class MarkedPtr>
epoch_based<UpdateThreshold>::guard_ptr<T, MarkedPtr>::guard_ptr(const guard_ptr& p) : guard_ptr(MarkedPtr(p)) {}

template <std::size_t UpdateThreshold>
template <class T, class MarkedPtr>
epoch_based<UpdateThreshold>::guard_ptr<T, MarkedPtr>::guard_ptr(guard_ptr&& p) : base(p.ptr) {
    p.ptr.reset();
}

template <std::size_t UpdateThreshold>
template <class T, class MarkedPtr>
auto epoch_based<UpdateThreshold>::guard_ptr<T, MarkedPtr>::operator=(const guard_ptr& p) -> guard_ptr& {
    if (&p == this)
        return *this;

    reset();
    this->ptr = p.ptr;
    if (this->ptr)
        local_thread_data().enter_critical();

    return *this;
}

template <std::size_t UpdateThreshold>
template <class T, class MarkedPtr>
auto epoch_based<UpdateThreshold>::guard_ptr<T, MarkedPtr>::operator=(guard_ptr&& p) -> guard_ptr& {
    if (&p == this)
        return *this;

    reset();
    this->ptr = std::move(p.ptr);
    p.ptr.reset();

    return *this;
}

template <std::size_t UpdateThreshold>
template <class T, class MarkedPtr>
void epoch_based<UpdateThreshold>::guard_ptr<T, MarkedPtr>::acquire(const concurrent_ptr<T>& p, std::memory_order order)  {
    if (p.load(std::memory_order_relaxed) == nullptr)
    {
        reset();
        return;
    }

    if (!this->ptr)
        local_thread_data().enter_critical();
    // (1) - this load operation potentially synchronizes-with any release operation on p.
    this->ptr = p.load(order);
    if (!this->ptr)
        local_thread_data().leave_critical();
}

template <std::size_t UpdateThreshold>
template <class T, class MarkedPtr>
bool epoch_based<UpdateThreshold>::guard_ptr<T, MarkedPtr>::acquire_if_equal(
    const concurrent_ptr<T>& p,
    const MarkedPtr& expected,
    std::memory_order order) 
{
    auto actual = p.load(std::memory_order_relaxed);
    if (actual == nullptr || actual != expected)
    {
        reset();
        return actual == expected;
    }

    if (!this->ptr)
        local_thread_data().enter_critical();
    // (2) - this load operation potentially synchronizes-with any release operation on p
    this->ptr = p.load(order);
    if (!this->ptr || this->ptr != expected)
    {
        local_thread_data().leave_critical();
        this->ptr.reset();
    }

    return this->ptr == expected;
}

template <std::size_t UpdateThreshold>
template <class T, class MarkedPtr>
void epoch_based<UpdateThreshold>::guard_ptr<T, MarkedPtr>::reset() {
    if (this->ptr)
        local_thread_data().leave_critical();
    this->ptr.reset();
}

template <std::size_t UpdateThreshold>
template <class T, class MarkedPtr>
void epoch_based<UpdateThreshold>::guard_ptr<T, MarkedPtr>::reclaim(Deleter d) {
    this->ptr->set_deleter(std::move(d));
    local_thread_data().add_retired_node(this->ptr.get());
    reset();
}

template <std::size_t UpdateThreshold>
struct epoch_based<UpdateThreshold>::thread_control_block : utils::thread_block_list<thread_control_block>::entry {
    thread_control_block() : is_in_critical_region(false), local_epoch(number_epochs) {}

    std::atomic<bool> is_in_critical_region;
    std::atomic<unsigned> local_epoch;
};

template <std::size_t UpdateThreshold>
struct epoch_based<UpdateThreshold>::thread_data
{
    void enter_critical() {
        if (++enter_count == 1)
            do_enter_critical();
    }

    void leave_critical() {
        assert(enter_count > 0);
        if (--enter_count == 0)
            do_leave_critical();
    }

    void add_retired_node(utils::deletable_object* p) {
        add_retired_node(p, control_block->local_epoch.load(std::memory_order_relaxed));
    }

    ~thread_data() {
        if (control_block == nullptr)
            return; // nothing to do

        // we can avoid creating an orphan in case we have no retired nodes left.
        if (std::any_of(retire_lists.begin(), retire_lists.end(), [](auto p) { return p != nullptr; }))
        {
            // global_epoch - 1 (mod number_epochs) guarantees a full cycle, making sure no
            // other thread may still have a reference to an object in one of the retire lists.
            auto target_epoch = (global_epoch.load(std::memory_order_relaxed) + number_epochs - 1) % number_epochs;
            assert(target_epoch < number_epochs);
            global_thread_block_list.abandon_retired_nodes(new utils::orphan<number_epochs>(target_epoch, retire_lists));
        }

        assert(control_block->is_in_critical_region.load(std::memory_order_relaxed) == false);
        global_thread_block_list.release_entry(control_block);
    }

private:
    void ensure_has_control_block() {
        if (control_block == nullptr)
            control_block = global_thread_block_list.acquire_entry();
    }

    void do_enter_critical() {
        ensure_has_control_block();

        control_block->is_in_critical_region.store(true, std::memory_order_relaxed);
        // (3) - this seq_cst-fence enforces a total order with itself
        std::atomic_thread_fence(std::memory_order_seq_cst);

        // (4) - this acquire-load synchronizes-with the release-CAS (7)
        auto epoch = global_epoch.load(std::memory_order_acquire);
        if (control_block->local_epoch.load(std::memory_order_relaxed) != epoch) // New epoch?
        {
            entries_since_update = 0;
        }
        else if (entries_since_update++ == UpdateThreshold)
        {
            entries_since_update = 0;
            const auto new_epoch = (epoch + 1) % number_epochs;
            if (!try_update_epoch(epoch, new_epoch))
                return;

            epoch = new_epoch;
        }
        else
            return;

        // we either just updated the global_epoch or we are observing a new epoch from some other thread
        // either way - we can reclaim all the objects from the old 'incarnation' of this epoch

        control_block->local_epoch.store(epoch, std::memory_order_relaxed);
        utils::delete_objects(retire_lists[epoch]);
    }

    void do_leave_critical() {
        // (5) - this release-store synchronizes-with the acquire-fence (6)
        control_block->is_in_critical_region.store(false, std::memory_order_release);
    }

    void add_retired_node(utils::deletable_object* p, size_t epoch) {
        assert(epoch < number_epochs);
        p->next = retire_lists[epoch];
        retire_lists[epoch] = p;
    }

    bool try_update_epoch(unsigned curr_epoch, unsigned new_epoch) {
        const auto old_epoch = (curr_epoch + number_epochs - 1) % number_epochs;
        auto prevents_update = [old_epoch](const thread_control_block& data)
        {
            // TSan does not support explicit fences, so we cannot rely on the acquire-fence (6)
            // but have to perform an acquire-load here to avoid false positives.
            constexpr auto memory_order = TSAN_MEMORY_ORDER(std::memory_order_acquire, std::memory_order_relaxed);
            return data.is_in_critical_region.load(memory_order) &&
                         data.local_epoch.load(std::memory_order_relaxed) == old_epoch;
        };

        // If any thread hasn't advanced to the current epoch, abort the attempt.
        auto can_update = !std::any_of(global_thread_block_list.begin(), global_thread_block_list.end(), prevents_update);
        if (!can_update)
            return false;

        if (global_epoch.load(std::memory_order_relaxed) == curr_epoch)
        {
            // (6) - this acquire-fence synchronizes-with the release-store (5)
            std::atomic_thread_fence(std::memory_order_acquire);

            // (7) - this release-CAS synchronizes-with the acquire-load (4)
            bool success = global_epoch.compare_exchange_strong(curr_epoch, new_epoch, std::memory_order_release, std::memory_order_relaxed);
            if (success)
                adopt_orphans();
        }

        // return true regardless of whether the CAS operation was successful or not, as it is not necessary to be successful
        return true;
    }

    void adopt_orphans() {
        auto current = global_thread_block_list.adopt_abandoned_retired_nodes();
        for (utils::deletable_object* next = nullptr; current != nullptr; current = next)
        {
            next = current->next;
            current->next = nullptr;
            add_retired_node(current, static_cast<utils::orphan<number_epochs>*>(current)->target_epoch);
        }
    }

    unsigned enter_count = 0;
    unsigned entries_since_update = 0;
    thread_control_block* control_block = nullptr;
    std::array<utils::deletable_object*, number_epochs> retire_lists = {};

    friend class epoch_based;
    ALLOCATION_COUNTER(epoch_based);
};

//GLOBALS
template <std::size_t UpdateThreshold>
std::atomic<unsigned> epoch_based<UpdateThreshold>::global_epoch;

template <std::size_t UpdateThreshold>
utils::thread_block_list<typename epoch_based<UpdateThreshold>::thread_control_block>
    epoch_based<UpdateThreshold>::global_thread_block_list;

template <std::size_t UpdateThreshold>
inline typename epoch_based<UpdateThreshold>::thread_data& epoch_based<UpdateThreshold>::local_thread_data() {
    static thread_local thread_data local_thread_data;
    return local_thread_data;
}

#ifdef TRACK_ALLOCATIONS
template <std::size_t UpdateThreshold>
utils::allocation_tracker epoch_based<UpdateThreshold>::allocation_tracker;

template <std::size_t UpdateThreshold>
inline void epoch_based<UpdateThreshold>::count_allocation()
{ local_thread_data().allocation_counter.count_allocation(); }

template <std::size_t UpdateThreshold>
inline void epoch_based<UpdateThreshold>::count_reclamation()
{ local_thread_data().allocation_counter.count_reclamation(); }
#endif
}}

#endif
