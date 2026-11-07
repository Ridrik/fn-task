## v1.2.0 - November 2026
### Added
- Added qualifiers on signature, such as 'const', l-value/r-value reference, and noexcept, alongside updated deduction guides when making tasks.
Example:
 ```cpp
    // Gets deducted as void(int) const, since lambdas are usually either const or mutable in
    // qualifications
    auto task = fn::Task{[](int a, double b) {}, 3.0};

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
    auto task2 = fn::makeTask(Wrapper{"Hello World"}, std::size_t{5});
    static_assert(decltype(task2)::matchesSignature<std::string()>());
    std::cout << "Message is " << task2() << '\n';
 ```
- Added reference option for `fn::NamedTask`, behaving similarly to `fn::Task`. See `fn::makeRefNamedTask`.
- Added ability to retrieve bound arguments from `fn::NamedTask`. Fetch it via `fn::NamedTask::getBoundArgs<I>()`.

### Removed
- Removed 'Mutable' template option, given that mutability is implicitly part of the signature with this new update and given the changes above.

## v1.1.3 - March 2026
### Added
- Explicit named task for 'this' pointer as `void*`, via `fn::makeThisTask`. Example:
 ```cpp
 auto task = fn::makeThisTask([](const Payload& payload, void* ptr){
    auto& self = *static_cast<MyModule>(self);
  // Process payload with your module
  }, myModulePtr);
```

## v1.1.2 - March 2026
### Added
- readme example for `fn::NamedTask`
### Fixed
- Fixed issue with static assert on invocation check for `fn::NamedTask`

## v1.1.1 - March 2026
### Fixed
- Fixed an issue related with front invocation, regarding proper forwarding of call arguments within a movable payload, so as to preserve either lvalue or rvalue references.

## v1.1.0 - March 2026
### Added
- `fn::NamedTask`: NamedTask is the opposite of `fn::Task`, being a typed callable. Its type is directly tied to the bound arguments given by the user, being powerful in automatically detecting such construction via its deduction guides. Because of this different nature, the indirection of a type-erased callable is no longer needed, and faster invocation performance may be achieved (see benchmark), at the expense of type-erased flexibility. It can be used, for example, for a common payload Task (as can be the case in pub-sub systems), alongside a `void*` to store typical `this` pointers of each module of yours. since `fn::NamedTask` is a callable itself, it can be naturally transferred to `fn::Task` whenever needed.
- Benchmarks: added initial and limited benchmarks, using `nanobench`, for comparison between `fn::Task`, `fn::NamedTask` and `std::function`, on both pure invocation cases and on "fetch and run" scenario (consumer simulation case).

## v1.0.0 - March 2026
Initial tag release
### Added
- fn::Task. fn::Task is a type-erased callable for c++23+, meant as a flexible alternative to std::function. It supports manual selection of inline stack size, automatic bind_front/bind_back functionality, mutability and heap toggles, as well as move only callables and extensible set of deduction guides.