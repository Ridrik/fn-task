
#include <iostream>
#include <memory>
#include <string>

#include "task/function_traits.hpp"
#include "task/named_task.hpp"
#include "task/task.hpp"

bool foo(std::string /*name*/, std::string& /*data*/, int /*version*/) {
    return false;
}

int main() {
    auto task = fn::NamedTask(fn::toFuncPtr([](int) {}), 2);
    auto str = std::string{"Hello"};
    auto task3 = fn::makeNamedTask<true>([](std::unique_ptr<int>& s) {}, std::make_unique<int>(2));
    task3();
    auto task4{std::move(task3)};

    using fTraits = fn::FunctionTraits<std::decay_t<decltype(task)>>;
    static_assert(fTraits::kValid);
    static_assert(fn::TaskTypeDeductor<decltype(task)>::kIsValid);
    auto t = fn::Task(task);

    static_assert(decltype(task)::matchesSignature<void()>());
}