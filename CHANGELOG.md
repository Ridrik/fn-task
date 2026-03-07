## v1.1.0 - March 2026
### Added
- `fn::NamedTask`: NamedTask is the opposite of `fn::Task`, being a typed callable. Its type is directly tied to the bound arguments given by the user, being powerful in automatically detecting such construction via its deduction guides. Because of this different nature, the indirection of a type-erased callable is no longer needed, and faster invocation performance may be achieved (see benchmark), at the expense of type-erased flexibility. It can be used, for example, for a common payload Task (as can be the case in pub-sub systems), alongside a `void*` to store typical `this` pointers of each module of yours. since `fn::NamedTask` is a callable itself, it can be naturally transferred to `fn::Task` whenever needed.
- Benchmarks: added initial and limited benchmarks, using `nanobench`, for comparison between `fn::Task`, `fn::NamedTask` and `std::function`, on both pure invocation cases and on "fetch and run" scenario (consumer simulation case).

## v1.0.0 - March 2026
Initial tag release
### Added
- fn::Task. fn::Task is a type-erased callable for c++23+, meant as a flexible alternative to std::function. It supports manual selection of inline stack size, automatic bind_front/bind_back functionality, mutability and heap toggles, as well as move only callables and extensible set of deduction guides.