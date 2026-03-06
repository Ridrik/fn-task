
#include "task/task.hpp"

#include <chrono>
#include <functional>
#include <nanobench.h>

#include <fault/fault.hpp>

#include "task/named_task.hpp"

namespace {

constexpr std::string_view kSvk{"Hello World"};

fn::TrivialTask32<std::uint64_t()> giveTask(int i) {
    switch (i) {
        case 0:
            return fn::Task{[](const std::uint64_t& e) -> std::uint64_t { return 1; },
                            std::uint64_t{}};
        case 1:
            return fn::Task{[]() -> std::uint64_t { return 0; }};
        default:
            return fn::Task{[](std::string_view e) -> std::uint64_t { return e.empty() ? 0U : 1U; },
                            kSvk};
    }
}

fn::TrivialTask16<std::uint64_t()> giveStaticTask() {
    static fn::TrivialTask16<std::uint64_t()> f =
        fn::Task{[](const std::uint64_t& e) -> std::uint64_t { return 1; }, std::uint64_t{}};
    return f;
}

std::uint64_t staticTask() {
    return 1;
}

std::uint64_t staticTask2() {
    return 5;
}

fn::FlexTask16<std::uint64_t()> giveStaticFlexTask() {
    static fn::FlexTask16<std::uint64_t()> f =
        fn::Task{[](const std::uint64_t& e) -> std::uint64_t { return 1; }, std::uint64_t{}};
    return f;
}

fn::Task32<std::uint64_t()> giveTaskWithCapture(int i) {
    static std::shared_ptr<int> p{std::make_shared<int>(0)};
    switch (i) {
        case 0:
            return fn::Task{[](const std::shared_ptr<int>& ptr) -> std::uint64_t { return *ptr; },
                            p};
        case 1:
            return fn::Task{
                [](const std::shared_ptr<int>& ptr) -> std::uint64_t { return *ptr + 1; }, p};
        default:
            return fn::Task{
                [](const std::shared_ptr<int>& ptr) -> std::uint64_t { return *ptr + 2; }, p};
    }
}

fn::Task<std::uint64_t(), true, 32, false, false, true> giveTaskWithCaptureExt(int i) {
    static std::shared_ptr<int> p{std::make_shared<int>(0)};
    using R = fn::Task<std::uint64_t(), true, 32, false, false, true>;
    switch (i) {
        case 0:
            return R{[](const std::shared_ptr<int>& ptr) -> std::uint64_t { return *ptr; }, p};
        case 1:
            return R{[](const std::shared_ptr<int>& ptr) -> std::uint64_t { return *ptr + 1; }, p};
        default:
            return R{[](const std::shared_ptr<int>& ptr) -> std::uint64_t { return *ptr + 2; }, p};
    }
}

fn::Task32<std::uint64_t()> giveStaticTaskWithCapture() {
    static std::shared_ptr<int> p{std::make_shared<int>(0)};
    static auto f =
        fn::Task{[](const std::shared_ptr<int>& ptr) -> std::uint64_t { return *ptr + 1; }, p};
    return f;
}

fn::FlexTask16<std::uint64_t()> giveStaticFlexTaskWithCapture() {
    static std::shared_ptr<int> p{std::make_shared<int>(0)};
    static auto f = fn::FlexTask16<std::uint64_t()>{
        [](const std::shared_ptr<int>& ptr) -> std::uint64_t { return *ptr + 1; }, p};
    return f;
}

fn::Task64<std::uint64_t()> giveTaskWithMoreCapture(int i) {
    static std::shared_ptr<int> p{std::make_shared<int>(0)};
    void* node{nullptr};
    std::string_view strView{"hello"};
    switch (i) {
        case 0:
            return fn::Task{[](const std::shared_ptr<int>& ptr, void* n,
                               std::string_view s) -> std::uint64_t { return *ptr; },
                            p, node, strView};
        case 1:
            return fn::Task{[](const std::shared_ptr<int>& ptr, void* n,
                               std::string_view s) -> std::uint64_t { return *ptr + 1; },
                            p, node, strView};
        default:
            return fn::Task{[](const std::shared_ptr<int>& ptr, void* n,
                               std::string_view s) -> std::uint64_t { return *ptr + 2; },
                            p, node, strView};
    }
}

fn::FlexTask<std::uint64_t()> giveFlexTaskWithMoreCapture(int i) {
    static std::shared_ptr<int> p{std::make_shared<int>(0)};
    void* node{nullptr};
    std::string_view strView{"hello"};
    switch (i) {
        case 0:
            return fn::makeFlexTask([](const std::shared_ptr<int>& ptr, void* n,
                                       std::string_view s) -> std::uint64_t { return *ptr; },
                                    p, node, strView);
        case 1:
            return fn::makeFlexTask([](const std::shared_ptr<int>& ptr, void* n,
                                       std::string_view s) -> std::uint64_t { return *ptr + 1; },
                                    p, node, strView);
        default:
            return fn::makeFlexTask([](const std::shared_ptr<int>& ptr, void* n,
                                       std::string_view s) -> std::uint64_t { return *ptr + 2; },
                                    p, node, strView);
    }
}

fn::Task<std::uint64_t(), true, 64, false, false, true> giveTaskWithMoreCaptureExt(int i) {
    static std::shared_ptr<int> p{std::make_shared<int>(0)};
    void* node{nullptr};
    std::string_view strView{"hello"};
    using F = fn::Task<std::uint64_t(), true, 64, false, false, true>;
    switch (i) {
        case 0:
            return F{[](const std::shared_ptr<int>& ptr, void* n,
                        std::string_view s) -> std::uint64_t { return *ptr; },
                     p, node, strView};
        case 1:
            return F{[](const std::shared_ptr<int>& ptr, void* n,
                        std::string_view s) -> std::uint64_t { return *ptr + 1; },
                     p, node, strView};
        default:
            return F{[](const std::shared_ptr<int>& ptr, void* n,
                        std::string_view s) -> std::uint64_t { return *ptr + 2; },
                     p, node, strView};
    }
}

fn::Task64<std::uint64_t()> giveStaticTaskWithMoreCapture() {
    static auto f = fn::makeTask(
        [](const std::shared_ptr<int>& ptr, void* const& n,
           const std::string_view& s) -> std::uint64_t {
            return *ptr + 1 + s.size() + static_cast<std::uint64_t>(n != nullptr);
        },
        std::make_shared<int>(0), static_cast<void*>(nullptr), std::string_view{"hello"});
    return f;
}

fn::Task64<std::uint64_t()> giveStaticTaskWithMoreCaptureAsLambda() {
    static auto f = fn::Task{[p = std::make_shared<int>(0), strView = std::string_view{"hello"},
                              node = static_cast<void*>(nullptr)]() -> std::uint64_t {
        return *p + 1 + strView.size() + static_cast<std::uint64_t>(node != nullptr);
    }};
    return f;
}

fn::FlexTask16<std::uint64_t()> giveStaticFlexTaskWithMoreCapture() {
    static auto f = fn::FlexTask16<std::uint64_t()>{
        [](const std::shared_ptr<int>& ptr, void* n, std::string_view s) -> std::uint64_t {
            return *ptr + 1 + s.size() + static_cast<std::uint64_t>(n != nullptr);
        },
        std::make_shared<int>(0), static_cast<void*>(nullptr), std::string_view{"hello"}};
    return f;
}

fn::FlexTask16<std::uint64_t()> giveStaticFlexTaskWithMoreCaptureAsLambda() {
    static auto f = fn::FlexTask16<std::uint64_t()>{
        [p = std::make_shared<int>(0), strView = std::string_view{"hello"},
         node = static_cast<void*>(nullptr)]() -> std::uint64_t {
            return *p + 1 + strView.size() + static_cast<std::uint64_t>(node != nullptr);
        }};
    return f;
}

auto giveStaticTaskWithMoreCaptureExt() {
    static std::shared_ptr<int> p{std::make_shared<int>(0)};
    void* node{nullptr};
    std::string_view strView{"hello"};
    static auto f = fn::Task<std::uint64_t(), true, 64, false, false, true>{
        [](const std::shared_ptr<int>& ptr, void* n, std::string_view s) -> std::uint64_t {
            return *ptr + 1 + s.size() + static_cast<std::uint64_t>(n != nullptr);
        },
        p, node, strView};
    return f;
}

std::function<std::uint64_t()> giveFunc(int i) {
    switch (i) {
        case 0:
            return [e = std::uint64_t{}]() { return e + 1; };
        case 1:
            return []() -> std::uint64_t { return 0; };
        default:
            return [e = kSvk]() { return e.empty() ? 0U : 1U; };
    }
}

std::function<std::uint64_t()> giveStaticFunc() {
    static std::function<std::uint64_t()> f = [e = std::uint64_t{}]() mutable { return e++; };
    return f;
}

std::function<std::uint64_t()> giveFuncWithCapture(int i) {
    static std::shared_ptr<int> p{std::make_shared<int>(0)};
    switch (i) {
        case 0:
            return [ptr = p]() mutable { return *ptr; };
        case 1:
            return [ptr = p]() mutable { return *ptr + 1; };
        default:
            return [ptr = p]() mutable { return *ptr + 2; };
    }
}

std::function<std::uint64_t()> giveStaticFuncWithCapture() {
    static std::shared_ptr<int> p{std::make_shared<int>(0)};
    static std::function<std::uint64_t()> f = [ptr = p]() mutable { return *ptr + 1; };
    return f;
}

std::function<std::uint64_t()> giveFuncWithMoreCapture(int i) {
    static std::shared_ptr<int> p{std::make_shared<int>(0)};
    void* node{nullptr};
    std::string_view strView{"hello"};
    switch (i) {
        case 0:
            return [ptr = p, strView, node]() mutable { return *ptr; };
        case 1:
            return [ptr = p, strView, node]() mutable { return *ptr + 1; };
        default:
            return [ptr = p, strView, node]() mutable { return *ptr + 2; };
    }
}

std::function<std::uint64_t()> giveStaticFuncWithMoreCapture() {
    static std::function<std::uint64_t()> f = [ptr = std::make_shared<int>(0),
                                               strView = std::string_view{"hello"},
                                               node = static_cast<void*>(nullptr)]() mutable {
        return *ptr + 1 + strView.size() + static_cast<std::uint64_t>(node != nullptr);
    };
    return f;
}

template <typename F, auto StaticFunc>
    requires std::constructible_from<F, std::nullptr_t> && requires(F f) { f = StaticFunc(); }
void doRunFetchRunInstance(std::string_view name, ankerl::nanobench::Bench& b) {
    F f{nullptr};
    b.run(name.data(), [&] {
        ankerl::nanobench::doNotOptimizeAway(f = StaticFunc());
        ankerl::nanobench::doNotOptimizeAway(f());
    });
}

template <typename F, auto StaticFuncTag, auto StaticFuncTag2>
    requires((std::constructible_from<F, std::nullptr_t> &&
              requires(F f) {
                  f = fn::FunctionTag<StaticFuncTag>{};
                  f = fn::FunctionTag<StaticFuncTag2>{};
              }) ||
             requires(F f) {
                 f = StaticFuncTag;
                 f = StaticFuncTag2;
             })
void doRunFetchRunInstanceTag(std::string_view name, ankerl::nanobench::Bench& b) {
    F f{nullptr};
    bool on1st{false};
    b.run(name.data(), [&] {
        if constexpr (requires(F f) {
                          f = fn::FunctionTag<StaticFuncTag>{};
                          f = fn::FunctionTag<StaticFuncTag2>{};
                      }) {
            if (on1st) {
                on1st = false;
                f = F{fn::FunctionTag<StaticFuncTag>{}};
            } else {
                on1st = true;
                f = F{fn::FunctionTag<StaticFuncTag2>{}};
            }
            ankerl::nanobench::doNotOptimizeAway(f());
        } else {
            if (on1st) {
                on1st = false;
                f = StaticFuncTag;
            } else {
                on1st = true;
                f = StaticFuncTag2;
            }
            ankerl::nanobench::doNotOptimizeAway(f());
        }
    });
}

void flexTaskTest() {
    static std::shared_ptr<int> p{std::make_shared<int>(0)};
    static std::shared_ptr<int> p2{std::make_shared<int>(0)};
    fault::expect_at(p.use_count() == 1);
    {
        void* node{nullptr};
        std::string_view strView{"hello"};
        auto f = fn::FlexTask16<std::uint64_t()>{
            [](const std::shared_ptr<int>& ptr, void* n, std::string_view s) -> std::uint64_t {
                return *ptr + 1;
            },
            p, node, strView};
        fault::expect_at(p.use_count() == 2);

        {
            auto f2 = f;
            fault::expect_at(p.use_count() == 3);
            {
                auto f3{std::move(f2)};
                fault::expect_at(p.use_count() == 3);
            }
            fault::expect_at(p.use_count() == 2);
        }
        fault::expect_at(p.use_count() == 2);
        f = {[](const std::shared_ptr<int>& ptr, void* n, std::string_view s) -> std::uint64_t {
                 return *ptr + 1;
             },
             p2, node, strView};
        fault::expect_at(p.use_count() == 1);
        fault::expect_at(p2.use_count() == 2);
    }
    fault::expect_at(p.use_count() == 1);
    fault::expect_at(p2.use_count() == 1);

    int b{};
    std::shared_ptr<int> kekers{std::make_shared<int>(2)};
    fault::expect_at(kekers.use_count() == 1);
    {
        auto task = fn::makeImmutRefTask(
            [](int a, const std::shared_ptr<int>& xd, const std::shared_ptr<int>& xd2) {}, b,
            kekers, std::make_shared<int>(3));
        fault::expect_at(kekers.use_count() == 1);
        task();
        fault::expect_at(kekers.use_count() == 1);
        {
            auto task2 = fn::makeTask([](int a, std::shared_ptr<int>& xd) {}, b, std::ref(kekers));
            fault::expect_at(kekers.use_count() == 1);
        }
        fault::expect_at(kekers.use_count() == 1);

        {
            auto task3 = fn::makeTask([](int a, std::shared_ptr<int>& xd) {}, b, kekers);
            fault::expect_at(kekers.use_count() == 2);
            {
                auto task4 = task3;
                fault::expect_at(kekers.use_count() == 3);
                {
                    auto task5 = std::move(task4);
                    fault::expect_at(kekers.use_count() == 3);
                }
                fault::expect_at(kekers.use_count() == 2);
            }
            fault::expect_at(kekers.use_count() == 2);
        }
        fault::expect_at(kekers.use_count() == 1);

        constexpr auto kSRef = sizeof(int&);
    }
    fault::expect_at(kekers.use_count() == 1);
}

void doBenchFunctionsFetchAndRun() {
    ankerl::nanobench::Bench b;
    srand(time(nullptr));
    b.title("Simulating running functions (consumer thread)")
        .unit("uint64_t")
        .warmup(100)
        .minEpochIterations(100000000)
        .relative(true);
    b.performanceCounters(true);
    doRunFetchRunInstance<std::function<std::uint64_t()>, giveStaticFuncWithMoreCapture>(
        "std::function that captures std::shared_ptr, node pointer and string view", b);
    doRunFetchRunInstance<fn::Task64<std::uint64_t()>, giveStaticTaskWithMoreCapture>(
        "Task - Base fn::Task64 from Base fn::Task64 that captures std::shared_ptr, node pointer "
        "and "
        "string view",
        b);
    doRunFetchRunInstance<fn::Task64<std::uint64_t()>, giveStaticTaskWithMoreCaptureAsLambda>(
        "Task - Base fn::Task64 from Base fn::Task64 that captures std::shared_ptr, node pointer "
        "and "
        "string view but inside a lambda (e.g copies the lambda with these captures)",
        b);
    doRunFetchRunInstance<fn::FlexTask16<std::uint64_t()>, giveStaticFlexTaskWithMoreCapture>(
        "Task - fn::FlexTask16 from fn::FlexTask16 that captures std::shared_ptr, node pointer and "
        "string "
        "view",
        b);
    doRunFetchRunInstance<fn::FlexTask16<std::uint64_t()>,
                          giveStaticFlexTaskWithMoreCaptureAsLambda>(
        "Task - fn::FlexTask16 from fn::FlexTask16 that captures std::shared_ptr, node pointer and "
        "string "
        "view but inside a lambda (e.g copies the lambda with these captures)",
        b);
    doRunFetchRunInstance<decltype(giveStaticTaskWithMoreCaptureExt()),
                          giveStaticTaskWithMoreCaptureExt>(
        "Task - Extended Base fn::Task64 from Extended Base fn::Task64 that captures "
        "std::shared_ptr, node "
        "pointer and string "
        "view",
        b);
    doRunFetchRunInstance<std::function<std::uint64_t()>, giveStaticFuncWithCapture>(
        "std::function that captures std::shared_ptr", b);
    doRunFetchRunInstance<fn::Task32<std::uint64_t()>, giveStaticTaskWithCapture>(
        "Task - Base fn::Task32 from Base fn::Task32 that captures std::shared_ptr", b);
    doRunFetchRunInstance<fn::FlexTask16<std::uint64_t()>, giveStaticFlexTaskWithCapture>(
        "Task - fn::FlexTask16 from fn::FlexTask16 that captures std::shared_ptr", b);
    doRunFetchRunInstance<std::function<std::uint64_t()>, giveStaticFunc>(
        "std::function with trivial captures", b);
    doRunFetchRunInstance<fn::Task16<std::uint64_t()>, giveStaticTask>(
        "Task - Base Task16 from fn::TrivialTask16 with trivial captures", b);
    doRunFetchRunInstance<fn::Task<std::uint64_t(), true, 256, true>, giveStaticTask>(
        "Task - Large 256 bytes stack (unneeded), from fn::TrivialTask16 with trivial captures", b);
    doRunFetchRunInstance<fn::TrivialTask16<std::uint64_t()>, giveStaticTask>(
        "Task - fn::TrivialTask16 from fn::TrivialTask16 with trivial captures", b);
    doRunFetchRunInstance<fn::FlexTask16<std::uint64_t()>, giveStaticTask>(
        "Task - fn::FlexTask16 from fn::TrivialTask16 with trivial captures", b);
    doRunFetchRunInstance<fn::FlexTask16<std::uint64_t()>, giveStaticFlexTask>(
        "Task - fn::FlexTask16 from fn::FlexTask16 with trivial captures", b);
    doRunFetchRunInstanceTag<fn::StaticTrivialTask<std::uint64_t()>, staticTask, staticTask2>(
        "Static fn::Trivial Task (no buffer) - fetching static function and running it (no "
        "captures)",
        b);
    doRunFetchRunInstanceTag<fn::Task32<std::uint64_t()>, staticTask, staticTask2>(
        "fn::Task32 - fetching static function and running it (no captures)", b);
    doRunFetchRunInstanceTag<std::function<std::uint64_t()>, staticTask, staticTask2>(
        "std::function - fetching static function and running it (no captures)", b);
}

constexpr auto kInterval = std::chrono::milliseconds(1);

void doBenchFunctionsJustRun() {
    ankerl::nanobench::Bench b;
    srand(time(nullptr));
    b.title("Simulating invocation alone.")
        .unit("uint64_t")
        .warmup(100)
        .minEpochIterations(5000000000)
        .relative(true);
    b.performanceCounters(true);
    std::function<std::uint64_t()> f1 = giveFuncWithMoreCapture(0);
    std::function<std::uint64_t()> f2 = giveFunc(0);
    fn::Task64<std::uint64_t()> task1 = giveTask(0);
    fn::Task64<std::uint64_t()> task2 = giveTaskWithMoreCapture(0);
    const auto flexTask = giveFlexTaskWithMoreCapture(0);
    fn::TrivialTask64<std::uint64_t()> task3 = giveTask(0);
    fn::Task<std::uint64_t(), true, 32, false, false, true> task4 = giveTaskWithCaptureExt(0);
    fn::Task<std::uint64_t(), true, 64, false, false, true> task5 = giveTaskWithMoreCaptureExt(0);
    b.run("std::function that captures std::shared_ptr, node pointer and string view",
          [&] { ankerl::nanobench::doNotOptimizeAway(f1()); });
    b.run("Task that captures std::shared_ptr, node pointer and string view", [&] {
        static_assert(!decltype(task2)::isTrivial());
        static_assert(!decltype(giveTaskWithMoreCapture(0))::isTrivial());
        ankerl::nanobench::doNotOptimizeAway(task2());
    });
    b.run("FlexTask that captures std::shared_ptr, node pointer and string view",
          [&] { ankerl::nanobench::doNotOptimizeAway(flexTask()); });
    b.run(
        "FlexTask that captures std::shared_ptr, node pointer and string view, where invocation "
        "checks for nullptr",
        [&] { ankerl::nanobench::doNotOptimizeAway(flexTask.invoke()); });
    /*b.run(
        "Task - Extended Base from Base that captures std::shared_ptr, node pointer and string "
        "view",
        [&] {
            static_assert(!decltype(task5)::isTrivial() && decltype(task5)::isExtended());
            static_assert(!decltype(giveTaskWithMoreCaptureExt(0))::isTrivial());
            ankerl::nanobench::doNotOptimizeAway(task5());
        });
    b.run("Task - Extended Base from Base that captures std::shared_ptr", [&] {
        using F = decltype(task4);
        static_assert(!F::isTrivial());
        static_assert(F::isExtended());
        static_assert(!decltype(giveTaskWithCaptureExt(0))::isTrivial());
        ankerl::nanobench::doNotOptimizeAway(task4());
    });*/
    b.run("std::function with trivial captures",
          [&] { ankerl::nanobench::doNotOptimizeAway(f2()); });
    b.run("Task - Base with trivial captures", [&] {
        static_assert(!decltype(task1)::isTrivial());
        static_assert(decltype(giveTask(0))::isTrivial());
        ankerl::nanobench::doNotOptimizeAway(task1());
    });
    b.run("Task - fn::TrivialTask with trivial captures", [&] {
        static_assert(decltype(task3)::isTrivial());
        static_assert(decltype(giveTask(0))::isTrivial());
        ankerl::nanobench::doNotOptimizeAway(task3());
    });
}

void doBenchFunctions() {
    ankerl::nanobench::Bench b;
    srand(time(nullptr));
    b.title("Simulating moving inplace functions")
        .unit("uint64_t")
        .warmup(100)
        .minEpochIterations(20000000)
        .relative(true);
    b.performanceCounters(true);
    std::function<std::uint64_t()> f1;
    std::function<std::uint64_t()> f2;
    fn::Task32<std::uint64_t()> task1;
    fn::Task64<std::uint64_t()> task2;
    fn::TrivialTask64<std::uint64_t()> task3;
    fn::Task<std::uint64_t(), true, 32, false, false, true> task4;
    fn::Task<std::uint64_t(), true, 64, false, false, true> task5;
    b.run("std::function that captures std::shared_ptr, node pointer and string view",
          [&] { ankerl::nanobench::doNotOptimizeAway(f1 = giveFuncWithMoreCapture(rand() % 3)); });
    b.run("Task - Base from Base that captures std::shared_ptr, node pointer and string view", [&] {
        static_assert(!decltype(task2)::isTrivial());
        static_assert(!decltype(giveTaskWithMoreCapture(0))::isTrivial());
        ankerl::nanobench::doNotOptimizeAway(task2 = giveTaskWithMoreCapture(rand() % 3));
    });
    b.run(
        "Task - Extended Base from Base that captures std::shared_ptr, node pointer and string "
        "view",
        [&] {
            static_assert(!decltype(task5)::isTrivial() && decltype(task5)::isExtended());
            static_assert(!decltype(giveTaskWithMoreCaptureExt(0))::isTrivial());
            ankerl::nanobench::doNotOptimizeAway(task5 = giveTaskWithMoreCaptureExt(rand() % 3));
        });
    b.run("std::function that captures std::shared_ptr",
          [&] { ankerl::nanobench::doNotOptimizeAway(f1 = giveFuncWithCapture(rand() % 3)); });
    b.run("Task - Base from Base that captures std::shared_ptr", [&] {
        static_assert(!decltype(task2)::isTrivial());
        static_assert(!decltype(giveTaskWithCapture(0))::isTrivial());
        ankerl::nanobench::doNotOptimizeAway(task2 = giveTaskWithCapture(rand() % 3));
    });
    b.run("Task - Extended Base from Base that captures std::shared_ptr", [&] {
        using F = decltype(task4);
        static_assert(!F::isTrivial());
        static_assert(F::isExtended());
        static_assert(!decltype(giveTaskWithCaptureExt(0))::isTrivial());
        ankerl::nanobench::doNotOptimizeAway(task4 = giveTaskWithCaptureExt(rand() % 3));
    });
    b.run("std::function with trivial captures", [&] { f2 = giveFunc(rand() % 3); });
    b.run("Task - Base from trivial captures", [&] {
        static_assert(!decltype(task1)::isTrivial());
        static_assert(decltype(giveTask(0))::isTrivial());
        ankerl::nanobench::doNotOptimizeAway(task1 = giveTask(rand() % 3));
    });
    b.run("Task - fn::Trivial from trivial captures", [&] {
        static_assert(decltype(task3)::isTrivial());
        static_assert(decltype(giveTask(0))::isTrivial());
        ankerl::nanobench::doNotOptimizeAway(task3 = giveTask(rand() % 3));
    });
}

void doCompareOrder() {
    ankerl::nanobench::Bench b;
    srand(time(0));
    b.title("Simulating moving inplace functions")
        .unit("uint64_t")
        .minEpochIterations(50000000)
        .relative(true);
    b.performanceCounters(true);

    constexpr auto kFunc = [](std::string_view c, int a, bool b) -> bool {
        return static_cast<bool>(a + b);
    };

    const auto f = fn::makeImmutTask<kFunc>(false);
    const auto f2 = fn::makeImmutTask<bool(int, bool), kFunc>(std::string_view{});
    b.run("Normal order", [&] { ankerl::nanobench::doNotOptimizeAway(f({}, 2)); });
    b.run("Reserve Order", [&] { ankerl::nanobench::doNotOptimizeAway(f2(0, false)); });
}

void compareNamedTaskToTask() {
    ankerl::nanobench::Bench b;
    b.title("Comparing Named task to task vs task speed")
        .unit("uint64_t")
        .minEpochIterations(1000000000)
        .relative(true);
    b.performanceCounters(true);

    auto task = fn::Task([](int a, int v) { return a + v; }, 2);
    auto namedTask = fn::NamedTask([](int a, int v) { return a + v; }, 2);
    auto taskFromNamedTask = fn::Task(namedTask);
    auto stdFunc = std::function<int(int)>{[a = 2](int v) { return a + v; }};
    b.run("Named Task", [&] { ankerl::nanobench::doNotOptimizeAway(namedTask(1)); });
    b.run("Task From Named Task",
          [&] { ankerl::nanobench::doNotOptimizeAway(taskFromNamedTask(1)); });
    b.run("Task", [&] { ankerl::nanobench::doNotOptimizeAway(task(1)); });
    b.run("Task with nullptr check (like std::function)",
          [&] { ankerl::nanobench::doNotOptimizeAway(task.invoke(1)); });
    b.run("std::function", [&] { ankerl::nanobench::doNotOptimizeAway(stdFunc(1)); });
}

}  // namespace

int main() {
    fault::init({.crashDir = "tests/crash", .useUnsafeStacktraceOnSignalFallback = true});
    compareNamedTaskToTask();
}