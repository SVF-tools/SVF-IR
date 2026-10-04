#include <regex>
#include <iostream>

#include "Ast.h"
#include "Ops.h"

// TODO: there are many instances where you need to "just know" there is/isn't
// whitespace.

namespace {

using namespace SVFIR;

const std::string INDENTATION = "  ";

/// Add an INDENTATION to the start of every line.
std::string indent(const std::string &s) {
    static std::regex lineEndRe("\n");

    return std::regex_replace(
        s.empty() ? s : INDENTATION + s,
        lineEndRe,
        "\n" + INDENTATION
    );
}

/// Return a copy of s with whitespace between right parentheses removed.
std::string collapseRParens(const std::string &s) {
    static std::regex tgt("\\s+\\)");
    return std::regex_replace(s, tgt, ")");
}

/// Strings together each element of xs, calling pretty on each (TODO: not
/// expressed in the type), delimited by delim.
template <typename T>
std::string prettyVec(const std::vector<T> &xs, const std::string &delim) {
    std::string s;

    bool first = true;
    for (auto x : xs) {
        if (first) { first = false; }
        else { s += delim; }
        s += pretty(x);
    }

    return s;
}

/// If md has a value returns a pretty string of it preceded by a space,
/// otherwise returns an empty string.
std::string maybePrettyMd(const MaybeMetadata md) {
    return md.has_value() ? " " + pretty(md.value()) : "";
}

std::string par(const std::string &s) {
    return "(" + s + ")";
}

struct Pretty {
    std::string operator()(const Metadata &md) {
        return "(md " + SExprs::toString(md.data) + ")";
    }

    std::string operator()(const VoidType &) { return "void"; }

    std::string operator()(const IntType &it) {
        if (it.kind == IntType::Kind::I8) { return "i8"; }
        if (it.kind == IntType::Kind::I16) { return "i16"; }
        else if (it.kind == IntType::Kind::I32) { return "i32"; }
        else if (it.kind == IntType::Kind::I64) { return "i64"; }
        else if (it.kind == IntType::Kind::I128) { return "i128"; }
        else { assert(false); }
    }

    std::string operator()(const FloatType &ft) {
        if (ft.kind == FloatType::Kind::F16) { return "f16"; }
        else if (ft.kind == FloatType::Kind::F32) { return "f32"; }
        else if (ft.kind == FloatType::Kind::F64) { return "f64"; }
        else if (ft.kind == FloatType::Kind::F128) { return "f128"; }
        else { assert(false); }
    }

    std::string operator()(const PtrType &) { return "ptr"; }

    std::string operator()(const BoolType &) { return "bool"; }

    std::string operator()(const AggType &st) {
        return "(agg " + std::to_string(st.slots) + ")";
    }

    std::string operator()(const TypeId &tid) { return tid.id; }

    std::string operator()(const LocalId &lid) { return lid.id; }

    std::string operator()(const GlobalId &gid) { return gid.id; }

    std::string operator()(const BlockId &bid) { return bid.id; }

    std::string operator()(const TypedId &tid) {
        return "(" + pretty(tid.id) + " " + pretty(tid.type) + ")";
    }

    std::string operator()(const IntConstant &ic) { return ic.val; }

    std::string operator()(const FloatConstant &fc) { return fc.val; }

    std::string operator()(const NullConstant &) { return "null"; }

    std::string operator()(const BoolConstant &bc) {
        return bc.val ? "true" : "false";
    }

    std::string operator()(const SeqConstant &sc) {
        std::string vals;
        for (const Val v : sc.vals) { vals += " " + pretty(v); }
        return par("seq" + vals);
    }

    std::string operator()(const TypedConstant &tc) {
        return "(" + pretty(tc.constant) + " " + pretty(tc.type) + ")";
    }

    std::string operator()(const Preamble &p) {
        return "(preamble\n" +
            indent("(version " + p.version.toString() + ")") + "\n" +
            indent("(source " + p.source + ")") + "\n" +
            (p.md.has_value() ? indent(pretty(p.md.value())) + "\n" : "") +
            ")";
    }

    std::string operator()(const Variable &v) {
        return par(
            pretty(v.id) + " " +
            (v.val.has_value() ? pretty(v.val.value()) : "opaque") +
            maybePrettyMd(v.md)
        );
    }

    std::string operator()(const Param &p) {
        return par(pretty(p.id) + " " + pretty(p.type) + maybePrettyMd(p.md));
    }

    std::string operator()(const Function &f) {
        std::string params = prettyVec(f.params, " ");
        if (f.varargParam.has_value()) {
            if (!f.params.empty()) { params += " "; }
            // The '...' is already part of f.varargParam.
            params += pretty(f.varargParam.value());
        }

        std::string body;
        if (f.body.has_value()) { body = prettyVec(f.body.value(), "\n"); }

        return par(
            pretty(f.id) + " " +
            par(params) + " " +
            pretty(f.type) + " " +
            (f.body.has_value() ? "(\n" + indent(body) + "\n)": "opaque") +
            maybePrettyMd(f.md)
        );
    }

    std::string operator()(const TypeAlias &ta) {
        return par(
            pretty(ta.id) + " " +
            (ta.type.has_value() ? pretty(ta.type.value()) : "opaque") +
            maybePrettyMd(ta.md)
        );
    }

    std::string operator()(const Program &p) {
        auto block = [](const std::string &name, const std::string &block) {
            if (block.empty()) {
                return "(" + name + ")";
            } else {
                return "(" + name + "\n" + indent(block) + "\n)";
            }
        };

        return pretty(p.preamble) + "\n" +
            block("types", prettyVec(p.types, "\n")) + "\n" +
            block("variables", prettyVec(p.variables, "\n")) + "\n" +
            block("functions", prettyVec(p.functions, "\n")) +
            (p.md.has_value() ? "\n" + pretty(p.md.value()) : "");
    }

    std::string operator()(const PhiStmt &p) {
        std::string args;
        bool first = true;
        for (auto o : p.args) {
            if (first) { first = false; }
            else { args += " "; }
            args += par(pretty(o.val) + " " + pretty(o.definingBlock));
        }

        return par("phi " + pretty(p.id) + " " + args + maybePrettyMd(p.md));
    }

    std::string operator()(const CallStmt &c) {
        std::string ret;
        if (c.id.has_value()) { ret = pretty(c.id.value()); }
        else { ret = "void"; }
        std::string args = prettyVec(c.args, " ");
        return par(
            "call " +
            ret + " " +
            pretty(c.callee) + " " +
            par(args) +  // built string has trailing space.
            maybePrettyMd(c.md)
        );
    }

    std::string operator()(const BrStmt &br) {
        return par("br " + pretty(br.target) + maybePrettyMd(br.md));
    }

    std::string operator()(const BrifStmt &brif) {
        return par(
            "brif " +
            pretty(brif.cond) + " " +
            pretty(brif.ifTarget) + " " +
            pretty(brif.elseTarget) +
            maybePrettyMd(brif.md)
        );
    }

    std::string operator()(const RetStmt &ret) {
        const std::string maybeVal =
            ret.val.has_value() ? " " + pretty(ret.val.value()) : "";
        const std::string maybeMd = maybePrettyMd(ret.md);
        return par("ret" + maybeVal + maybeMd);
    }

    std::string operator()(const CmpStmt &cmp) {
        std::string op;
        if (cmp.op == CmpStmt::Operator::LT) { op = "<"; }
        else if (cmp.op == CmpStmt::Operator::LE) { op = "<="; }
        else if (cmp.op == CmpStmt::Operator::GT) { op = ">"; }
        else if (cmp.op == CmpStmt::Operator::GE) { op = ">="; }
        else if (cmp.op == CmpStmt::Operator::EQ) { op = "="; }
        else if (cmp.op == CmpStmt::Operator::NEQ) { op = "!="; }
        else { assert(false); }

        return par(
            "cmp " +
            pretty(cmp.id) + " " +
            op + " " +
            pretty(cmp.left) + " " +
            pretty(cmp.right) + " " +
            maybePrettyMd(cmp.md)
        );
    }

    std::string operator()(const AllocStmt &a) {
        std::string kind;
        if (a.kind == AllocStmt::Kind::HEAP) { kind = "heap"; }
        else if (a.kind == AllocStmt::Kind::STACK) { kind = "stack"; }
        else { assert(false); }
        return par("alloc " + kind + " " + pretty(a.id) + maybePrettyMd(a.md));
    }

    std::string operator()(const StoreStmt &s) {
        return par(
            "store " + pretty(s.val) + " " + pretty(s.dst) + maybePrettyMd(s.md)
        );
    }

    std::string operator()(const LoadStmt &l) {
        return par(
            "load " + pretty(l.tid) + " " + pretty(l.src) + maybePrettyMd(l.md)
        );
    }

    std::string operator()(const FieldStmt &i) {
        return par(
            "field " +
            pretty(i.id) + " " +
            pretty(i.src) + " " +
            pretty(i.index) + " " +
            maybePrettyMd(i.md)
        );
     }

    std::string operator()(const AddStmt &a) {
        return par(
            "add " +
            pretty(a.id) + " " +
            pretty(a.left) + " " +
            pretty(a.right) + " " +
            maybePrettyMd(a.md)
        );
    }

    std::string operator()(const SubStmt &s) {
        return par(
            "sub " +
            pretty(s.id) + " " +
            pretty(s.left) + " " +
            pretty(s.right) + " " +
            maybePrettyMd(s.md)
        );
    }

    std::string operator()(const MulStmt &m) {
        return par(
            "mul " +
            pretty(m.id) + " " +
            pretty(m.left) + " " +
            pretty(m.right) + " " +
            maybePrettyMd(m.md)
        );
    }

    std::string operator()(const DivStmt &d) {
        std::string kind;
        if (d.kind == DivStmt::Kind::UNSIGNED) { kind = "unsigned"; }
        else if (d.kind == DivStmt::Kind::SIGNED) { kind = "signed"; }
        else { assert(false); }

        return par(
            "div " +
            pretty(d.id) + " " +
            pretty(d.left) + " " +
            pretty(d.right) + " " +
            maybePrettyMd(d.md)
        );
    }

    std::string operator()(const RemStmt &r) {
        std::string kind;
        if (r.kind == RemStmt::Kind::UNSIGNED) { kind = "unsigned"; }
        else if (r.kind == RemStmt::Kind::SIGNED) { kind = "signed"; }
        else { assert(false); }

        return par(
            "rem " +
            pretty(r.id) + " " +
            pretty(r.left) + " " +
            pretty(r.right) + " " +
            maybePrettyMd(r.md)
        );
    }

    std::string operator()(const NotStmt &n) {
        return par(
            "not " + pretty(n.id) + " " + pretty(n.val) + maybePrettyMd(n.md)
        );
    }

    std::string operator()(const AndStmt &a) {
        return par(
            "and " +
            pretty(a.id) + " " +
            pretty(a.left) + " " +
            pretty(a.right) + " " +
            maybePrettyMd(a.md)
        );
    }

    std::string operator()(const OrStmt &o) {
        return par(
            "or " +
            pretty(o.id) + " " +
            pretty(o.left) + " " +
            pretty(o.right) + " " +
            maybePrettyMd(o.md)
        );
    }

    std::string operator()(const XorStmt &x) {
        return par(
            "xor " +
            pretty(x.id) + " " +
            pretty(x.left) + " " +
            pretty(x.right) + " " +
            maybePrettyMd(x.md)
        );
    }

    std::string operator()(const ShiftlStmt &s) {
        return par(
            "shiftl " +
            pretty(s.id) + " " +
            pretty(s.left) + " " +
            pretty(s.right) + " " +
            maybePrettyMd(s.md)
        );
    }

    std::string operator()(const ShiftrStmt &s) {
        std::string kind;
        if (s.kind == ShiftrStmt::Kind::LOGICAL) { kind = "logical"; }
        else if (s.kind == ShiftrStmt::Kind::ARITHMETIC) {
            kind = "arithmetic";
        } else { assert(false); }

        return par(
            "shiftl " +
            kind + " " +
            pretty(s.id) + " " +
            pretty(s.left) + " " +
            pretty(s.right) + " " +
            maybePrettyMd(s.md)
        );
    }

    std::string operator()(const AssignStmt &a) {
        return par(
            "assign " + pretty(a.id) + " " + pretty(a.val) + maybePrettyMd(a.md)
        );
    }

    std::string operator()(const VarargStmt &v) {
        return par(
            "vararg " +
            pretty(v.tid) + " " +
            pretty(v.index) + " " +
            maybePrettyMd(v.md)
        );
    }

    std::string operator()(const BlackholeStmt &b) {
        return par("blackhole " + pretty(b.tid) + " " + maybePrettyMd(b.md));
    }

    std::string operator()(const BasicBlock &b) {
        std::string stmts = prettyVec(b.stmts, "\n");
        return par(pretty(b.id) + " (\n" + indent(stmts) + ")");
    }

    std::string operator()(const Type &t) {
        return std::visit(Pretty(), t);
    }

    std::string operator()(const VarId &v) {
        return std::visit(Pretty(), v);
    }

    std::string operator()(const Constant &c) {
        return std::visit(Pretty(), c);
    }

    std::string operator()(const Val &v) {
        return std::visit(Pretty(), v);
    }

    std::string operator()(const PVal &v) {
        return std::visit(Pretty(), v);
    }

    std::string operator()(const IVal &v) {
        return std::visit(Pretty(), v);
    }

    std::string operator()(const GVal &v) {
        return std::visit(Pretty(), v);
    }

    std::string operator()(const Statement &s) {
        return std::visit(Pretty(), s);
    }
};

}  // Anonymous namespace

namespace SVFIR {

std::string pretty(const AnyNode &n) {
    return collapseRParens(std::visit(Pretty(), n));
}

}  // namespace SVFIR
