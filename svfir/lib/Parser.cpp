#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <sstream>
#include <fstream>
#include <stdint.h>
#include <string>

#include "Ast.h"
#include "Parser.h"
#include "Result.h"
#include "Span.h"

namespace {

using namespace SVFIR;
using namespace SVFIR::SExprs;

// TODO: Lists need to be checked that they don't have too many elements, e.g.,
// (version 1.5 x).

// TODO: Things like "if you intended an x here, note: err" can have err
// referring to a similar message, no good.

/// So we can use with std::all_of. It gets confused using std::isdigit
/// directly due to overloads.
bool isDigit(const char c) { return std::isdigit(c); }

/// Error parsing expected because we need, and don't have, a list.
ErrMsg notAList(const std::string &expected) {
    return ErrMsg("Expected a list when trying to parse " + expected + ".");
}

/// Error parsing expected because we need, and don't have, an atom.
ErrMsg notAnAtom(const std::string &expected) {
    return ErrMsg("Expected a atom when trying to parse " + expected + ".");
}

/// Error parsing encompassing as the list is too short trying for an expected.
ErrMsg listCutShort(
    const std::string &encompassing,
    const std::string &expected
) {
    return ErrMsg(
        "Expected " + expected + " " +
        "while parsing " + encompassing + " " +
        "(list too short)."
    );
}

/// Error parsing encompassing as we expected keyword kw but got actual.
ErrMsg badKw(
    const std::string &encompassing,
    const std::string &kw,
    const std::string &actual
) {
    return ErrMsg(
        "Expected " + kw + " " +
        "(got " + actual + ") " +
        "while parsing " + encompassing + "."
    );
}

/// That IDs need two characters. kind should be capitalised.
ErrMsg shortId(const std::string &kind) {
    return ErrMsg(kind + " IDs must be two or more characters");
}

/// That IDs need to start with a special character. kind should be capitalised.
ErrMsg noSpecialCharId(const std::string &kind, const std::string &ch) {
    return ErrMsg(kind + " IDs must begin with a " + ch + ".");
}

/// <md>
Result<Metadata, ErrMsg> parseMetadata(const SExpr s) {
    if (!isList(s)) { return notAList("metadata"); }
    const List list = std::get<List>(s);

    auto it = list.children.cbegin(), end = list.children.cend();
    if (it == end) { return listCutShort("metadata", "'md'"); }
    if (!isAtom(*it)) { return notAnAtom("'md'"); }
    const std::string kw = std::get<Atom>(*it).val;
    if (kw != "md") { return badKw("metadata", "'md'", kw); }

    ++it;
    if (it == end) { return listCutShort("metadata", "sexpr data"); }
    return Metadata(*it, span(*it));
}

/// <tid>
Result<TypeId, ErrMsg> parseTypeId(const SExpr s) {
    if (!isAtom(s)) { return notAnAtom("type ID"); }
    const std::string text = std::get<Atom>(s).val;
    if (text.size() < 2) { return shortId("Type"); }
    if (text[0] != '~') { return noSpecialCharId("Type", "~"); }
    return TypeId(text, span(s));
}

/// <type>
Result<Type, ErrMsg> parseType(const SExpr s) {
    const Span sp = span(s);
    if (isAtom(s)) {
        const Atom atom = std::get<Atom>(s);
        if (atom.val == "void") { return Type(VoidType(sp)); }
        else if (atom.val == "ptr") { return Type(PtrType(sp)); }
        else if (atom.val == "bool") { return Type(BoolType(sp)); }
        else if (atom.val.size() > 1 && atom.val[0] == 'i') {
            using Kind = IntType::Kind;
            // + 1 to skip the 'i'.
            const int width = std::atoi(atom.val.c_str() + 1);
            if (width == 8) { return Type(IntType(Kind::I8, sp)); }
            else if (width == 16) { return Type(IntType(Kind::I16, sp)); }
            else if (width == 32) { return Type(IntType(Kind::I32, sp)); }
            else if (width == 64) { return Type(IntType(Kind::I64, sp)); }
            else if (width == 128) { return Type(IntType(Kind::I128, sp)); }
            else {
                return ErrMsg(
                    "Bad width for int type (" + std::to_string(width) + ")."
                );
            }
        } else if (!atom.val.empty() && atom.val[0] == 'f') {
            using Kind = FloatType::Kind;
            // + 1 to skip the 'f'.
            const int width = std::atoi(atom.val.c_str() + 1);
            if (width == 16) { return Type(FloatType(Kind::F16, sp)); }
            else if (width == 32) { return Type(FloatType(Kind::F32, sp)); }
            else if (width == 64) { return Type(FloatType(Kind::F64, sp)); }
            else if (width == 128) { return Type(FloatType(Kind::F128, sp)); }
            else {
                return ErrMsg(
                    "Bad width for float type (" + std::to_string(width) + ")."
                );
            }
        } else if (!atom.val.empty() && atom.val[0] == '~') {
            const Result<TypeId, ErrMsg> id = parseTypeId(s);
            if (isErr(id)) {
                return ErrMsg(
                    "Invalid scalar type.\n"
                    "If you intended a type ID here, note: " +
                    getErr(id)
                );
            }
            return Type(getVal(id));
        } else { return ErrMsg("Invalid scalar type."); }
    } else {
        assert(isList(s));
        const List list = std::get<List>(s);

        auto it = list.children.cbegin(), end = list.children.cend();
        if (it == end) { return listCutShort("aggregate type", "'agg'"); }
        if (!isAtom(*it)) { return notAnAtom("'agg'"); }
        const std::string kw = std::get<Atom>(*it).val;
        if (kw != "agg") { badKw("aggregate type", "'agg'", kw); }


        ++it;
        if (it == end) {
            return listCutShort("aggregate type", "number of slots");
        }
        if (!isAtom(*it)) { return notAnAtom("number of slots"); }
        const std::string slotsStr = std::get<Atom>(*it).val;
        if (!std::all_of(slotsStr.begin(), slotsStr.end(), isDigit)) {
            // TODO: what if it starts with 0
            return ErrMsg(
                "aggregate type's number of slots is not positive number "
                "(" + slotsStr + ")."
            );
        }
        errno = 0;
        const long long slots = std::strtoull(slotsStr.c_str(), nullptr, 10);
        if (errno != 0 || slots > UINT64_MAX) {
            return ErrMsg(
                "Too many slots (" + slotsStr + ") for aggregate type."
            );
        }

        return Type(AggType(slots, sp));
    }
}

/// <lid>
Result<LocalId, ErrMsg> parseLocalId(const SExpr s) {
    if (!isAtom(s)) { return notAnAtom("local ID"); }
    const std::string text = std::get<Atom>(s).val;
    if (text.size() < 2) { return shortId("Local"); }
    if (text[0] != '%') { return noSpecialCharId("Local", "%"); }
    return LocalId(text, span(s));
}

/// <gid>
Result<GlobalId, ErrMsg> parseGlobalId(const SExpr s) {
    if (!isAtom(s)) { return notAnAtom("global ID"); }
    const std::string text = std::get<Atom>(s).val;
    if (text.size() < 2) { return shortId("Global"); }
    if (text[0] != '@') { return noSpecialCharId("Global", "@"); }
    return GlobalId(text, span(s));
}

/// <bid>
Result<BlockId, ErrMsg> parseBlockId(const SExpr s) {
    if (!isAtom(s)) { return notAnAtom("basic block ID"); }
    const std::string text = std::get<Atom>(s).val;
    if (text.size() < 2) { return shortId("Basic block"); }
    if (text[0] != '!') { return noSpecialCharId("Basic block", "!"); }
    return BlockId(text, span(s));
}

/// <var-id>
Result<VarId, ErrMsg> parseVarId(const SExpr s) {
    // Error check here so we know parseLocalId/parseGlobalId will pass.
    if (!isAtom(s)) { return notAnAtom("local/global (var) ID"); }
    const std::string text = std::get<Atom>(s).val;
    if (text.size() < 2) { return shortId("Local/global (var)"); }
    if (text[0] == '%') { return getVal(parseLocalId(s)); }
    else if (text[0] == '@') { return getVal(parseGlobalId(s)); }
    else { return noSpecialCharId("Local/global (var)", "%/@"); }
}

/// <typed-id>
Result<TypedId, ErrMsg> parseTypedId(const SExpr s) {
    if (!isList(s)) { return notAList("typed (local) ID"); }

    const List list = std::get<List>(s);
    auto it = list.children.cbegin(), end = list.children.cend();

    if (it == end) { return listCutShort("typed (local) ID", "ID"); }
    const Result<LocalId, ErrMsg> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) { return listCutShort("typed (local) ID", "type"); }
    const Result<Type, ErrMsg> type = parseType(*it);
    if (isErr(type)) { return getErr(type); }

    return TypedId(getVal(lid), getVal(type), span(s));
}

/// <param>
Result<Param, ErrMsg> parseParam(const SExpr s) {
    if (!isList(s)) { return notAList("Param"); }

    const List list = std::get<List>(s);
    auto it = list.children.cbegin(), end = list.children.cend();

    if (it == end) { return listCutShort("parameter", "ID"); }
    const Result<LocalId, ErrMsg> id = parseLocalId(*it);
    if (isErr(id)) { return getErr(id); }

    ++it;
    if (it == end) { return listCutShort("parameter", "type"); }
    const Result<Type, ErrMsg> type = parseType(*it);
    if (isErr(type)) { return getErr(type); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Param(getVal(id), getVal(type), md, span(s));
}

Result<TypedConstant, ErrMsg> parseTypedConst(SExpr s);
/// <const>
Result<Constant, ErrMsg> parseConst(const SExpr s) {
    const Span sp = span(s);
    if (isAtom(s)) {
        const std::string val = std::get<Atom>(s).val;
        if (val.empty()) { return ErrMsg("Expected constant, got empty atom"); }
        else if (val == "null") { return Constant(NullConstant(sp)); }
        else if (val == "true") { return Constant(BoolConstant(true, sp)); }
        else if (val == "false") { return Constant(BoolConstant(false, sp)); }
        else if (val == "-inf" || val == "+inf" || val == "nan") {
            return Constant(FloatConstant(val, sp));
        } else {  // Try for an int or numeric float.
            // But first, a small-effort check if the user *may* have intended
            // a numeric constant.
            if (val[0] != '-' && !isDigit(val[0])) {
                return ErrMsg("Invalid constant, got '" + val + "'.");
            }

            const auto numStart =
                val[0] == '-' ? val.cbegin() + 1 : val.cbegin();
            if (std::all_of(numStart, val.cend(), isDigit)) {
                return Constant(IntConstant(val, sp));
            }

            const auto dotPos = std::find(numStart, val.cend(), '.');
            if (dotPos == val.cend()) {
                return ErrMsg(
                    "Invalid constant, got '" + val + "'. "
                    "If you intended a float constant, a '.' is required."
                );
            }
            if (dotPos + 1 == val.cend()) {
                return ErrMsg(
                    "Invalid constant, got '" + val + "'. "
                    "If you intended a float constant, "
                    "at least one digit is required after the '.'."
                );
            }

            auto it = dotPos + 1;
            for (; it != val.cend(); ++it) { }
            if (it != val.cend()) {
                if (*it != 'e') {
                    return ErrMsg(
                        "Invalid constant, got '" + val + "'. "
                        "If you intended a float constant, "
                        "you may have intended the exponent signifier 'e'."
                    );
                }
                ++it;
                if (*it == '-') { ++it; }
                if (!std::all_of(it, val.cend(), isDigit)) {
                    return ErrMsg(
                        "Invalid constant, got '" + val + "'. "
                        "If you intended a float constant, "
                        "the exponent must be a positive or negative integer."
                    );
                }
            }

            return Constant(FloatConstant(val, sp));
        }
    } else {
        assert(isList(s));
        SExprSeq elems = std::get<List>(s).children;
        auto it = elems.cbegin(), end = elems.cend();
        if (it == end) { return listCutShort("sequence", "'seq'"); }

        if (!isAtom(*it)) { return notAnAtom("'seq'"); }
        const std::string kw = std::get<Atom>(*it).val;
        if (kw != "seq") { return badKw("sequence (constant)", "'seq'", kw); }

        ++it;
        std::vector<Val> vals;
        for (; it != end; ++it) {
            const Result<TypedConstant, ErrMsg> tc = parseTypedConst(*it);
            if (isErr(tc)) {
                const Result<VarId, ErrMsg> vi = parseVarId(*it);
                if (isErr(vi)) {
                    return ErrMsg(
                        "Elements of a sequence must be a typed constant "
                        "or a local/global (var) ID."
                    );
                } else { vals.push_back(getVal(vi)); }
            } else { vals.push_back(getVal(tc)); }
        }

        return Constant(SeqConstant(vals, sp));
    }
    assert(false);
}

/// <typed-const>
Result<TypedConstant, ErrMsg> parseTypedConst(SExpr s) {
    if (!isList(s)) { return notAList("typed constant"); }

    const List list = std::get<List>(s);
    auto it = list.children.cbegin(), end = list.children.cend();
    if (it == end) { return listCutShort("typed constant", "constant"); }

    const Result<Constant, ErrMsg> constant = parseConst(*it);
    if (isErr(constant)) { return getErr(constant); }

    ++it;
    const Result<Type, ErrMsg> type = parseType(*it);
    if (isErr(type)) { return getErr(type); }

    return TypedConstant(getVal(constant), getVal(type), span(s));
}

/// Version in preamble.
Result<Version, ErrMsg> parseVersion(SExpr s) {
    if (!isList(s)) { return notAList("version"); }
    const List list = std::get<List>(s);

    auto it = list.children.cbegin(), vend = list.children.cend();
    if (it == vend) { return listCutShort("version", "'version'"); }
    if (!isAtom(*it)) { return notAnAtom("'version'"); }
    const std::string kw = std::get<Atom>(*it).val;
    if (kw != "version") { return badKw("version", "'version'", kw); }

    ++it;
    if (it == vend) { return listCutShort("version", "version number"); }
    if (!isAtom(*it)) { return notAnAtom("version number"); }
    const std::string versionStr = std::get<Atom>(*it).val;

    const size_t dot = versionStr.find(".", 0);
    if (dot == std::string::npos) {
        return ErrMsg("Version number missing '.'.");
    } else if (dot == 0) {
        return ErrMsg("Version number missing major version (before '.').");
    } else if (dot == versionStr.size() - 1) {
        return ErrMsg("Version number missing minor version (after '.').");
    }

    const std::string majorStr = versionStr.substr(0, dot);
    if (!std::all_of(majorStr.cbegin(), majorStr.cend(), isDigit)) {
        return ErrMsg("Major version contains non-digit.");
    }
    const long majorVersion = std::strtoul(majorStr.c_str(), nullptr, 10);
    if (majorVersion > UINT16_MAX) {
        return ErrMsg(
            "Major version too large. "
            "(Max: " + std::to_string(UINT16_MAX) + ".)"
        );
    }

    const std::string minorStr = versionStr.substr(0, dot);
    if (!std::all_of(minorStr.cbegin(), minorStr.cend(), isDigit)) {
        return ErrMsg("Minor version contains non-digit.");
    }
    const long minorVersion = std::strtoul(minorStr.c_str(), nullptr, 10);
    if (minorVersion > UINT16_MAX) {
        return ErrMsg(
            "Minor version too large. "
            "(Max: " + std::to_string(UINT16_MAX) + ".)"
        );
    }

    return Version(majorVersion, minorVersion);
}

/// <preamble>
Result<Preamble, ErrMsg> parsePreamble(SExpr s) {
    if (!isList(s)) { return notAList("preamble"); }

    const List list = std::get<List>(s);
    auto it = list.children.cbegin(), end = list.children.cend();

    // Preamble keyword.
    if (it == end) { return listCutShort("preamble", "'preamble'"); }
    if (!isAtom(*it)) { return notAnAtom("'preamble'"); }
    const std::string kw = std::get<Atom>(*it).val;
    if (kw != "preamble") { return badKw("preamble", "'preamble'", kw); }

    // Version.
    ++it;
    if (it == end) { return listCutShort("preamble", "version"); }
    const Result<Version, ErrMsg> version = parseVersion(*it);
    if (isErr(version)) { return getErr(version); }

    // Source.
    ++it;
    if (it == end) { return listCutShort("preamble", "source"); }
    if (!isList(*it)) { return notAList("source"); }

    const List sourceList = std::get<List>(*it);
    auto sit = sourceList.children.cbegin(), send = sourceList.children.cend();
    if (sit == send) { return listCutShort("source", "'source'"); }
    if (!isAtom(*sit)) { return notAnAtom("'source'"); }
    const std::string skw = std::get<Atom>(*sit).val;
    if (skw != "source") { return badKw("source", "'source'", kw); }

    ++sit;
    if (sit == send) { return listCutShort("source", "source descriptor"); }
    if (!isAtom(*sit)) { return notAnAtom("source descriptor"); }
    const std::string source = std::get<Atom>(*sit).val;

    // Metadata.
    MaybeMetadata md = std::nullopt;
    ++it;
    if (it != end) {
        Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Preamble(getVal(version), source, md, span(s));
}

/// <gval>
Result<GVal, ErrMsg> parseGVal(SExpr s) {
    static const std::string expectation =
        "Expected a typed constant or a global ID.";
    if (isAtom(s)) {
        // Try for <gid>.
        const Result<GlobalId, ErrMsg> gid = parseGlobalId(s);
        if (!isErr(gid)) { return GVal(getVal(gid)); }
        else {
            return ErrMsg(
                expectation + " "
                "If you intended a global ID here, note: " + getErr(gid)
            );
        }
    } else {
        // Try for <typed-const>
        assert(isList(s));
        const Result<TypedConstant, ErrMsg> tc = parseTypedConst(s);
        if (!isErr(tc)) { return GVal(getVal(tc)); }
        else {
            return ErrMsg(
                expectation + " "
                "If you intended a typed constant here, note: " + getErr(tc)
            );
        }
    }
    assert(false);
}

/// <val>
Result<Val, ErrMsg> parseVal(SExpr s) {
    static const std::string expectation =
        "Expected a typed constant or a local/global (var) ID.";
    if (isAtom(s)) {
        const Result<VarId, ErrMsg> vid = parseVarId(s);
        if (!isErr(vid)) { return Val(getVal(vid)); }
        else {
            return ErrMsg(
                expectation + " "
                "If you intended a local/global (var) ID here, note: " +
                getErr(vid)
            );
        }
    } else {
        assert(isList(s));
        const Result<TypedConstant, ErrMsg> tc = parseTypedConst(s);
        if (!isErr(tc)) { return Val(getVal(tc)); }
        else {
            return ErrMsg(
                expectation + " "
                "If you intended a typed constant here, note: " + getErr(tc)
            );
        }
    }
    assert(false);
}

/// <pval>
Result<PVal, ErrMsg> parsePVal(SExpr s) {
    if (!isAtom(s)) {
        return notAnAtom("Global/local (var) ID or null constant.");
    }

    const Result<Constant, ErrMsg> c = parseConst(s);
    if (!isErr(c) && std::holds_alternative<NullConstant>(getVal(c))) {
        return PVal(std::get<NullConstant>(getVal(c)));
    }

    const Result<VarId, ErrMsg> vid = parseVarId(s);
    if (!isErr(vid)) { return PVal(getVal(vid)); }
    else {
        return ErrMsg(
            "Expected a null constant or a local/global (var) ID. "
            "If you intended a local/global (var) ID here, note: " +
            getErr(vid)
        );
    }
    assert(false);
}

/// <ival>
Result<IVal, ErrMsg> parseIVal(SExpr s) {
    if (!isAtom(s)) {
        return notAnAtom("Global/local (var) ID or integer constant.");
    }

    const Result<Constant, ErrMsg> c = parseConst(s);
    if (!isErr(c) && std::holds_alternative<IntConstant>(getVal(c))) {
        return IVal(std::get<IntConstant>(getVal(c)));
    }

    const Result<VarId, ErrMsg> vid = parseVarId(s);
    if (!isErr(vid)) { return IVal(getVal(vid)); }
    else {
        return ErrMsg(
            "Expected an integer constant or a local/global (var) ID. "
            "If you intended a local/global (var) ID here, note: " +
            getErr(vid)
        );
    }
    assert(false);
}

/// <variable>
Result<Variable, ErrMsg> parseVariable(SExpr s) {
    if (!isList(s)) { return notAList("variable"); }

    const List list = std::get<List>(s);
    auto it = list.children.cbegin(), end = list.children.cend();

    if (it == end) { return listCutShort("variable", "variable name"); }
    const Result<GlobalId, ErrMsg> gid = parseGlobalId(*it);
    if (isErr(gid)) { return getErr(gid); }

    ++it;
    if (it == end) { return listCutShort("variable", "value/opaqueness"); }
    std::optional<GVal> gval;
    if (atomEq(*it, "opaque")) { gval = std::nullopt; }
    else {
        const Result<GVal, ErrMsg> gvalr = parseGVal(*it);
        if (isErr(gvalr)) {
            return ErrMsg(
                "Expected global ID, typed constant, or 'opaque' as "
                "variable value. If you intended a value here, note: " +
                getErr(gvalr)
            );
        } else { gval.emplace(getVal(gvalr)); }
    }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Variable(getVal(gid), gval, md, span(s));
}

/// <variables>
Result<std::vector<Variable>, ErrMsg> parseVariables(SExpr s) {
    if (!isList(s)) { return notAList("variables"); }
    const List list = std::get<List>(s);
    auto it = list.children.cbegin(), end = list.children.cend();

    if (it == end) { return listCutShort("variables", "'variables'"); }
    if (!isAtom(*it)) { return notAnAtom("'variables'"); }
    const std::string kw = std::get<Atom>(*it).val;
    if (kw != "variables") { return badKw("variables", "'variables'", kw); }

    ++it;
    std::vector<Variable> variables;
    for (; it != end; ++it) {
        const Result<Variable, ErrMsg> variable = parseVariable(*it);
        if (isErr(variable)) { return ErrMsg(getErr(variable)); }
        variables.push_back(getVal(variable));
    }

    return variables;
}

/// To be called on (phi ...) only.
Result<Statement, ErrMsg> parsePhiStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "phi"));

    ++it;
    if (it == end) { return listCutShort("phi statement", "local ID"); }
    const Result<LocalId, ErrMsg> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (!isList(*it)) { return notAList("phi operands"); }
    std::vector<PhiStmt::Operand> operands;
    for (auto s : std::get<List>(*it).children) {
        if (!isList(s)) { return notAList("phi operand"); }
        const List pair = std::get<List>(s);

        if (pair.children.size() != 2) {
            return ErrMsg(
                "Phi operands must be value/block ID pairs. This operand has " +
                std::to_string(pair.children.size()) + " elements."
            );
        }

        const Result<Val, ErrMsg> val = parseVal(pair.children[0]);
        if (isErr(val)) { return getErr(val); }

        const Result<BlockId, ErrMsg> bid = parseBlockId(pair.children[1]);
        if (isErr(bid)) { return getErr(bid); }

        operands.push_back(PhiStmt::Operand(getVal(val), getVal(bid)));
    }
    if (operands.empty()) {
        return ErrMsg("Phi statements must have at least one operand.");
    }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(PhiStmt(getVal(lid), operands, md, span(l)));
}

/// To be called on (call ...) only.
Result<Statement, ErrMsg> parseCallStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "call"));

    ++it;
    if (it == end) {
        return listCutShort("call statement", "local ID or 'void'");
    }
    if (!isAtom(*it)) { return notAnAtom("local ID or 'void'"); }
    std::optional<LocalId> lid = std::nullopt;
    if (!atomEq(*it, "void")) {
        // Try a local ID.
        const Result<LocalId, ErrMsg> lidr = parseLocalId(*it);
        if (isErr(lidr)) {
            return ErrMsg(
                "Expected a local ID or 'void'. "
                "If you intended a local ID here, note: " + getErr(lidr)
            );
        }
        lid.emplace(getVal(lidr));
    }

    ++it;
    if (it == end) { return listCutShort("call statement", "callee"); }
    const Result<PVal, ErrMsg> callee = parsePVal(*it);
    if (isErr(callee)) { return getErr(callee); }

    ++it;
    if (it == end) { return listCutShort("call statement", "arguments"); }
    if (!isList(*it)) { return notAList("call arguments"); }
    std::vector<Val> args;
    for (const auto a : std::get<List>(*it).children) {
        const Result<Val, ErrMsg> val = parseVal(a);
        if (isErr(val)) { return getErr(val); }
        args.push_back(getVal(val));
    }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(CallStmt(lid, getVal(callee), args, md, span(l)));
}

/// To be called on (br ...) only.
Result<Statement, ErrMsg> parseBrStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "br"));

    ++it;
    if (it == end) { return listCutShort("branch statement", "target ID"); }
    const Result<BlockId, ErrMsg> target = parseBlockId(*it);
    if (isErr(target)) { return getErr(target); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(BrStmt(getVal(target), md, span(l)));
}

/// To be called on (brif ...) only.
Result<Statement, ErrMsg> parseBrifStmt(List l) {
    static const std::string brifDesc = "conditional branch statement";

    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "brif"));

    ++it;
    if (it == end) { return listCutShort(brifDesc, "condition value"); }
    const Result<Val, ErrMsg> val = parseVal(*it);
    if (isErr(val)) { return getErr(val); }

    ++it;
    if (it == end) { return listCutShort(brifDesc, "if target ID"); }
    const Result<BlockId, ErrMsg> ifTarget = parseBlockId(*it);
    if (isErr(ifTarget)) { return getErr(ifTarget); }

    ++it;
    if (it == end) { return listCutShort(brifDesc, "else target ID"); }
    const Result<BlockId, ErrMsg> elseTarget = parseBlockId(*it);
    if (isErr(elseTarget)) { return getErr(elseTarget); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(
        BrifStmt(getVal(val), getVal(ifTarget), getVal(elseTarget), md, span(l))
    );
}

/// To be called on (ret ...) only.
Result<Statement, ErrMsg> parseRet(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "ret"));

    ++it;
    if (it == end) {
        return listCutShort("return statement", "value or 'void'");
    }
    std::optional<Val> val;
    if (atomEq(*it, "void")) { val = std::nullopt; }
    else {
        const Result<Val, ErrMsg> valr = parseVal(*it);
        if (isErr(valr)) {
            return ErrMsg(
                "Expected a value or 'void'. "
                "If you intended a value here, note: " + getErr(valr)
            );
        } else { val.emplace(getVal(valr)); }
    }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return RetStmt(val, md, span(l));
}

/// To be called on (cmp ...) only.
Result<Statement, ErrMsg> parseCmpStmt(List l) {
    static const std::string cmpDesc = "comparison statement";

    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "cmp"));

    ++it;
    if (it == end) { return listCutShort(cmpDesc, "local ID"); }
    const Result<LocalId, ErrMsg> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    CmpStmt::Operator op;
    if (!isAtom(*it)) { return notAnAtom("comparison operator"); }
    const std::string kw = std::get<Atom>(*it).val;
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
            kw
        );
    }

    ++it;
    if (it == end) { return listCutShort(cmpDesc, "left operand"); }
    const Result<Val, ErrMsg> left = parseVal(*it);
    if (isErr(left)) { return getErr(left); }

    ++it;
    if (it == end) { return listCutShort(cmpDesc, "right operand"); }
    const Result<Val, ErrMsg> right = parseVal(*it);
    if (isErr(right)) { return getErr(right); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(
        CmpStmt(getVal(lid), op, getVal(left), getVal(right), md, span(l))
    );
}

/// To be called on (alloc ...) only.
Result<Statement, ErrMsg> parseAllocStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "alloc"));

    ++it;
    if (it == end) { return listCutShort("allocation statement", "kind"); }
    if (!isAtom(*it)) { return notAnAtom("'heap' or 'stack'"); }
    const std::string kw = std::get<Atom>(*it).val;
    AllocStmt::Kind kind;
    if (kw == "heap") { kind = AllocStmt::Kind::HEAP; }
    else if (kw == "stack") { kind = AllocStmt::Kind::STACK; }
    else {
        return badKw("allocation statement (kind)", "'heap' or 'stack'", kw);
    }

    ++it;
    if (it == end) { return listCutShort("allocation statement", "local ID"); }
    const Result<LocalId, ErrMsg> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(AllocStmt(kind, getVal(lid), md, span(l)));
}

/// To be called on (store ...) only.
Result<Statement, ErrMsg> parseStoreStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "store"));

    ++it;
    if (it == end) { return listCutShort("store statement", "value"); }
    const Result<Val, ErrMsg> val = parseVal(*it);
    if (isErr(val)) { return getErr(val); }

    ++it;
    if (it == end) {
        return listCutShort("store statement", "destination pointer");
    }
    const Result<PVal, ErrMsg> dst = parsePVal(*it);
    if (isErr(dst)) { return getErr(dst); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(StoreStmt(getVal(val), getVal(dst), md, span(l)));
}

/// To be called on (load ...) only.
Result<Statement, ErrMsg> parseLoadStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "load"));

    ++it;
    if (it == end) {
        return listCutShort("load statement", "(typed) local ID");
    }
    const Result<TypedId, ErrMsg> tlid = parseTypedId(*it);
    if (isErr(tlid)) { return getErr(tlid); }

    ++it;
    if (it == end) { return listCutShort("load statement", "source pointer"); }
    const Result<PVal, ErrMsg> src = parsePVal(*it);
    if (isErr(src)) { return getErr(src); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(LoadStmt(getVal(tlid), getVal(src), md, span(l)));
}

/// To be called on (field ...) only.
Result<Statement, ErrMsg> parseFieldStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "field"));

    ++it;
    if (it == end) { return listCutShort("field statement", "local ID"); }
    const Result<LocalId, ErrMsg> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) { return listCutShort("field statement", "source pointer"); }
    const Result<PVal, ErrMsg> src = parsePVal(*it);
    if (isErr(src)) { return getErr(src); }

    ++it;
    if (it == end) { return listCutShort("field statement", "field index"); }
    const Result<IVal, ErrMsg> index = parseIVal(*it);
    if (isErr(index)) { return getErr(index); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(
        FieldStmt(getVal(lid), getVal(src), getVal(index), md, span(l))
    );
}

/// To be called on (add/mul/sub ...) only.
Result<Statement, ErrMsg> parseAddMulSubStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(
        it != end &&
        (atomEq(*it, "add") || atomEq(*it, "sub") || atomEq(*it, "mul"))
    );

    const std::string kind = std::get<Atom>(*it).val;
    const std::string stmtDesc = kind + " statement";

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "local ID"); }
    const Result<LocalId, ErrMsg> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "left operand"); }
    const Result<Val, ErrMsg> left = parseVal(*it);
    if (isErr(left)) { return getErr(left); }

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "right operand"); }
    const Result<Val, ErrMsg> right = parseVal(*it);
    if (isErr(right)) { return getErr(right); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    if (kind == "add") {
        return Statement(
            AddStmt(getVal(lid), getVal(left), getVal(right), md, span(l))
        );
    } else if (kind == "sub") {
        return Statement(
            SubStmt(getVal(lid), getVal(left), getVal(right), md, span(l))
        );
    } else if (kind == "mul") {
        return Statement(
            MulStmt(getVal(lid), getVal(left), getVal(right), md, span(l))
        );
    } else {
        assert(false);
    }
}


/// To be called on (div/rem ...) only.
Result<Statement, ErrMsg> parseDivRemStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && (atomEq(*it, "div") || atomEq(*it, "rem")));

    const std::string kind = std::get<Atom>(*it).val;
    const std::string stmtDesc = kind + " statement";

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "'signed'/'unsigned'"); }
    if (!isAtom(*it)) { return notAnAtom("'signed'/'unsigned'"); }
    const std::string kw = std::get<Atom>(*it).val;
    bool sign;
    if (kw == "unsigned") { sign = false; }
    else if (kw == "signed") { sign = true; }
    else { return badKw(stmtDesc + " (sign)", "'signed'/'unsigned'", kw); }

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "local ID"); }
    const Result<LocalId, ErrMsg> lidr = parseLocalId(*it);
    if (isErr(lidr)) { return getErr(lidr); }
    const LocalId lid = getVal(lidr);

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "left operand"); }
    const Result<Val, ErrMsg> leftr = parseVal(*it);
    if (isErr(leftr)) { return getErr(leftr); }
    const Val left = getVal(leftr);

    ++it;
    if (it == end) { return listCutShort(stmtDesc, "right operand"); }
    const Result<Val, ErrMsg> rightr = parseVal(*it);
    if (isErr(rightr)) { return getErr(rightr); }
    const Val right = getVal(rightr);

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    if (kind == "div") {
        const DivStmt::Kind kind =
            sign ? DivStmt::Kind::SIGNED : DivStmt::Kind::UNSIGNED;
        return Statement(DivStmt(kind, lid, left, right, md, span(l)));
    } else if (kind == "rem") {
        const RemStmt::Kind kind =
            sign ? RemStmt::Kind::SIGNED : RemStmt::Kind::UNSIGNED;
        return Statement(RemStmt(kind, lid, left, right, md, span(l)));
    } else {
        assert(false);
    }
}

/// To be called on (not ...) only.
Result<Statement, ErrMsg> parseNotStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "not"));

    ++it;
    if (it == end) { return listCutShort("not statement", "local ID"); }
    const Result<LocalId, ErrMsg> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) { return listCutShort("not statement", "operand"); }
    const Result<Val, ErrMsg> val = parseVal(*it);
    if (isErr(val)) { return getErr(val); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(NotStmt(getVal(lid), getVal(val), md, span(l)));
}

/// To be called on (and/or/xor/shiftl ...) only.
Result<Statement, ErrMsg> parseAndOrXorShiftlStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(
        it != end &&
        (
            atomEq(*it, "and") ||
            atomEq(*it, "or") ||
            atomEq(*it, "xor") ||
            atomEq(*it, "shiftl")
        )
    );

    const std::string kind = std::get<Atom>(*it).val;
    const std::string stmtDesc = kind + " statement";

    ++it;
    if (it == end) { listCutShort(stmtDesc, "local ID"); }
    const Result<LocalId, ErrMsg> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) { listCutShort(stmtDesc, "left operand"); }
    const Result<Val, ErrMsg> left = parseVal(*it);
    if (isErr(left)) { return getErr(left); }

    ++it;
    if (it == end) { listCutShort(stmtDesc, "right operand"); }
    const Result<Val, ErrMsg> right = parseVal(*it);
    if (isErr(right)) { return getErr(right); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    if (kind == "and") {
        return Statement(
            AndStmt(getVal(lid), getVal(left), getVal(right), md, span(l))
        );
    } else if (kind == "or") {
        return Statement(
            OrStmt(getVal(lid), getVal(left), getVal(right), md, span(l))
        );
    } else if (kind == "xor") {
        return Statement(
            XorStmt(getVal(lid), getVal(left), getVal(right), md, span(l))
        );
    } else if (kind == "shiftl") {
        return Statement(
            ShiftlStmt(getVal(lid), getVal(left), getVal(right), md, span(l))
        );
    } else { assert(false); }
}

/// To be called on (shiftr ...) only.
Result<Statement, ErrMsg> parseShiftrStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "shiftr"));

    ++it;
    if (it == end) {
        return listCutShort("shiftr statement", "'logical'/'arithmetic'");
    }
    if (!isAtom(*it)) { return notAnAtom("'logical'/'arithmetic'"); }
    const std::string kw = std::get<Atom>(*it).val;
    ShiftrStmt::Kind kind;
    if (kw == "logical") { kind = ShiftrStmt::Kind::LOGICAL; }
    else if (kw == "arithmetic") { kind = ShiftrStmt::Kind::ARITHMETIC; }
    else {
        return badKw("shiftr statement (kind)", "'logical'/'arithmetic'", kw);
    }

    ++it;
    if (it == end) { return listCutShort("shiftr statement", "local ID"); }
    const Result<LocalId, ErrMsg> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) { return listCutShort("shiftr statement", "left operand"); }
    const Result<Val, ErrMsg> left = parseVal(*it);
    if (isErr(left)) { return getErr(left); }

    ++it;
    if (it == end) { return listCutShort("shiftr statement", "right operand"); }
    const Result<Val, ErrMsg> right = parseVal(*it);
    if (isErr(right)) { return getErr(right); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(
        ShiftrStmt(kind, getVal(lid), getVal(left), getVal(right), md, span(l))
    );
}

/// To be called on (assign ...) only.
Result<Statement, ErrMsg> parseAssignStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "assign"));

    ++it;
    if (it == end) { return listCutShort("assign statement", "local ID"); }
    const Result<LocalId, ErrMsg> lid = parseLocalId(*it);
    if (isErr(lid)) { return getErr(lid); }

    ++it;
    if (it == end) { return listCutShort("assign statement", "value"); }
    const Result<Val, ErrMsg> val = parseVal(*it);
    if (isErr(val)) { return getErr(val); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(AssignStmt(getVal(lid), getVal(val), md, span(l)));
}

/// To be called on (vararg ...) only.
Result<Statement, ErrMsg> parseVarargStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "vararg"));

    ++it;
    if (it == end) { return listCutShort("vararg statement", "(typed) ID"); }
    const Result<TypedId, ErrMsg> tlid = parseTypedId(*it);
    if (isErr(tlid)) { return getErr(tlid); }

    ++it;
    if (it == end) { return listCutShort("vararg statement", "index"); }
    const Result<IVal, ErrMsg> val = parseIVal(*it);
    if (isErr(val)) { return getErr(val); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(VarargStmt(getVal(tlid), getVal(val), md, span(l)));
}

/// To be called on (blackhole ...) only.
Result<Statement, ErrMsg> parseBlackholeStmt(List l) {
    auto it = l.children.cbegin(), end = l.children.cend();
    assert(it != end && atomEq(*it, "blackhole"));

    ++it;
    if (it == end) { return listCutShort("blackhole statement", "(typed) ID"); }
    const Result<TypedId, ErrMsg> tlid = parseTypedId(*it);
    if (isErr(tlid)) { return getErr(tlid); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != end) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(BlackholeStmt(getVal(tlid), md, span(l)));
}

Result<Statement, ErrMsg> parseStmt(SExpr s) {
    if (!isList(s)) { return notAList("statement"); }
    const List l = std::get<List>(s);

    if (!isAtom(l.children[0])) { return notAnAtom("statement keyword"); }
    const std::string instr = std::get<Atom>(l.children[0]).val;

    if (instr == "phi") { return parsePhiStmt(l); }
    else if (instr == "call") { return parseCallStmt(l); }
    else if (instr == "br") { return parseBrStmt(l); }
    else if (instr == "brif") { return parseBrifStmt(l); }
    else if (instr == "ret") { return parseRet(l); }
    else if (instr == "cmp") { return parseCmpStmt(l); }
    else if (instr == "alloc") { return parseAllocStmt(l); }
    else if (instr == "store") { return parseStoreStmt(l); }
    else if (instr == "load") { return parseLoadStmt(l); }
    else if (instr == "field") { return parseFieldStmt(l); }
    else if (instr == "add" || instr == "sub" || instr == "mul") {
        return parseAddMulSubStmt(l);
    } else if (instr == "div" || instr == "rem") {
        return parseDivRemStmt(l);
    } else if (instr == "not") { return parseNotStmt(l); }
    else if (
        instr == "and" ||
        instr == "or" ||
        instr == "xor" ||
        instr == "shiftl"
    ) { return parseAndOrXorShiftlStmt(l); }
    else if (instr == "shiftr") { return parseShiftrStmt(l); }
    else if (instr == "assign") { return parseAssignStmt(l); }
    else if (instr == "vararg") { return parseVarargStmt(l); }
    else if (instr == "blackhole") { return parseBlackholeStmt(l); }
    else { return ErrMsg("unknown instruction"); }
    assert(false);
    // TODO: conversions.
}

Result<BasicBlock, ErrMsg> parseBasicBlock(SExpr s) {
    if (!isList(s)) { return ErrMsg("basic block"); }
    const SExprSeq elems = std::get<List>(s).children;

    if (elems.size() != 2 && elems.size() != 3) {
        return ErrMsg("basic block should be a list of 2 or 3 elements");
    }

    const Result<BlockId, ErrMsg> bid = parseBlockId(elems[0]);
    if (isErr(bid)) { return getErr(bid); }

    std::vector<Statement> stmts;
    if (!isList(elems[1])) { return ErrMsg("basic block's statements"); }
    for (auto s : std::get<List>(elems[1]).children) {
        const Result<Statement, ErrMsg> stmt = parseStmt(s);
        if (isErr(stmt)) { return getErr(stmt); }
        stmts.push_back(getVal(stmt));
    }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 3) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[2]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return BasicBlock(getVal(bid), stmts, md, span(s));
}

Result<std::vector<BasicBlock>, ErrMsg> parseFunctionBlocks(SExpr s) {
    if (!isList(s)) { return notAList("function's basic blocks"); }
    std::vector<BasicBlock> blocks;
    for (auto rawBlock : std::get<List>(s).children) {
        const Result<BasicBlock, ErrMsg> block = parseBasicBlock(rawBlock);
        if (isErr(block)) { return getErr(block); }
        blocks.push_back(getVal(block));
    }

    return blocks;
}

Result<std::tuple<std::vector<Param>, std::optional<LocalId>>, ErrMsg>
parseFunctionParams(SExpr s) {
    if (!isList(s)) { return notAList("function parameter list"); }
    std::vector<Param> params;
    std::optional<LocalId> varargParam = std::nullopt;
    const SExprSeq rawParams = std::get<List>(s).children;
    for (size_t i = 0; i < rawParams.size(); ++i) {
        const SExpr rawParam = rawParams[i];
        const Result<Param, ErrMsg> param = parseParam(rawParam);
        if (!isErr(param)) { params.push_back(getVal(param)); }
        else {
            assert(isErr(param));
            if (i == rawParams.size() - 1) {
                // We might have a vararg parameter.
                const Result<LocalId, ErrMsg> lidr = parseLocalId(rawParam);
                if (!isErr(lidr)) {
                    const LocalId lid = getVal(lidr);
                    // 5: 1 for %, 1+ for name, 3 for ...
                    if (
                        lid.id.size() >= 5 &&
                        lid.id.substr(lid.id.size() - 3, lid.id.size()) == "..."
                    ) { varargParam.emplace(lid); }
                    else { return ErrMsg("expected param or vararg param"); }
                } else { return ErrMsg("expected param or vararg param"); }
            } else {
                // No, we expected a parameter and got an error.
                return ErrMsg(getErr(param));
            }
        }
    }

    return std::make_tuple(params, varargParam);
}

Result<TypeAlias, ErrMsg> parseTypeAlias(SExpr s) {
    if (!isList(s)) { return notAList("type alias"); }

    SExprSeq elems = std::get<List>(s).children;
    if (elems.size() != 2 && elems.size() != 3) {
        return ErrMsg("type alias should be a 2-3 size list");
    }

    const Result<TypeId, ErrMsg> tid = parseTypeId(elems[0]);
    if (isErr(tid)) { return getErr(tid); }

    std::optional<Type> type;
    if (atomEq(elems[1], "opaque")) { type = std::nullopt; }
    else {
        const Result<Type, ErrMsg> typer = parseType(elems[1]);
        if (isErr(typer)) { return ErrMsg("expected type or opaque"); }
        type.emplace(getVal(typer));
    }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 3) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[2]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return TypeAlias(getVal(tid), type, md, span(s));
}

Result<std::vector<TypeAlias>, ErrMsg> parseTypes(SExpr s) {
    if (!isList(s)) { return notAList("top-level type aliases"); }

    SExprSeq elems = std::get<List>(s).children;
    if (elems.empty() || !atomEq(elems[0], "types")) {
        return ErrMsg("expected types kw");
    }

    std::vector<TypeAlias> tas;
    // [0] is 'types', so ignore it.
    for (auto it = elems.cbegin() + 1; it != elems.cend(); ++it) {
        const Result<TypeAlias, ErrMsg> ta = parseTypeAlias(*it);
        if (isErr(ta)) { return getErr(ta); }
        tas.push_back(getVal(ta));
    }

    return tas;
}

Result<Function, ErrMsg> parseFunction(SExpr s) {
    if (!isList(s)) { return notAList("function"); }

    const SExprSeq elems = std::get<List>(s).children;
    if (elems.size() != 4 && elems.size() != 5) {
        return ErrMsg("function needs 3-5 elems");
    }

    const Result<GlobalId, ErrMsg> gidr = parseGlobalId(elems[0]);
    if (isErr(gidr)) { return getErr(gidr); }
    const GlobalId gid = getVal(gidr);

    Result<std::tuple<std::vector<Param>, std::optional<LocalId>>, ErrMsg>
    paramsr = parseFunctionParams(elems[1]);
    if (isErr(paramsr)) { return getErr(paramsr); }
    const std::vector<Param> params = std::get<0>(getVal(paramsr));
    const std::optional<LocalId> vaParam = std::get<1>(getVal(paramsr));

    const Result<Type, ErrMsg> typer = parseType(elems[2]);
    if (isErr(typer)) { return getErr(typer); }
    const Type type = getVal(typer);

    std::optional<std::vector<BasicBlock>> bbs;
    if (atomEq(elems[3], "opaque")) { bbs = std::nullopt; }
    else {
        const Result<std::vector<BasicBlock>, ErrMsg> bbsr =
            parseFunctionBlocks(elems[3]);
        if (isErr(bbsr)) { return ErrMsg("expected basic blocks or opaque"); }
        bbs.emplace(getVal(bbsr));
    }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 5) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[4]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Function(gid, params, vaParam, type, bbs, md, span(s));
}

Result<std::vector<Function>, ErrMsg> parseFunctions(SExpr s) {
    if (!isList(s)) { return notAList("top-level functions"); }

    const SExprSeq elems = std::get<List>(s).children;
    if (elems.size() < 1 || !atomEq(elems[0], "functions")) {
        return ErrMsg("expected functions kw");
    }

    std::vector<Function> functions;
    // [0] is 'functions', so ignore it.
    for (auto it = elems.cbegin() + 1; it != elems.cend(); ++it) {
        const Result<Function, ErrMsg> function = parseFunction(*it);
        if (isErr(function)) { return ErrMsg(getErr(function)); }
        functions.push_back(getVal(function));
    }

    return functions;
}

Result<Program, ErrMsg> parseProgram(SExprSeq ss) {
    auto it = ss.cbegin();

    if (it == ss.cend()) { return ErrMsg("expected preamble"); }
    const Result<Preamble, ErrMsg> preamble = parsePreamble(*it);
    if (isErr(preamble)) { return getErr(preamble); }

    ++it;
    if (it == ss.cend()) { return ErrMsg("expected types"); }
    const Result<std::vector<TypeAlias>, ErrMsg> types = parseTypes(*it);
    if (isErr(types)) { return getErr(types); }

    ++it;
    if (it == ss.cend()) { return ErrMsg("expected variables"); }
    const Result<std::vector<Variable>, ErrMsg> variables = parseVariables(*it);
    if (isErr(variables)) { return getErr(variables); }

    ++it;
    if (it == ss.cend()) { return ErrMsg("expected functions"); }
    const Result<std::vector<Function>, ErrMsg> functions = parseFunctions(*it);
    if (isErr(functions)) { return getErr(functions); }

    ++it;
    MaybeMetadata md = std::nullopt;
    if (it != ss.cend()) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));

        if (it + 1 != ss.cend()) { return ErrMsg("extra elements to program"); }
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

Result<Program, ErrMsg> parse(InputStream &stream) {
    SExprs::ParseResult<SExprs::SExprSeq> ssr = SExprs::parseSeq(stream);
    if (std::holds_alternative<SExprs::Err>(ssr)) {
        return Result<Program, ErrMsg>(std::get<SExprs::Err>(ssr));
    }

    SExprs::SExprSeq ss = std::get<SExprs::SExprSeq>(ssr);
    return parseProgram(ss);
}

Result<Program, ErrMsg> parseFile(const std::string filename) {
    std::ifstream fs;
    fs.open(filename);

    if (!fs.is_open()) {
        return ErrMsg(strerror(errno));
    }

    InputStream is = InputStream(fs);
    return parse(is);
}

Result<Program, ErrMsg> parseString(const std::string str) {
    std::istringstream ss = std::istringstream(str);
    InputStream is = InputStream(ss);
    return parse(is);
}

}  // namespace SVFIR
