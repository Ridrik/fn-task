#include <functional>
#include <iostream>
#include <string>

#include <task/named_task.hpp>
#include <task/task.hpp>

#include "task/function_traits.hpp"

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

int main() {
    // Make NamedTask with deduction guides, similarly to fn::Task/fn::makeTask
    auto task = fn::NamedTask([](int, double) {}, 2.0);
    auto task2 = fn::NamedTask([](int, double) {}, 2.0);

    float c{5.0F};

    auto fTask = fn::makeRefNamedTask([](int a, float kek) {}, c);
    auto fTask2 = fTask;
    fTask = fTask2;

    const auto& theDouble = task.getBoundArg<0>();
    // task = task2; // Error, even though they look identical, each lambda internally has a
    // different compiler generated type.

    // To solve this and make a fn::NamedTask shareable for the same signature and captured
    // arguments, user can cast captureless lambdas explicitly to raw function pointers. See below:
    auto task3 = fn::NamedTask(fn::toFuncPtr([](int, double) {}), 2.0);
    // Or, activate boolean template in factory function, to transform it to raw function pointer
    auto task4 = fn::makeNamedTask<true>([](int, double) {}, 2.0);
    // Now works
    task3 = task4;

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

    auto compileFunc = fn::makeNamedTask<foo>("David");
    auto str = std::string("Hello World");
    compileFunc(str, 2);
    auto compileFunc2 = fn::makeNamedTask<foo>(3);
    compileFunc2("David", str);
    ;
    auto compileFunc3 = fn::makeNamedTask<foo>("David", std::ref(str));  // Captures as reference
    compileFunc3(2);

    // const auto compileFunc4 = fn::makeNamedTask<foo>("David", std::ref(str));
    // compileFunc4(2); // This fails, since namedTask is marked const, and captured argument needs
    // to be used as std::string&
    // This works
    auto str2 = std::string{"Hi There"};
    const auto compileFunc4 = fn::makeRefNamedTask<bar>(str2);
    const auto compileFunc45 = fn::makeNamedTask<bar>(std::ref(str2));
    auto compileFunc46 = fn::makeNamedTask(fn::toFuncPtr<bar_mimic>(), std::ref(str2));
    auto compileFunc5 = fn::makeRefNamedTask(fn::toFuncPtr<bar_mimic>(), str2);
    compileFunc4(1);
    compileFunc5(1);
    compileFunc45.getBoundArg<0>();
    std::string& kekers = compileFunc5.getBoundArg<0>();
    using S1 = decltype(compileFunc5)::Signature;
    using S2 = decltype(compileFunc46)::Signature;
    compileFunc5 = compileFunc46;
    compileFunc46 = compileFunc5;
    std::cout << "str is " << str2 << '\n';

    auto str3 = std::string{"Hi!"};
    auto funcWithLiterals = fn::makeRefNamedTask(
        [](const char* a, int&& b, std::string& c) noexcept {}, "hi there", 3, str3);

    funcWithLiterals.getBoundArg<2>();
    using S = decltype(funcWithLiterals)::Signature;

    auto nTask = fn::Task{funcWithLiterals};
    static_assert(decltype(nTask)::matchesSignature<decltype(funcWithLiterals)::Signature>());
}