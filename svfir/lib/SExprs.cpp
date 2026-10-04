#include <iostream>
#include "InputStream.h"
#include "SExprs.h"

namespace {

using namespace SVFIR;
using namespace SVFIR::SExprs;

const char _COMMENT = ';';
const char _QUOTE = '"';
const char _LPAREN = '(';
const char _RPAREN = ')';
const char _NEWLINE = '\n';

/// Does stream point to '""'.
bool _atDoubleQuote(InputStream &stream) {
    const std::vector<char> next2 = stream.peekn(2);
    return next2.size() == 2 && next2[0] == _QUOTE && next2[1] == _QUOTE;
}

/// Is c a special character or whitespace?
bool _isDelimiter(const char c) {
    return (
        c == _COMMENT ||
        c == _QUOTE ||
        c == _LPAREN ||
        c == _RPAREN ||
        std::isspace(c)
    );
}

/// Make stream point to the next line after a comment. stream must point
/// to the start of a comment.
void _skipComment(InputStream &stream) {
    assert(stream.peek() == _COMMENT);
    while (!stream.atEof() && stream.peek() != _NEWLINE) {
        stream.advance(1);
    }

    // Jump to next line.
    stream.advance(1);
}

/// Make stream point to the next non-whitespace. stream must point to
/// whitespace.
void _skipWhitespace(InputStream &stream) {
    assert(std::isspace(stream.peek()));
    while (std::isspace(stream.peek())) {
        stream.advance(1);
    }
}

/// Skips whitespace and comments stream may be pointing at.
/// Returns whether anything was skipped.
bool _skipNonSignificant(InputStream &stream) {
    const Loc originalLoc = stream.cursor();

    while (
        !stream.atEof() &&
        (stream.peek() == _COMMENT || std::isspace(stream.peek()))
    ) {
        if (stream.peek() == _COMMENT) { _skipComment(stream); }
        else { _skipWhitespace(stream); }
    }

    return stream.cursor() != originalLoc;
}

/// Parse an Atom. stream must point to the start of the atom.
/// Very inefficient.
ParseResult<SExpr> _parseAtom(InputStream &stream) {
    assert(!_isDelimiter(stream.peek()));

    Loc start = stream.cursor();

    std::string atom = "";
    char c = stream.peek();
    if (c == _QUOTE) {
        stream.advance(1);
        char c = stream.peek();
        while (!stream.atEof() || (c == _QUOTE && !_atDoubleQuote(stream))) {
            if (_atDoubleQuote(stream)) {
                atom += _QUOTE;
                stream.advance(2);
            } else {
                atom += c;
                stream.advance(1);
            }

            c = stream.peek();
        }

        // We haven't consumed the closing quote so EOF indicates an error.
        if (stream.atEof()) {
            return ParseResult<SExpr>(Err("unclosed quote"));
        }

        // Consume closing quote.
        stream.advance(1);
    } else {
        char c = stream.peek();
        while (!stream.atEof() && !_isDelimiter(c)) {
            atom += c;
            stream.advance(1);
            c = stream.peek();
        }
    }

    return ParseResult<SExpr>(Atom(atom, Span(start, stream.cursor())));
}

/// Parse a List. stream must point to the start of the list.
ParseResult<SExpr> _parseList(InputStream &stream) {
    assert(stream.peek() == _LPAREN);

    SExprSeq children = SExprSeq();
    Loc start = stream.cursor();

    stream.advance(1);
    char c = stream.peek();
    while (!stream.atEof() && c != _RPAREN) {
        if (_skipNonSignificant(stream)) { }
        else {
            ParseResult<SExpr> res = SVFIR::SExprs::parse(stream);
            if (std::holds_alternative<SExpr>(res)) {
                children.push_back(std::get<SExpr>(res));
            } else {
                assert(std::holds_alternative<Err>(res));
                return res;
            }
        }
        c = stream.peek();
    }

    if (stream.peek() != _RPAREN) {
        return ParseResult<SExpr>(Err("unclosed list"));
    }

    stream.advance(1);
    return ParseResult<SExpr>(SExpr(List(children, Span(start, stream.cursor()))));
}

}  // Anonymous namespace

namespace SVFIR {
namespace SExprs {

ParseResult<SExpr> parse(InputStream &stream) {
    while (!stream.atEof()) {
        const char c = stream.peek();
        if (_skipNonSignificant(stream)) { }
        else if (c == _LPAREN) { return _parseList(stream); }
        else if (c == _RPAREN) {
            return ParseResult<SExpr>(Err("unexpected ')'"));
        } else { return _parseAtom(stream); }
        // The above calls will appropriately advance.
    }

    return ParseResult<SExpr>(Err("no sexpr given"));
}

/// Parse a stream purported to be a sequence of consecutive s-expressions.
ParseResult<SExprSeq> parseSeq(InputStream &stream) {
    SExprSeq seq;
    while (!stream.atEof()) {
        if (_skipNonSignificant(stream)) { }
        else {
            ParseResult<SExpr> res = parse(stream);
            if (std::holds_alternative<SExpr>(res)) {
                seq.push_back(std::get<SExpr>(res));
            } else {
                assert(std::holds_alternative<Err>(res));
                return ParseResult<SExprSeq>(std::get<Err>(res));
            }
        }
    }

    return seq;
}

bool isAtom(const SExpr &sexpr) {
    return std::holds_alternative<Atom>(sexpr);
}

bool isList(const SExpr &sexpr) {
    return std::holds_alternative<List>(sexpr);
}

bool atomEq(const SExpr &s, const std::string &x) {
    return isAtom(s) && std::get<Atom>(s).val == x;
}

Span span(const SExpr &sexpr) {
    if (isList(sexpr)) {
        return std::get<List>(sexpr).span;
    } else {
        assert(isAtom(sexpr));
        return std::get<Atom>(sexpr).span;
    }
}

std::string toString(const SExpr &sexpr) {
    std::string out;

    if (isAtom(sexpr)) {
        const Atom atom = std::get<Atom>(sexpr);
        if (atom.val.find(_QUOTE) != std::string::npos) {
            out.reserve(out.size() + atom.val.size() + 2);
            out.push_back(_QUOTE);
            for (const char c : atom.val) {
                out.push_back(c);
                // Re-introduce double (escaped) quotes.
                if (c == _QUOTE) { out.push_back(_QUOTE); }
            }
            out.push_back(_QUOTE);
        } else {
            out.append(atom.val);
        }
    } else {
        assert(isList(sexpr));
        const List list = std::get<List>(sexpr);
        out.push_back(_LPAREN);
        for (size_t i = 0; i < list.children.size(); ++i) {
            if (i != 0) { out.append(" "); }
            out.append(toString(list.children[i]));
        }
        out.push_back(_RPAREN);
    }

    return out;
}

}  // namespace SExprs
}  // namespace SVFIR
