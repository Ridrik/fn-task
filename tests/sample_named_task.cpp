#include <functional>
#include <iostream>
#include <string>
#include <type_traits>

#include <task/named_task.hpp>
#include <task/task.hpp>

bool foo(std::string /*name*/,  // NOLINT(performance-unnecessary-value-param)
         std::string& /*data*/, int /*version*/) {
    return false;
}

bool bar(std::string& data, int /*version*/) noexcept {
    data += ',';
    return false;
}

bool bar_mimic(std::string& data, int /*version*/) {
    data += ',';
    return false;
}

static constexpr auto kRefBar = std::ref(bar);

int main() {
    // Make NamedTask with deduction guides, similarly to fn::Task/fn::makeTask
    auto task = fn::NamedTask([](int, double) {}, 2.0);
    auto task2 = fn::NamedTask([](int, double) {}, 2.0);
    // task = task2; // Error, even though they look identical, each lambda internally has a
    // different compiler generated type.

    // To solve this and make a fn::NamedTask shareable for the same signature and captured
    // arguments, user can cast captureless lambdas explicitly to raw function pointers. See below:
    auto task3 = fn::NamedTask(fn::toFuncPtr([](int, double) {}), 2.0);
    // Or, activate boolean template in factory function, to transform it to raw function pointer
    auto task4 = fn::makeNamedTask<true>([](int, double) {}, 2.0);
    // Now works
    task3 = task4;

    float c{5.0F};
    // Ref NamedTask automatically stores references with reference_wrapper (r-value values are
    // still stored by value)
    auto fTask = fn::makeRefNamedTask(fn::toFuncPtr([](int, double) {}), c);
    // This is equivalent to calling base builder with reference_wrapping references
    auto fTaskMimic = fn::makeNamedTask(fn::toFuncPtr([](int, double) {}), std::ref(c));
    static_assert(std::is_same_v<decltype(fTask), decltype(fTaskMimic)>);
    auto fTask2 = fTask;
    fTask = fTask2;
    const auto& doubleArg = task.getBoundArg<0>();

    // Common use case: 'this' pointer as void*
    class MyModule {};
    auto module = MyModule{};
    MyModule* mPtr{&module};
    using Payload = double;  // Whatever payload you wish
    auto subscriberTask = fn::makeNamedTask<true>(
        [](Payload payload, void* ptr) {  // You may switch order, it automatically detects front or
                                          // back positioning of captured arguments
            auto& self = *static_cast<MyModule*>(ptr);
            // Call your module's entry point for Payload to process it.
        },
        static_cast<void*>(mPtr));
    // Even more simply (Use this)
    auto subTask = fn::makeThisTask(
        [](Payload payload, void* ptr) noexcept { auto& self = *static_cast<MyModule*>(ptr); },
        mPtr);
    auto subTask2 = fn::makeThisTask(
        [](Payload payload, void* ptr) noexcept { auto& self = *static_cast<MyModule*>(ptr); },
        mPtr);
    subTask2 = subTask;
    static_assert(decltype(subTask)::matchesSignature<void(Payload) const & noexcept>());
    subscriberTask(5.5);

    // NamedTask from free function (needs not to store it inside)
    auto compileFunc = fn::makeNamedTask<foo>("David");
    auto str = std::string("Hello World");
    compileFunc(str, 2);
    auto compileFunc2 = fn::makeNamedTask<foo>(3);
    compileFunc2("David", str);
    auto compileFunc3 = fn::makeNamedTask<foo>("David", std::ref(str));  // Captures as reference
    compileFunc3(2);
    auto compileFunc4 = fn::makeRefNamedTask<foo>(str);

    // All the following yield same NamedTask
    auto str2 = std::string{"Hi There"};
    const auto compileFunc5 = fn::makeRefNamedTask<bar>(str2);
    const auto compileFunc6 = fn::makeNamedTask<bar>(std::ref(str2));
    std::string& strFetch = compileFunc5.getBoundArg<0>();
    using S1 = decltype(compileFunc5)::Signature;
    using S2 = decltype(compileFunc6)::Signature;
    const auto compileFunc6Copy = fn::makeNamedTask(compileFunc5);
    const auto compileFunc6Copy2 = fn::NamedTask{compileFunc5};
    static_assert(std::is_same_v<decltype(compileFunc6), decltype(compileFunc6Copy)>);
    static_assert(std::is_same_v<decltype(compileFunc6), decltype(compileFunc6Copy2)>);
    static_assert(std::is_same_v<decltype(compileFunc5), decltype(compileFunc6)>);

    // Similar for constant template parameters
    auto compileFunc7 = fn::makeNamedTask(fn::toFuncPtr<bar_mimic>(), std::ref(str2));
    auto compileFunc8 = fn::makeRefNamedTask(fn::toFuncPtr<bar_mimic>(), str2);
    static_assert(std::is_same_v<decltype(compileFunc7), decltype(compileFunc8)>);

    // A NamedTask can shelter another NamedTask by reference
    auto refNTaskFromAutoTask = fn::makeRefNamedTask(compileFunc5);
    static_assert(sizeof(refNTaskFromAutoTask) ==
                  sizeof(std::reference_wrapper<decltype(compileFunc5)>));
    std::cout << "str is " << str2 << '\n';

    // A NamedTask allows to retrieve any bound argument in it
    auto str3 = std::string{"Hi!"};
    auto funcWithLiterals = fn::makeRefNamedTask(
        [](const char* a, int&& b, std::string& c) noexcept {}, "hi there", 3, str3);
    std::string& strArg = funcWithLiterals.getBoundArg<2>();
    using S = decltype(funcWithLiterals)::Signature;

    auto nTask = fn::Task{funcWithLiterals};
    static_assert(decltype(nTask)::matchesSignature<decltype(funcWithLiterals)::Signature>());

    // Bind support
    auto bindBack = std::bind_back([](int a, double b) {}, 42.0);
    auto bindBackTask = fn::NamedTask{bindBack};
    bindBackTask(2);
    auto bindBackTaskCopy = fn::makeNamedTask(bindBackTask);
    auto refBindTask = fn::makeRefNamedTask(bindBackTask);

    {
        std::string s{"hi"};
        struct Functor {
            std::string data;

            void operator()(std::string& other) & {
                // data += other;
            }
        };
        auto sRef = std::ref(s);
        std::string&& sRRef = std::move(sRef.get());
        using strRefT = std::reference_wrapper<std::string>;
        using unref = std::unwrap_ref_decay_t<strRefT>;
        // static_assert(std::is_invocable_v<Functor, unref&&>);
        auto functor = Functor{};
        static_assert(std::is_invocable_v<std::reference_wrapper<Functor>, std::string&>);
        auto a = fn::makeRefNamedTask(functor, s);
        a();
        auto aa = fn::makeTask(Functor{}, s);
    }
}