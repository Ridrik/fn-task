## v1.0.0 - March 2026
Initial tag release
### Added
- fn::Task. fn::Task is a type-erased callable for c++23+, meant as a flexible alternative to std::function. It supports manual selection of inline stack size, automatic bind_front/bind_back functionality, mutability and heap toggles, as well as move only callables and extensible set of deduction guides.