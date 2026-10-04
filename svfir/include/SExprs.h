#ifndef SEXPRS_H
#define SEXPRS_H

#include <cassert>
#include <istream>
#include <stack>
#include <string>
#include <variant>
#include <vector>

#include "InputStream.h"
#include "Span.h"

namespace SVFIR {

/// SExpr parser with grammar:
///  atom : '"' ({not "}|'""')* '"'
///       | {not ;, not ", not (, not ), not whitespace}*
///  sexpr : <atom>
///        | '(' <sexpr>* ')'
/// Any whitespace is permitted anywhere outside single-quoted elements.
/// A(n unquoted) ';' indicates a comment. All characters after the start of
/// a comment are ignored till the end of the line.
namespace SExprs {

struct Atom;
struct List;
using SExpr = std::variant<List, Atom>;
using SExprSeq = std::vector<SExpr>;

/// Leaf element of an s-expression.
struct Atom {
    const std::string val;
    const Span span;

    Atom(const std::string val, const Span span) : val(val), span(span) { }
};

/// List element of an s-exprssion, ( ... ).
struct List {
    const SExprSeq children;
    const Span span;

    List(const SExprSeq children, const Span span)
    : children(children), span(span) { }
};

using Err = std::string;
/// Parsing will return either some nice data structure or an error message.
template <typename T>
using ParseResult = std::variant<T, Err>;

/// Parse an S-expression, including allowance for whitespace or comments at
/// the start.
ParseResult<SExpr> parse(InputStream &);

/// Parse a sequence of S-expressions separated by whitespace, comments, or
/// nothing.
ParseResult<SExprSeq> parseSeq(InputStream &);

/// Shortcut to std::holds_alternative<Atom>(...).
bool isAtom(const SExpr &);

/// Shortcut to std::holds_alternative<List>(...).
bool isList(const SExpr &);

/// Returns true when s is an atom and s's value equals x.
bool atomEq(const SExpr &s, const std::string &x);

/// Return the associated span.
Span span(const SExpr &);

/// String of an s-expression. Parsed s-expressions don't have their
/// formatting maintained.
std::string toString(const SExpr &);

}  // namespace SExpr
}  // namespace SVFIR

#endif // SEXPRS_H
