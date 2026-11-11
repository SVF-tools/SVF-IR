# Style

* 4-space indentation.
* `camelCase` names, except `GLOBALS_WHICH_ARE_LIKE_THIS`.
* `PascalCase` for file names.
* Camel or Pascal case names with acronyms capitalise the first letter or none.
  * Examples: `Ast.h`, `TypedId`, `cfg`.
* Line length of 80.
* All blocks must have braces.
  * A for-loop or an if-statement on a single line looks like
    `if (foo) { bar; }`
* No exceptions thrown.
* C++17.
* *Very* generous use of `const`.
* *Very* generous use of references, `const` of course.
* Prefer enum classes to enums.
* Generous use of anonymous namespaces in .cpp files.
* Prefer public `const` fields to getters and setters in structs/classes.
* No `auto` except for lambdas and iterators.
* No pointers.
* No concern for lifetimes in the public interface, that is, deal in values.
  * Generally try to avoid needing to think about lifetimes but there may be a
    need for references for performance's sake (see `struct Typer` for example).
* No inheritence, prefer `std::variant`.
  * Inheritence is okay to gather methods but not for dynamic dispatch.
* Never pass mutable references to functions.
  * `std::optional` rather than returning a boolean and modifying a mutable
    object (or returning a pointer that can be NULL; but no pointers anyway).
* Use `insist` instead of `assert`.
* Use `unreachable` to mark unreachable code.
* Switch statements must have a `default` case.
* When switch-ing/if-ing on some variants/enums, the `default` or `else` case
  should be marked `unreachable` or `insist` on the non-explicitly handled cases.
* `libsvfir` is to never take on any external dependencies.
