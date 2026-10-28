# SVF-IR -- SVF Intermediate Representation

SVF-IR is intended to be the sole IR that
[SVF](https://github.com/SVF-tools/SVF) consumes. Having a translation from
other IRs to SVF-IR will thus allow programs in said IRs to be analysed by SVF.
SVF will then have no notion of any input IR except SVF-IR, freeing it up to
focus on analysis. (That's not *exactly* true as we may pass in some metadata,
through SVF-IR's metadata facilities, e.g., vtable information, that 1) SVF-IR
does not understand, and 2) SVF understands.)

Along with the core parser and similar tools and the SVF-IR definition, this
repository will host multiple translators to SVF-IR.

The syntax of the textual format is defined in `doc/grammar.txt`.

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
* `testsvfir`: this executable contains tests for SVFIR (against `libsvfir.a`).
   It is currently quite bare.

SVFIR requires C++17.

## Making changes

* Adding or removing source files requires updating `sources.cmake`.
* Making changes to the definition requires updating `doc/grammar.txt`.

## TODO

* Tests.
* Spec (prose, to go alongside grammar).
* Validation, typing.
* Organise interface (Ops.h, unideal).
