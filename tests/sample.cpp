#include <iostream>
#include <memory>
#include <string>

#include "task/named_task.hpp"
#include "task/task.hpp"

bool foo(std::string /*name*/, std::string& /*data*/, int /*version*/) {
    return false;
}

int main() {
    // Task takes a callable and optionally capture arguments, which are expected to be part of
    // the erased callable. It automatically detects the task signature, mutability,
    // copiability, and size. Here, integer is captured (bind_front like), hence signature is
    // Task<void(std::string)>
    auto task =
        fn::makeTask([](int age, std::string name) { std::cout << "Name: {}" << name << '\n'; }, 0);
    static_assert(decltype(task)::matchesSignature<void(std::string)>());
    task("Hello World");
    // Same body, but now it captured the string, hence the callable becomes Task<void(int)>
    auto task2 =
        fn::makeTask([](int age, std::string name) { std::cout << "Name: {}" << name << '\n'; },
                     std::string("Hello World"));
    static_assert(decltype(task2)::matchesSignature<void(int)>());
    task2(10);
    auto str = std::string{"Hello World"};
    // You can capture by reference, equivalently to '&str' in lambda
    auto task2_half = fn::makeTask(
        [](int age, const std::string& name) { std::cout << "Name: {}" << name << '\n'; },
        std::cref(str));  // Or std::ref if one wishes to mutate
    task2_half(10);
    str = "Hi";
    task2_half(10);

    // You can still use lambdas alone with captured list
    // Note: fn::Task has similar deduction guides as to fn::makeTask
    auto task3 = fn::Task{
        [name = std::string("Hello World")](int age) { std::cout << "Name: {}" << name << '\n'; }};
    task3(10);

    // You can also explicitly initialize it:
    // If you want to allow heap (similar to std::function), you may use 'FlexTask' alias.
    auto task4 = fn::FlexTask<void(int)>{
        [name = std::string("Hello World")](int age) { std::cout << "Name: {}" << name << '\n'; }};
    auto flexTask = fn::makeFlexTask<void(int)>(
        [name = std::string("Hello World")](int age) { std::cout << "Name: {}" << name << '\n'; });

    // Or, have it on the stack -> way faster copies and moves if you frequently transfer tasks (e.g
    // thread pools, producer/consumer schemes)
    auto task5 = fn::Task64<void(int)>{
        [name = std::string("Hello World")](int age) { std::cout << "Name: {}" << name << '\n'; }};

    // auto _ = fn::Task64<void(int&)>{
    //    [name = std::string("Hello World")](int& age) mutable { name = "Hi"; }}; // Error,
    //    fn::Task are immutable by default

    // auto _ = fn::Task64<void(int&)>{[](int& age, std::string& name) mutable { name = "Hi"; },
    //                                 std::string("Hello World")}; Similar error

    // This works
    auto _ = fn::TaskMut<void(int&)>{[](int& age, std::string& name) mutable { name = "Hi"; },
                                     std::string("Hello World")};

    // Deducts move only tasks
    auto uniqueTask = fn::makeTask([](std::unique_ptr<int>& a) { return *a; },
                                   std::make_unique<int>(5));  // Makes it mutable
    // OR
    auto _ = fn::UniqueTask<int()>{[](const std::unique_ptr<int>& a) { return *a; },
                                   std::make_unique<int>(5)};
    static_assert(!uniqueTask.isCopyable());
    // This would fail
    // auto _ =
    //    fn::makeImmutTask([](std::unique_ptr<int>& a) { return *a; }, std::make_unique<int>(5));

    constexpr int kVersion{1};
    // fn::Task allows binding compile time callables, making it as lightweight as possible (without
    // captured arguments, no buffer even exists!)
    auto bindTask = fn::makeTask<foo>();
    auto data = std::string{"Some data"};
    bindTask("My name", data, 1);
}