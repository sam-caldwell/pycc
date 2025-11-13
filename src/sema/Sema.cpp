/***
 * Name: pycc::sema::Sema (impl)
 * Purpose: Minimal semantic checks with basic type env and source spans.
 */
#include "sema/Sema.h"
#include "sema/TypeEnv.h"
#include <sstream>
#include <functional>

#include "ast/AssignStmt.h"
#include "ast/Binary.h"
#include "ast/BinaryOperator.h"
#include "ast/BoolLiteral.h"
#include "ast/Call.h"
#include "ast/Expr.h"
#include "ast/ExprStmt.h"
#include "ast/FloatLiteral.h"
#include "ast/FunctionDef.h"
#include "ast/IfStmt.h"
#include "ast/IntLiteral.h"
#include "ast/ListLiteral.h"
#include "ast/Module.h"
#include "ast/Name.h"
#include "ast/Node.h"
#include "ast/NodeKind.h"
#include "ast/NoneLiteral.h"
#include "ast/ReturnStmt.h"
#include "ast/StringLiteral.h"
#include "ast/TupleLiteral.h"
#include "ast/TypeKind.h"
#include "ast/Unary.h"
#include "ast/UnaryOperator.h"
#include "ast/VisitorBase.h"

namespace pycc::sema {

using Type = ast::TypeKind;

struct Sig { Type ret; std::vector<Type> params; };

static bool typeIsInt(Type typeVal) { return typeVal == Type::Int; }
static bool typeIsBool(Type typeVal) { return typeVal == Type::Bool; }
static bool typeIsFloat(Type typeVal) { return typeVal == Type::Float; }
static bool typeIsStr(Type typeVal) { return typeVal == Type::Str; }

static void addDiag(std::vector<Diagnostic>& diags, const std::string& msg, const ast::Node* n) {
  Diagnostic diag;
  diag.message = msg;
  if (n != nullptr) { diag.file = n->file; diag.line = n->line; diag.col = n->col; }
  diags.push_back(std::move(diag));
}

struct ExpressionTyper : public ast::VisitorBase {
  // retParamIdxs: mapping of function name -> parameter index when return is trivially forwarded
  ExpressionTyper(const TypeEnv& env_, const std::unordered_map<std::string, Sig>& sigs_,
                  const std::unordered_map<std::string, int>& retParamIdxs_, std::vector<Diagnostic>& diags_)
    : env(&env_), sigs(&sigs_), retParamIdxs(&retParamIdxs_), diags(&diags_) {}
  const TypeEnv* env{nullptr};
  const std::unordered_map<std::string, Sig>* sigs{nullptr};
  const std::unordered_map<std::string, int>* retParamIdxs{nullptr};
  std::vector<Diagnostic>* diags{nullptr};
  Type out{Type::NoneType};
  bool ok{true};

  void visit(const ast::IntLiteral& n) override {
    out = Type::Int;
    auto& mutableInt = const_cast<ast::IntLiteral&>(n); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableInt.setType(out);
    mutableInt.setCanonicalKey(std::string("i:") + std::to_string(static_cast<long long>(n.value)));
  }
  void visit(const ast::BoolLiteral& n) override {
    out = Type::Bool;
    auto& mutableBool = const_cast<ast::BoolLiteral&>(n); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableBool.setType(out);
    mutableBool.setCanonicalKey(std::string("b:") + (n.value ? "1" : "0"));
  }
  void visit(const ast::FloatLiteral& n) override {
    out = Type::Float;
    auto& mutableFloat = const_cast<ast::FloatLiteral&>(n); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableFloat.setType(out);
    constexpr int kDoublePrecision = 17;
    std::ostringstream oss;
    oss.setf(std::ios::fmtflags(0), std::ios::floatfield);
    oss.precision(kDoublePrecision);
    oss << n.value;
    mutableFloat.setCanonicalKey(std::string("f:") + oss.str());
  }
  void visit(const ast::NoneLiteral& n) override {
    out = Type::NoneType;
    auto& mutableNone = const_cast<ast::NoneLiteral&>(n); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableNone.setType(out);
    mutableNone.setCanonicalKey("none");
  }
  void visit(const ast::StringLiteral& n) override {
    out = Type::Str;
    auto& mutableString = const_cast<ast::StringLiteral&>(n); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableString.setType(out);
    mutableString.setCanonicalKey(std::string("s:") + std::to_string(n.value.size()));
  }
  void visit(const ast::ObjectLiteral& obj) override {
    out = Type::NoneType;
    for (const auto& f : obj.fields) {
      if (f) {
        ExpressionTyper sub{*env, *sigs, *retParamIdxs, *diags};
        f->accept(sub);
        if (!sub.ok) { ok = false; return; }
      }
    }
    // Treat object as opaque pointer; no concrete type mapping yet
    auto& m = const_cast<ast::ObjectLiteral&>(obj); // NOLINT
    m.setType(Type::NoneType);
    m.setCanonicalKey("obj");
  }
  void visit(const ast::Name& n) override {
    auto resolvedType = env->get(n.id);
    if (!resolvedType) { addDiag(*diags, std::string("undefined name: ") + n.id, &n); ok = false; return; }
    out = *resolvedType; auto& mutableName = const_cast<ast::Name&>(n); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableName.setType(out); mutableName.setCanonicalKey(std::string("n:") + n.id);
  }
  void visit(const ast::Unary& unaryNode) override {
    ast::Expr* operandExpr = unaryNode.operand.get();
    if (operandExpr == nullptr) { addDiag(*diags, "null operand", &unaryNode); ok = false; return; }
    ExpressionTyper sub{*env, *sigs, *retParamIdxs, *diags};
    operandExpr->accept(sub); if (!sub.ok) { ok = false; return; }
    if (unaryNode.op == ast::UnaryOperator::Neg) {
      if (!typeIsInt(sub.out)) { addDiag(*diags, "unary '-' requires int", &unaryNode); ok = false; return; }
      out = Type::Int; auto& mutableUnary = const_cast<ast::Unary&>(unaryNode); // NOLINT(cppcoreguidelines-pro-type-const-cast)
      mutableUnary.setType(out);
      if (unaryNode.operand && unaryNode.operand->canonical()) { mutableUnary.setCanonicalKey("u:neg:(" + *unaryNode.operand->canonical() + ")"); }
    } else {
      if (!typeIsBool(sub.out)) { addDiag(*diags, "'not' requires bool", &unaryNode); ok = false; return; }
      out = Type::Bool; auto& mutableUnary2 = const_cast<ast::Unary&>(unaryNode); // NOLINT(cppcoreguidelines-pro-type-const-cast)
      mutableUnary2.setType(out);
      if (unaryNode.operand && unaryNode.operand->canonical()) { mutableUnary2.setCanonicalKey("u:not:(" + *unaryNode.operand->canonical() + ")"); }
    }
  }
  // NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
  void visit(const ast::Binary& binaryNode) override {
    ExpressionTyper lhsTyper{*env, *sigs, *retParamIdxs, *diags};
    ExpressionTyper rhsTyper{*env, *sigs, *retParamIdxs, *diags};
    binaryNode.lhs->accept(lhsTyper);
    if (!lhsTyper.ok) { ok = false; return; }
    binaryNode.rhs->accept(rhsTyper);
    if (!rhsTyper.ok) { ok = false; return; }
    // Arithmetic
    if (binaryNode.op == ast::BinaryOperator::Add || binaryNode.op == ast::BinaryOperator::Sub || binaryNode.op == ast::BinaryOperator::Mul || binaryNode.op == ast::BinaryOperator::Div || binaryNode.op == ast::BinaryOperator::Mod) {
      if (typeIsInt(lhsTyper.out) && typeIsInt(rhsTyper.out)) { out = Type::Int; return; }
      if (binaryNode.op != ast::BinaryOperator::Mod && typeIsFloat(lhsTyper.out) && typeIsFloat(rhsTyper.out)) { out = Type::Float; return; }
      addDiag(*diags, "arithmetic operands must both be int or both be float (mod only for int)", &binaryNode); ok = false; return;
    }
    // Comparisons
    if (binaryNode.op == ast::BinaryOperator::Eq || binaryNode.op == ast::BinaryOperator::Ne || binaryNode.op == ast::BinaryOperator::Lt || binaryNode.op == ast::BinaryOperator::Le || binaryNode.op == ast::BinaryOperator::Gt || binaryNode.op == ast::BinaryOperator::Ge) {
      // Allow eq/ne None comparisons regardless of other type
      if ((binaryNode.op == ast::BinaryOperator::Eq || binaryNode.op == ast::BinaryOperator::Ne) &&
          (binaryNode.lhs->kind == ast::NodeKind::NoneLiteral || binaryNode.rhs->kind == ast::NodeKind::NoneLiteral)) {
        out = Type::Bool; auto& mutableBinary = const_cast<ast::Binary&>(binaryNode); // NOLINT(cppcoreguidelines-pro-type-const-cast)
        mutableBinary.setType(out);
        if (binaryNode.lhs && binaryNode.rhs && binaryNode.lhs->canonical() && binaryNode.rhs->canonical()) {
          mutableBinary.setCanonicalKey("cmp_none:(" + *binaryNode.lhs->canonical() + "," + *binaryNode.rhs->canonical() + ")");
        }
        return;
      }
      const bool bothInt = typeIsInt(lhsTyper.out) && typeIsInt(rhsTyper.out);
      const bool bothFloat = typeIsFloat(lhsTyper.out) && typeIsFloat(rhsTyper.out);
      if (!(bothInt || bothFloat)) { addDiag(*diags, "comparison operands must both be int or both be float", &binaryNode); ok = false; return; }
      out = Type::Bool; auto& mutableBinary = const_cast<ast::Binary&>(binaryNode); // NOLINT(cppcoreguidelines-pro-type-const-cast)
      mutableBinary.setType(out);
      if (binaryNode.lhs && binaryNode.rhs && binaryNode.lhs->canonical() && binaryNode.rhs->canonical()) {
        mutableBinary.setCanonicalKey("cmp:(" + *binaryNode.lhs->canonical() + "," + *binaryNode.rhs->canonical() + ")");
      }
      return;
    }
    // Logical
    if (binaryNode.op == ast::BinaryOperator::And || binaryNode.op == ast::BinaryOperator::Or) {
      if (!typeIsBool(lhsTyper.out) || !typeIsBool(rhsTyper.out)) { addDiag(*diags, "logical operands must be bool", &binaryNode); ok = false; return; }
      out = Type::Bool; auto& mutableBinary2 = const_cast<ast::Binary&>(binaryNode); // NOLINT(cppcoreguidelines-pro-type-const-cast)
      mutableBinary2.setType(out);
      if (binaryNode.lhs && binaryNode.rhs && binaryNode.lhs->canonical() && binaryNode.rhs->canonical()) {
        mutableBinary2.setCanonicalKey("log:(" + *binaryNode.lhs->canonical() + "," + *binaryNode.rhs->canonical() + ")");
      }
      return;
    }
    // Arithmetic (typed) — set canonical for safe recognition
    if ((binaryNode.op == ast::BinaryOperator::Add || binaryNode.op == ast::BinaryOperator::Sub || binaryNode.op == ast::BinaryOperator::Mul || binaryNode.op == ast::BinaryOperator::Div || binaryNode.op == ast::BinaryOperator::Mod) && ( (typeIsInt(lhsTyper.out)&&typeIsInt(rhsTyper.out)) || (typeIsFloat(lhsTyper.out)&&typeIsFloat(rhsTyper.out)) )) {
      auto& mutableBinary3 = const_cast<ast::Binary&>(binaryNode); // NOLINT(cppcoreguidelines-pro-type-const-cast)
      mutableBinary3.setType(typeIsInt(lhsTyper.out) ? Type::Int : Type::Float);
      if (binaryNode.lhs && binaryNode.rhs && binaryNode.lhs->canonical() && binaryNode.rhs->canonical()) {
        const char* opStr = "?";
        switch (binaryNode.op) {
          case ast::BinaryOperator::Add: opStr = "+"; break;
          case ast::BinaryOperator::Sub: opStr = "-"; break;
          case ast::BinaryOperator::Mul: opStr = "*"; break;
          case ast::BinaryOperator::Div: opStr = "/"; break;
          case ast::BinaryOperator::Mod: opStr = "%"; break;
          default: break;
        }
        mutableBinary3.setCanonicalKey(std::string("bin:") + opStr + ":(" + *binaryNode.lhs->canonical() + "," + *binaryNode.rhs->canonical() + ")");
      }
      return;
    }
    addDiag(*diags, "unsupported binary operator", &binaryNode); ok = false;
  }
  void visit(const ast::ExprStmt& exprStmtUnused) override { addDiag(*diags, "internal error: exprstmt is not expression", &exprStmtUnused); ok = false; }
  void visit(const ast::TupleLiteral& tupleLiteral) override {
    // Infer element types; mark this as a tuple
    for (const auto& element : tupleLiteral.elements) {
      ExpressionTyper elemTyper{*env, *sigs, *retParamIdxs, *diags};
      element->accept(elemTyper);
      if (!elemTyper.ok) { ok = false; return; }
    }
    out = Type::Tuple;
    auto& mutableTuple = const_cast<ast::TupleLiteral&>(tupleLiteral); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableTuple.setType(out);
    std::string key = "tuple:(";
    for (size_t i = 0; i < tupleLiteral.elements.size(); ++i) {
      const auto& element = tupleLiteral.elements[i];
      if (i > 0) { key += ","; }
      if (element->canonical()) { key += *element->canonical(); }
      else { key += "?"; }
    }
    key += ")";
    mutableTuple.setCanonicalKey(key);
  }
  void visit(const ast::ListLiteral& listLiteral) override {
    for (const auto& element : listLiteral.elements) {
      ExpressionTyper elemTyper{*env, *sigs, *retParamIdxs, *diags};
      element->accept(elemTyper);
      if (!elemTyper.ok) { ok = false; return; }
    }
    out = Type::List;
    auto& mutableList = const_cast<ast::ListLiteral&>(listLiteral); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableList.setType(out);
    std::string key = "list:(";
    for (size_t i = 0; i < listLiteral.elements.size(); ++i) {
      const auto& element = listLiteral.elements[i];
      if (i > 0) { key += ","; }
      if (element->canonical()) { key += *element->canonical(); }
      else { key += "?"; }
    }
    key += ")";
    mutableList.setCanonicalKey(key);
  }
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  void visit(const ast::Call& callNode) override {
    if (!callNode.callee || callNode.callee->kind != ast::NodeKind::Name) { addDiag(*diags, "unsupported callee expression", &callNode); ok = false; return; }
    auto* nameNode = static_cast<const ast::Name*>(callNode.callee.get());
    // Builtins: len(x) -> int; isinstance(x, T) -> bool
    if (nameNode->id == "len") {
      if (callNode.args.size() != 1) { addDiag(*diags, "len() takes exactly one argument", &callNode); ok = false; return; }
      ExpressionTyper argTyper{*env, *sigs, *retParamIdxs, *diags}; callNode.args[0]->accept(argTyper); if (!argTyper.ok) { ok = false; return; }
      // Allow len of tuple/list/str; others will be flagged later if desired
      out = Type::Int; const_cast<ast::Call&>(callNode).setType(out); // NOLINT(cppcoreguidelines-pro-type-const-cast)
      return;
    }
    if (nameNode->id == "obj_get") {
      if (callNode.args.size() != 2) { addDiag(*diags, "obj_get() takes two arguments", &callNode); ok = false; return; }
      // First arg is an object pointer (opaque), second must be int index
      ExpressionTyper idxTyper{*env, *sigs, *retParamIdxs, *diags}; callNode.args[1]->accept(idxTyper); if (!idxTyper.ok) { ok = false; return; }
      if (!typeIsInt(idxTyper.out)) { addDiag(*diags, "obj_get index must be int", callNode.args[1].get()); ok = false; return; }
      out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); // treat as string pointer result
      return;
    }
    if (nameNode->id == "isinstance") {
      if (callNode.args.size() != 2) { addDiag(*diags, "isinstance() takes two arguments", &callNode); ok = false; return; }
      // We don't deeply validate type arg here; return bool
      out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); // NOLINT(cppcoreguidelines-pro-type-const-cast)
      return;
    }
    auto sigIt = sigs->find(nameNode->id);
    if (sigIt == sigs->end()) { addDiag(*diags, std::string("unknown function: ") + nameNode->id, &callNode); ok = false; return; }
    const auto& sig = sigIt->second;
    if (sig.params.size() != callNode.args.size()) { addDiag(*diags, std::string("arity mismatch calling function: ") + nameNode->id, &callNode); ok = false; return; }
    for (size_t i = 0; i < callNode.args.size(); ++i) {
      ExpressionTyper argTyper{*env, *sigs, *retParamIdxs, *diags}; callNode.args[i]->accept(argTyper); if (!argTyper.ok) { ok = false; return; }
      if (argTyper.out != sig.params[i]) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; }
    }
    out = sig.ret; auto& mutableCall = const_cast<ast::Call&>(callNode); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableCall.setType(out);
    // Interprocedural canonical propagation for trivial forwarders: f(x, ...) -> x
    if (callNode.callee && callNode.callee->kind == ast::NodeKind::Name) {
      const auto* cname = static_cast<const ast::Name*>(callNode.callee.get());
      auto it = retParamIdxs->find(cname->id);
      if (it != retParamIdxs->end()) {
        const int idx = it->second;
        if (idx >= 0 && static_cast<size_t>(idx) < callNode.args.size()) {
          const auto& arg = callNode.args[idx];
          if (arg && arg->canonical()) { mutableCall.setCanonicalKey(*arg->canonical()); }
        }
      }
    }
  }
  void visit(const ast::ReturnStmt& returnStmt) override { addDiag(*diags, "internal error: return is not expression", &returnStmt); ok = false; }
  void visit(const ast::AssignStmt& assignStmt) override { addDiag(*diags, "internal error: assign is not expression", &assignStmt); ok = false; }
  void visit(const ast::IfStmt& ifStmt) override { addDiag(*diags, "internal error: if is not expression", &ifStmt); ok = false; }
  void visit(const ast::FunctionDef& functionDef) override { addDiag(*diags, "internal error: function is not expression", &functionDef); ok = false; }
  void visit(const ast::Module& module) override { addDiag(*diags, "internal error: module is not expression", &module); ok = false; }
};

static bool inferExprType(const ast::Expr* expr,
                          const TypeEnv& env,
                          const std::unordered_map<std::string, Sig>& sigs,
                          const std::unordered_map<std::string, int>& retParamIdxs,
                          Type& outType,
                          std::vector<Diagnostic>& diags) {
  if (expr == nullptr) { addDiag(diags, "null expression", nullptr); return false; }
  ExpressionTyper exprTyper{env, sigs, retParamIdxs, diags};
  expr->accept(exprTyper);
  if (!exprTyper.ok) { return false; }
  outType = exprTyper.out;
  const_cast<ast::Expr*>(expr)->setType(outType); // NOLINT(cppcoreguidelines-pro-type-const-cast)
  return true;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static,readability-function-size)
bool Sema::check(ast::Module& mod, std::vector<Diagnostic>& diags) {
  std::unordered_map<std::string, Sig> sigs;
  for (const auto& fn : mod.functions) {
    Sig s; s.ret = fn->returnType;
    for (const auto& p : fn->params) s.params.push_back(p.type);
    sigs[fn->name] = std::move(s);
  }
  // Build a trivial interprocedural summary: which function consistently returns a specific parameter index
  std::unordered_map<std::string, int> retParamIdxs; // func -> param index
  for (const auto& fn : mod.functions) {
    int retIdx = -1; bool hasReturn = false; bool consistent = true;
    std::function<void(const ast::Stmt&)> walk;
    walk = [&](const ast::Stmt& st) {
      if (!consistent) return;
      if (st.kind == ast::NodeKind::ReturnStmt) {
        hasReturn = true;
        const auto* r = static_cast<const ast::ReturnStmt*>(&st);
        if (!(r->value && r->value->kind == ast::NodeKind::Name)) { consistent = false; return; }
        const auto* n = static_cast<const ast::Name*>(r->value.get());
        int idxFound = -1;
        for (size_t i = 0; i < fn->params.size(); ++i) { if (fn->params[i].name == n->id) { idxFound = static_cast<int>(i); break; } }
        if (idxFound < 0) { consistent = false; return; }
        if (retIdx < 0) retIdx = idxFound; else if (retIdx != idxFound) { consistent = false; }
        return;
      }
      if (st.kind == ast::NodeKind::IfStmt) {
        const auto* iff = static_cast<const ast::IfStmt*>(&st);
        for (const auto& s2 : iff->thenBody) { if (s2) walk(*s2); }
        for (const auto& s3 : iff->elseBody) { if (s3) walk(*s3); }
        return;
      }
    };
    for (const auto& st : fn->body) { if (st) walk(*st); }
    if (hasReturn && consistent && retIdx >= 0) { retParamIdxs[fn->name] = retIdx; }
  }

  for (const auto& fn : mod.functions) {
    if (!(typeIsInt(fn->returnType) || typeIsBool(fn->returnType) || typeIsFloat(fn->returnType) || typeIsStr(fn->returnType) || fn->returnType == Type::Tuple)) { Diagnostic d; d.message = "only int/bool/float/str/tuple returns supported"; diags.push_back(std::move(d)); return false; }
    TypeEnv env;
    for (const auto& p : fn->params) {
      if (!(typeIsInt(p.type) || typeIsBool(p.type) || typeIsFloat(p.type) || typeIsStr(p.type))) { Diagnostic d; d.message = "only int/bool/float/str params supported"; diags.push_back(std::move(d)); return false; }
      env.define(p.name, p.type, {fn->name, 0, 0});
    }
    struct StmtChecker : public ast::VisitorBase {
      StmtChecker(const ast::FunctionDef& fn_, const std::unordered_map<std::string, Sig>& sigs_,
                  const std::unordered_map<std::string, int>& retParamIdxs_, TypeEnv& env_, std::vector<Diagnostic>& diags_)
        : fn(fn_), sigs(sigs_), retParamIdxs(retParamIdxs_), env(env_), diags(diags_) {}
      const ast::FunctionDef& fn;
      const std::unordered_map<std::string, Sig>& sigs;
      const std::unordered_map<std::string, int>& retParamIdxs;
      TypeEnv& env;
      std::vector<Diagnostic>& diags;
      bool ok{true};

      bool infer(const ast::Expr* e, Type& out) { return inferExprType(e, env, sigs, retParamIdxs, out, diags); }

      void visit(const ast::AssignStmt& a) override {
        Type t{}; if (!infer(a.value.get(), t)) { ok = false; return; }
        // Allow pointer-typed aggregates like list as variables
        if (!(typeIsInt(t) || typeIsBool(t) || typeIsFloat(t) || typeIsStr(t) || t == Type::List)) {
          addDiag(diags, "only int/bool/float/str/list variables supported", &a); ok = false; return;
        }
        auto existing = env.get(a.target);
        if (!existing) env.define(a.target, t, {a.file, a.line, a.col});
        else if (*existing != t) { addDiag(diags, std::string("type mismatch on assignment: ") + a.target, &a); ok = false; }
      }

      void visit(const ast::IfStmt& iff) override {
        Type ct{}; if (!infer(iff.cond.get(), ct)) { ok = false; return; }
        if (!typeIsBool(ct)) { addDiag(diags, "if condition must be bool", &iff); ok = false; return; }
        TypeEnv thenL = env, elseL = env;
        // Flow refinement for isinstance(x, T)
        if (iff.cond && iff.cond->kind == ast::NodeKind::Call) {
          auto* call = static_cast<const ast::Call*>(iff.cond.get());
          if (call->callee && call->callee->kind == ast::NodeKind::Name) {
            auto* cname = static_cast<const ast::Name*>(call->callee.get());
            if (cname->id == "isinstance" && call->args.size() == 2) {
              if (call->args[0] && call->args[0]->kind == ast::NodeKind::Name && call->args[1] && call->args[1]->kind == ast::NodeKind::Name) {
                auto* v = static_cast<const ast::Name*>(call->args[0].get());
                auto* tname = static_cast<const ast::Name*>(call->args[1].get());
                Type newT = Type::NoneType;
                if (tname->id == "int") newT = Type::Int;
                else if (tname->id == "bool") newT = Type::Bool;
                else if (tname->id == "float") newT = Type::Float;
                else if (tname->id == "str") newT = Type::Str;
                if (newT != Type::NoneType) thenL.define(v->id, newT, {v->file, v->line, v->col});
              }
            }
          }
        }
        // Flow refinement for eq/ne None in condition: if (x == None) or if (x != None)
        if (iff.cond && iff.cond->kind == ast::NodeKind::BinaryExpr) {
          auto* b = static_cast<const ast::Binary*>(iff.cond.get());
          bool eq = (b->op == ast::BinaryOperator::Eq);
          bool ne = (b->op == ast::BinaryOperator::Ne);
          auto refine = [&](const ast::Expr* e1, const ast::Expr* e2, bool setNone) {
            if (e1 && e1->kind == ast::NodeKind::Name && e2 && e2->kind == ast::NodeKind::NoneLiteral) {
              auto* n = static_cast<const ast::Name*>(e1);
              if (setNone) thenL.define(n->id, Type::NoneType, {n->file, n->line, n->col});
            }
          };
          if (eq) { refine(b->lhs.get(), b->rhs.get(), true); refine(b->rhs.get(), b->lhs.get(), true); }
          if (ne) { /* else branch remains original; no-op here */ }
        }
        // then branch
        for (const auto& s2 : iff.thenBody) {
          if (s2->kind == ast::NodeKind::AssignStmt) {
            auto* a2 = static_cast<const ast::AssignStmt*>(s2.get());
            Type t2{}; if (!infer(a2->value.get(), t2)) { ok = false; return; }
            thenL.define(a2->target, t2, {a2->file, a2->line, a2->col});
          } else if (s2->kind == ast::NodeKind::ReturnStmt) {
            auto* r2 = static_cast<const ast::ReturnStmt*>(s2.get());
            Type rt{}; if (!infer(r2->value.get(), rt)) { ok = false; return; }
            if (rt != fn.returnType) { addDiag(diags, "return type mismatch in then branch", r2); ok = false; return; }
          }
        }
        // else branch
        for (const auto& s3 : iff.elseBody) {
          if (s3->kind == ast::NodeKind::AssignStmt) {
            auto* a3 = static_cast<const ast::AssignStmt*>(s3.get());
            Type t3{}; if (!infer(a3->value.get(), t3)) { ok = false; return; }
            elseL.define(a3->target, t3, {a3->file, a3->line, a3->col});
          } else if (s3->kind == ast::NodeKind::ReturnStmt) {
            auto* r3 = static_cast<const ast::ReturnStmt*>(s3.get());
            Type rt{}; if (!infer(r3->value.get(), rt)) { ok = false; return; }
            if (rt != fn.returnType) { addDiag(diags, "return type mismatch in else branch", r3); ok = false; return; }
          }
        }
      }

      void visit(const ast::ReturnStmt& r) override {
        Type t{}; if (!infer(r.value.get(), t)) { ok = false; return; }
        if (t != fn.returnType) { addDiag(diags, std::string("return type mismatch in function: ") + fn.name, &r); ok = false; }
      }

      void visit(const ast::ExprStmt& s) override { if (s.value) { Type tmp{}; (void)infer(s.value.get(), tmp); } }

      // Unused in stmt context
      void visit(const ast::Module&) override {}
      void visit(const ast::FunctionDef&) override {}
      void visit(const ast::IntLiteral&) override {}
      void visit(const ast::BoolLiteral&) override {}
      void visit(const ast::FloatLiteral&) override {}
      void visit(const ast::Name&) override {}
      void visit(const ast::Call&) override {}
      void visit(const ast::Binary&) override {}
      void visit(const ast::Unary&) override {}
      void visit(const ast::StringLiteral&) override {}
      void visit(const ast::TupleLiteral&) override {}
      void visit(const ast::ListLiteral&) override {}
      void visit(const ast::ObjectLiteral&) override {}
      void visit(const ast::NoneLiteral&) override {}
    };

    StmtChecker C{*fn, sigs, retParamIdxs, env, diags};
    for (const auto& st : fn->body) {
      st->accept(C);
      if (!C.ok) return false;
    }
  }
  return diags.empty();
}

} // namespace pycc::sema
