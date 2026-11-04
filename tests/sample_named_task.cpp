#include <functional>
#include <string>

#include <task/named_task.hpp>
#include <task/task.hpp>

bool foo(std::string /*name*/,  // NOLINT(performance-unnecessary-value-param)
         std::string& /*data*/, int /*version*/) {
    return false;
}

bool bar(std::string /*name*/,  // NOLINT(performance-unnecessary-value-param)
         const std::string& /*data*/, int /*version*/) noexcept {
    return false;
}

bool bar_mimic(std::string /*name*/,  // NOLINT(performance-unnecessary-value-param)
               const std::string& /*data*/, int /*version*/) {
    return false;
}

int main() {
    // Make NamedTask with deduction guides, similarly to fn::Task/fn::makeTask
    auto task = fn::NamedTask([](int, double) {}, 2.0);
    auto task2 = fn::NamedTask([](int, double) {}, 2.0);

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

    auto compileFunc3 = fn::makeNamedTask<foo>("David", std::ref(str));  // Captures as reference
    compileFunc3(2);

    // const auto compileFunc4 = fn::makeNamedTask<foo>("David", std::ref(str));
    // compileFunc4(2); // This fails, since namedTask is marked const, and captured argument needs
    // to be used as std::string&
    // This works
    const auto compileFunc4 = fn::makeNamedTask<bar>("David", std::ref(str));
    auto compileFunc5 = fn::makeNamedTask(fn::toFuncPtr<bar_mimic>(), "David", std::ref(str));
    compileFunc4(2);
    compileFunc5 = compileFunc4;
    auto compileFunc6 = decltype(compileFunc5){compileFunc4};
}