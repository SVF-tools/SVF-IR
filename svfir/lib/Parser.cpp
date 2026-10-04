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

/// So we can use with std::all_of. It gets confused using std::isdigit
/// directly due to overloads.
bool isDigit(const char c) { return std::isdigit(c); }

/// Error parsing expected because we need, and don't have, a list.
ErrMsg notAList(const std::string &expected) {
    return ErrMsg("Expected a list when trying to parse " + expected + ".");
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

/// <md>
Result<Metadata, ErrMsg> parseMetadata(const SExpr s) {
    if (!isList(s)) { return notAList("metadata"); }
    const List list = std::get<List>(s);

    if (list.children.size() != 2) {
        return ErrMsg("md must be a list of 2 elems");
    }

    if (!atomEq(list.children[0], "md")) { return ErrMsg("expected md"); }

    // list.children[1] is by construction an sexpr.
    return Metadata(list.children[1], span(list.children[1]));
}

/// <tid>
Result<TypeId, ErrMsg> parseTypeId(const SExpr s) {
    if (!isAtom(s)) { return ErrMsg("expected atom for type id"); }
    const std::string text = std::get<Atom>(s).val;
    if (text.size() < 2) { return ErrMsg("type id invalid (too short)"); }
    if (text[0] != '~') { return ErrMsg("type id must start with %"); }
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
            else { return ErrMsg("bad width for int"); }
        } else if (!atom.val.empty() && atom.val[0] == 'f') {
            using Kind = FloatType::Kind;
            // + 1 to skip the 'f'.
            const int width = std::atoi(atom.val.c_str() + 1);
            if (width == 16) { return Type(FloatType(Kind::F16, sp)); }
            else if (width == 32) { return Type(FloatType(Kind::F32, sp)); }
            else if (width == 64) { return Type(FloatType(Kind::F64, sp)); }
            else if (width == 128) { return Type(FloatType(Kind::F128, sp)); }
            else { return ErrMsg("bad width for float"); }
        } else if (!atom.val.empty() && atom.val[0] == '~') {
            const Result<TypeId, ErrMsg> id = parseTypeId(s);
            if (isErr(id)) { return getErr(id); }
            return Type(getVal(id));
        } else { return ErrMsg("invalid type"); }
    } else {
        assert(isList(s));
        const List list = std::get<List>(s);
        if (
            list.children.size() != 2 ||
            !atomEq(list.children[0], "agg") ||
            !isAtom(list.children[1])
        ) { return ErrMsg("expected agg type (list of 2 elements)"); }
        const std::string slotsStr =
            std::get<Atom>(list.children[1]).val;
        if (!std::all_of(slotsStr.begin(), slotsStr.end(), isDigit)) {
            // TODO: what if it starts with 0
            return ErrMsg("seq slots not positive number");
        }
        errno = 0;
        const long long slots =
            std::strtoull(slotsStr.c_str(), nullptr, 10);
        if (errno != 0 || slots > UINT64_MAX) {
            return ErrMsg("seq type slots too large");
        }

        return Type(AggType(slots, sp));
    }
}

/// <lid>
Result<LocalId, ErrMsg> parseLocalId(const SExpr s) {
    if (!isAtom(s)) { return ErrMsg("expected atom for local id"); }
    const std::string text = std::get<Atom>(s).val;
    if (text.size() < 2) { return ErrMsg("local id invalid (too short)"); }
    if (text[0] != '%') { return ErrMsg("local id must start with %"); }
    return LocalId(text, span(s));
}

/// <gid>
Result<GlobalId, ErrMsg> parseGlobalId(const SExpr s) {
    if (!isAtom(s)) { return ErrMsg("expected atom for global id"); }
    const std::string text = std::get<Atom>(s).val;
    if (text.size() < 2) { return ErrMsg("global id invalid (too short)"); }
    if (text[0] != '@') { return ErrMsg("global id must start with %"); }
    return GlobalId(text, span(s));
}

/// <bid>
Result<BlockId, ErrMsg> parseBlockId(const SExpr s) {
    if (!isAtom(s)) { return ErrMsg("expected atom for block id"); }
    const std::string text = std::get<Atom>(s).val;
    if (text.size() < 2) { return ErrMsg("block id invalid (too short)"); }
    if (text[0] != '!') { return ErrMsg("block id must start with %"); }
    return BlockId(text, span(s));
}

Result<VarId, ErrMsg> parseVarId(const SExpr s) {
    // Error check here so we know parseLocalId/parseGlobalId will pass.
    if (!isAtom(s)) { return ErrMsg("expected atom for type id"); }
    const std::string text = std::get<Atom>(s).val;
    if (text.size() < 2) { return ErrMsg("type id invalid (too short)"); }
    if (text[0] == '%') { return getVal(parseLocalId(s)); }
    else if (text[0] == '@') { return getVal(parseGlobalId(s)); }
    else { return ErrMsg("var id must be local or global (% or @)."); }
}

Result<TypedId, ErrMsg> parseTypedId(const SExpr s) {
    if (!isList(s)) { return notAList("typed (local) ID"); }

    const List list = std::get<List>(s);
    if (list.children.size() != 2) {
        return ErrMsg("typed lid must be a list of two elements");
    }

    const Result<LocalId, ErrMsg> lid = parseLocalId(list.children[0]);
    if (isErr(lid)) { return getErr(lid); }

    const Result<Type, ErrMsg> type = parseType(list.children[1]);
    if (isErr(type)) { return getErr(type); }

    return TypedId(getVal(lid), getVal(type), span(s));
}

/// <param>
Result<Param, ErrMsg> parseParam(const SExpr s) {
    if (!isList(s)) { return notAList("Param"); }

    const List list = std::get<List>(s);
    if (!(list.children.size() == 2 || list.children.size() == 3)) {
        return ErrMsg("param is 2 or 3 elements");
    }

    const Result<LocalId, ErrMsg> id = parseLocalId(list.children[0]);
    if (isErr(id)) { return getErr(id); }

    const Result<Type, ErrMsg> type = parseType(list.children[1]);
    if (isErr(type)) { return getErr(type); }

    MaybeMetadata md = std::nullopt;
    if (list.children.size() == 3) {
        Result<Metadata, ErrMsg> mdr = parseMetadata(list.children[2]);
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
        const std::string text = std::get<Atom>(s).val;
        if (text.empty()) { return ErrMsg("empty constant"); }
        else if (text == "null") { return Constant(NullConstant(sp)); }
        else if (text == "true") { return Constant(BoolConstant(true, sp)); }
        else if (text == "false") { return Constant(BoolConstant(false, sp)); }
        else if (text == "-inf" || text == "+inf" || text == "nan") {
            return Constant(FloatConstant(text, sp));
        } else { // Try for an int or numeric float.
            const auto numStart =
                text[0] == '-' ? text.cbegin() + 1 : text.cbegin();
            if (std::all_of(numStart, text.cend(), isDigit)) {
                return Constant(IntConstant(text, sp));
            }

            const auto dotPos = std::find(numStart, text.cend(), '.');
            if (dotPos == text.cend()) {
                return ErrMsg("float constant needs a .");
            }
            if (dotPos + 1 == text.cend()) {
                return ErrMsg("float constant needs digit(s) after .");
            }

            auto it = dotPos + 1;
            for (; it != text.cend(); ++it) { }
            if (it != text.cend()) {
                if (*it != 'e') { return ErrMsg("float expected e"); }
                ++it;
                if (*it == '-') { ++it; }
                if (!std::all_of(it, text.cend(), isDigit)) {
                    return ErrMsg("exponent must be digits");
                }
            }

            return Constant(FloatConstant(text, sp));
        }
    } else {
        assert(isList(s));
        SExprSeq elems = std::get<List>(s).children;
        if (elems.empty()) {
            return ErrMsg("seq const needs at least one elem");
        }
        if (!atomEq(elems[0], "seq")) { return ErrMsg("expected seq"); }

        std::vector<Val> vals;
        for (auto it = elems.cbegin() + 1; it != elems.cend(); ++it) {
            const Result<TypedConstant, ErrMsg> typedConst = parseTypedConst(*it);
            if (isErr(typedConst)) {
                const Result<VarId, ErrMsg> varId = parseVarId(*it);
                if (isErr(varId)) {
                    return ErrMsg("expected typed const or var id");
                } else { vals.push_back(getVal(varId)); }
            } else { vals.push_back(getVal(typedConst)); }
        }

        return Constant(SeqConstant(vals, sp));
    }
}

Result<TypedConstant, ErrMsg> parseTypedConst(SExpr s) {
    if (!isList(s)) { return notAList("typed constant"); }

    const SExprSeq elems = std::get<List>(s).children;
    if (elems.size() != 2) { return ErrMsg("typed const needs 2 elems"); }

    const Result<Constant, ErrMsg> constant = parseConst(elems[0]);
    if (isErr(constant)) { return ErrMsg("expected constant"); }

    const Result<Type, ErrMsg> type = parseType(elems[1]);
    if (isErr(type)) { return ErrMsg("expected type"); }

    return TypedConstant(getVal(constant), getVal(type), span(s));
}

Result<Preamble, ErrMsg> parsePreamble(SExpr s) {
    if (!isList(s)) { return notAList("preamble"); }

    const SExprSeq elems = std::get<List>(s).children;
    auto it = elems.cbegin();

    // Preamble keyword.
    if (it == elems.cend()) { return listCutShort("preamble", "keyword"); }
    if (!atomEq(*it, "preamble")) {
        return ErrMsg("expected preamble keyword");
    }

    // Version.
    ++it;
    if (it == elems.cend()) { return listCutShort("preamble", "version"); }
    if (!isList(*it)) { return notAList("version"); }
    const List versionList = std::get<List>(*it);
    if (
        versionList.children.size() != 2 ||
        !isAtom(versionList.children[0]) ||
        !isAtom(versionList.children[1])
    ) { return ErrMsg("version should be a list of 2 atoms"); }
    if (!atomEq(versionList.children[0], "version")) {
        return ErrMsg("missing version");
    }
    const std::string versionStr =
        std::get<Atom>(versionList.children[1]).val;
    const size_t dotPos = versionStr.find(".", 0);
    if (
        dotPos == std::string::npos ||   // Not found.
        dotPos == 0 ||                   // No major version.
        dotPos == versionStr.size() - 1  // No minor version.
    ) { return ErrMsg("version expected to be [major].[minor]"); }
    const std::string majorStr = versionStr.substr(0, dotPos);
    const std::string minorStr =
        versionStr.substr(dotPos + 1, versionStr.size());
    if (
        !std::all_of(majorStr.cbegin(), majorStr.cend(), isDigit) ||
        !std::all_of(minorStr.cbegin(), minorStr.cend(), isDigit)
     ) { return ErrMsg("non-number in version"); }
    const long majorVersion = std::strtoul(minorStr.c_str(), nullptr, 10);
    if (majorVersion > UINT16_MAX) { return ErrMsg("major version too large"); }
    const long minorVersion = std::strtoul(majorStr.c_str(), nullptr, 10);
    if (minorVersion > UINT16_MAX) { return ErrMsg("minor version too large"); }
    const Version version = Version(majorVersion, minorVersion);

    // Source.
    ++it;
    if (it == elems.cend()) { return listCutShort("preamble", "source"); }
    if (!isList(*it)) { return notAList("source"); }
    const List sourceList = std::get<List>(*it);
    if (
        sourceList.children.size() != 2 ||
        !isAtom(sourceList.children[0]) ||
        !isAtom(sourceList.children[1])
    ) { return ErrMsg("source should be a list of 2 atoms"); }
    if (!atomEq(sourceList.children[0], "source")) {
        return ErrMsg("missing source");
    }
    const std::string source =
        std::get<Atom>(sourceList.children[1]).val;

    // Metadata.
    MaybeMetadata md = std::nullopt;
    ++it;
    if (it != elems.cend()) {
        Result<Metadata, ErrMsg> mdr = parseMetadata(*it);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Preamble(version, source, md, span(s));
}

Result<GVal, ErrMsg> parseGVal(SExpr s) {
    // Try for <gid>.
    if (isAtom(s)) {
        const std::string text = std::get<Atom>(s).val;
        if (!text.empty() && text[0] == '@') {
            const Result<GlobalId, ErrMsg> gid = parseGlobalId(s);
            if (!isErr(gid)) { return GVal(getVal(gid)); }
        }
    }

    // Try for <typed-const>
    const Result<TypedConstant, ErrMsg> tc = parseTypedConst(s);
    if (isErr(tc)) {
        return ErrMsg("expected gval, not const: " + getErr(tc) + "(nor gid)");
    }
    return GVal(getVal(tc));
}

Result<Val, ErrMsg> parseVal(SExpr s) {
    if (isAtom(s)) {
        const Result<VarId, ErrMsg> vid = parseVarId(s);
        if (isErr(vid)) { return ErrMsg("expected val" + getErr(vid)); }
        return Val(getVal(vid));
    } else {
        assert(isList(s));
        const Result<TypedConstant, ErrMsg> tc = parseTypedConst(s);
        if (isErr(tc)) { return ErrMsg("expected val" + getErr(tc)); }
        return Val(getVal(tc));
    }
    assert(false);
}

Result<PVal, ErrMsg> parsePVal(SExpr s) {
    const Result<VarId, ErrMsg> vid = parseVarId(s);
    if (!isErr(vid)) { return PVal(getVal(vid)); }
    const Result<Constant, ErrMsg> constant = parseConst(s);
    if (
        isErr(constant) ||
        !std::holds_alternative<NullConstant>(getVal(constant))
    ) { return ErrMsg("expected pval"); }
    return PVal(std::get<NullConstant>(getVal(constant)));
}

Result<IVal, ErrMsg> parseIVal(SExpr s) {
    const Result<VarId, ErrMsg> vid = parseVarId(s);
    if (!isErr(vid)) { return IVal(getVal(vid)); }
    const Result<Constant, ErrMsg> constant = parseConst(s);
    if (
        isErr(constant) ||
        !std::holds_alternative<IntConstant>(getVal(constant))
    ) { return ErrMsg("expected pval"); }
    return IVal(std::get<IntConstant>(getVal(constant)));
}

Result<Variable, ErrMsg> parseVariable(SExpr s) {
    if (!isList(s)) { return notAList("variable"); }

    const SExprSeq elems = std::get<List>(s).children;
    if (elems.size() != 2 && elems.size() != 3) {
        return ErrMsg("variable needs 2-3 elems");
    }

    const Result<GlobalId, ErrMsg> gidr = parseGlobalId(elems[0]);
    if (isErr(gidr)) { return ErrMsg("expected gid"); }
    const GlobalId gid = getVal(gidr);

    std::optional<GVal> gval;
    if (atomEq(elems[1], "opaque")) { gval = std::nullopt; }
    else {
        const Result<GVal, ErrMsg> gvalr = parseGVal(elems[1]);
        if (isErr(gvalr)) { return ErrMsg("expected gval or opaque"); }
        gval.emplace(getVal(gvalr));
    }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 3) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[2]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Variable(gid, gval, md, span(s));
}

Result<std::vector<Variable>, ErrMsg> parseVariables(SExpr s) {
    if (!isList(s)) { return notAList("variables"); }

    const SExprSeq elems = std::get<List>(s).children;
    if (
        elems.size() < 1 ||
        !atomEq(elems[0], "variables")
    ) { return ErrMsg("expected variables kw"); }

    std::vector<Variable> variables;
    // [0] is 'variables', so ignore it.
    for (auto it = elems.cbegin() + 1; it != elems.cend(); ++it) {
        const Result<Variable, ErrMsg> variable = parseVariable(*it);
        if (isErr(variable)) { return ErrMsg(getErr(variable)); }
        variables.push_back(getVal(variable));
    }

    return variables;
}

Result<Statement, ErrMsg> parsePhiStmt(List l) {
    const SExprSeq elems = l.children;
    assert(atomEq(elems[0], "phi"));
    if (elems.size() != 3 && elems.size() != 4) {
        return ErrMsg("phi list should be length 3-4");
    }

    const Result<LocalId, ErrMsg> lid = parseLocalId(elems[1]);
    if (isErr(lid)) { return ErrMsg("expected local id"); }

    if (!isList(elems[2])) { return notAList("phi operands"); }
    std::vector<PhiStmt::Operand> operands;
    for (auto s : std::get<List>(elems[2]).children) {
        if (!isList(s)) { return notAList("phi operand"); }
        const List pair = std::get<List>(s);
        if (pair.children.size() != 2) { return ErrMsg("expected pair"); }
        const Result<Val, ErrMsg> val = parseVal(pair.children[0]);
        if (isErr(val)) { return getErr(val); }
        const Result<BlockId, ErrMsg> bid = parseBlockId(pair.children[1]);
        if (isErr(bid)) { return getErr(bid); }
        operands.push_back(PhiStmt::Operand(getVal(val), getVal(bid)));
    }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 4) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[3]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(PhiStmt(getVal(lid), operands, md, span(l)));
}

Result<Statement, ErrMsg> parseCallStmt(List l) {
    const SExprSeq elems = l.children;
    assert(atomEq(elems[0], "call"));
    if (elems.size() != 4 && elems.size() != 5) {
        return ErrMsg("call list should be length 4-5");
    }

    std::optional<LocalId> lid = std::nullopt;
    if (!atomEq(elems[1], "void")) {
        const Result<LocalId, ErrMsg> lidr = parseLocalId(elems[1]);
        if (isErr(lidr)) { return getErr(lidr); }
        lid.emplace(getVal(lidr));
    }

    const Result<PVal, ErrMsg> callee = parsePVal(elems[2]);
    if (isErr(callee)) { return getErr(callee); }

    if (!isList(elems[3])) { return notAList("call arguments"); }
    std::vector<Val> args;
    for (auto s : std::get<List>(elems[3]).children) {
        const Result<Val, ErrMsg> val = parseVal(s);
        if (isErr(val)) { return getErr(val); }
        args.push_back(getVal(val));
    }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 5) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[4]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(CallStmt(lid, getVal(callee), args, md, span(l)));
}

Result<Statement, ErrMsg> parseBrStmt(List l) {
    const SExprSeq elems = l.children;
    assert(atomEq(elems[0], "br"));
    if (elems.size() != 2 && elems.size() != 3) {
        return ErrMsg("br list should be length 2-3");
    }

    const Result<BlockId, ErrMsg> target = parseBlockId(elems[1]);
    if (isErr(target)) { return getErr(target); }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 3) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[2]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(BrStmt(getVal(target), md, span(l)));
}

Result<Statement, ErrMsg> parseBrifStmt(List l) {
    const SExprSeq elems = l.children;
    assert(atomEq(elems[0], "brif"));
    if (elems.size() != 4 && elems.size() != 5) {
        return ErrMsg("brif list should be length 4-5");
    }

    const Result<Val, ErrMsg> val = parseVal(elems[1]);
    if (isErr(val)) { return getErr(val); }

    const Result<BlockId, ErrMsg> ifTarget = parseBlockId(elems[2]);
    if (isErr(ifTarget)) { return getErr(ifTarget); }

    const Result<BlockId, ErrMsg> elseTarget = parseBlockId(elems[3]);
    if (isErr(elseTarget)) { return getErr(elseTarget); }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 5) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[4]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(
        BrifStmt(getVal(val), getVal(ifTarget), getVal(elseTarget), md, span(l))
    );
}

Result<Statement, ErrMsg> parseRet(List l) {
    const SExprSeq elems = l.children;
    assert(atomEq(elems[0], "ret"));
    if (elems.size() < 1 || elems.size() > 3) {
        return ErrMsg("ret list should be length 1-3");
    }

    if (elems.size() == 1) {
        return Statement(RetStmt(std::nullopt, std::nullopt, span(l)));
    } else if (elems.size() == 2) {
        const Result<Val, ErrMsg> val = parseVal(elems[1]);
        if (isErr(val)) {
            const Result<Metadata, ErrMsg> md = parseMetadata(elems[1]);
            if (isErr(md)) { return ErrMsg("expected val or metadata"); }
            return Statement(RetStmt(std::nullopt, getVal(md), span(l)));
        } else {
            return Statement(RetStmt(getVal(val), std::nullopt, span(l)));
        }
    } else {
        assert(elems.size() == 3);

        const Result<Val, ErrMsg> val = parseVal(elems[1]);
        if (isErr(val)) { return getErr(val); }

        const Result<Metadata, ErrMsg> md = parseMetadata(elems[2]);
        if (isErr(md)) { return getErr(md); }

        return Statement(RetStmt(getVal(val), getVal(md), span(l)));
    }
    assert(false);
}

Result<Statement, ErrMsg> parseCmpStmt(List l) {
    const SExprSeq elems = l.children;
    assert(atomEq(elems[0], "cmp"));
    if (elems.size() != 5 && elems.size() != 6) {
        return ErrMsg("cmp list should be length 5-6");
    }

    const Result<LocalId, ErrMsg> lid = parseLocalId(elems[1]);
    if (isErr(lid)) { return getErr(lid); }

    CmpStmt::Operator op;
    if (atomEq(elems[2], "<")) { op = CmpStmt::Operator::LT; }
    else if (atomEq(elems[2], "<=")) { op = CmpStmt::Operator::LE; }
    else if (atomEq(elems[2], ">")) { op = CmpStmt::Operator::GT; }
    else if (atomEq(elems[2], ">=")) { op = CmpStmt::Operator::GE; }
    else if (atomEq(elems[2], "=")) { op = CmpStmt::Operator::EQ; }
    else if (atomEq(elems[2], "!=")) { op = CmpStmt::Operator::NEQ; }
    else { return ErrMsg("expected cmp operator"); }

    const Result<Val, ErrMsg> v1 = parseVal(elems[3]);
    if (isErr(v1)) { return getErr(v1); }

    const Result<Val, ErrMsg> v2 = parseVal(elems[4]);
    if (isErr(v2)) { return getErr(v2); }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 6) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[5]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(
        CmpStmt(getVal(lid), op, getVal(v1), getVal(v2), md, span(l))
    );
}

Result<Statement, ErrMsg> parseAllocStmt(List l) {
    const SExprSeq elems = l.children;
    assert(atomEq(elems[0], "alloc"));
    if (elems.size() != 3 && elems.size() != 4) {
        return ErrMsg("alloc list should be length 3-4");
    }

    AllocStmt::Kind kind;
    if (atomEq(elems[1], "heap")) { kind = AllocStmt::Kind::HEAP; }
    else if (atomEq(elems[1], "stack")) { kind = AllocStmt::Kind::STACK; }
    else { return ErrMsg("expected heap or stack"); }

    const Result<LocalId, ErrMsg> lid = parseLocalId(elems[2]);
    if (isErr(lid)) { return getErr(lid); }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 4) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[3]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(AllocStmt(kind, getVal(lid), md, span(l)));
}

Result<Statement, ErrMsg> parseStoreStmt(List l) {
    const SExprSeq elems = l.children;
    assert(atomEq(elems[0], "store"));
    if (elems.size() != 3 && elems.size() != 4) {
        return ErrMsg("store list should be length 3-4");
    }

    const Result<Val, ErrMsg> val = parseVal(elems[1]);
    if (isErr(val)) { return getErr(val); }

    const Result<PVal, ErrMsg> dst = parsePVal(elems[2]);
    if (isErr(dst)) { return getErr(dst); }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 4) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[3]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(StoreStmt(getVal(val), getVal(dst), md, span(l)));
}

Result<Statement, ErrMsg> parseLoadStmt(List l) {
    const SExprSeq elems = l.children;
    assert(atomEq(elems[0], "load"));
    if (elems.size() != 3 && elems.size() != 4) {
        return ErrMsg("load list should be length 3-4");
    }

    const Result<TypedId, ErrMsg> tlid = parseTypedId(elems[1]);
    if (isErr(tlid)) { return getErr(tlid); }

    const Result<PVal, ErrMsg> src = parsePVal(elems[2]);
    if (isErr(src)) { return getErr(src); }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 4) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[3]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(LoadStmt(getVal(tlid), getVal(src), md, span(l)));
}

Result<Statement, ErrMsg> parseFieldStmt(List l) {
    const SExprSeq elems = l.children;
    assert(atomEq(elems[0], "field"));
    if (elems.size() != 4 && elems.size() != 5) {
        return ErrMsg("field list should be length 4-5");
    }

    const Result<LocalId, ErrMsg> lid = parseLocalId(elems[1]);
    if (isErr(lid)) { return getErr(lid); }

    const Result<PVal, ErrMsg> src = parsePVal(elems[2]);
    if (isErr(src)) { return getErr(src); }

    const Result<IVal, ErrMsg> index = parseIVal(elems[3]);
    if (isErr(index)) { return getErr(index); }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 5) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[4]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(
        FieldStmt(getVal(lid), getVal(src), getVal(index), md, span(l))
    );
}

Result<Statement, ErrMsg> parseAddMulSubStmt(List l) {
    const SExprSeq elems = l.children;
    if (elems.size() != 4 && elems.size() != 5) {
        return ErrMsg("add/mul/sub list should be length 4-5");
    }

    const Result<LocalId, ErrMsg> lid = parseLocalId(elems[1]);
    if (isErr(lid)) { return getErr(lid); }

    const Result<Val, ErrMsg> v1 = parseVal(elems[2]);
    if (isErr(v1)) { return getErr(v1); }

    const Result<Val, ErrMsg> v2 = parseVal(elems[3]);
    if (isErr(v2)) { return getErr(v2); }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 5) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[4]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    if (atomEq(elems[0], "add")) {
        return Statement(
            AddStmt(getVal(lid), getVal(v1), getVal(v2), md, span(l))
        );
    } else if (atomEq(elems[0], "sub")) {
        return Statement(
            SubStmt(getVal(lid), getVal(v1), getVal(v2), md, span(l))
        );
    } else if (atomEq(elems[0], "mul")) {
        return Statement(
            MulStmt(getVal(lid), getVal(v1), getVal(v2), md, span(l))
        );
    } else {
        assert(false);
    }
}

Result<Statement, ErrMsg> parseDivRemStmt(List l) {
    const SExprSeq elems = l.children;
    if (elems.size() != 5 && elems.size() != 6) {
        return ErrMsg("div/rem list should be length 5-6");
    }

    bool sign;
    if (atomEq(elems[1], "unsigned")) { sign = false; }
    else if (atomEq(elems[1], "signed")) { sign = true; }
    else { return ErrMsg("expected unsigned or signed"); }

    const Result<LocalId, ErrMsg> lidr = parseLocalId(elems[2]);
    if (isErr(lidr)) { return getErr(lidr); }
    const LocalId lid = getVal(lidr);

    const Result<Val, ErrMsg> leftr = parseVal(elems[3]);
    if (isErr(leftr)) { return getErr(leftr); }
    const Val left = getVal(leftr);

    const Result<Val, ErrMsg> rightr = parseVal(elems[4]);
    if (isErr(rightr)) { return getErr(rightr); }
    const Val right = getVal(rightr);

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 6) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[5]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    if (atomEq(elems[0], "div")) {
        const DivStmt::Kind kind =
            sign ? DivStmt::Kind::SIGNED : DivStmt::Kind::UNSIGNED;
        return Statement(DivStmt(kind, lid, left, right, md, span(l)));
    } else if (atomEq(elems[0], "rem")) {
        const RemStmt::Kind kind =
            sign ? RemStmt::Kind::SIGNED : RemStmt::Kind::UNSIGNED;
        return Statement(RemStmt(kind, lid, left, right, md, span(l)));
    } else {
        assert(false);
    }
}

Result<Statement, ErrMsg> parseNotStmt(List l) {
    const SExprSeq elems = l.children;
    if (elems.size() != 3 && elems.size() != 4) {
        return ErrMsg("not list should be length 3-4");
    }

    const Result<LocalId, ErrMsg> lid = parseLocalId(elems[1]);
    if (isErr(lid)) { return getErr(lid); }

    const Result<Val, ErrMsg> val = parseVal(elems[2]);
    if (isErr(val)) { return getErr(val); }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 4) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[5]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(NotStmt(getVal(lid), getVal(val), md, span(l)));
}

Result<Statement, ErrMsg> parseAndOrXorShiftlStmt(List l) {
    const SExprSeq elems = l.children;
    if (elems.size() != 4 && elems.size() != 5) {
        return ErrMsg("not list should be length 4-5");
    }

    const Result<LocalId, ErrMsg> lid = parseLocalId(elems[1]);
    if (isErr(lid)) { return getErr(lid); }

    const Result<Val, ErrMsg> left = parseVal(elems[2]);
    if (isErr(left)) { return getErr(left); }

    const Result<Val, ErrMsg> right = parseVal(elems[3]);
    if (isErr(right)) { return getErr(right); }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 5) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[4]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    if (atomEq(elems[0], "and")) {
        return Statement(
            AndStmt(getVal(lid), getVal(left), getVal(right), md, span(l))
        );
    } else if (atomEq(elems[0], "or")) {
        return Statement(
            OrStmt(getVal(lid), getVal(left), getVal(right), md, span(l))
        );
    } else if (atomEq(elems[0], "xor")) {
        return Statement(
            XorStmt(getVal(lid), getVal(left), getVal(right), md, span(l))
        );
    } else if (atomEq(elems[0], "shiftl")) {
        return Statement(
            ShiftlStmt(getVal(lid), getVal(left), getVal(right), md, span(l))
        );
    } else { assert(false); }
}

Result<Statement, ErrMsg> parseShiftrStmt(List l) {
    const SExprSeq elems = l.children;
    if (elems.size() != 5 && elems.size() != 6) {
        return ErrMsg("not list should be length 5-6");
    }

    ShiftrStmt::Kind kind;
    if (atomEq(elems[1], "logical")) { kind = ShiftrStmt::Kind::LOGICAL; }
    else if (atomEq(elems[1], "arithmetic")) {
        kind = ShiftrStmt::Kind::ARITHMETIC;
    } else { return ErrMsg("expected logical or arithmetic"); }

    const Result<LocalId, ErrMsg> lid = parseLocalId(elems[2]);
    if (isErr(lid)) { return getErr(lid); }

    const Result<Val, ErrMsg> left = parseVal(elems[3]);
    if (isErr(left)) { return getErr(left); }

    const Result<Val, ErrMsg> right = parseVal(elems[4]);
    if (isErr(right)) { return getErr(right); }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 6) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[5]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(
        ShiftrStmt(kind, getVal(lid), getVal(left), getVal(right), md, span(l))
    );
}

Result<Statement, ErrMsg> parseAssignStmt(List l) {
    const SExprSeq elems = l.children;
    assert(atomEq(elems[0], "assign"));
    if (elems.size() != 3 && elems.size() != 4) {
        return ErrMsg("assign list should be length 3-4");
    }

    const Result<LocalId, ErrMsg> lid = parseLocalId(elems[1]);
    if (isErr(lid)) { return getErr(lid); }

    const Result<Val, ErrMsg> val = parseVal(elems[2]);
    if (isErr(val)) { return getErr(val); }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 4) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[3]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(AssignStmt(getVal(lid), getVal(val), md, span(l)));
}

Result<Statement, ErrMsg> parseVarargStmt(List l) {
    const SExprSeq elems = l.children;
    assert(atomEq(elems[0], "vararg"));
    if (elems.size() != 3 && elems.size() != 4) {
        return ErrMsg("vararg list should be length 3-4");
    }

    const Result<TypedId, ErrMsg> tlid = parseTypedId(elems[1]);
    if (isErr(tlid)) { return getErr(tlid); }

    const Result<IVal, ErrMsg> val = parseIVal(elems[2]);
    if (isErr(val)) { return getErr(val); }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 4) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[3]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(VarargStmt(getVal(tlid), getVal(val), md, span(l)));
}

Result<Statement, ErrMsg> parseBlackholeStmt(List l) {
    const SExprSeq elems = l.children;
    assert(atomEq(elems[0], "blackhole"));
    if (elems.size() != 2 && elems.size() != 3) {
        return ErrMsg("blackhole list should be length 2-3");
    }

    const Result<TypedId, ErrMsg> tlid = parseTypedId(elems[1]);
    if (isErr(tlid)) { return getErr(tlid); }

    MaybeMetadata md = std::nullopt;
    if (elems.size() == 3) {
        const Result<Metadata, ErrMsg> mdr = parseMetadata(elems[3]);
        if (isErr(mdr)) { return getErr(mdr); }
        md.emplace(getVal(mdr));
    }

    return Statement(BlackholeStmt(getVal(tlid), md, span(l)));
}

Result<Statement, ErrMsg> parseStmt(SExpr s) {
    if (!isList(s)) { return notAList("statement"); }
    const List l = std::get<List>(s);

    if (!isAtom(l.children[0])) { return ErrMsg("expected instr"); }
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
