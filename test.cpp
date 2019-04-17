#include "epoch_based.hpp"
#include <iostream>

using Reclaimer = reclamation::techniques::epoch_based<0>;

struct Foo : Reclaimer::enable_concurrent_ptr<Foo, 2>
{
  Foo** instance;
  Foo(Foo** instance) : instance(instance) {}
  virtual ~Foo() { if (instance) *instance = nullptr; }
};

template <typename T>
using concurrent_ptr = Reclaimer::concurrent_ptr<T>;
template <typename T> using marked_ptr = typename concurrent_ptr<T>::marked_ptr;

struct FooWithCustomDeleter;

struct MyDeleter {
    FooWithCustomDeleter* ref;
    void operator()(FooWithCustomDeleter* obj);
};

struct FooWithCustomDeleter : Reclaimer::enable_concurrent_ptr<FooWithCustomDeleter, 2, MyDeleter> {};

void MyDeleter::operator()(FooWithCustomDeleter* obj) {
    assert(ref == obj);
    delete obj;
    std::cout << "Custom deleter called.\n";
}

struct EpochBasedTest {
    Foo* foo = new Foo(&foo);
    marked_ptr<Foo> mp = marked_ptr<Foo>(foo, 3);

    // With UpdateThreshold set to 0, the creation of a guard_ptr to a dummy object
    // will trigger an epoch update
    void update_epoch() {
        Foo dummy(nullptr);
        concurrent_ptr<Foo>::guard_ptr gp(&dummy);
    }

    void wrap_around_epochs() {
        update_epoch();
        update_epoch();
        update_epoch();
    }

    // mark returns the same mark as original marked pointer
    void test1() {
        concurrent_ptr<Foo>::guard_ptr gp(mp);
        assert(mp.mark() == gp.mark());
    }

    // get returns the same pointer as original marked pointer
    void test2() {
        concurrent_ptr<Foo>::guard_ptr gp(mp);
        assert(mp.get() == gp.get());
    }

    // reset releases ownership and sets pointer to null
    void test3() {
        concurrent_ptr<Foo>::guard_ptr gp(mp);
        gp.reset();
        assert(gp.get() == nullptr);
    }

    // reclaim releases ownership and the object gets deleted when advancing two epochs
    void test4() {
        concurrent_ptr<Foo>::guard_ptr gp(mp);
        gp.reclaim();
        assert(foo != nullptr);
        this->mp = nullptr;
        wrap_around_epochs();
        assert(gp.get() == nullptr);
    }

    // object cannot be reclaimed as long as another guard_ptr guards it
    void test5() {
        concurrent_ptr<Foo>::guard_ptr gp(mp);
        concurrent_ptr<Foo>::guard_ptr gp2(mp);
        gp.reclaim();
        wrap_around_epochs();
        assert(foo != nullptr);
    }

    // copy constructor leads to shared ownership, preventing the object from being reclaimed
    void test6() {
        concurrent_ptr<Foo>::guard_ptr gp(mp);
        concurrent_ptr<Foo>::guard_ptr gp2(gp);
        gp.reclaim();
        this->mp = nullptr;
        wrap_around_epochs();
        assert(foo != nullptr);
    }

    // assignment operation leads to shared ownership, preventing the object from being deleted
    void test7() {
        concurrent_ptr<Foo>::guard_ptr gp(mp);
        concurrent_ptr<Foo>::guard_ptr gp2{};
        gp2 = gp;
        gp.reclaim();
        this->mp = nullptr;
        wrap_around_epochs();
        assert(foo != nullptr);
    }

    // move constructor moves ownership and resets source object
    void test8() {
        concurrent_ptr<Foo>::guard_ptr gp(mp);
        concurrent_ptr<Foo>::guard_ptr gp2(std::move(gp));
        assert(gp.get() == nullptr);
        gp2.reclaim();
        this->mp = nullptr;
        wrap_around_epochs();
        assert(foo == nullptr);
    }

    // move assignment moves ownership and resets source object
    void test9() {
        concurrent_ptr<Foo>::guard_ptr gp(mp);
        concurrent_ptr<Foo>::guard_ptr gp2{};
        gp2 = std::move(gp);
        gp2.reclaim();
        this->mp = nullptr;
        wrap_around_epochs();
        assert(gp.get() == nullptr);
        assert(foo == nullptr);
    }

    // custom deleter
    void test10() {
        FooWithCustomDeleter* fo = new FooWithCustomDeleter();
        concurrent_ptr<FooWithCustomDeleter>::guard_ptr gp(new FooWithCustomDeleter());
        gp.reclaim(MyDeleter{gp.get()});
        wrap_around_epochs();
    }

    ~EpochBasedTest() {
        wrap_around_epochs();
        if (mp == nullptr)
            assert(foo == nullptr);
        else
            delete foo;
    }
};



int main(int argc, char const *argv[])
{
    {
        EpochBasedTest a;
        a.test1();
    }

    {
        EpochBasedTest a;
        a.test2();
    }

    {
        EpochBasedTest a;
        a.test3();
    }

    {
        EpochBasedTest a;
        a.test4();
    }

    {
        EpochBasedTest a;
        a.test5();
    }
    
    {
        EpochBasedTest a;
        a.test6();
    }

    {
        EpochBasedTest a;
        a.test7();
    }
    
    {
        EpochBasedTest a;
        a.test8();
    }

    {
        EpochBasedTest a;
        a.test9();
    }
    
    {
        EpochBasedTest a;
        a.test10();
    }
    
    return 0;
}