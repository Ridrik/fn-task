#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <task/task.hpp>

#include "task/deductors.hpp"
#include "task/function_traits.hpp"

bool foo(const std::string& /*name*/, std::string& /*data*/, int /*version*/) noexcept {
    return false;
}

struct A {
    int a{};
    std::string_view operator()(double& b) const&& noexcept {
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
    double thisHere = 3.0;
    auto thisTask = fn::makeRefTask(a, 3.0);
    thisTask();
    auto task = fn::makeTask(
        [](int&& age, const std::string& name) mutable noexcept {
            std::cout << "Name: {}" << name << '\n';
        },
        0);
    static_assert(decltype(task)::matchesSignature<void(const std::string&) noexcept>());
    auto taskMimic = fn::Task<void(const std::string&) noexcept>{task};
    auto taskMimic2 = fn::Task<void(const std::string&)>{task};
    task("Hello World");
    // Same body, but now it captured the string, hence the callable becomes Task<void(int)>
    auto task2 = fn::makeTask(
        [](int age, const std::string& name) noexcept { std::cout << "Name: {}" << name << '\n'; },
        std::string("Hello World"));
    static_assert(decltype(task2)::matchesSignature<void(int) const noexcept>());
    task2(10);
    auto str = std::string{"Hello World"};
    // You can capture by reference, equivalently to '&str' in lambda
    auto task2_half = fn::makeTask(
        [](int age, std::reference_wrapper<const std::string> name) {
            std::cout << "Name: {}" << name.get() << '\n';
        },
        std::cref(str));  // Or std::ref if one wishes to mutate
    // You can still use lambdas alone with captured list
    // Note: fn::Task has similar deduction guides as to fn::makeTask
    auto task3 = fn::Task{
        [name = std::string("Hello World")](int age) { std::cout << "Name: {}" << name << '\n'; }};
    task3(10);

    // You can also explicitly initialize it:
    // If you want to allow heap (similar to std::function), you may use 'FlexTask' alias.
    auto task4 = fn::FlexTask<void(int)>{
        [name = std::string("Hello World")](int age) { std::cout << "Name: {}" << name << '\n'; }};
    auto flexTask = fn::makeFlexTask<void(int) const>(
        [name = std::string("Hello World")](int age) { std::cout << "Name: {}" << name << '\n'; });

    // Or, have it on the stack -> way faster copies and moves if you frequently transfer tasks (e.g
    // thread pools, producer/consumer schemes)
    auto task5 = fn::Task64<void(int)>{
        [name = std::string("Hello World")](int age) { std::cout << "Name: {}" << name << '\n'; }};

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
    // This would fail
    // auto _ =
    //    fn::makeImmutTask([](std::unique_ptr<int>& a) { return *a; }, std::make_unique<int>(5));

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
    static_assert(decltype(taskk2)::matchesSignature<std::string()>());
    std::cout << "Message is " << taskk2() << '\n';

    // Bind support
    auto bindBack = std::bind_back([](int a) {}, 42);
    auto bindBackTask = fn::Task{bindBack};
    bindBackTask();
}