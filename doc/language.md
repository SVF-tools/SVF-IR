SVFIR [DRAFT]
=====

SVFIR is an intermediate representation for static analysis. It is concerned
only with analysis (thus excluding, for example, optimisation, transformation,
and execution) and largely only those supported by SVF. SVFIR must be
sufficiently expressible to be translated to from a variety of languages but
is intended to be rather lightweight and generally excludes language-specific
features. Where this is too restrictive, metadata, which can be attached to
most language constructs, can fill the gap.

SVFIR features a human-readable s-expression based syntax which is described in
the Grammar section and implicitly referred to throughout this document.
TODO: SVFIR will feature a binary format ...

Some semantics are described as consumer defined (CD). A consumer may interpret
anything CD as desired but this should be documented by the consumer.

This document describes what a valid SVFIR program looks like. This document
first describes some basic building blocks, then explains what a program is,
then explains the major components of a program.

S-expressions
-------------
`<atom>`, `<list>`, `<s-expr>`

All language elements are s-expressions. All s-expressions are either atoms
(`<atom>`) or lists (`<list>`). An atom can contain any character and a list
is a sequence of s-expressions.

Comments
--------

Comments begin with a semicolon and extend to the end of the line.
TODO ...

Types
-----
`<type>`, `<scalar-type>`, `<agg-type>`

All values have a type. This type may either be a scalar type (`<scalar-type>`)
or an aggregate type (`<agg-type>`).

Scalar types are:

* Integers. Two's complement integers. TODO: unclear on widths
* Floating point numbers. IEEE 754 floats of width 8, 16, 32, 64, and 128.
* Pointers. All pointers are opaque.
* Void. Exclusively used to indicate a function returning nothing. There are
  no values of type void.

Aggregate types refer to cardinality-known ordered sequence of elements of
unspecified scalar types (consequently, all aggregate types are "flattened").
Each element is said to fill a "slot".

Constants
---------
`<const>`, `<scalar-const>`, `<seq-const>`, `<typed-const>`

Constants for all the aforementioned types except void are available:

* Integers. Positive or negative integers.
* Floating point numbers. Positive or negative numbers with a decimal point
  and optionally a positive or negative exponent, negative or positive
  infinity, or NaN.
* Pointers. Null. Pointers cannot be constructed from integer constants (see
  convert statement).
* Sequences. An ordered sequence of scalar constants and variables. Each
  constant or variable corresponds to a slot. Sequences are of aggregate types.

In most places in the language, constants must be annotated with a type
(`<typed-const>`). The type and the raw constant must match.

Identifiers
-----------
`<lid>`, `<gid>`, `<bid>`, `<var-id>`

Identifiers refer to local variables (`<lid>`), global variables (`<gid>`),
or basic blocks (`<bid>`). These identifiers are atoms starting with '%', '@',
or '!' respectively. The term variable refers to local and global variables.
All variables are in SSA form.

Values
------
`<val>`

The term value refers to variable or a (typed) constant.

Metadata
--------
`<md>`

Metadata (`<md>`) may be optionally attached to most language constructs. It
has no enforced schema and takes the shape of an s-expression. It is CD how
metadata is to be interpreted; to accept, reject, ignore, understand, etc.

Programs
--------
`<program>`

An SVFIR program is made up of a preamble (`<preamble>`) which includes
mandatory metadata about the program, an unordered list of global variables
(`<variables>`), an unordered list of functions (`<functions>`), and optionally
extra metadata.

Preamble
--------
`<preamble>`

The preamble is an ordered list specifying:

* Version. Of SVFIR. SVFIR versions have a major and a minor component.
  TODO: this is vague, versioning has not been really specified yet.
* Source. An atom specifying how this SVFIR program came to be, for example,
  the language it was translated from. It is CD how this is understood. This
  is useful for naming a metadata schema and defining CD semantics.

Global variables
----------------
`<gvariable>`, `<variables>`

Global variables are pointers whose location is initialised with a value. This
may be explicitly unspecified in the program itself in which case the global
variable is referred to as opaque. Within the program, the global variable's
memory location can be initialised with a constant or another global variable
(i.e., a pointer to another global variable). Global variables are in scope
for the entirety of the program.

The location of a global variable and whether it is read-only or read-write is
CD.

Global variables may optionally have metadata attached.

Functions
---------
`<function>`, `<functions>`, `<param>`

Functions are pointers whose location is initialised with a function body.

The signature of a function is a combination of its return type, the types of
its parameters (in order), and whether it is a variadic function. Ordinary
parameters are ordered and each parameter is a typed local identifier which
is in scope for the entirety of the function. A function may have no ordinary
parameters. Ordinary parameters may optionally have metadata attached. The
final element of the parameter list may be an ellipsis indicating that this
function is a variadic function. Variadic functions have, after the ordinary
parameters, an unspecified number of parameters of unspecified types, that is,
callers may pass corresponding arguments after those corresponding to the
ordinary parameters.

Every function has a body. This body may be unspecified in the program itself,
in which case the function is referred to as an opaque function. Nothing
about the function body of an opaque function is known. Otherwise, the body
is made up of an ordered list of basic blocks. This list must contain at
least one element.

Functions are in scope for the entirety of the program. Local variables and
basic blocks are scoped for the entirety of their function.

The location of a function and whether it is read-only or read-write is CD.

Functions may optionally have metadata attached.

Basic blocks
------------
`<basic-block>`

Basic blocks do not exist except in the context of functions thus this
description happens in the context of the function body.

Basic blocks are named, ordered lists of statements which are used to form the
body of a function. The statements in a basic block are executed consecutively
with nothing in between. Basic blocks must have at least one statement.
The final statement must be a terminator statement (br, brif, or ret) and all
other statements must not be terminator statements.

The first basic block of a function body is known as the entry basic block. It
is executed immediately on function entry and cannot have any predecessors.

Names of basic blocks in a function body must be unique.

Basic blocks may optionally have metadata attached.

Statements
----------
`<stmt>`

Statements do not exist except in the context of basic blocks (and transitively
only in the context of functions) thus this description happens in that
context.

Statements may define variables (all but br, brif, and ret) and may use
variables (all but br and blackhole). Being in SSA form, local variables may
only be defined once. Any statement using a local variable must be dominated
by the statement defining it except in phi statements, the details of which
are described below. Any statement may use any defined basic block from within
the same function, global variable, or function.

All statements may optionally have metadata attached.

The statements are:

* Phi statements, phi.

  `'(' 'phi' <lid:r> '(' ('(' <val:v> <bid:b> ')')+ ')' <md>? ')'`

  Selects a value into introduced variable r. The value is selected from a
  list of value-block pair arguments where the block part of the argument
  corresponds to the predecessor block which lead to the execution of the
  block containing this phi statement. Phi statements must have exactly
  one argument for each predecessor block to the block it belongs to. All
  values in the argument list must be of the same type and this is the type
  that r then takes. Given an argument with value v and predecessor block b,
  the block defining v must be b or dominate b.

* Call statements, call.

  `'(' 'call' ('void'|<typed-lid>:r) <pval:fp> '(' <val:a>* ')' <md>? ')'`

  Calls the function at fp with arguments (as) passing execution to the entry
  block of the function at fn. void must and can only be specified when fn
  does not return a value. Otherwise, once/if fn returns, the introduced
  variable in r takes the returned value. There must exist a function with
  a signature suitable for the return type (specified in r or void) and the
  types of the provided arguments. The semantics when the function at fp
  does not match the return and argument types is CD.

* Branch statements, br.

  `'(' 'br' <bid:b> <md>? ')'`

  Unconditionally jump execution to basic block b. The block this statement
  belongs to is considered a predecessor block to b.

* Conditional branch statements, brif.

  `'(' 'brif' <val:c> <bid:b1> <bid:b2> <md>? ')'`

  Conditionally move execution to another basic block. If the condition value
  c is non-zero, jump to basic block b1, otherwise to basic block b2. The
  condition value c must be of an integer type. The block this statement
  belongs to is considered to be a predecessor block to both b1 and b2.

* Return statements, ret.

  `'(' 'ret' (<val>|'void') <md>? ')'`

  Return execution to the statement after the call which entered the function
  this statement belongs to. If said function returns void, it is only valid
  to specify void as the "value", otherwise the value's type must match the
  return type of the function.

* Comparison statements, cmp.

  `'(' 'cmp' <cmp-op:op> <lid:r> <val:v1> <val:v2> <md>? ')'`

  Compares v1 and v2 according to op with introduced variable r taking on
  the value 1 if the statement is true and 0 otherwise. v1 and v2 must have
  the same type and r is an 8-bit integer. The following comparisons are
  valid given the type of the operands and all others are invalid:

  * Integer. =/!=/(un/signed </<=/>/>=). That is, for less/greater than
    comparison, the signed-ness of the operands must be specified.
  * Float. =/!=/</<=/>/>=.
  * Pointer. =/!=.

* Allocate statements, alloc.

  `'(' 'alloc' ('heap'|'stack') <lid:x> <md>? ')'`

  Allocates a heap or stack object, as specified, assigning introduced variable
  r a pointer to it.

* Store statements, store.

  `'(' 'store' <val:v> <pval:p> <md>? ')'`

  Stores the value v to the location pointed-to by pointer p. Storing to null
  is CD.

* Load statements, load.

  `'(' 'load' <typed-lid:r> <pval:p> <md>? ')'`

  Loads the object pointed-to by pointer p to the introduced variable in r.
  The type of r must be specified and this type is what the object is
  interpreted to be. Loading from null is CD.

* Field statements, field.

  `'(' 'field' <lid:r> <pval:p> <ival:idx> <md>? ')'`

  TODO


* Bitcast statements, bitcast.

  `'(' 'bitcast' <lid:r> <val:v> <type:t> <md>? ')'`

  Reinterprets a value v to be of type t into introduced variable r. The
  type of v and t must be of the same bit width. The type of v and t must
  both be types with a known width.

* Convert statements, convert.

  `'(' 'convert' <lid:r> <val:v> (<type:t>|<ext-type:t'>) <md>? ')'`

  Converts a value v to a value of type t or that in t' into introduced
  variable r. The possible conversions for v to t/the type in t' are:

  * Any integer type → pointer.
  * pointer → any integer type.
  * Any integer type → any float type.
  * Any float type → any integer type.
  * Any float type → any float type.
  * Any integer type → narrower integer type. Truncates.
  * Any integer type → wider integer type. Zero-extends or sign-extends
    according to the extension type.
  * Any type → the same type. Acts as a copy.
  When converting from an integer type to a wider integer type, the extension
  type must be specified (`<ext-type>`). Otherwise, a sole type must be
  specified.


* Add statements, add.

  `'(' 'add' <lid:r> <val:v1> <val:v2> <md>? ')'`

  Adds v1 and v2 into introduced variable r. v1 and v2 must be identical
  integer or float types. r is of this type. When v1 and v2 are integers,
  overflow can occur.

* Subtract statements, sub.

  `'(' 'sub' <lid:r> <val:v1> <val:v2> <md>? ')'`

  Subtracts v2 from v1 into introduced variable r. v1 and v2 must be identical
  integer or float types. r is of this type. When v1 and v2 are integers,
  overflow can occur.

* Multiply statements, mul.

  `'(' 'mul' <lid:r> <val:v1> <val:v2> <md>? ')'`

  Multiplies v1 and v2 into introduced variable r. v1 and v2 must be identical
  integer or float types. r is of this type. When v1 and v2 are integers,
  overflow may occur.

* Divide statements, div.

  `'(' 'div' ('unsigned'|'signed')? <lid:r> <val:v1> <val:v2> <md>? ')'`

  Divides v1 by v2 producing the quotient into introduced variable r. v1 and v2
  must be identical integer or float types. r is of this type. If v1 and v2 are
  integers, the quotient is rounded towards zero. If v1 and v2 are integers,
  the signed-ness of the operands must be specified. Division by zero is CD.

* Remainder statements, rem.

  `'(' 'rem' ('unsigned'|'signed')? <lid> <val> <val> <md>? ')'`

  Divides v1 by v2 producing the remainder into introduced variable r. v1 and
  v2 must be identical integer or float types. r is of this type. If v1 and
  v2 are integers, the signed-ness of the operands must be specified. Division
  by zero is CD.

* Not statements, not.

  `'(' 'not' <lid:r> <val:v> <md>? ')'`

  Performs a bitwise-not of value v into introduced variable r. The operand v
  must be of an integer type. r is of this type.

* And statements, and.

  `'(' 'and' <lid:r> <val:v1> <val:v2> <md>? ')'`

  Performs a bitwise-and of values v1 and v2 into introduced variable r.
  Both operands must be of the same integer type. r is of this type.

* Or statements, or.

  `'(' 'or' <lid:r> <val:v1> <val:v2> <md>? ')'`

  Performs a bitwise-or of values v1 and v2 into introduced variable r.
  Both operands must be of the same integer type. r is of this type.

* Exclusive or statements, xor.

  `'(' 'xor' <lid:r> <val:v1> <val:v2> <md>? ')'`

  Performs a bitwise-xor of values v1 and v2 into introduced variable r.
  Both operands must be of the same integer type. r is of this type.

* Left shift statements, shiftl.

  `'(' 'shiftl' <lid:r> <val:v1> <val:v2> <md>? ')'`

  Shifts value v1 left v2 times into introduced variable r. The rightmost
  v2 bits of r are zero. v1 and v2 must be of identical integer types. r is
  of this type.

* Right shift statements, shiftr.

  `'(' 'shiftr' ('logical'|'arithmetic') <lid:r> <val:v1> <val:v2> <md>? ')'`

  Shifts value v1 right v2 times into introduced variable r. The leftmost
  v2 bits of r are zero when specified as logical and one when specified as
  arithmetic. v1 and v2 must be of identical integer types. r is of this type.

* Variadic argument statements, vararg.

  `'(' 'vararg' <typed-lid:r> <ival:i> <md>? ')'`

  Variadic argument statements take the i-th (which may be static or variable)
  variadic argument to the function and assign it to the introduced variable
  in r. Its type must be specified. Variadic argument statements may only
  occur in variadic functions. Behaviour when the index i is greater than
  the actual number of variadic arguments available is CD.

* Blackhole statements, blackhole.

  `'(' 'blackhole' ('pure'|'effectful') (<typed-lid:r>|'void') <md>? ')'`

  Does anything. A blackhole statement may be pure whereby no side effects take
  place, or effectful whereby side effects are unrestricted. The definition
  of side effect is CD. A blackhole statement may produce a value (into
  the introduced variable in r) or not (void). Where a value is produced,
  its type must be specified.

Memory
------

TODO

Grammar
-------
```
<pos-number> : ('1'|'2'|'3'|'4'|'5'|'6'|'7'|'8'|'9')<digit>*
<digit> : '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9'

<atom> : {non-whitespace}* | '"'{non-double-quote or two double-quotes}*'"'
<list> : '(' <s-expr>* ')'
<s-expr> : <list> | <atom>

<name> : TODO
<lid> : '%'<name>
<gid> : '@'<name>
<bid> : '!'<name>
<var-id> : <lid> | <gid>
<typed-lid> : '(' <type> <lid> ')'

<int-type> : 'i'('8'|'16'|'32'|'64')    ; TODO
<float-type> : 'f'('16'|'32'|'64'|'128')
<scalar-type> : 'void' | 'ptr' | <int-type> | <float-type>

<agg-type> : '(' 'agg' ('0'|<pos-number>) ')'

; See commit a6f7e2f4e8d5a0019a215189da5ae47245bb2d70 for type aliases.
<type> : <scalar-type> | <agg-type>

<int-const> : '-'?<digit>+
<flt-const> : <int-const>'.'<digit>+('e''-'?<digit>+)? | '-inf' | '+inf' | 'nan'
<null-const> : 'null'

<seq-const> : '(' 'seq' (<scalar-const>|<var-id>)* ')'

<scalar-const> : <int-const> | <flt-const> | <null-const>
<const> : <scalar-const> | <seq-const>

<typed-const> : '(' <type> <const> <md>? ')'

<md> : '(' 'md' <s-expr> ')'

<val> : <var-id> | <typed-const>
<gval> : <gid> | <typed-const>
<pval> : <var-id> | <null-const>
<ival> : <var-id> | <int-const>

<program> : <preamble> <variables> <functions> <md>?

<preamble> :
  '(' 'preamble'
  '(' 'version' <digit>+'.'<digit>+ ')'
  '(' 'source' <atom> ')'
  ')'

<function> : '(' <gid> <type> <params> ('(' <basic-block>+ ')'|'opaque') <md>? ')'
<functions> : '(' 'functions' <function>* ')'

<gvariable> : '(' <gid> (<gval>|'opaque') <md>? ')'
<variables> : '(' 'variables' <gvariable>* ')'

<param> : '(' <type> <lid> <md>? ')'
<params> : '(' <param>* '...'? ')'

<basic-block> : '(' <bid> '(' <stmt>+ ')' <md>? ')'

<cmp-op> : '<' | '<=' | '>' | '>=' | '=' | '!='
         | '(signed <)' | '(signed <=)' | '(signed >)' | '(signed >=)'
         | '(unsigned <)' | '(unsigned <=)' | '(unsigned >)' | '(unsigned >=)'

<stmt>
  : <control-stmt>
  | <cmp-stmt>
  | <mem-stmt>
  | <conversion-stmt>
  | <arith-stmt>
  | <bit-stmt>

<control-stmt> : '(' 'phi' <lid> '(' ('(' <val> <bid> ')')+ ')' <md>? ')'
               | '(' 'call' ('void'|<typed-lid>) <pval> '(' <val>* ')' <md>? ')'
               | '(' 'br' <bid> <md>? ')'
               | '(' 'brif' <val> <bid> <bid> <md>? ')'
               | '(' 'ret' (<val>|'void') <md>? ')'

<cmp-stmt> : '(' 'cmp' <cmp-op> <lid> <val> <val> <md>? ')'

<mem-stmt> : '(' 'alloc' ('heap'|'stack') <lid> <md>? ')'
           | '(' 'store' <val> <pval> <md>? ')'
           | '(' 'load' <typed-lid> <pval> <md>? ')'
           | '(' 'field' <lid> <pval> <ival> <md>? ')'

<ext-type> : '(' ('zero'|'sign') <type> ')'
<conversion-stmt> : '(' 'bitcast' <lid> <val> <type> <md>? ')'
                  | '(' 'convert' <lid> <val> (<type>|<ext-type>) <md>? ')'

<arith-stmt> : '(' 'add' <lid> <val> <val> <md>? ')'
             | '(' 'sub' <lid> <val> <val> <md>? ')'
             | '(' 'mul' <lid> <val> <val> <md>? ')'
             | '(' 'div' ('unsigned'|'signed')? <lid> <val> <val> <md>? ')'
             | '(' 'rem' ('unsigned'|'signed')? <lid> <val> <val> <md>? ')'

<bit-stmt> : '(' 'not' <lid> <val> <md>? ')'
           | '(' 'and' <lid> <val> <val> <md>? ')'
           | '(' 'or' <lid> <val> <val> <md>? ')'
           | '(' 'xor' <lid> <val> <val> <md>? ')'
           ; TODO: shiftl and shiftr should probably take an <ival> for the
           ;       second argument but that will force a separation of these
           ;       operations from BinOp in the AST. Needs thought.
           | '(' 'shiftl' <lid> <val> <val> <md>? ')'
           | '(' 'shiftr' ('logical'|'arithmetic') <lid> <val> <val> <md>? ')'

<misc-stmt> : '(' 'vararg' <typed-lid> <ival> <md>? ')'
            | '(' 'blackhole' ('pure'|'effectful') (<typed-lid>|'void') <md>? ')'
```
