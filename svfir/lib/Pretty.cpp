#include <regex>
#include <iostream>

#include "Contracts.h"
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
    // Delete catch-all so compiler catches missing overloads.
    template<class T> void operator()(const T &) = delete;

    // Need to recurse further.
    template <typename... T>
    std::string operator()(const std::variant<T...> &v) {
        return std::visit(*this, v);
    }

    std::string operator()(const Metadata &md) {
        return "(md " + SExprs::toString(md.data) + ")";
    }

    std::string operator()(const VoidDecType &) { return "void"; }

    std::string operator()(const IntDecType &it) {
        if (it.kind == IntDecType::Kind::I8) { return "i8"; }
        if (it.kind == IntDecType::Kind::I16) { return "i16"; }
        else if (it.kind == IntDecType::Kind::I32) { return "i32"; }
        else if (it.kind == IntDecType::Kind::I64) { return "i64"; }
        else if (it.kind == IntDecType::Kind::I128) { return "i128"; }
        else { unreachable(); }
    }

    std::string operator()(const FloatDecType &ft) {
        if (ft.kind == FloatDecType::Kind::F16) { return "f16"; }
        else if (ft.kind == FloatDecType::Kind::F32) { return "f32"; }
        else if (ft.kind == FloatDecType::Kind::F64) { return "f64"; }
        else if (ft.kind == FloatDecType::Kind::F128) { return "f128"; }
        else { unreachable(); }
    }

    std::string operator()(const PtrDecType &) { return "ptr"; }

    std::string operator()(const AggDecType &st) {
        return "(agg " + std::to_string(st.slots) + ")";
    }

    std::string operator()(const Id &id) { return id.name; }

    std::string operator()(const TypedId &tid) {
        return "(" + pretty(tid.type) + " " + pretty(tid.id) + ")";
    }

    std::string operator()(const IntConstant &ic) { return ic.val; }

    std::string operator()(const FloatConstant &fc) { return fc.val; }

    std::string operator()(const NullConstant &) { return "null"; }

    std::string operator()(const SeqConstant &sc) {
        std::string vals;
        for (const Val &v : sc.vals) { vals += " " + pretty(v); }
        return par("seq" + vals);
    }

    std::string operator()(const TypedConstant &tc) {
        return "(" + pretty(tc.type) + " " + pretty(tc.constant) + ")";
    }

    std::string operator()(const Preamble &p) {
        return "(preamble\n" +
            indent("(version " + p.version.toString() + ")") + "\n" +
            indent("(source " + p.source + ")") +
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
        return par(pretty(p.type) + " " + pretty(p.id) + maybePrettyMd(p.md));
    }

    std::string operator()(const Function &f) {
        std::string params = prettyVec(f.params, " ");
        if (f.vararg) {
            if (!f.params.empty()) { params += " "; }
            params += "...";
        }

        return par(
            pretty(f.id) + " " +
            pretty(f.type) + " " +
            par(params) + " " +
            (f.body.has_value() ?
                "(\n" + indent(prettyVec(f.body.value(), "\n")) + ")" :
                "opaque"
            ) +
            maybePrettyMd(f.md)
        );
    }

    std::string operator()(const Program &p) {
        auto block = [](const std::string &name, const std::string &block) {
            if (block.empty()) {
                return "(" + name + ")";
            } else {
                return "(" + name + "\n" + indent(block) + ")";
            }
        };

        return pretty(p.preamble) + "\n" +
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
            args += par(pretty(o.val) + " " + pretty(o.pred));
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
        const std::string val =
            ret.val.has_value() ? pretty(ret.val.value()) : "void";
        const std::string maybeMd = maybePrettyMd(ret.md);
        return par("ret " + val + maybeMd);
    }

    std::string operator()(const AllocStmt &a) {
        std::string kind;
        if (a.kind == AllocStmt::Kind::HEAP) { kind = "heap"; }
        else if (a.kind == AllocStmt::Kind::STACK) { kind = "stack"; }
        else { unreachable(); }
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
            pretty(i.index) +
            maybePrettyMd(i.md)
        );
    }

    std::string operator()(const BitCastStmt &r) {
        return par(
            "bitcast " +
            pretty(r.id) + " " +
            pretty(r.val) + " " +
            pretty(r.type) +
            maybePrettyMd(r.md)
        );
    }

    std::string operator()(const ConvertStmt &c) {
        std::string type;
        if (c.extKind.has_value()) {
            const ConvertStmt::ExtKind ek = c.extKind.value();
            std::string eks;
            if (ek == ConvertStmt::ExtKind::ZERO) { eks = "zero"; }
            else if (ek == ConvertStmt::ExtKind::SIGN) { eks = "sign"; }
            else { unreachable(); }
            type = par(eks + " " + pretty(c.type));
        } else { type = pretty(c.type); }

        return par(
            "convert " +
            pretty(c.id) + " " +
            pretty(c.val) + " " +
            type +
            maybePrettyMd(c.md)
        );
    }

    std::string operator()(const BinaryOpStmt &b) {
        std::string kind;
        switch (b.op) {
        case BinaryOpStmt::Op::ADD: kind = "add"; break;
        case BinaryOpStmt::Op::SUB: kind = "sub"; break;
        case BinaryOpStmt::Op::MUL: kind = "mul"; break;
        case BinaryOpStmt::Op::UDIV: kind = "div unsigned"; break;
        case BinaryOpStmt::Op::SDIV: kind = "div signed"; break;
        case BinaryOpStmt::Op::UREM: kind = "rem unsigned"; break;
        case BinaryOpStmt::Op::SREM: kind = "rem signed"; break;
        case BinaryOpStmt::Op::AND: kind = "and"; break;
        case BinaryOpStmt::Op::OR: kind = "or"; break;
        case BinaryOpStmt::Op::XOR: kind = "xor"; break;
        case BinaryOpStmt::Op::SHIFTL: kind = "shiftl"; break;
        case BinaryOpStmt::Op::ASHIFTR: kind = "shiftr arithmetic"; break;
        case BinaryOpStmt::Op::LSHIFTR: kind = "shiftr logical"; break;
        case BinaryOpStmt::Op::LT: kind = "cmp <"; break;
        case BinaryOpStmt::Op::LE: kind = "cmp <="; break;
        case BinaryOpStmt::Op::GT: kind = "cmp >"; break;
        case BinaryOpStmt::Op::GE: kind = "cmp >="; break;
        case BinaryOpStmt::Op::EQ: kind = "cmp ="; break;
        case BinaryOpStmt::Op::NEQ: kind = "cmp !="; break;
        default: unreachable();
        }

        return par(
            kind + " " +
            pretty(b.id) + " " +
            pretty(b.left) + " " +
            pretty(b.right) +
            maybePrettyMd(b.md)
        );
    }

    std::string operator()(const UnaryOpStmt &u) {
        std::string kind;
        switch (u.op) {
        case UnaryOpStmt::Op::NOT: kind = "not"; break;
        default: unreachable();
        }

        return par(
            kind + " " +
            pretty(u.id) + " " +
            pretty(u.val) +
            maybePrettyMd(u.md)
        );
    }

    std::string operator()(const VarargStmt &v) {
        return par(
            "vararg " +
            pretty(v.tid) + " " +
            pretty(v.index) +
            maybePrettyMd(v.md)
        );
    }

    std::string operator()(const BlackholeStmt &b) {
        return par(
            "blackhole " +
            std::string(b.pure ? "pure" : "effectful") + " " +
            (b.tid.has_value() ? pretty(b.tid.value()) : "void") +
            maybePrettyMd(b.md)
        );
    }

    std::string operator()(const BasicBlock &b) {
        std::string stmts = prettyVec(b.stmts, "\n");
        return par(pretty(b.id) + " (\n" + indent(stmts) + ")");
    }
};

}  // Anonymous namespace

namespace SVFIR {

std::string pretty(const AnyNode &n) {
    return std::visit(Pretty(), n);
}

}  // namespace SVFIR
