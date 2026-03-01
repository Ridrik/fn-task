
# `fn::Task`

### Description

`fn::Task` is a type-erased callable for c++23+, meant as a flexible alternative to std::function. It supports manual selection of inline stack size, automatic bind_front/bind_back functionality, mutability and heap toggles, as well as move only callables and extensible set of deduction guides.

---

## Key Features
* Captures arguments thrown in following the inner callable, in an extensive and automatic way, either bind_front or bind_back.
* Extensive user-control: You control the stack buffer, mutability, heap-allowance, copiability, etc.
* Immutable, stack-only by default. Allow heap with `fn::FlexTask` or by manually selecting the boolean template. Similar for mutable with `fn::TaskMut`, etc.
* Allows taking references, similar to capturing lambda. Simply use std::ref or std::cref on a captured argument, storing a reference instead.
* Referencing tasks: If you know all your arguments are references, you can use `fn::makeRefTask` instead. All l-values given will be stored via references. R-values given are still stored to avoid dangling
* Movable arguments. If mutability is selected, inner callable may move the type-erased arguments in the body, perfect for one-shot Tasks.
* Extensive deduction guides, both directly with `fn::Task{Args&&...}` or via factory functions `fn::makeTask`, `fn::makeTask<Signature>`, `fn::makeMutTask`, `fn::makeRefTask`, etc.


---

## Quick Start

### 1. Integration
Add this to your `CMakeLists.txt` to integrate `fn::Task` directly into your project:

```cmake
include(FetchContent)
FetchContent_Declare(
    task
    GIT_REPOSITORY [https://github.com/Ridrik/fn-task.git](https://github.com/Ridrik/fn-task.git)
    GIT_TAG v1.0.0
)
FetchContent_MakeAvailable(task)

# Link to your application
target_link_libraries(my_app PRIVATE fn::task)
```

---

## Example

```cpp


#include <iostream>
#include <memory>
#include <string>

#include "task/task.hpp"

bool foo(std::string /*name*/, std::string& /*data*/, int /*version*/) {
    return false;
}

int main() {
    // Task takes a callable and optionally capture arguments, which are expected to be part of the
    // erased callable. It automatically detects the task signature, mutability, copiability, and
    // size. Here, integer is captured (bind_front like), hence signature is Task<void(std::string)>
    auto task =
        fn::makeTask([](int age, std::string name) { std::cout << "Name: {}" << name << '\n'; }, 0);
    static_assert(task.matchesSignature<void(std::string)>());
    task("Hello World");
    // Same body, but now it captured the string, hence the callable becomes Task<void(int)>
    auto task2 =
        fn::makeTask([](int age, std::string name) { std::cout << "Name: {}" << name << '\n'; },
                     std::string("Hello World"));
    static_assert(task2.matchesSignature<void(int)>());
    task2(10);

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
    auto bindTask = fn::makeTask<foo>(kVersion);
    auto data = std::string{"Some data"};
    bindTask("My name", data);
}

```

---

## License
`fn::Task` is licensed under the **MIT License** (see `LICENSE` file).
