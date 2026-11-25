
# `fn::Task`

### Description

`fn::Task` is a type-erased callable for c++23+, meant as a flexible alternative to std::function, std::move_only_function and std::copyable_function. It supports manual selection of inline stack size, automatic bind_front/bind_back functionality, manual index binding, cv-ref signatures and heap toggles, as well as move only callables and extensible set of deduction guides.

In addition, `fn::NamedTask` is the opposite: a typed callable. Its type is directly tied to the bound arguments that construct it. This allows the indirection of typical erased types to be eliminated, thus providing an even more performant callable, at the expense of losing flexibility for different tasks. Notable, `fn::NamedTask` can be used to hold 'this' pointer of each module using a single `void*` argument, allowing a top performance speed for regular invocations. It can also be naturally transferred to `fn::Task` when needed.

---

## Key Features
* Captures arguments thrown in following the inner callable, in an extensive and automatic way
* Allows constness, referencing (&, &&) as well as `noexcept` qualifications to its signature, which are enforced in the way that the callable and captured arguments are used.
* Extensive user-control: You control the inline buffer, heap-allowance, copiability, and even alignment.
* Allows taking references, via its wrappers `fn::ref` and `fn::cref`, or globally through factory methods.
* A dedicated and extensive factory build system, that deducts the type given the thrown arguments, and allows to easily toggle and select the settings (templates, that become part of the type) for the newly built task


---

## Quick Start

### 1. Integration
Add this to your `CMakeLists.txt` to integrate `fn::Task` directly into your project:

```cmake
include(FetchContent)
FetchContent_Declare(
    task
    GIT_REPOSITORY [https://github.com/Ridrik/fn-task.git](https://github.com/Ridrik/fn-task.git)
    GIT_TAG v1.5.2
)
FetchContent_MakeAvailable(task)

# Link to your application
target_link_libraries(my_app PRIVATE fn::task)
```

Quick usage:

```cpp


#include <iostream>
#include <memory>
#include <string>

#include "task/task.hpp"

bool foo(double val) {
    return val > 0.0;
}

int main() {
    auto task = fn::Task{[](const std::string& str, double val) { return true; }, 5.0};
    auto task2 = fn::Task<bool(const std::string&) const>{
        [](int val, const std::string& str) { return true; }, 2};
    auto taskStatic = fn::makeTask<foo>(5.0);
    taskStatic();
    auto task3 = fn::buildTask([](int val, std::string str, double number) {}, 3, 5.0)
                    .bindTo<0, 2>()
                    .build();
    task3("hey");
}

```

---

## Type 

```cpp
template <typename Signature, Copyability cb, InlineSize size, Triviality t, bool Extended,
          HeapPolicy hp, Alignment align>   
struct Task;
``` 

* **Signature**: signature of the Task, specifying the input arguments, return type, and optionally specifiers, such as const, reference qualifiers and noexcept specifier. cvref/noexcept specifiers are enforced internally at invocation between bound callable and arguments
* **Copyability**: toggle for copyable-enabled Task. False implies it cannot be copied, similarly to `std::move_only_function`. Use `fn::UniqueTask` for equivalent use of a non-copyable `fn::Task`.
* **InlineSize**: Inline size to hold bound data. Given callable and arguments will always be attempted to be stored in this storage. If not possible, `fn::Task` construction fails unless HeapPolicy is set to allow, in which it behaves similarly to `std::function`, `std::copyable_function`, etc...
* **Triviality**: A toggle to make a more lightweight `fn::Task`, allowing for trivial copies of the inline storage between tasks. Requirements for `std::is_trivially_copy_constructible_v`, `std::is_trivially_move_constructible_v` and `std::is_trivially_destructible_v` need to be met for Bound data (for non-copyable types, copy constructible check does not exist). Triviality is never allowed for heap-allowed types.
* **Extended**: an internal boolean option to manage copy/move/destruction in a different way, not to the interest of the user. To be removed eventually.
* **HeapPolicy**: Policy toggle for heap. Disabled by default. Enabling allows holding data beyond the inline size, at the trade-off of heap allocation and further call indirection.
* **Alignment**: size for the alignment of the buffer. it defaults to the alignment of `std::max_align_t`. Higher alignments are supported including at heap allocation.

---

## Full Example


### `fn::Task`

```cpp


#include <iostream>
#include <memory>
#include <string>

#include "task/task.hpp"

bool foo(std::string /*name*/, std::string& /*data*/, int /*version*/) {
    return false;
}

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
}

```


### `Named Task`

```cpp
#include <string>

#include <task/named_task.hpp>
#include <task/task.hpp>

bool foo(std::string /*name*/, std::string& /*data*/, int /*version*/) {
    return false;
}

bool bar(std::string /*name*/, const std::string& /*data*/, int /*version*/) {
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

    const auto& doubleArg = task.getBoundArg<0>();

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

    std::string& strArg = funcWithLiterals.getBoundArg<2>();
    using S = decltype(funcWithLiterals)::Signature;

    auto nTask = fn::Task{funcWithLiterals};
    static_assert(decltype(nTask)::matchesSignature<decltype(funcWithLiterals)::Signature>());
}
```

---

### Signature qualifiers and its relation to the Task object itself, compared with standard type-erased callables

**Note** Signature qualfiers are enforced to the inner captured arguments, but they are not forward to the operator() of the task itself. This is an intentional design choice to decouple Task reassignment from mutability of bound data itself. For instance:

```cpp
const fn::NamedTask<void()>& reftask = task;
refTask();
```

is allowed, even though the signature is non-const. With `std::copyable_function`, such an operation wouldn't be allowed, as the standard forwards qualifiers directly to the wrapper's operator(). The difference allows `fn::Task` to distinguish between side-effects of its bound arguments (given its signature) from reassignment itself, which belongs to the wrapper itself.

**Author's reasoning:**

"In a typical class, both const and non-const methods may exist, and a const object still allows for const methods to be called. Furthermore, typical classes offer all kinds of different possible functionalities (like handlers, subscribers, events, etc...) which need not necessarily public methods. In contrast, a type erased callable's main purpose is to be invoked. Making a const callable of a non-const signature un-invokable severily limits its use in real-world situations. On the other hand, having signature express to users whether internal data may mutate or not is clear enough, allowing for constness of the wrapper itself to be independent and instead made to restrict `fn::Task`/`fn::NamedTask` reassignments."

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

| relative | ns/uint64_t |     uint64_t/s | err% | total | Simulating running functions (consumer thread)                                                                                                                            |
| -------: | ----------: | -------------: | ---: | ----: | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
|   100.0% |       67.55 |  14,803,682.81 | 2.7% | 84.89 | `std::function that captures std::shared_ptr, node pointer and string view`                                                                                               |
|   479.0% |       14.10 |  70,913,758.76 | 0.6% | 17.29 | `Task - Base fn::Task64 from Base fn::Task64 that captures std::shared_ptr, node pointer and string view`                                                                 |
|   481.6% |       14.03 |  71,296,216.76 | 2.0% | 16.97 | `Task - Base fn::Task64 from Base fn::Task64 that captures std::shared_ptr, node pointer and string view but inside a lambda (e.g copies the lambda with these captures)` |
|   104.7% |       64.53 |  15,496,022.38 | 1.7% | 77.99 | `Task - fn::FlexTask16 from fn::FlexTask16 that captures std::shared_ptr, node pointer and string view`                                                                   |
|   104.6% |       64.61 |  15,477,598.83 | 0.9% | 78.50 | `Task - fn::FlexTask16 from fn::FlexTask16 that captures std::shared_ptr, node pointer and string view but inside a lambda (e.g copies the lambda with these captures)`   |
|   468.7% |       14.41 |  69,385,855.91 | 0.4% | 17.43 | `Task - Extended Base fn::Task64 from Extended Base fn::Task64 that captures std::shared_ptr, node pointer and string view`                                               |
|   110.1% |       61.37 |  16,295,232.75 | 1.6% | 74.62 | `std::function that captures std::shared_ptr`                                                                                                                             |
|   565.3% |       11.95 |  83,681,802.66 | 1.0% | 14.53 | `Task - Base fn::Task32 from Base fn::Task32 that captures std::shared_ptr`                                                                                               |
|   115.1% |       58.69 |  17,038,872.37 | 0.4% | 71.72 | `Task - fn::FlexTask16 from fn::FlexTask16 that captures std::shared_ptr`                                                                                                 |
|   627.7% |       10.76 |  92,929,056.06 | 0.2% | 13.08 | `std::function with trivial captures`                                                                                                                                     |
| 2,483.9% |        2.72 | 367,714,421.01 | 0.7% |  3.31 | `Task - Base Task16 from fn::TrivialTask16 with trivial captures`                                                                                                         |
| 1,002.4% |        6.74 | 148,398,985.69 | 1.1% |  8.25 | `Task - Large 256 bytes stack (unneeded), from fn::TrivialTask16 with trivial captures`                                                                                   |
| 3,846.9% |        1.76 | 569,482,728.92 | 1.6% |  2.18 | `Task - fn::TrivialTask16 from fn::TrivialTask16 with trivial captures`                                                                                                   |
| 2,492.7% |        2.71 | 369,017,396.39 | 0.6% |  3.30 | `Task - fn::FlexTask16 from fn::TrivialTask16 with trivial captures`                                                                                                      |
| 1,295.8% |        5.21 | 191,828,541.97 | 0.9% |  6.34 | `Task - fn::FlexTask16 from fn::FlexTask16 with trivial captures`                                                                                                         |
| 4,551.6% |        1.48 | 673,805,763.68 | 0.8% |  1.80 | `Static fn::Trivial Task (no buffer) - fetching static function and running it (no captures)`                                                                             |
| 3,004.7% |        2.25 | 444,805,515.05 | 0.7% |  2.73 | `fn::Task32 - fetching static function and running it (no captures)`                                                                                                      |
| 1,118.4% |        6.04 | 165,565,285.30 | 0.3% |  7.33 | `std::function - fetching static function and running it (no captures)`                                                                                                   |

Main observation: Tasks that inline storage, without need of heap, have a substantial performance increase over similar Erased types that allocate on the heap, where this allocation is the main overhead of both types.
When `std::function` and `fn::FlexTask` are both used with similar inline storage, and where both allocate on the heap with similar arguments, results indicate comparable performance between both. `std::function` invocable checks for nullptr before the call, whereas `fn::Task` does not do that on `operator()`, but rather on explicit `::invoke` method.

### 2. Invocation of `fn::Task`, `fn::NamedTask` and `std::function`

A basic benchmark is done on trivial arguments. On each, a callable that stores an integer and has a int(int) Signature is created, outputing the sum of the call argument and the bound argument.
Results as follow:

| relative | ns/uint64_t |       uint64_t/s | err% | total | Comparing Named task to task vs task speed     |
| -------: | ----------: | ---------------: | ---: | ----: | :--------------------------------------------- |
|   100.0% |        0.31 | 3,177,874,772.31 | 2.8% |  3.84 | `Named Task`                                   |
|    25.3% |        1.24 |   803,429,279.35 | 2.0% | 15.49 | `Task From Named Task`                         |
|    25.5% |        1.23 |   811,259,956.27 | 0.5% | 14.78 | `Task`                                         |
|    20.3% |        1.55 |   645,841,880.71 | 3.4% | 18.49 | `Task with nullptr check (like std::function)` |
|    20.9% |        1.51 |   662,681,600.46 | 1.0% | 18.04 | `std::function`                                |

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

| Component                                              | Purpose                                    | License |
| ------------------------------------------------------ | ------------------------------------------ | ------- |
| [**nanobench**](https://github.com/martinus/nanobench) | Microbenchmarking functionality for C++    | MIT     |
| [**fault**](https://github.com/Ridrik/fault)           | Crash handler, Panic and Assertion library | MIT     |

---

## License
`fn::Task` is licensed under the **MIT License** (see `LICENSE` file).
