#ifndef _DELETABLE_OBJECT_
#define _DELETABLE_OBJECT_

#include <memory>
#include <type_traits>
#include <array>

namespace reclamation { namespace techniques { namespace utils {

struct deletable_object {
    deletable_object* next = nullptr;
    virtual void delete_self() = 0;

protected:
    virtual ~deletable_object() = default;
};

void delete_objects(deletable_object*& list) {
    auto current = list;
    for (deletable_object* next = nullptr; current != nullptr; current = next) {
        next = current->next;
        current->delete_self();
    }
    list = nullptr;
}

template <class Derived, class DeleterT, class Base>
struct deletable_object_with_non_empty_deleter : Base
{
    using Deleter = DeleterT;
    virtual void delete_self() override {
        Deleter& my_deleter = reinterpret_cast<Deleter&>(deleter_buffer);
        Deleter deleter(std::move(my_deleter));
        my_deleter.~Deleter();

        deleter(static_cast<Derived*>(this));
    }

    void set_deleter(Deleter deleter) {
        new (&deleter_buffer) Deleter(std::move(deleter));
    }

private:
    using buffer = typename std::aligned_storage<sizeof(Deleter), alignof(Deleter)>::type;
    buffer deleter_buffer;
};

template <class Derived, class DeleterT, class Base>
struct deletable_object_with_empty_deleter : Base {
    using Deleter = DeleterT;
    virtual void delete_self() override {
        static_assert(std::is_default_constructible<Deleter>::value, "empty deleters must be default constructible");
        Deleter deleter{};
        deleter(static_cast<Derived*>(this));
    }

    void set_deleter(Deleter deleter) {}
};

template <class Derived, class Deleter = std::default_delete<Derived>, class Base = deletable_object>
using deletable_object_impl = std::conditional_t<std::is_empty<Deleter>::value,
    deletable_object_with_empty_deleter<Derived, Deleter, Base>,
    deletable_object_with_non_empty_deleter<Derived, Deleter, Base>
>;

template <unsigned Epochs>
struct orphan : utils::deletable_object_impl<orphan<Epochs>>
{
    const unsigned target_epoch;
    
    orphan(unsigned target_epoch, std::array<utils::deletable_object*, Epochs> &retire_lists): target_epoch(target_epoch), retire_lists(retire_lists) {}

    ~orphan() {
        for (auto p: retire_lists)
            utils::delete_objects(p);
    }

private:
  std::array<utils::deletable_object*, Epochs> retire_lists;
};

}}}

#endif
