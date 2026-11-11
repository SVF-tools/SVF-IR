#include <regex>
#include <iostream>

#include "Contracts.h"
#include "Ast.h"
#include "Ops.h"
#include "TypedProgram.h"

namespace {

using namespace SVFIR;

class Pretty {
    /// If available, we'll print types alongside IDs.
    const std::optional<SymbolTable> symbols;
    /// Current function, if any, we are currently looking at.
    std::optional<Name> curFn;

public:
    Pretty(const std::optional<SymbolTable> &symbols) : symbols(symbols) { }

    // Delete catch-all so compiler catches missing overloads.
    template<class T> void operator()(const T &) = delete;

    // Need to recurse further.
    template <typename... T>
    std::string operator()(const std::variant<T...> &v) {
        return std::visit(*this, v);
    }

    std::string operator()(const Metadata &md) {
        return par("md " + SExprs::toString(md.data));
    }

    std::string operator()(const VoidType &) { return "void"; }

    std::string operator()(const IntType &it) {
        if (it.kind == IntType::Kind::I8) { return "i8"; }
        if (it.kind == IntType::Kind::I16) { return "i16"; }
        else if (it.kind == IntType::Kind::I32) { return "i32"; }
        else if (it.kind == IntType::Kind::I64) { return "i64"; }
        else if (it.kind == IntType::Kind::I128) { return "i128"; }
        else { unreachable(); }
    }

    std::string operator()(const FloatType &ft) {
        if (ft.kind == FloatType::Kind::F16) { return "f16"; }
        else if (ft.kind == FloatType::Kind::F32) { return "f32"; }
        else if (ft.kind == FloatType::Kind::F64) { return "f64"; }
        else if (ft.kind == FloatType::Kind::F128) { return "f128"; }
        else { unreachable(); }
    }

    std::string operator()(const PtrType &) { return "ptr"; }

    std::string operator()(const AggType &st) {
        return par("agg " + std::to_string(st.slots));
    }

    /// type tells to include or exclude the type of id regardless of
    /// the state. This is for e.g. typed IDs and parameters where a
    /// type is already specified (which the typer must follow).
    std::string operator()(const Id &id, const bool type = true) {
        std::string maybeType;
        if (type && symbols.has_value() && id.isLocal()) {
            insist(curFn.has_value());
            const Type &type =
                symbols.value().fnTables.at(curFn.value()).at(id.name);
            maybeType = " [" + operator()(type) + "]";
        }
        return id.name + maybeType;
    }

    std::string operator()(const TypedId &tid) {
        return par(operator()(tid.type) + " " + operator()(tid.id, false));
    }

    std::string operator()(const IntConstant &ic) { return ic.val; }

    std::string operator()(const FloatConstant &fc) { return fc.val; }

    std::string operator()(const NullConstant &) { return "null"; }

    std::string operator()(const SeqConstant &sc) {
        std::string vals;
        for (const Val &v : sc.vals) { vals += " " + operator()(v); }
        return par("seq" + vals);
    }

    std::string operator()(const TypedConstant &tc) {
        return par(operator()(tc.type) + " " + operator()(tc.constant));
    }

    std::string operator()(const Preamble &p) {
        return par("preamble\n" +
            indent(par("version " + p.version.toString())) + "\n" +
            indent(par("source " + p.source))
        );
    }

    std::string operator()(const GlobalVar &v) {
        return par(
            operator()(v.id) + " " +
            (v.val.has_value() ? operator()(v.val.value()) : "opaque") +
            maybeOpMd(" ", v.md)
        );
    }

    std::string operator()(const Param &p) {
        return par(
            operator()(p.type) + " " +
            operator()(p.id, false) +
            maybeOpMd(" ", p.md)
        );
    }

    std::string operator()(const Function &f) {
        curFn.emplace(f.id.name);

        std::string params = opVec(f.params, " ");
        if (f.vararg) {
            if (!f.params.empty()) { params += " "; }
            params += "...";
        }

        const std::string fnstr = par(
            operator()(f.id) + " " +
            operator()(f.type) + " " +
            par(params) + " " +
            (f.body.has_value() ?
                par("\n" + indent(opVec(f.body.value(), "\n"))) :
                "opaque"
            ) +
            maybeOpMd(" ", f.md)
        );

        curFn = std::nullopt;

        return fnstr;
    }

    std::string operator()(const TypedProgram &tp) {
        return operator()(tp.program);
    }

    std::string operator()(const Program &p) {
        auto block = [this](const std::string &name, const std::string &block) {
            if (block.empty()) {
                return par(name);
            } else {
                return par(name + "\n" + indent(block));
            }
        };

        return operator()(p.preamble) + "\n" +
            block("variables", opVec(p.globalVars, "\n")) + "\n" +
            block("functions", opVec(p.functions, "\n")) +
            (p.md.has_value() ? "\n" + operator()(p.md.value()) : "");
    }

    std::string operator()(const PhiStmt &p) {
        std::string args;
        bool first = true;
        for (auto o : p.args) {
            if (first) { first = false; }
            else { args += " "; }
            args += par(operator()(o.val) + " " + operator()(o.pred));
        }

        return par(
            "phi " + operator()(p.id) + " " + par(args) + maybeOpMd(" ", p.md)
        );
    }

    std::string operator()(const CallStmt &c) {
        std::string ret;
        if (c.tid.has_value()) { ret = operator()(c.tid.value()); }
        else { ret = "void"; }
        std::string args = opVec(c.args, " ");
        return par(
            "call " +
            ret + " " +
            operator()(c.callee) + " " +
            par(args) +
            maybeOpMd(" ", c.md)
        );
    }

    std::string operator()(const BrStmt &br) {
        return par("br " + operator()(br.target) + maybeOpMd(" ", br.md));
    }

    std::string operator()(const BrifStmt &brif) {
        return par(
            "brif " +
            operator()(brif.cond) + " " +
            operator()(brif.ifTarget) + " " +
            operator()(brif.elseTarget) +
            maybeOpMd(" ", brif.md)
        );
    }

    std::string operator()(const RetStmt &ret) {
        const std::string val =
            ret.val.has_value() ? operator()(ret.val.value()) : "void";
        const std::string maybeMd = maybeOpMd(" ", ret.md);
        return par("ret " + val + maybeMd);
    }

    std::string operator()(const AllocStmt &a) {
        std::string kind;
        if (a.kind == AllocStmt::Kind::HEAP) { kind = "heap"; }
        else if (a.kind == AllocStmt::Kind::STACK) { kind = "stack"; }
        else { unreachable(); }
        return par(
            "alloc " + kind + " " + operator()(a.id) + maybeOpMd(" ", a.md)
        );
    }

    std::string operator()(const StoreStmt &s) {
        return par(
            "store " +
            operator()(s.val) + " " +
            operator()(s.dst) +
            maybeOpMd(" ", s.md)
        );
    }

    std::string operator()(const LoadStmt &l) {
        return par(
            "load " +
            operator()(l.tid) + " " +
            operator()(l.src) +
            maybeOpMd(" ", l.md)
        );
    }

    std::string operator()(const FieldStmt &i) {
        return par(
            "field " +
            operator()(i.id) + " " +
            operator()(i.src) + " " +
            operator()(i.index) +
            maybeOpMd(" ", i.md)
        );
    }

    std::string operator()(const BitCastStmt &r) {
        return par(
            "bitcast " +
            operator()(r.id) + " " +
            operator()(r.val) + " " +
            operator()(r.type) +
            maybeOpMd(" ", r.md)
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
            type = par(eks + " " + operator()(c.type));
        } else { type = operator()(c.type); }

        return par(
            "convert " +
            operator()(c.id) + " " +
            operator()(c.val) + " " +
            type +
            maybeOpMd(" ", c.md)
        );
    }

    std::string operator()(const BinaryOpStmt &b) {
        std::string kind;
        switch (b.op) {
        case BinaryOpStmt::Op::ADD: kind = "add"; break;
        case BinaryOpStmt::Op::SUB: kind = "sub"; break;
        case BinaryOpStmt::Op::MUL: kind = "mul"; break;
        case BinaryOpStmt::Op::DIV: kind = "div"; break;
        case BinaryOpStmt::Op::UDIV: kind = "div unsigned"; break;
        case BinaryOpStmt::Op::SDIV: kind = "div signed"; break;
        case BinaryOpStmt::Op::REM: kind = "rem"; break;
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
        case BinaryOpStmt::Op::SLT: kind = "cmp (signed <)"; break;
        case BinaryOpStmt::Op::SLE: kind = "cmp (signed <=)"; break;
        case BinaryOpStmt::Op::SGT: kind = "cmp (signed >)"; break;
        case BinaryOpStmt::Op::SGE: kind = "cmp (signed >=)"; break;
        case BinaryOpStmt::Op::ULT: kind = "cmp (unsigned <)"; break;
        case BinaryOpStmt::Op::ULE: kind = "cmp (unsigned <=)"; break;
        case BinaryOpStmt::Op::UGT: kind = "cmp (unsigned >)"; break;
        case BinaryOpStmt::Op::UGE: kind = "cmp (unsigned >=)"; break;
        case BinaryOpStmt::Op::EQ: kind = "cmp ="; break;
        case BinaryOpStmt::Op::NEQ: kind = "cmp !="; break;
        default: unreachable();
        }

        return par(
            kind + " " +
            operator()(b.id) + " " +
            operator()(b.left) + " " +
            operator()(b.right) +
            maybeOpMd(" ", b.md)
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
            operator()(u.id) + " " +
            operator()(u.val) +
            maybeOpMd(" ", u.md)
        );
    }

    std::string operator()(const VarargStmt &v) {
        return par(
            "vararg " +
            operator()(v.tid) + " " +
            operator()(v.index) +
            maybeOpMd(" ", v.md)
        );
    }

    std::string operator()(const BlackholeStmt &b) {
        return par(
            "blackhole " +
            std::string(b.pure ? "pure" : "effectful") + " " +
            (b.tid.has_value() ? operator()(b.tid.value()) : "void") +
            maybeOpMd(" ", b.md)
        );
    }

    std::string operator()(const BasicBlock &b) {
        std::string stmts = opVec(b.stmts, "\n");
        return par(operator()(b.id) + " " + par("\n" + indent(stmts)));
    }

private:
    const std::string INDENTATION = "  ";

    /// Add an INDENTATION to the start of every line in s.
    std::string indent(const std::string &s) {
        static std::regex lineEndRe("\n");

        return std::regex_replace(
            s.empty() ? s : INDENTATION + s,
            lineEndRe,
            "\n" + INDENTATION
        );
    }

    /// Strings together each element of xs, calling operator() on each,
    /// delimited by delim.
    template <typename T>
    std::string opVec(const std::vector<T> &xs, const std::string &delim) {
        std::string s;
        bool first = true;
        for (auto x : xs) {
            if (first) { first = false; }
            else { s += delim; }
            s += operator()(x);
        }
        return s;
    }

    /// If md has a value returns an operator() string of it preceded by
    /// prefix (usually a space), otherwise returns an empty string.
    std::string maybeOpMd(const std::string &prefix, const MaybeMetadata md) {
        return md.has_value() ? prefix + operator()(md.value()) : "";
    }

    /// Wraps a string in parentheses.
    std::string par(const std::string &s) { return "(" + s + ")"; }
};

}  // Anonymous namespace

namespace SVFIR {

std::string pretty(const std::variant<AnyNode, TypedProgram> &n) {
    Pretty pretty =
        is<TypedProgram>(n) ?
        Pretty(get<TypedProgram>(n).symbols) : Pretty(std::nullopt);
    return std::visit(pretty, n);
}

}  // namespace SVFIR
