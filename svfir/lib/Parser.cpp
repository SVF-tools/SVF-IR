#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <sstream>
#include <fstream>
#include <stdint.h>
#include <string>

#include "Abort.h"
#include "Ast.h"
#include "Err.h"
#include "Parser.h"
#include "Result.h"
#include "Span.h"

namespace {

using namespace SVFIR;
using namespace SVFIR::SExprs;

// TODO: Things like "if you intended an x here, note: err" can have err
// referring to a similar message, no good.

/// So we can use with std::all_of. It gets confused using std::isdigit
/// directly due to overloads.
bool isDigit(const char c) { return std::isdigit(c); }

/// Error parsing expected because we need, and don't have, a list.
Err notAList(const std::string &expected, const Span &span) {
    return Err("Expected a list when trying to parse " + expected + ".", span);
}

/// Error parsing expected because we need, and don't have, an atom.
Err notAnAtom(const std::string &expected, const Span &span) {
    return Err("Expected a atom when trying to parse " + expected + ".", span);
}

/// Error parsing encompassing as the list is too short trying for an expected.
Err listCutShort(
    const std::string &encompassing,
    const std::string &expected,
    const Span &span
) {
    return Err(
        "Expected " + expected + " " +
        "while parsing " + encompassing + " " +
        "(list too short).",
        span
    );
}

/// Error parsing l because there are too many elements.
Err listTooLong(const std::string &l, const Span &span) {
    return Err("Too many elements while parsing " + l, span);
}

/// Error parsing encompassing as we expected keyword kw but got actual.
Err badKw(
    const std::string &encompassing,
    const std::string &kw,
    const std::string &actual,
    const Span &span
) {
    return Err(
        "Expected " + kw + " " +
        "(got " + actual + ") " +
        "while parsing " + encompassing + ".",
        span
    );
}

/// That IDs need two characters. kind should be capitalised.
Err shortId(const std::string &kind, const Span &span) {
    return Err(kind + " IDs must be two or more characters", span);
}

/// That IDs need to start with a special character. kind should be capitalised.
Err noSpecialCharId(
    const std::string &kind,
    const std::string &ch,
    const Span &span
) {
    return Err(kind + " IDs must begin with a " + ch + ".", span);
}

/// <md>
Result<Metadata, Err> parseMetadata(const SExpr &s) {
    if (!isList(s)) { return notAList("metadata", span(s)); }
    const List &list = std::get<List>(s);

    auto it = list.children.cbegin(), end = list.children.cend();
    if (it == end) { return listCutShort("metadata", "'md'", span(s)); }
    if (!isAtom(*it)) { return notAnAtom("'md'", span(*it)); }
    const std::string &kw = std::get<Atom>(*it).val;
    if (kw != "md") { return badKw("metadata", "'md'", kw, span(*it)); }

    ++it;
    if (it == end) { return listCutShort("metadata", "sexpr data", span(s)); }
    if (it + 1 != end) { return listTooLong("metadata", span(s)); }
    return Metadata(*it, span(*it));
}

/// <tid>
Result<Id, Err> parseTypeId(const SExpr &s) {
    if (!isAtom(s)) { return notAnAtom("type ID", span(s)); }
    const std::string &text = std::get<Atom>(s).val;
    if (text.size() < 2) { return shortId("Type", span(s)); }
    if (text[0] != '~') { return noSpecialCharId("Type", "~", span(s)); }
    return Id(text, span(s));
}

/// <type>
Result<DecType, Err> parseType(const SExpr &s) {
    const Span &sp = span(s);
    if (isAtom(s)) {
        const Atom &atom = std::get<Atom>(s);
        if (atom.val == "void") { return DecType(VoidDecType(sp)); }
        else if (atom.val == "ptr") { return DecType(PtrDecType(sp)); }
        else if (atom.val.size() > 1 && atom.val[0] == 'i') {
            using Kind = IntDecType::Kind;
            // + 1 to skip the 'i'.
            const int width = std::atoi(atom.val.c_str() + 1);
            if (width == 8) { return DecType(IntDecType(Kind::I8, sp)); }
            else if (width == 16) { return DecType(IntDecType(Kind::I16, sp)); }
            else if (width == 32) { return DecType(IntDecType(Kind::I32, sp)); }
            else if (width == 64) { return DecType(IntDecType(Kind::I64, sp)); }
            else if (width == 128) {
                return DecType(IntDecType(Kind::I128, sp));
            } else {
                return Err(
                    "Bad width for int type (" + std::to_string(width) + ").",
                    sp
                );
            }
        } else if (!atom.val.empty() && atom.val[0] == 'f') {
            using Kind = FloatDecType::Kind;
            // + 1 to skip the 'f'.
            const int width = std::atoi(atom.val.c_str() + 1);
            if (width == 16) { return DecType(FloatDecType(Kind::F16, sp)); }
            else if (width == 32) {
                return DecType(FloatDecType(Kind::F32, sp));
            } else if (width == 64) {
                return DecType(FloatDecType(Kind::F64, sp));
            } else if (width == 128) {
                return DecType(FloatDecType(Kind::F128, sp));
            } else {
                return Err(
                    "Bad width for float type (" + std::to_string(width) + ").",
                    sp
                );
            }
        } else if (!atom.val.empty() && atom.val[0] == '~') {
            const Result<Id, Err> id = parseTypeId(s);
            if (isErr(id)) {
                return Err(
                    "Invalid scalar type.\n"
                    "If you intended a type ID here, note:\n  " +
                    getErr(id).toString(),
                    span(s)
                );
            }
            return DecType(getVal(id));
        } else { return Err("Invalid scalar type.", sp); }
    } else {
        abortif(isList(s));
        const List &list = std::get<List>(s);
        const Span &sp = span(s);

        auto it = list.children.cbegin(), end = list.children.cend();
        if (it == end) { return listCutShort("aggregate type", "'agg'", sp); }
        if (!isAtom(*it)) { return notAnAtom("'agg'", span(*it)); }
        const std::string &kw = std::get<Atom>(*it).val;
        if (kw != "agg") { badKw("aggregate type", "'agg'", kw, span(*it)); }


        ++it;
        if (it == end) {
            return listCutShort("aggregate type", "number of slots", sp);
        }
        if (!isAtom(*it)) { return notAnAtom("number of slots", span(*it)); }
        const std::string &slotsStr = std::get<Atom>(*it).val;
        if (!std::all_of(slotsStr.begin(), slotsStr.end(), isDigit)) {
            // TODO: what if it starts with 0
            return Err(
                "aggregate type's number of slots is not positive number "
                "(" + slotsStr + ").",
                span(*it)
            );
        }
        errno = 0;
        const long long slots = std::strtoull(slotsStr.c_str(), nullptr, 10);
        if (errno != 0 || slots > UINT64_MAX) {
            return Err(
                "Too many slots (" + slotsStr + ") for aggregate type.",
                span(*it)
            );
        }

        if (it + 1 != end) { return listTooLong("aggregate type", sp); }

        return DecType(AggDecType(slots, sp));
    }
}

/// <lid>
Result<Id, Err> parseLocalId(const SExpr &s) {
    if (!isAtom(s)) { return notAnAtom("local ID", span(s)); }
    const std::string &text = std::get<Atom>(s).val;
    if (text.size() < 2) { return shortId("Local", span(s)); }
    if (text[0] != '%') { return noSpecialCharId("Local", "%", span(s)); }
    return Id(text, span(s));
}

/// <gid>
Result<Id, Err> parseGlobalId(const SExpr &s) {
    if (!isAtom(s)) { return notAnAtom("global ID", span(s)); }
    const std::string &text = std::get<Atom>(s).val;
    if (text.size() < 2) { return shortId("Global", span(s)); }
    if (text[0] != '@') { return noSpecialCharId("Global", "@", span(s)); }
    return Id(text, span(s));
}

/// <bid>
Result<Id, Err> parseBlockId(const SExpr &s) {
    if (!isAtom(s)) { return notAnAtom("basic block ID", span(s)); }
    const std::string &text = std::get<Atom>(s).val;
    if (text.size() < 2) { return shortId("Basic block", span(s)); }
    if (text[0] != '!') { return noSpecialCharId("Basic block", "!", span(s)); }
    return Id(text, span(s));
}

/// <var-id>
Result<Id, Err> parseVarId(const SExpr &s) {
    // Error check here so we know parseLocalId/parseGlobalId will pass.
    if (!isAtom(s)) { return notAnAtom("local/global (var) ID", span(s)); }
    const std::string &text = std::get<Atom>(s).val;
    if (text.size() < 2) { return shortId("Local/global (var)", span(s)); }
    if (text[0] == '%') { return getVal(parseLocalId(s)); }
    else if (text[0] == '@') { return getVal(parseGlobalId(s)); }
    else { return noSpecialCharId("Local/global (var)", "%/@", span(s)); }
}

/// <typed-id>
Result<TypedId, Err> parseTypedId(const SExpr &s) {
    if (!isList(s)) { return notAList("typed (local) ID", span(s)); }
    const Span &sp = span(s);

    const List &list = std::get<List>(s);
    auto it = list.children.cbegin(), end = list.children.cend();

    if (it == end) { return listCutShort("typed (local) ID", "type", sp); }
    const Result<DecType, Err> type = parseType(*it);
    if (isErr(type)) { return getErr(type); }

    ++it;
    if (it == end) { return listCutShort("typed (local) ID", "ID", sp); }
    const Result<Id, Err> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    if (it + 1 != end) { return listTooLong("typed (local) ID", sp); }

    return TypedId(getVal(lid), getVal(type), span(s));
}

/// <param>
Result<Param, Err> parseParam(const SExpr &s) {
    if (!isList(s)) { return notAList("Param", span(s)); }
    const Span &sp = span(s);

    const List &list = std::get<List>(s);
    auto it = list.children.cbegin(), end = list.children.cend();

    if (it == end) { return listCutShort("parameter", "type", sp); }
    const Result<DecType, Err> type = parseType(*it);
    if (isErr(type)) { return getErr(type); }

    ++it;
    if (it == end) { return listCutShort("parameter", "ID", sp); }
    const Result<Id, Err> id = parseLocalId(*it);
    if (isErr(id)) { return getErr(id); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
        if (it + 1 != end) { return listTooLong("aggregate type", sp); }
    }

    return Param(getVal(id), getVal(type), md, span(s));
}

Result<TypedConstant, Err> parseTypedConst(const SExpr &s);
/// <const>
Result<Constant, Err> parseConst(const SExpr &s) {
    const Span &sp = span(s);
    if (isAtom(s)) {
        const std::string &val = std::get<Atom>(s).val;
        if (val.empty()) {
            return Err("Expected constant, got empty atom", sp);
        }
        else if (val == "null") { return Constant(NullConstant(sp)); }
        else if (val == "-inf" || val == "+inf" || val == "nan") {
            return Constant(FloatConstant(val, sp));
        } else {  // Try for an int or numeric float.
            // But first, a small-effort check if the user *may* have intended
            // a numeric constant.
            if (val[0] != '-' && !isDigit(val[0])) {
                return Err("Invalid constant, got '" + val + "'.", sp);
            }

            const auto numStart =
                val[0] == '-' ? val.cbegin() + 1 : val.cbegin();
            if (std::all_of(numStart, val.cend(), isDigit)) {
                return Constant(IntConstant(val, sp));
            }

            const auto dotPos = std::find(numStart, val.cend(), '.');
            if (dotPos == val.cend()) {
                return Err(
                    "Invalid constant, got '" + val + "'. "
                    "If you intended a float constant, a '.' is required.",
                    sp
                );
            }
            if (dotPos + 1 == val.cend()) {
                return Err(
                    "Invalid constant, got '" + val + "'. "
                    "If you intended a float constant, "
                    "at least one digit is required after the '.'.",
                    sp
                );
            }

            auto it = dotPos + 1;
            for (; it != val.cend(); ++it) { }
            if (it != val.cend()) {
                if (*it != 'e') {
                    return Err(
                        "Invalid constant, got '" + val + "'. "
                        "If you intended a float constant, "
                        "you may have intended the exponent signifier 'e'.",
                        sp
                    );
                }
                ++it;
                if (*it == '-') { ++it; }
                if (!std::all_of(it, val.cend(), isDigit)) {
                    return Err(
                        "Invalid constant, got '" + val + "'. "
                        "If you intended a float constant, "
                        "the exponent must be a positive or negative integer.",
                        sp
                    );
                }
            }

            return Constant(FloatConstant(val, sp));
        }
    } else {
        abortif(isList(s));
        SExprSeq elems = std::get<List>(s).children;
        auto it = elems.cbegin(), end = elems.cend();
        if (it == end) { return listCutShort("sequence", "'seq'", span(s)); }

        if (!isAtom(*it)) { return notAnAtom("'seq'", span(*it)); }
        const std::string &kw = std::get<Atom>(*it).val;
        if (kw != "seq") {
            return badKw("sequence (constant)", "'seq'", kw, span(*it));
        }

        ++it;
        std::vector<Val> vals;
        for (; it != end; ++it) {
            const Result<TypedConstant, Err> tc = parseTypedConst(*it);
            if (isErr(tc)) {
                const Result<Id, Err> vi = parseVarId(*it);
                if (isErr(vi)) {
                    return Err(
                        "Elements of a sequence must be a typed constant "
                        "or a local/global (var) ID, if you intended the "
                        "latter, note:\n  " + getErr(vi).toString(),
                        span(*it)
                    );
                } else { vals.push_back(getVal(vi)); }
            } else { vals.push_back(getVal(tc)); }
        }

        return Constant(SeqConstant(vals, sp));
    }
    unreachable();
}

/// <typed-const>
Result<TypedConstant, Err> parseTypedConst(const SExpr &s) {
    if (!isList(s)) { return notAList("typed constant", span(s)); }

    const List &list = std::get<List>(s);
    auto it = list.children.cbegin(), end = list.children.cend();
    if (it == end) {
        return listCutShort("typed constant", "constant", span(s));
    }

    const Result<DecType, Err> type = parseType(*it);
    if (isErr(type)) { return getErr(type); }

    ++it;
    const Result<Constant, Err> constant = parseConst(*it);
    if (isErr(constant)) { return getErr(constant); }

    if (it + 1 != end) { return listTooLong("typed constant", span(s)); }

    return TypedConstant(getVal(constant), getVal(type), span(s));
}

/// Version in preamble.
Result<Version, Err> parseVersion(const SExpr &s) {
    if (!isList(s)) { return notAList("version", span(s)); }
    const List &list = std::get<List>(s);
    const Span &sp = span(s);

    auto it = list.children.cbegin(), end = list.children.cend();
    if (it == end) { return listCutShort("version", "'version'", sp); }
    if (!isAtom(*it)) { return notAnAtom("'version'", span(*it)); }
    const std::string &kw = std::get<Atom>(*it).val;
    if (kw != "version") {
        return badKw("version", "'version'", kw, span(*it));
    }

    ++it;
    if (it == end) { return listCutShort("version", "version number", sp); }
    if (!isAtom(*it)) { return notAnAtom("version number", span(*it)); }
    const std::string &versionStr = std::get<Atom>(*it).val;
    const Span &vsp = span(*it);

    const size_t dot = versionStr.find(".", 0);
    if (dot == std::string::npos) {
        return Err("Version number missing '.'.", vsp);
    } else if (dot == 0) {
        return Err("Version number missing major version (before '.').", vsp);
    } else if (dot == versionStr.size() - 1) {
        return Err("Version number missing minor version (after '.').", vsp);
    }

    const std::string &majorStr = versionStr.substr(0, dot);
    if (!std::all_of(majorStr.cbegin(), majorStr.cend(), isDigit)) {
        return Err("Major version contains non-digit.", vsp);
    }
    const long majorVersion = std::strtoul(majorStr.c_str(), nullptr, 10);
    if (majorVersion > UINT16_MAX) {
        return Err(
            "Major version too large. "
            "(Max: " + std::to_string(UINT16_MAX) + ".)",
            vsp
        );
    }

    const std::string &minorStr = versionStr.substr(0, dot);
    if (!std::all_of(minorStr.cbegin(), minorStr.cend(), isDigit)) {
        return Err("Minor version contains non-digit.", vsp);
    }
    const long minorVersion = std::strtoul(minorStr.c_str(), nullptr, 10);
    if (minorVersion > UINT16_MAX) {
        return Err(
            "Minor version too large. "
            "(Max: " + std::to_string(UINT16_MAX) + ".)",
            vsp
        );
    }

    if (it + 1 != end) { return listTooLong("version", span(s)); }

    return Version(majorVersion, minorVersion);
}

/// <preamble>
Result<Preamble, Err> parsePreamble(const SExpr &s) {
    if (!isList(s)) { return notAList("preamble", span(s)); }
    const Span &sp = span(s);

    const List &list = std::get<List>(s);
    auto it = list.children.cbegin(), end = list.children.cend();

    // Preamble keyword.
    if (it == end) { return listCutShort("preamble", "'preamble'", sp); }
    if (!isAtom(*it)) { return notAnAtom("'preamble'", span(*it)); }
    const std::string &kw = std::get<Atom>(*it).val;
    if (kw != "preamble") {
        return badKw("preamble", "'preamble'", kw, span(*it));
    }

    // Version.
    ++it;
    if (it == end) { return listCutShort("preamble", "version", sp); }
    const Result<Version, Err> version = parseVersion(*it);
    if (isErr(version)) { return getErr(version); }

    // Source.
    ++it;
    if (it == end) { return listCutShort("preamble", "source", sp); }
    if (!isList(*it)) { return notAList("source", span(s)); }

    const List &sourceList = std::get<List>(*it);
    const Span &ssp = span(sourceList);
    auto sit = sourceList.children.cbegin(), send = sourceList.children.cend();
    if (sit == send) { return listCutShort("source", "'source'", ssp); }
    if (!isAtom(*sit)) { return notAnAtom("'source'", span(*sit)); }
    const std::string &skw = std::get<Atom>(*sit).val;
    if (skw != "source") { return badKw("source", "'source'", kw, span(*sit)); }

    ++sit;
    if (sit == send) {
        return listCutShort("source", "source descriptor", ssp);
    }
    if (!isAtom(*sit)) { return notAnAtom("source descriptor", span(*sit)); }
    const std::string &source = std::get<Atom>(*sit).val;

    if (it + 1 != end) { return listTooLong("preamble", sp); }

    return Preamble(getVal(version), source, span(s));
}

/// <gval>
Result<Val, Err> parseGVal(const SExpr &s) {
    static const std::string &expectation =
        "Expected a typed constant or a global ID.";
    if (isAtom(s)) {
        // Try for <gid>.
        const Result<Id, Err> gid = parseGlobalId(s);
        if (!isErr(gid)) { return Val(getVal(gid)); }
        else {
            return Err(
                expectation + " "
                "If you intended a global ID here, note:\n  " +
                getErr(gid).toString(),
                span(s)
            );
        }
    } else {
        // Try for <typed-const>
        abortif(isList(s));
        const Result<TypedConstant, Err> tc = parseTypedConst(s);
        if (!isErr(tc)) { return Val(getVal(tc)); }
        else {
            return Err(
                expectation + " "
                "If you intended a typed constant here, note:\n  " +
                getErr(tc).toString(),
                span(s)
            );
        }
    }
    unreachable();
}

/// <val>
Result<Val, Err> parseVal(const SExpr &s) {
    static const std::string &expectation =
        "Expected a typed constant or a local/global (var) ID.";
    if (isAtom(s)) {
        const Result<Id, Err> vid = parseVarId(s);
        if (!isErr(vid)) { return Val(getVal(vid)); }
        else {
            return Err(
                expectation + " "
                "If you intended a local/global (var) ID here, note:\n  " +
                getErr(vid).toString(),
                span(s)
            );
        }
    } else {
        abortif(isList(s));
        const Result<TypedConstant, Err> tc = parseTypedConst(s);
        if (!isErr(tc)) { return Val(getVal(tc)); }
        else {
            return Err(
                expectation + " "
                "If you intended a typed constant here, note:\n  " +
                getErr(tc).toString(),
                span(s)
            );
        }
    }
    unreachable();
}

/// <pval>
Result<PVal, Err> parsePVal(const SExpr &s) {
    if (!isAtom(s)) {
        return notAnAtom("Global/local (var) ID or null constant.", span(s));
    }

    const Result<Constant, Err> c = parseConst(s);
    if (!isErr(c) && std::holds_alternative<NullConstant>(getVal(c))) {
        return PVal(std::get<NullConstant>(getVal(c)));
    }

    const Result<Id, Err> vid = parseVarId(s);
    if (!isErr(vid)) { return PVal(getVal(vid)); }
    else {
        return Err(
            "Expected a null constant or a local/global (var) ID. "
            "If you intended a local/global (var) ID here, note:\n  " +
            getErr(vid).toString(),
            span(s)
        );
    }
    unreachable();
}

/// <ival>
Result<IVal, Err> parseIVal(const SExpr &s) {
    if (!isAtom(s)) {
        return notAnAtom("Global/local (var) ID or integer constant.", span(s));
    }

    const Result<Constant, Err> c = parseConst(s);
    if (!isErr(c) && std::holds_alternative<IntConstant>(getVal(c))) {
        return IVal(std::get<IntConstant>(getVal(c)));
    }

    const Result<Id, Err> vid = parseVarId(s);
    if (!isErr(vid)) { return IVal(getVal(vid)); }
    else {
        return Err(
            "Expected an integer constant or a local/global (var) ID. "
            "If you intended a local/global (var) ID here, note:\n  " +
            getErr(vid).toString(),
            span(s)
        );
    }
    unreachable();
}

/// <variable>
Result<Variable, Err> parseVariable(const SExpr &s) {
    if (!isList(s)) { return notAList("variable", span(s)); }
    const Span &sp = span(s);

    const List &list = std::get<List>(s);
    auto it = list.children.cbegin(), end = list.children.cend();

    if (it == end) { return listCutShort("variable", "variable name", sp); }
    const Result<Id, Err> gid = parseGlobalId(*it);
    if (isErr(gid)) { return getErr(gid); }

    ++it;
    if (it == end) { return listCutShort("variable", "value/'opaque'", sp); }
    std::optional<Val> gval;
    if (atomEq(*it, "opaque")) { gval = std::nullopt; }
    else {
        const Result<Val, Err> gvalr = parseGVal(*it);
        if (isErr(gvalr)) {
            return Err(
                "Expected global ID, typed constant, or 'opaque' as "
                "variable value. If you intended a value here, note:\n  " +
                getErr(gvalr).toString(),
                span(*it)
            );
        } else { gval.emplace(getVal(gvalr)); }
    }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("variable", sp); }
    }

    return Variable(getVal(gid), gval, md, span(s));
}

/// <variables>
Result<std::vector<Variable>, Err> parseVariables(const SExpr &s) {
    if (!isList(s)) { return notAList("variables", span(s)); }
    const List &list = std::get<List>(s);
    auto it = list.children.cbegin(), end = list.children.cend();

    if (it == end) { return listCutShort("variables", "'variables'", span(s)); }
    if (!isAtom(*it)) { return notAnAtom("'variables'", span(*it)); }
    const std::string &kw = std::get<Atom>(*it).val;
    if (kw != "variables") {
        return badKw("variables", "'variables'", kw, span(*it));
    }

    ++it;
    std::vector<Variable> variables;
    for (; it != end; ++it) {
        const Result<Variable, Err> variable = parseVariable(*it);
        if (isErr(variable)) { return getErr(variable); }
        variables.push_back(getVal(variable));
    }

    return variables;
}

/// To be called on (phi ...) only.
Result<Statement, Err> parsePhiStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "phi"));

    ++it;
    if (it == end) {
        return listCutShort("phi statement", "local ID", span(l));
    }
    const Result<Id, Err> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (!isList(*it)) { return notAList("phi operands", span(*it)); }
    std::vector<PhiStmt::Operand> operands;
    for (auto s : std::get<List>(*it).children) {
        if (!isList(s)) { return notAList("phi operand", span(l)); }
        const List &pair = std::get<List>(s);

        if (pair.children.size() != 2) {
            return Err(
                "Phi operands must be value/block ID pairs. This operand has " +
                std::to_string(pair.children.size()) + " elements.",
                span(pair)
            );
        }

        const Result<Val, Err> val = parseVal(pair.children[0]);
        if (isErr(val)) { return getErr(val); }

        const Result<Id, Err> bid = parseBlockId(pair.children[1]);
        if (isErr(bid)) { return getErr(bid); }

        operands.push_back(PhiStmt::Operand(getVal(val), getVal(bid)));
    }
    if (operands.empty()) {
        return Err("Phi statements must have at least one operand.", span(l));
    }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("phi statement", span(l)); }
    }

    return Statement(PhiStmt(getVal(lid), operands, md, span(l)));
}

/// To be called on (call ...) only.
Result<Statement, Err> parseCallStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "call"));
    const Span &sp = span(l);

    ++it;
    if (it == end) {
        return listCutShort("call statement", "local ID or 'void'", sp);
    }
    if (!isAtom(*it)) { return notAnAtom("local ID or 'void'", span(*it)); }
    std::optional<Id> lid = std::nullopt;
    if (!atomEq(*it, "void")) {
        // Try a local ID.
        const Result<Id, Err> lidr = parseLocalId(*it);
        if (isErr(lidr)) {
            return Err(
                "Expected a local ID or 'void'. "
                "If you intended a local ID here, note:\n  " +
                getErr(lidr).toString(),
                span(*it)
            );
        }
        lid.emplace(getVal(lidr));
    }

    ++it;
    if (it == end) { return listCutShort("call statement", "callee", sp); }
    const Result<PVal, Err> callee = parsePVal(*it);
    if (isErr(callee)) { return getErr(callee); }

    ++it;
    if (it == end) { return listCutShort("call statement", "arguments", sp); }
    if (!isList(*it)) { return notAList("call arguments", span(*it)); }
    std::vector<Val> args;
    for (const auto a : std::get<List>(*it).children) {
        const Result<Val, Err> val = parseVal(a);
        if (isErr(val)) { return getErr(val); }
        args.push_back(getVal(val));
    }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("call statement", sp); }
    }

    return Statement(CallStmt(lid, getVal(callee), args, md, span(l)));
}

/// To be called on (br ...) only.
Result<Statement, Err> parseBrStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "br"));

    ++it;
    if (it == end) {
        return listCutShort("branch statement", "target ID", span(l));
    }
    const Result<Id, Err> target = parseBlockId(*it);
    if (isErr(target)) { return getErr(target); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("br statement", span(l)); }
    }

    return Statement(BrStmt(getVal(target), md, span(l)));
}

/// To be called on (brif ...) only.
Result<Statement, Err> parseBrifStmt(const List &l) {
    static const std::string &brifDesc = "conditional branch statement";

    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "brif"));
    const Span &sp = span(l);

    ++it;
    if (it == end) { return listCutShort(brifDesc, "condition value", sp); }
    const Result<Val, Err> val = parseVal(*it);
    if (isErr(val)) { return getErr(val); }

    ++it;
    if (it == end) { return listCutShort(brifDesc, "if target ID", sp); }
    const Result<Id, Err> ifTarget = parseBlockId(*it);
    if (isErr(ifTarget)) { return getErr(ifTarget); }

    ++it;
    if (it == end) { return listCutShort(brifDesc, "else target ID", sp); }
    const Result<Id, Err> elseTarget = parseBlockId(*it);
    if (isErr(elseTarget)) { return getErr(elseTarget); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) {
            return listTooLong("conditional branch statement", sp);
        }
    }

    return Statement(
        BrifStmt(getVal(val), getVal(ifTarget), getVal(elseTarget), md, span(l))
    );
}

/// To be called on (ret ...) only.
Result<Statement, Err> parseRet(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "ret"));

    ++it;
    if (it == end) {
        return listCutShort("return statement", "value or 'void'", span(l));
    }
    std::optional<Val> val;
    if (atomEq(*it, "void")) { val = std::nullopt; }
    else {
        const Result<Val, Err> valr = parseVal(*it);
        if (isErr(valr)) {
            return Err(
                "Expected a value or 'void'. "
                "If you intended a value here, note:\n  " +
                getErr(valr).toString(),
                span(*it)
            );
        } else { val.emplace(getVal(valr)); }
    }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("return statement", span(l)); }
    }

    return RetStmt(val, md, span(l));
}

/// To be called on (cmp ...) only.
Result<Statement, Err> parseCmpStmt(const List &l) {
    static const std::string &cmpDesc = "comparison statement";

    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "cmp"));

    ++it;
    CmpStmt::Operator op;
    if (!isAtom(*it)) { return notAnAtom("comparison operator", span(*it)); }
    const std::string &kw = std::get<Atom>(*it).val;
    if (kw == "<") { op = CmpStmt::Operator::LT; }
    else if (kw == "<=") { op = CmpStmt::Operator::LE; }
    else if (kw == ">") { op = CmpStmt::Operator::GT; }
    else if (kw == ">=") { op = CmpStmt::Operator::GE; }
    else if (kw == "=") { op = CmpStmt::Operator::EQ; }
    else if (kw == "!=") { op = CmpStmt::Operator::NEQ; }
    else {
        return badKw(
            "comparison statement (operator)",
            "'<', '<=', '>', '>=', '=', or '!='",
            kw,
            span(*it)
        );
    }

    ++it;
    if (it == end) { return listCutShort(cmpDesc, "local ID", span(l)); }
    const Result<Id, Err> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) { return listCutShort(cmpDesc, "left operand", span(l)); }
    const Result<Val, Err> left = parseVal(*it);
    if (isErr(left)) { return getErr(left); }

    ++it;
    if (it == end) { return listCutShort(cmpDesc, "right operand", span(l)); }
    const Result<Val, Err> right = parseVal(*it);
    if (isErr(right)) { return getErr(right); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) {
            return listTooLong("comparison statement", span(l));
        }
    }

    return Statement(
        CmpStmt(getVal(lid), op, getVal(left), getVal(right), md, span(l))
    );
}

/// To be called on (alloc ...) only.
Result<Statement, Err> parseAllocStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "alloc"));
    const Span &sp = span(l);

    ++it;
    if (it == end) { return listCutShort("allocation statement", "kind", sp); }
    if (!isAtom(*it)) { return notAnAtom("'heap' or 'stack'", span(*it)); }
    const std::string &kw = std::get<Atom>(*it).val;
    AllocStmt::Kind kind;
    if (kw == "heap") { kind = AllocStmt::Kind::HEAP; }
    else if (kw == "stack") { kind = AllocStmt::Kind::STACK; }
    else {
        return badKw(
            "allocation statement (kind)",
            "'heap' or 'stack'",
            kw,
            span(*it)
        );
    }

    ++it;
    if (it == end) {
        return listCutShort("allocation statement", "local ID", sp);
    }
    const Result<Id, Err> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("allocation statement", sp); }
    }

    return Statement(AllocStmt(kind, getVal(lid), md, span(l)));
}

/// To be called on (store ...) only.
Result<Statement, Err> parseStoreStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "store"));

    ++it;
    if (it == end) { return listCutShort("store statement", "value", span(l)); }
    const Result<Val, Err> val = parseVal(*it);
    if (isErr(val)) { return getErr(val); }

    ++it;
    if (it == end) {
        return listCutShort("store statement", "destination pointer", span(l));
    }
    const Result<PVal, Err> dst = parsePVal(*it);
    if (isErr(dst)) { return getErr(dst); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("store statement", span(l)); }
    }

    return Statement(StoreStmt(getVal(val), getVal(dst), md, span(l)));
}

/// To be called on (load ...) only.
Result<Statement, Err> parseLoadStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "load"));

    ++it;
    if (it == end) {
        return listCutShort("load statement", "(typed) local ID", span(l));
    }
    const Result<TypedId, Err> tlid = parseTypedId(*it);
    if (isErr(tlid)) { return getErr(tlid); }

    ++it;
    if (it == end) {
        return listCutShort("load statement", "source pointer", span(l));
    }
    const Result<PVal, Err> src = parsePVal(*it);
    if (isErr(src)) { return getErr(src); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("load statement", span(l)); }
    }

    return Statement(LoadStmt(getVal(tlid), getVal(src), md, span(l)));
}

/// To be called on (field ...) only.
Result<Statement, Err> parseFieldStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "field"));
    const Span &sp = span(l);

    ++it;
    if (it == end) { return listCutShort("field statement", "local ID", sp); }
    const Result<Id, Err> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) {
        return listCutShort("field statement", "source pointer", sp);
    }
    const Result<PVal, Err> src = parsePVal(*it);
    if (isErr(src)) { return getErr(src); }

    ++it;
    if (it == end) {
        return listCutShort("field statement", "field index", sp);
    }
    const Result<IVal, Err> index = parseIVal(*it);
    if (isErr(index)) { return getErr(index); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("field statement", sp); }
    }

    return Statement(
        FieldStmt(getVal(lid), getVal(src), getVal(index), md, span(l))
    );
}

/// To be called on (bitcast/convert ...) only.
Result<Statement, Err> parseBitCastConvertStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && (atomEq(*it, "BitCast") || atomEq(*it, "convert")));

    const std::string &kind = std::get<Atom>(*it).val;
    const std::string &stmtDesc = kind + " statement";

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "local ID", span(l)); }
    const Result<Id, Err> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "value", span(l)); }
    const Result<Val, Err> val = parseVal(*it);
    if (isErr(val)) { return getErr(val); }

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "type", span(l)); }
    const Result<DecType, Err> type = parseType(*it);
    if (isErr(type)) { return getErr(type); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong(stmtDesc, span(l)); }
    }

    if (kind == "bitcast") {
        return Statement(BitCastStmt(
            getVal(lid), getVal(val), getVal(type), md, span(l)
        ));
    } else if (kind == "convert") {
        return Statement(
            ConvertStmt(getVal(lid), getVal(val), getVal(type), md, span(l))
        );
    }
    unreachable();
}

/// To be called on (inttrunc ...) only.
Result<Statement, Err> parseIntTruncStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "inttrunc"));
    const Span &sp = span(l);

    ++it;
    if (it == end) {
        return listCutShort("inttrunc statement", "local ID", sp);
    }
    const Result<Id, Err> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) { return listCutShort("inttrunc statement", "value", sp); }
    const Result<Val, Err> val = parseVal(*it);
    if (isErr(val)) { return getErr(val); }

    ++it;
    if (it == end) { return listCutShort("inttrunc statement", "type", sp); }
    const Result<DecType, Err> type = parseType(*it);
    if (isErr(type)) { return getErr(type); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("inttrunc statement", sp); }
    }

    return Statement(
        IntTruncStmt(getVal(lid), getVal(val), getVal(type), md, span(l))
    );
}

/// To be called on (intext ...) only.
Result<Statement, Err> parseIntExtStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "intext"));
    const Span &sp = span(l);

    ++it;
    if (it == end) {
        return listCutShort("intext statement", "'zero'/'sign'", sp);
    }
    if (!isAtom(*it)) { return notAnAtom("'zero'/'sign'", span(*it)); }
    const std::string &kw = std::get<Atom>(*it).val;
    IntExtStmt::Kind kind;
    if (kw == "zero") { kind = IntExtStmt::Kind::ZERO; }
    else if (kw == "sign") { kind = IntExtStmt::Kind::SIGN; }
    else {
        return badKw("intext statement (kind)", "'zero'/'sign'", kw, span(*it));
    }

    ++it;
    if (it == end) { return listCutShort("intext statement", "local ID", sp); }
    const Result<Id, Err> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) {
        return listCutShort("intext statement", "left operand", sp);
    }
    const Result<Val, Err> val = parseVal(*it);
    if (isErr(val)) { return getErr(val); }

    ++it;
    if (it == end) { return listCutShort("intext statement", "type", sp); }
    const Result<DecType, Err> type = parseType(*it);
    if (isErr(type)) { return getErr(type); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("intextstatement", sp); }
    }

    return Statement(
        IntExtStmt(kind, getVal(lid), getVal(val), getVal(type), md, span(l))
    );
}

/// To be called on (add/mul/sub ...) only.
Result<Statement, Err> parseAddMulSubStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(
        it != end &&
        (atomEq(*it, "add") || atomEq(*it, "sub") || atomEq(*it, "mul"))
    );

    const std::string &kind = std::get<Atom>(*it).val;
    const std::string &stmtDesc = kind + " statement";

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "local ID", span(l)); }
    const Result<Id, Err> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "left operand", span(l)); }
    const Result<Val, Err> left = parseVal(*it);
    if (isErr(left)) { return getErr(left); }

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "right operand", span(l)); }
    const Result<Val, Err> right = parseVal(*it);
    if (isErr(right)) { return getErr(right); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong(stmtDesc, span(l)); }
    }

    BinaryOpStmt::Op op;
    if (kind == "add") { op = BinaryOpStmt::Op::ADD; }
    else if (kind == "sub") { op = BinaryOpStmt::Op::SUB; }
    else if (kind == "mul") { op = BinaryOpStmt::Op::MUL; }
    else { unreachable(); }

    return Statement(
        BinaryOpStmt(op, getVal(lid), getVal(left), getVal(right), md, span(l))
    );
}

/// To be called on (div/rem ...) only.
Result<Statement, Err> parseDivRemStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && (atomEq(*it, "div") || atomEq(*it, "rem")));
    const Span &sp = span(l);

    const std::string &kind = std::get<Atom>(*it).val;
    const std::string &stmtDesc = kind + " statement";

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "'signed'/'unsigned'", sp); }
    if (!isAtom(*it)) { return notAnAtom("'signed'/'unsigned'", span(*it)); }
    const std::string &kw = std::get<Atom>(*it).val;
    bool isSigned;
    if (kw == "unsigned") { isSigned = false; }
    else if (kw == "signed") { isSigned = true; }
    else {
        return badKw(
            stmtDesc + " (sign)",
            "'signed'/'unsigned'",
            kw,
            span(*it)
        );
    }

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "local ID", sp); }
    const Result<Id, Err> lidr = parseLocalId(*it);
    if (isErr(lidr)) { return getErr(lidr); }
    const Id lid = getVal(lidr);

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "left operand", sp); }
    const Result<Val, Err> leftr = parseVal(*it);
    if (isErr(leftr)) { return getErr(leftr); }
    const Val left = getVal(leftr);

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "right operand", sp); }
    const Result<Val, Err> rightr = parseVal(*it);
    if (isErr(rightr)) { return getErr(rightr); }
    const Val right = getVal(rightr);

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong(stmtDesc, sp); }
    }

    BinaryOpStmt::Op op;
    if (kind == "div") {
        op = isSigned ? BinaryOpStmt::Op::SDIV : BinaryOpStmt::Op::UDIV;
    } else if (kind == "rem") {
        op = isSigned ? BinaryOpStmt::Op::SREM : BinaryOpStmt::Op::UREM;
    } else { unreachable(); }

    return Statement(BinaryOpStmt(op, lid, left, right, md, span(l)));
}

/// To be called on (not ...) only.
Result<Statement, Err> parseNotStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "not"));
    const Span &sp = span(l);

    ++it;
    if (it == end) { return listCutShort("not statement", "local ID", sp); }
    const Result<Id, Err> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) { return listCutShort("not statement", "operand", sp); }
    const Result<Val, Err> val = parseVal(*it);
    if (isErr(val)) { return getErr(val); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("not statement", sp); }
    }

    return Statement(UnaryOpStmt(
        UnaryOpStmt::Op::NOT, getVal(lid), getVal(val), md, span(l)
    ));
}

/// To be called on (and/or/xor/shiftl ...) only.
Result<Statement, Err> parseAndOrXorShiftlStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(
        it != end &&
        (
            atomEq(*it, "and") ||
            atomEq(*it, "or") ||
            atomEq(*it, "xor") ||
            atomEq(*it, "shiftl")
        )
    );

    const std::string &kind = std::get<Atom>(*it).val;
    const std::string &stmtDesc = kind + " statement";

    ++it;
    if (it == end) { listCutShort(stmtDesc, "local ID", span(l)); }
    const Result<Id, Err> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) { listCutShort(stmtDesc, "left operand", span(l)); }
    const Result<Val, Err> left = parseVal(*it);
    if (isErr(left)) { return getErr(left); }

    ++it;
    if (it == end) { listCutShort(stmtDesc, "right operand", span(l)); }
    const Result<Val, Err> right = parseVal(*it);
    if (isErr(right)) { return getErr(right); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong(stmtDesc, span(l)); }
    }

    BinaryOpStmt::Op op;
    if (kind == "and") { op = BinaryOpStmt::Op::AND; }
    else if (kind == "or") { op = BinaryOpStmt::Op::OR; }
    else if (kind == "xor") { op = BinaryOpStmt::Op::XOR; }
    else if (kind == "shiftl") { op = BinaryOpStmt::Op::SHIFTL; }
    else { unreachable(); }

    return Statement(
        BinaryOpStmt(op, getVal(lid), getVal(left), getVal(right), md, span(l))
    );
}

/// To be called on (shiftr ...) only.
Result<Statement, Err> parseShiftrStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "shiftr"));
    const Span &sp = span(l);

    ++it;
    if (it == end) {
        return listCutShort("shiftr statement", "'logical'/'arithmetic'", sp);
    }
    if (!isAtom(*it)) { return notAnAtom("'logical'/'arithmetic'", span(*it)); }
    const std::string &kw = std::get<Atom>(*it).val;
    BinaryOpStmt::Op op;
    if (kw == "logical") { op = BinaryOpStmt::Op::LSHIFTR; }
    else if (kw == "arithmetic") { op = BinaryOpStmt::Op::ASHIFTR; }
    else {
        return badKw(
            "shiftr statement (kind)",
            "'logical'/'arithmetic'",
            kw,
            span(*it)
        );
    }

    ++it;
    if (it == end) { return listCutShort("shiftr statement", "local ID", sp); }
    const Result<Id, Err> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) {
        return listCutShort("shiftr statement", "left operand", sp);
    }
    const Result<Val, Err> left = parseVal(*it);
    if (isErr(left)) { return getErr(left); }

    ++it;
    if (it == end) {
        return listCutShort("shiftr statement", "right operand", sp);
    }
    const Result<Val, Err> right = parseVal(*it);
    if (isErr(right)) { return getErr(right); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("shiftr statement", sp); }
    }

    return Statement(
        BinaryOpStmt(op, getVal(lid), getVal(left), getVal(right), md, span(l))
    );
}

/// To be called on (assign ...) only.
Result<Statement, Err> parseAssignStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "assign"));
    const Span &sp = span(l);

    ++it;
    if (it == end) {
        return listCutShort("assignment statement", "local ID", sp);
    }
    const Result<Id, Err> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) {
        return listCutShort("assignment statement", "value", sp);
    }
    const Result<Val, Err> val = parseVal(*it);
    if (isErr(val)) { return getErr(val); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("assignment statement", sp); }
    }

    return Statement(AssignStmt(getVal(lid), getVal(val), md, span(l)));
}

/// To be called on (vararg ...) only.
Result<Statement, Err> parseVarargStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "vararg"));
    const Span &sp = span(l);

    ++it;
    if (it == end) {
        return listCutShort("vararg statement", "(typed) ID", sp);
    }
    const Result<TypedId, Err> tlid = parseTypedId(*it);
    if (isErr(tlid)) { return getErr(tlid); }

    ++it;
    if (it == end) { return listCutShort("vararg statement", "index", sp); }
    const Result<IVal, Err> val = parseIVal(*it);
    if (isErr(val)) { return getErr(val); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("vararg statement", sp); }
    }

    return Statement(VarargStmt(getVal(tlid), getVal(val), md, span(l)));
}

/// To be called on (blackhole ...) only.
Result<Statement, Err> parseBlackholeStmt(const List &l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    abortif(it != end && atomEq(*it, "blackhole"));

    ++it;
    if (it == end) {
        return listCutShort(
            "blackhole statement",
            "'pure'/'effectful'",
            span(l)
        );
    }
    if (!isAtom(*it)) {
        return notAnAtom("'pure'/'effectful'", span(*it));
    }
    const std::string &kw = std::get<Atom>(*it).val;
    bool pure;
    if (kw == "pure") { pure = true; }
    else if (kw == "effectful") { pure = false; }
    else {
        return badKw(
            "blackhole statement (kind)",
            "'pure'/'effectful'",
            kw,
            span(*it)
        );
    }

    ++it;
    if (it == end) {
        return listCutShort("blackhole statement", "(typed) ID", span(l));
    }
    const Result<TypedId, Err> tlid = parseTypedId(*it);
    if (isErr(tlid)) { return getErr(tlid); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) {
            return listTooLong("blackhole statement", span(l));
        }
    }

    return Statement(BlackholeStmt(pure, getVal(tlid), md, span(l)));
}

/// <stmt>
Result<Statement, Err> parseStmt(const SExpr &s) {
    if (!isList(s)) { return notAList("statement", span(s)); }
    const List &l = std::get<List>(s);
    auto it = l.children.cbegin(), end = l.children.cend();

    if (it == end) {
        return listCutShort("statement", "statement kind", span(l));
    }
    if (!isAtom(*it)) { return notAnAtom("statement kind", span(*it)); }
    const std::string &kind = std::get<Atom>(*it).val;

    if (kind == "phi") { return parsePhiStmt(l); }
    else if (kind == "call") { return parseCallStmt(l); }
    else if (kind == "br") { return parseBrStmt(l); }
    else if (kind == "brif") { return parseBrifStmt(l); }
    else if (kind == "ret") { return parseRet(l); }
    else if (kind == "cmp") { return parseCmpStmt(l); }
    else if (kind == "alloc") { return parseAllocStmt(l); }
    else if (kind == "store") { return parseStoreStmt(l); }
    else if (kind == "load") { return parseLoadStmt(l); }
    else if (kind == "field") { return parseFieldStmt(l); }
    else if (kind == "bitcast" || kind == "convert") {
        return parseBitCastConvertStmt(l);
    }
    else if (kind == "inttrunc") { return parseIntTruncStmt(l); }
    else if (kind == "intext") { return parseIntExtStmt(l); }
    else if (kind == "add" || kind == "sub" || kind == "mul") {
        return parseAddMulSubStmt(l);
    } else if (kind == "div" || kind == "rem") {
        return parseDivRemStmt(l);
    } else if (kind == "not") { return parseNotStmt(l); }
    else if (
        kind == "and" ||
        kind == "or" ||
        kind == "xor" ||
        kind == "shiftl"
    ) { return parseAndOrXorShiftlStmt(l); }
    else if (kind == "shiftr") { return parseShiftrStmt(l); }
    else if (kind == "assign") { return parseAssignStmt(l); }
    else if (kind == "vararg") { return parseVarargStmt(l); }
    else if (kind == "blackhole") { return parseBlackholeStmt(l); }
    else { return Err("Unknown statement kind: '" + kind + "'", span(*it)); }
    unreachable();
}

/// <basic-block>
Result<BasicBlock, Err> parseBasicBlock(const SExpr &s) {
    if (!isList(s)) { return notAList("basic block", span(s)); }
    const List &l = std::get<List>(s);
    auto it = l.children.cbegin(), end = l.children.cend();

    if (it == end) { return listCutShort("basic block", "block ID", span(s)); }
    const Result<Id, Err> bid = parseBlockId(*it);
    if (isErr(bid)) { return getErr(bid); }

    ++it;
    if (it == end) {
        return listCutShort("basic block", "statement list", span(s));
    }
    if (!isList(*it)) { return notAList("basic block", span(*it)); }
    std::vector<Statement> stmts;
    for (auto s : std::get<List>(*it).children) {
        const Result<Statement, Err> stmt = parseStmt(s);
        if (isErr(stmt)) { return getErr(stmt); }
        stmts.push_back(getVal(stmt));
    }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("basic block", span(s)); }
    }

    return BasicBlock(getVal(bid), stmts, md, span(s));
}

Result<std::vector<BasicBlock>, Err> parseFunctionBlocks(const SExpr &s) {
    if (!isList(s)) { return notAList("function's basic blocks", span(s)); }
    std::vector<BasicBlock> blocks;
    for (auto rawBlock : std::get<List>(s).children) {
        const Result<BasicBlock, Err> block = parseBasicBlock(rawBlock);
        if (isErr(block)) { return getErr(block); }
        blocks.push_back(getVal(block));
    }

    return blocks;
}

Result<std::tuple<std::vector<Param>, bool>, Err> parseFunctionParams(
    const List &l
) {
    std::vector<Param> params;
    bool vararg = false;
    for (size_t i = 0; i < l.children.size(); ++i) {
        const Result<Param, Err> param = parseParam(l.children[i]);
        if (!isErr(param)) { params.push_back(getVal(param)); }
        else {
            abortif(isErr(param));
            if (i == l.children.size() - 1) {
                // We might have a vararg parameter.
                if (atomEq(l.children[i], "...")) {
                    vararg = true;
                } else {
                    return Err(
                        "Expected a parameter or a vararg parameter as "
                        "final parameter. A vararg parameter is '...'. "
                        "If you intended a regular parameter here, "
                        "also note:\n  " + getErr(param).toString(),
                        span(l.children[i])
                    );
                }
            } else {
                // No, we definitely expected a parameter and got an error.
                return getErr(param);
            }
        }
    }

    return std::make_tuple(params, vararg);
}

/// <function>
Result<Function, Err> parseFunction(const SExpr &s) {
    if (!isList(s)) { return notAList("function", span(s)); }
    const List &l = std::get<List>(s);
    auto it = l.children.cbegin(), end = l.children.cend();

    if (it == end) { return listCutShort("function", "name", span(s)); }
    const Result<Id, Err> gidr = parseGlobalId(*it);
    if (isErr(gidr)) { return getErr(gidr); }
    const Id gid = getVal(gidr);

    ++it;
    if (it == end) { return listCutShort("function", "return type", span(s)); }
    const Result<DecType, Err> typer = parseType(*it);
    if (isErr(typer)) { return getErr(typer); }
    const DecType type = getVal(typer);

    ++it;
    if (it == end) { return listCutShort("function", "parameters", span(s)); }
    if (!isList(*it)) { return notAList("function parameters", span(*it)); }
    Result<std::tuple<std::vector<Param>, bool>, Err> paramsr =
        parseFunctionParams(std::get<List>(*it));
    if (isErr(paramsr)) { return getErr(paramsr); }
    const std::vector<Param> params = std::get<0>(getVal(paramsr));
    const bool vararg = std::get<1>(getVal(paramsr));

    ++it;
    if (it == end) {
        return listCutShort("function", "basic block list/opaque", span(s));
    }
    std::optional<std::vector<BasicBlock>> bbs;
    if (isAtom(*it)) {
        const std::string &kw = std::get<Atom>(*it).val;
        if (kw != "opaque") {
            badKw(
                "function (body)",
                "opaque (or basic block list)",
                kw,
                span(*it)
            );
        }
    } else {
        abortif(isList(*it));
        const Result<std::vector<BasicBlock>, Err>
        bbsr = parseFunctionBlocks(std::get<List>(*it));
        if (isErr(bbsr)) {
            return Err(
                "Expected a basic block list or 'opaque' as the function body. "
                "If you intended a basic block list here, note:\n  " +
                getErr(bbsr).toString(),
                span(*it)
            );
        }
        bbs.emplace(getVal(bbsr));
    }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("function", span(s)); }
    }

    return Function(gid, params, vararg, type, bbs, md, span(s));
}

/// <functions>
Result<std::vector<Function>, Err> parseFunctions(const SExpr &s) {
    if (!isList(s)) { return notAList("top-level functions", span(s)); }
    const List &l = std::get<List>(s);
    auto it = l.children.cbegin(), end = l.children.cend();

    if (it == end) { return listCutShort("functions", "'functions'", span(s)); }
    if (!isAtom(*it)) { return notAnAtom("'functions'", span(*it)); }
    const std::string &kw = std::get<Atom>(*it).val;
    if (kw != "functions") {
        return badKw("functions list", "'functions'", kw, span(*it));
    }

    ++it;
    std::vector<Function> functions;
    for (; it != end; ++it) {
        const Result<Function, Err> function = parseFunction(*it);
        if (isErr(function)) { return getErr(function); }
        functions.push_back(getVal(function));
    }

    return functions;
}

/// <type-alias>
Result<TypeAlias, Err> parseTypeAlias(const SExpr &s) {
    if (!isList(s)) { return notAList("type alias", span(s)); }
    const List &l = std::get<List>(s);
    auto it = l.children.cbegin(), end = l.children.cend();
    const Span &sp = span(s);

    if (it == end) { return listCutShort("type alias", "name (type ID)", sp); }
    const Result<Id, Err> tid = parseTypeId(*it);
    if (isErr(tid)) { return getErr(tid); }

    ++it;
    if (it == end) { return listCutShort("type alias", "type/'opaque'", sp); }
    std::optional<DecType> type;
    if (atomEq(*it, "opaque")) { type = std::nullopt; }
    else {
        const Result<DecType, Err> typer = parseType(*it);
        if (isErr(typer)) {
            return Err(
                "Expected a type to alias or 'opaque'. "
                "If you intended a type here, note:\n  " +
                getErr(typer).toString(),
                span(*it)
            );
        }
        type.emplace(getVal(typer));
    }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) { return listTooLong("type alias", sp); }
    }

    return TypeAlias(getVal(tid), type, md, span(s));
}

/// <types>
Result<std::vector<TypeAlias>, Err> parseTypes(const SExpr &s) {
    if (!isList(s)) { return notAList("top-level type aliases", span(s)); }
    const List &l = std::get<List>(s);
    auto it = l.children.cbegin(), end = l.children.cend();

    if (it == end) { return listCutShort("type aliases", "'types'", span(s)); }
    if (!isAtom(*it)) { return notAnAtom("'types'", span(*it)); }
    const std::string &kw = std::get<Atom>(*it).val;
    if (kw != "types") {
        return badKw("type aliases", "'types'", kw, span(*it));
    }

    ++it;
    std::vector<TypeAlias> tas;
    for (; it != end; ++it) {
        const Result<TypeAlias, Err> ta = parseTypeAlias(*it);
        if (isErr(ta)) { return getErr(ta); }
        tas.push_back(getVal(ta));
    }

    return tas;
}

/// <program>
Result<Program, Err> parseProgram(SExprSeq ss) {
    auto it = ss.cbegin(), end = ss.cend();

    if (it == end) { return Err("Program missing preamble block."); }
    const Result<Preamble, Err> preamble = parsePreamble(*it);
    if (isErr(preamble)) { return getErr(preamble); }

    ++it;
    if (it == end) { return Err("Program missing type aliases block."); }
    const Result<std::vector<TypeAlias>, Err> types = parseTypes(*it);
    if (isErr(types)) { return getErr(types); }

    ++it;
    if (it == end) { return Err("Program missing variables block."); }
    const Result<std::vector<Variable>, Err> variables = parseVariables(*it);
    if (isErr(variables)) { return getErr(variables); }

    ++it;
    if (it == end) { return Err("Program missing functions block."); }
    const Result<std::vector<Function>, Err> functions = parseFunctions(*it);
    if (isErr(functions)) { return getErr(functions); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, Err> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != end) {
            return Err("Unexpected element after program metadata.");
        }
    }

    return Program(
        getVal(preamble),
        getVal(types),
        getVal(variables),
        getVal(functions),
        md,
        Span(span(*(ss.begin())).start, span(*(ss.end() - 1)).end)
    );
}

}  // Anonymous namespace

namespace SVFIR {

Result<Program, Err> parse(InputStream &stream) {
    const Result<SExprs::SExprSeq, Err> &ssr = SExprs::parseSeq(stream);
    if (isErr(ssr)) { return getErr(ssr); }

    const SExprs::SExprSeq &ss = std::get<SExprs::SExprSeq>(ssr);
    return parseProgram(ss);
}

Result<Program, Err> parseFile(const std::string &filename) {
    std::ifstream fs;
    fs.open(filename);

    if (!fs.is_open()) {
        return Err(strerror(errno));
    }

    InputStream is = InputStream(fs);
    return parse(is);
}

Result<Program, Err> parseString(const std::string &str) {
    std::istringstream ss = std::istringstream(str);
    InputStream is = InputStream(ss);
    return parse(is);
}

}  // namespace SVFIR
