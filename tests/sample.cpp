#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <task/task.hpp>

#include "task/deductors.hpp"
#include "task/function_traits.hpp"
#include "task/payload.hpp"

bool foo(const std::string& /*name*/, std::string& /*data*/, int /*version*/) noexcept {
    return false;
}

struct A {
    int a{};
    std::string_view operator()(const double& b) const noexcept {
        //++a;
        return "";
    }
};

struct B {
    int a{};
    std::string_view operator()(double& b) noexcept {
        //++a;
        return "";
    }
};

int main() {
    // Task takes a callable and optionally capture arguments, which are expected to be part of
    // the erased callable. It automatically detects the task signature, mutability,
    // copiability, and size. Here, integer is captured (bind_front like), hence signature is
    // Task<void(std::string)>
    A a{};
    B b{};
    double thisHere = 3.0;
    auto thisTask = fn::makeTask<std::string_view() const>(std::cref(a), thisHere);
    static_assert(decltype(thisTask)::matchesSignature<std::string_view() const>());
    auto thisTaskCopy = fn::Task{thisTask};
    thisTask();
    auto task = fn::makeTask(
        [](int&& /*age*/, const std::string& name) mutable noexcept {  // NOLINT
            std::cout << "Name: " << name << '\n';
        },
        0);
    static_assert(decltype(task)::matchesSignature<void(const std::string&) && noexcept>());
    auto taskMimic = fn::Task<void(const std::string&) noexcept>{task};
    auto taskMimic2 = fn::Task<void(const std::string&)>{task};
    task("Hello World");
    // Same body, but now it captured the string, hence the callable becomes Task<void(int)>
    auto task2 = fn::makeTask(
        [](int age, const std::string& name) noexcept { std::cout << "Name: " << name << '\n'; },
        std::string("Hello World"));
    static_assert(decltype(task2)::matchesSignature<void(int) const & noexcept>());
    task2(10);
    auto str = std::string{"Hello World"};
    // You can capture by reference, equivalently to '&str' in lambda
    auto task2_half = fn::makeTask(
        [](int age, std::reference_wrapper<const std::string> name) {
            std::cout << "Name: " << name.get() << '\n';
        },
        std::cref(str));  // Or std::ref if one wishes to mutate
    // You can still use lambdas alone with captured list
    // Note: fn::Task has similar deduction guides as to fn::makeTask
    auto task3 = fn::Task{
        [name = std::string("Hello World")](int age) { std::cout << "Name: " << name << '\n'; }};
    task3(10);

    // You can also explicitly initialize it:
    // If you want to allow heap (similar to std::function), you may use 'FlexTask' alias.
    auto task4 = fn::FlexTask<void(int)>{
        [name = std::string("Hello World")](int age) { std::cout << "Name: " << name << '\n'; }};
    auto flexTask = fn::makeFlexTask<void(int) const>(
        [name = std::string("Hello World")](int age) { std::cout << "Name: " << name << '\n'; });
    auto flexTaskCopy = fn::makeTask(flexTask);
    auto flexTaskCopy2 = fn::makeFlexTask(flexTask);
    static_assert(std::is_same_v<decltype(flexTask), decltype(flexTaskCopy)>);
    static_assert(std::is_same_v<decltype(flexTask), decltype(flexTaskCopy2)>);

    // Or, have it on the stack -> way faster copies and moves if you frequently transfer tasks (e.g
    // thread pools, producer/consumer schemes)
    auto task5 = fn::Task64<void(int)>{
        [name = std::string("Hello World")](int age) { std::cout << "Name: " << name << '\n'; }};

    auto _ = fn::Task64<void(int&) &>{
        [name = std::string("Hello World")](int& age) mutable { name = "Hi"; }};  // Error,
    //    fn::Task are immutable by default

    // auto _ = fn::Task64<void(int&)>{[](int& age, std::string& name) mutable { name = "Hi"; },
    //                                 std::string("Hello World")}; Similar error

    // This works
    auto refSigTask = fn::Task64<void(int&) &>{
        [](int& age, std::string& name) mutable { name = "Hi"; }, std::string("Hello World")};

    // Deducts move only tasks
    auto uniqueTask = fn::makeTask([](std::unique_ptr<int>& a) { return *a; },
                                   std::make_unique<int>(5));  // Makes it mutable
    // OR
    auto uniqueTask2 = fn::UniqueTask<int()>{[](const std::unique_ptr<int>& a) { return *a; },
                                             std::make_unique<int>(5)};
    static_assert(!decltype(uniqueTask)::isCopyable());

    using sig = fn::FunctionTraits<decltype([a = std::string{}]() {})>::signature;

    constexpr int kVersion{1};
    // fn::Task allows binding compile time callables, making it as lightweight as possible (without
    // captured arguments, no buffer even exists!)
    auto bindTask = fn::makeTask<foo>();
    static_assert(
        decltype(bindTask)::matchesSignature<bool(const std::string&, std::string&, int) const &
                                             noexcept>());
    auto data = std::string{"Some data"};
    bindTask("My name", data, 1);

    // Gets deducted as void(int) const, since lambdas are usually either const or mutable in
    // qualifications
    auto taskk = fn::Task{[](int a, double b) {}, 3.0};

    class Wrapper {
       public:
        std::string str;

        std::string operator()(std::size_t& s) && {
            s += str.size();
            return std::move(str);
        }
    };

    // operator itself is &&, while captured argument std::size_t is taken as '&', hence signature
    // is a cv-less std::string()
    auto taskk2 = fn::makeTask(Wrapper{"Hello World"}, std::size_t{5});
    static_assert(fn::HasFunctionTraits<std::decay_t<decltype(taskk2)>>);
    auto taskk3 = fn::makeRefTask(taskk2);
    static_assert(decltype(taskk2)::matchesSignature<std::string()>());
    std::cout << "Message is " << taskk2() << '\n';

    // Bind support
    double temp{};
    auto mutLambda = [temp](int a) mutable {};
    mutLambda(2);
    auto bindBack = std::bind_back([temp](int a) mutable {}, 42);
    auto bindBackTask = fn::makeTask(bindBack);
    bindBackTask();

    {
        std::string s{"hi"};
        struct Functor {
            std::string data;

            void operator()(std::string&& other) const {
                // data += other;
            }
        };
        auto functor = Functor{};
        auto funcTask = fn::makeTask(functor, fn::ref(s));
        funcTask();
        auto funcTask2 = fn::makeTask(Functor{}, s);
    }

    {
        // Test without deductors
        struct Functor {
            std::string data;

            void operator()(std::string&& other) && {
                // data += other;
            }
        };
        auto hi = std::string{"hi"};
        auto functor = Functor{};
        using kekers =
            std::add_const_t<fn::UnwrapBoundRefWrapT<fn::BoundRefWrapTag<std::string&, false>>>;
        auto task = fn::Task<void() const, fn::Copyability::kCopyable, fn::InlineSize{48},
                             fn::Triviality::kNonTrivial>{fn::ref(functor), fn::ref(hi)};
    }

    {
        // Factory test
        auto hi = std::string{"hi"};
        auto hey = std::string{"hey"};
        auto task =
            fn::buildTask(
                [](std::string& a, const std::string& b) noexcept -> int {
                    a = "Hi There";
                    return 3;
                },
                hi)
                .withSignature<double(std::string)>()  // int can convert to double, std::string can
                                                       // convert to const std::string&
                .withInlineSize<8>()
                .moveOnly()
                .withAlignment<8>()
                .allowHeap()
                .bindFront()
                .byRef()
                .build();
        task(hey);
        std::cout << "New string is: " << hi << '\n';  // Prints "Hi There"
        auto task2 = std::move(task);

        // Or, building the print once and applying its type to any task
        constexpr auto kMySettings = fn::getSettings()
                                         .withSignature<void(std::string&)>()
                                         .withInlineSize<8>()
                                         .moveOnly()
                                         .withAlignment<8>()
                                         .allowHeap()
                                         .bindFront()
                                         .byRef();
        auto taskMimic = fn::makeTask<kMySettings>(
            [](std::string& a, std::string& b) noexcept { a = "Hi There"; }, hi);
        // Or
        auto newTask =
            fn::buildTask([](double c, std::string&) {}, 3.0).withSettings<kMySettings>().build();
        newTask.swap(taskMimic);

        auto otherTask = fn::buildTask([](int, std::string&&) {}, fn::ref(hi)).build();
        otherTask(3);

        // Error: std::reference_wrapper<std::string> isn't implicitly convertible to std::string&&
        // auto otherTask2 = fn::buildTask([](int, std::string&&) {}, std::ref(hi)).build();
    }
}