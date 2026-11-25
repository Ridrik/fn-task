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
    // A Task is constructed with a callable, and optionally bound arguments that will bind to the
    // callable. The bind direction (back, front) is deducted, as well as the signature, size and
    // other settings

    // captures integer, is being taken as rvalue-ref by the callable. Signature is hence void(const
    // std::string&) && noexcept
    auto task = fn::makeTask([](int&& /*age*/, const std::string& name) noexcept {}, 0);
    static_assert(decltype(task)::matchesSignature<void(const std::string&) && noexcept>());

    // If you want better control of settings and to convert signatures, you can use factory builder

    // Here, we strip '&&' and noexcept from the signature. This is allowed. The other way around
    // would be illegal (par one exception).
    auto taskWithBuilder =
        fn::buildTask([](int&& /*unused*/, const std::string& /*unused*/) noexcept {}, 0)
            .withSignature<void(const std::string&)>()
            .build();

    // You can also control size, heap policy, etc, directly with factory functions or directly
    // given type

    // Alias with heap policy and 8 bytes of inline size
    auto flexTask = fn::FlexTask8<void(double) &>{[](std::string& /*unused*/, double /*unused*/) {},
                                                  std::string{"hey"}};
    // Or
    auto flexTaskCopy = fn::buildTask([](std::string& str, double number) {}, std::string{"hey"})
                            .allowHeap()
                            .withInlineSize<8>()
                            .build();
    static_assert(std::is_same_v<decltype(flexTask), decltype(flexTaskCopy)>);

    // References: Task allows taking the callable or bound arguments as references, via
    // fn::ref/fn::cref, or globally with factory method
    auto s = std::string{"hi there"};
    // Note that signature is void() const, since we don't own the string, just a reference. In this
    // case, all qualifiers would be possible, hence it's allowed to convert to '&' and '&&'
    auto taskWithRef = fn::buildTask([](std::string&& /*unused*/) {}, fn::ref(s)).build();
    static_assert(decltype(taskWithRef)::matchesSignature<void() const>());
    auto taskWithRef2 = fn::buildTask([](std::string&& /*unused*/) {}, fn::ref(s))
                            .withSignature<void() &>()
                            .build();
    // byRef() stores references of any lvalue-ref argument forward by the constructor
    auto taskWithRef3 =
        fn::buildTask([](std::string&& /*unused*/) {}, s).byRef().withSignature<void() &>().build();

    // What about std::reference_wrapper? You can also use them, but C++ rules apply:

    // Error: std::reference_wrapper cannot implicitly convert to rvalue-ref of the referenced type
    // auto taskWithRef4 = fn::buildTask([](std::string&& str) {}, std::ref(s)).build();

    // This works
    auto taskWithRef5 = fn::buildTask([](std::string& str) {}, std::ref(s)).build();

    // You can still use lambdas alone with captured list
    // Note: fn::Task has similar deduction guides as to fn::makeTask
    auto task3 = fn::Task{
        [name = std::string("Hello World")](int age) { std::cout << "Name: " << name << '\n'; }};
    task3(10);

    // fn::Task and its factories have various deductors for size, copyability, bind, signature, etc
    // Deducts move only tasks
    auto uniqueTask = fn::makeTask([](std::unique_ptr<int>& a) { return *a; },
                                   std::make_unique<int>(5));  // Makes it mutable
    // OR, with alias
    auto uniqueTask2 = fn::UniqueTask<int()>{[](const std::unique_ptr<int>& a) { return *a; },
                                             std::make_unique<int>(5)};
    static_assert(!decltype(uniqueTask)::isCopyable());

    constexpr int kVersion{1};
    // fn::Task allows binding compile time callables, making it as lightweight as possible (without
    // captured arguments, no buffer even exists!)
    auto bindTask = fn::buildTask<foo>().build();
    auto data = std::string{"Some data"};
    bindTask("My name", data, kVersion);

    // Explict binding and full settings

    {
        auto hi = std::string{"hi"};
        auto hey = std::string{"hey"};
        auto task =
            fn::buildTask(
                [](std::string& a, const std::string& b) noexcept -> int {
                    a = "Hi There";
                    return 3;
                },
                hi)
                .withSignature<double(std::string)>()  // int can convert to double, std::string
                                                       // can convert to const std::string&
                .withInlineSize<8>()
                .moveOnly()
                .withAlignment<8>()
                .allowHeap()
                .bindFront()
                .byRef()
                .build();
    }

    // like() factory method
    {
        auto hi = std::string{"hi"};
        auto otherTask = fn::buildTask([](int, std::string&&) noexcept {}, fn::ref(hi)).build();
        // like() to ensure it will match the type of another Task
        auto otherTask2 = fn::buildTask([](int) noexcept {}).like(otherTask).build();
        static_assert(decltype(otherTask)::matchesSignature<decltype(otherTask2)::Signature>());
        otherTask(3);
    }

    // bind_to - Explicit binding, a new feature with fn::Task (and fn::NamedTask)
    {
        auto str = std::string{"hello"};
        auto customBindTask =
            fn::buildTask([](std::string, const int&, std::string&) {}, str, fn::ref(str))
                .bindTo<0, 2>()
                .build();
    }

    // Other examples...
    task("Hello World");
    // Same body, but now it captured the string, hence the callable becomes Task<void(int)>
    auto task2 = fn::makeTask(
        [](int age, const std::string& name) noexcept { std::cout << "Name: " << name << '\n'; },
        std::string("Hello World"));
    static_assert(decltype(task2)::matchesSignature<void(int) const noexcept>());
    task2(10);
    auto str = std::string{"Hello World"};
    // You can capture by reference, equivalently to '&str' in lambda
    auto task2_half = fn::makeTask(
        [](int age, std::reference_wrapper<const std::string> name) {
            std::cout << "Name: " << name.get() << '\n';
        },
        std::cref(str));

    // You can also explicitly initialize it:
    // If you want to allow heap (similar to std::function), you may use 'FlexTask' alias.
    auto task4 = fn::FlexTask<void(int)>{
        [name = std::string("Hello World")](int age) { std::cout << "Name: " << name << '\n'; }};
    auto flexTaskNewCopy = fn::makeTask(flexTask);
    auto flexTaskNewCopy2 = fn::makeFlexTask(flexTask);
    static_assert(std::is_same_v<decltype(flexTask), decltype(flexTaskNewCopy)>);
    static_assert(std::is_same_v<decltype(flexTask), decltype(flexTaskNewCopy2)>);

    // Or, have it on the stack -> way faster copies and moves if you frequently transfer tasks (e.g
    // thread pools, producer/consumer schemes)
    auto task5 = fn::Task64<void(int)>{
        [name = std::string("Hello World")](int age) { std::cout << "Name: " << name << '\n'; }};

    auto _ = fn::Task64<void(int&) &>{
        [name = std::string("Hello World")](int& age) mutable { name = "Hi"; }};  // Error,

    // This works
    auto refSigTask = fn::Task64<void(int&) &>{
        [](int& age, std::string& name) mutable { name = "Hi"; }, std::string("Hello World")};

    // Bind support
    double temp{};
    auto mutLambda = [temp](int a) mutable {};
    mutLambda(2);
    auto tmpLambda = [temp](int a) mutable {};
    static_assert(fn::HasFunctionTraits<std::reference_wrapper<decltype(tmpLambda)>>);
    auto bindBack = std::bind_back(std::ref(tmpLambda), 42);
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
        auto funcTask = fn::makeTask(fn::cref(functor), fn::ref(s));
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
        auto task = fn::buildTask(fn::ref(functor), fn::ref(hi)).build();
        task();
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
    }
}