# SVFIR -- SVF Intermediate Representation

SVFIR is intended to be the sole IR that
[SVF](https://github.com/SVF-tools/SVF) consumes. Having a translation from
other IRs to SVFIR will thus allow programs in said IRs to be analysed by SVF.
SVF will then have no notion of any input IR except SVFIR, freeing it up to
focus on analysis. (That's not *exactly* true as we may pass in some metadata,
through SVFIR's metadata facilities, e.g., vtable information, that 1) SVFIR
does not understand, and 2) SVF understands.)

Along with the core parser and similar tools and the SVFIR definition, this
repository will host multiple translators to SVFIR.

The language is described in `doc/language.txt`.

## Building

Build with CMake. From the top-level:
```
cmake -S. -DCMAKE_BUILD_TYPE=Debug -Bdebug-build
cmake --build debug-build
```

This will produce a debug build in directory `debug-build`. Alternatively,
`CMAKE_BUILD_TYPE` can be set to `Release` for a release build.

The build directory will now contain:
* `libsvfir.a`: this library contains that needed for parsing, type checking,
  and manipulating SVFIR programs in memory.
* `test`: this executable contains tests for SVFIR (against `libsvfir.so`).
   It is currently quite bare.

SVFIR requires C++17.

A PDF version of the specification can be built with Pandoc.
```
pandoc -f markdown -t pdf doc/language.md -o svfir.pdf
```

## Public Table of Contents

In `include/SVFIR`:
* `Parser.h`. Produce an AST from text via `parseString` and `parseFile`,
  or more generically, `parse`.
* `TypedProgram.h`. Type check and produce a symbol table via
  `typeProgram`.
* `Linker.h`. Fills opaque functions/variables from other programs.
* `Pretty.h`. Turn programs back to strings via `pretty`.
* `Ast.h`. Defines all AST nodes.
* `AstHelpers.h`. Conveniences for operating with the AST.
* `Statements.h`. Conveniences for working with statements.
* `ControlFlowGraph.h`. Intra-procedural control flow graphs.
* `DominatorTree.h`. Dominator and post-dominator trees.
* `UnifyReturns.h`. Combine returns in functions with `unifyReturns`.
* `Validation.h`. Type checking and symbol table construction. Unlikely
  to be need directly.
* `Contracts.h`, `Err.h`, `Index.h`, `InputStream.h`, `Result.h`, `Span.h`,
  `SExprs.h`, `Variant.h`. Basic data structures and functions used extensively
  throughout the library.

## Making changes

* Adding or removing source files requires updating `cmake/sources.cmake`.
* Making changes to the definition requires updating `doc/language.txt`.

## Testing

All tests are run from `test/test.cpp`. From there, new tests should be added
to `main` through collecting related tests and running them in a call to
`runTests`. Those tests are simple functions (`TestFunction` in `test.h`)
returning true or false on success or failure respectively.  This is made
more ergonomic by `TEST` in `test.h`. The pattern is to define these tests in a
`.cpp` file and `#include` that directly in `test/test.cpp`.

## TODO

* Tests.
* Reorganise TypedProgram.
  * Should it include graphs? Should it be closed to the outside?
* Actually define versioning.
* Serialised/binary format.
* LLVM->SVFIR translator.
* SVFIR->SVF translator (SVF repo).
