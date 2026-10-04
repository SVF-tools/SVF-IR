# SVF-IR -- SVF Intermediate Representation

SVF-IR is intended to be the sole IR that
[SVF](https://github.com/SVF-tools/SVF) consumes. Having a translation from
other IRs to SVF-IR will thus allow programs in said IRs to be analysed by SVF.
SVF will then have no notion of any input IR except SVF-IR, freeing it up to
focus on analysis. (That's not *exactly* true as we may pass in some metadata,
e.g., vtable information, that 1) SVF-IR does not understand, and 2) SVF
understands.)

Along with the core parser and similar tools and the SVF-IR definition, this
repository will host multiple translators to SVF-IR.

The syntax of the textual format is defined in `doc/ir.txt`.

## TODO

* Tests.
* Error handling in parser.
* Adding spans to AST nodes.
* Conversion statements.
* Explanation of SVF-IR.
