
# `fn::Task`

### Description

`fn::Task` is a type-erased callable for c++23+, meant as a flexible alternative to std::function. It supports manual selection of inline stack size, automatic bind_front/bind_back functionality, mutability and heap toggles, as well as move only callables and extensible set of deduction guides.

In addition, `fn::NamedTask` is the opposite: a typed callable. Its type is directly tied to the bound arguments that construct it. This allows the indirection of typical erased types to be eliminated, thus providing an even more performant callable, at the expense of losing flexibility for different tasks. Notable, `fn::NamedTask` can be used to hold 'this' pointer of each module using a single `void*` argument, allowing a top performance speed for regular invocations. It can also be naturally transferred to `fn::Task` when needed.

---

## Key Features
* Captures arguments thrown in following the inner callable, in an extensive and automatic way, either bind_front or bind_back.
* Extensive user-control: You control the stack buffer, mutability, heap-allowance, copiability, etc.
* Immutable, stack-only by default. Allow heap with `fn::FlexTask` or by manually selecting the boolean template. Similar for mutable with `fn::TaskMut`, etc.
* Allows taking references, similar to capturing lambda. Simply use std::ref or std::cref on a captured argument, storing a reference instead.
* Referencing tasks: If you know all your arguments are references, you can use `fn::makeRefTask` instead. All l-values given will be stored via references. R-values given are still stored to avoid dangling
* Movable arguments. If mutability is selected, inner callable may move the type-erased arguments in the body, perfect for one-shot Tasks.
* Extensive deduction guides, both directly with `fn::Task{Args&&...}` or via factory functions `fn::makeTask`, `fn::makeTask<Signature>`, `fn::makeRefTask`, `fn::makeMutTask`, `fn::makeRefTask`, etc.


---

## Quick Start

### 1. Integration
Add this to your `CMakeLists.txt` to integrate `fn::Task` directly into your project:

```cmake
include(FetchContent)
FetchContent_Declare(
    task
    GIT_REPOSITORY [https://github.com/Ridrik/fn-task.git](https://github.com/Ridrik/fn-task.git)
    GIT_TAG v1.1.0
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
## Benchmarks

> [!IMPORTANT]
> This section uses limited comparison analysis. Always benchmark on your own platform and within your own architecture.

Basic benchmark analysis makes use of `nanobench`. You may see the implementation under `tests/task_nanobench.cpp`.

Platform: Windows-10 64 bit   
CPU: AMD Ryzen 7 4800H   
Compiler/Toolchain: MinGW (UCRT64)   
Optimization level: -O3 -march=native   

### 1. Fetch and run

The following benchmark compares `fn::Task` variations, with different captured arguments, against `std::function`. The simulation consists on fetching (copying) a given callable and invoking it. It could serve as a basic comparison to a consumer module, where a given thread might fetch a task/function to invoke it.


Results:

| relative |         ns/uint64_t |          uint64_t/s |    err% |     total | Simulating running functions (consumer thread)
|---------:|--------------------:|--------------------:|--------:|----------:|:-----------------------------------------------
|   100.0% |               67.55 |       14,803,682.81 |    2.7% |     84.89 | `std::function that captures std::shared_ptr, node pointer and string view`
|   479.0% |               14.10 |       70,913,758.76 |    0.6% |     17.29 | `Task - Base fn::Task64 from Base fn::Task64 that captures std::shared_ptr, node pointer and string view`
|   481.6% |               14.03 |       71,296,216.76 |    2.0% |     16.97 | `Task - Base fn::Task64 from Base fn::Task64 that captures std::shared_ptr, node pointer and string view but inside a lambda (e.g copies the lambda with these captures)`       
|   104.7% |               64.53 |       15,496,022.38 |    1.7% |     77.99 | `Task - fn::FlexTask16 from fn::FlexTask16 that captures std::shared_ptr, node pointer and string view`
|   104.6% |               64.61 |       15,477,598.83 |    0.9% |     78.50 | `Task - fn::FlexTask16 from fn::FlexTask16 that captures std::shared_ptr, node pointer and string view but inside a lambda (e.g copies the lambda with these captures)`
|   468.7% |               14.41 |       69,385,855.91 |    0.4% |     17.43 | `Task - Extended Base fn::Task64 from Extended Base fn::Task64 that captures std::shared_ptr, node pointer and string view`
|   110.1% |               61.37 |       16,295,232.75 |    1.6% |     74.62 | `std::function that captures std::shared_ptr`
|   565.3% |               11.95 |       83,681,802.66 |    1.0% |     14.53 | `Task - Base fn::Task32 from Base fn::Task32 that captures std::shared_ptr`
|   115.1% |               58.69 |       17,038,872.37 |    0.4% |     71.72 | `Task - fn::FlexTask16 from fn::FlexTask16 that captures std::shared_ptr`
|   627.7% |               10.76 |       92,929,056.06 |    0.2% |     13.08 | `std::function with trivial captures`
| 2,483.9% |                2.72 |      367,714,421.01 |    0.7% |      3.31 | `Task - Base Task16 from fn::TrivialTask16 with trivial captures`
| 1,002.4% |                6.74 |      148,398,985.69 |    1.1% |      8.25 | `Task - Large 256 bytes stack (unneeded), from fn::TrivialTask16 with trivial captures`
| 3,846.9% |                1.76 |      569,482,728.92 |    1.6% |      2.18 | `Task - fn::TrivialTask16 from fn::TrivialTask16 with trivial captures`
| 2,492.7% |                2.71 |      369,017,396.39 |    0.6% |      3.30 | `Task - fn::FlexTask16 from fn::TrivialTask16 with trivial captures`
| 1,295.8% |                5.21 |      191,828,541.97 |    0.9% |      6.34 | `Task - fn::FlexTask16 from fn::FlexTask16 with trivial captures`
| 4,551.6% |                1.48 |      673,805,763.68 |    0.8% |      1.80 | `Static fn::Trivial Task (no buffer) - fetching static function and running it (no captures)`
| 3,004.7% |                2.25 |      444,805,515.05 |    0.7% |      2.73 | `fn::Task32 - fetching static function and running it (no captures)`
| 1,118.4% |                6.04 |      165,565,285.30 |    0.3% |      7.33 | `std::function - fetching static function and running it (no captures)`

Main observation: Tasks that inline storage, without need of heap, have a substantial performance increase over similar Erased types that allocate on the heap, where this allocation is the main overhead of both types.
When `std::function` and `fn::FlexTask` are both used with similar inline storage, and where both allocate on the heap with similar arguments, results indicate comparable performance between both. `std::function` invocable checks for nullptr before the call, whereas `fn::Task` does not do that on `operator()`, but rather on explicit `::invoke` method.

### 2. Invocation of `fn::Task`, `fn::NamedTask` and `std::function`

A basic benchmark is done on trivial arguments. On each, a callable that stores an integer and has a int(int) Signature is created, outputing the sum of the call argument and the bound argument.
Results as follow:

| relative |         ns/uint64_t |          uint64_t/s |    err% |     total | Comparing Named task to task vs task speed
|---------:|--------------------:|--------------------:|--------:|----------:|:-------------------------------------------
|   100.0% |                0.31 |    3,177,874,772.31 |    2.8% |      3.84 | `Named Task`
|    25.3% |                1.24 |      803,429,279.35 |    2.0% |     15.49 | `Task From Named Task`
|    25.5% |                1.23 |      811,259,956.27 |    0.5% |     14.78 | `Task`
|    20.3% |                1.55 |      645,841,880.71 |    3.4% |     18.49 | `Task with nullptr check (like std::function)`
|    20.9% |                1.51 |      662,681,600.46 |    1.0% |     18.04 | `std::function`

One can observe the better performance of `fn::NamedTask` at the nanosecond level, since the pointer access overhead no longer exists. The remaining options all display similar performance, noting that `std::function` does nullptr check by default, whereas `fn::Task` only does it on explicit `::invoke()` method.

---

## ❓ FAQ

### 1. What does it mean for a task to be Trivial?
>A Trivial task is one where all stored arguments are trivially copy/move constructible and trivially destructible (for move-only tasks, copy constructibility is not a requirement). Because of this, no manager pointer is needed, and copying/moving can default to byte-level copy. This makes the task more lightweight, easier to be optimized for transfers, potentially making it much faster for copy/moves relative to std::function and non-trivial Tasks.

### 2. What's the overhead of calling a fn::Task?
>Optimally, it should come down to 1 pointer indirection. Invoking a task inlines to accessing the internal invoker pointer (a raw function pointer), passing the buffer with it. This invoker is what encodes the actual callable given by the user.

### 3. What's the meaning of template bool Extended?
>The Extended option makes it so that the storage consists of 3 pointers (cloner, mover, deleter), as opposed to a single manager pointer that encodes all 3 operations, but with the need of an operation argument to decide which one to execute. It can provide a faster access to copy/move/delete (direct pointer known), though it also means extra pointers need to be copied themselves as well as the size increase (usually 16 bytes). It's meant as a playground for now, use it only if you want to conduct your own experiment. Note that the development of the `Extended` option needs to be completed to allow heap usage.


---

## 🧩 Third-Party Components and Licenses

**fn-task** does not use external libraries for its implementation, but makes use of the following components for testing purposes:

| Component | Purpose | License |
| ---------- | -------- | -------- |
| [**nanobench**](https://github.com/martinus/nanobench) | Microbenchmarking functionality for C++ | MIT |
| [**fault**](https://github.com/Ridrik/fault) | Crash handler, Panic and Assertion library | MIT |

---

## License
`fn::Task` is licensed under the **MIT License** (see `LICENSE` file).
