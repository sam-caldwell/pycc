/***
 * Name: pycc::sema::Sema (impl)
 * Purpose: Minimal semantic checks with basic type env and source spans.
 */
#include "sema/Sema.h"
#include "sema/TypeEnv.h"
#include <cstddef>
#include <cstdint>
#include <ios>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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

struct Sig { Type ret{Type::NoneType}; std::vector<Type> params; };

static bool typeIsInt(Type typeVal) { return typeVal == Type::Int; }
static bool typeIsBool(Type typeVal) { return typeVal == Type::Bool; }
static bool typeIsFloat(Type typeVal) { return typeVal == Type::Float; }
static bool typeIsStr(Type typeVal) { return typeVal == Type::Str; }

static void addDiag(std::vector<Diagnostic>& diags, const std::string& msg, const ast::Node* n) { // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
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
  uint32_t outSet{0};
  bool ok{true};

  void visit(const ast::IntLiteral& n) override {
    out = Type::Int;
    outSet = TypeEnv::maskForKind(out);
    auto& mutableInt = const_cast<ast::IntLiteral&>(n); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableInt.setType(out);
    mutableInt.setCanonicalKey(std::string("i:") + std::to_string(static_cast<long long>(n.value)));
  }
  void visit(const ast::BoolLiteral& n) override {
    out = Type::Bool;
    outSet = TypeEnv::maskForKind(out);
    auto& mutableBool = const_cast<ast::BoolLiteral&>(n); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableBool.setType(out);
    mutableBool.setCanonicalKey(std::string("b:") + (n.value ? "1" : "0"));
  }
  void visit(const ast::FloatLiteral& n) override {
    out = Type::Float;
    outSet = TypeEnv::maskForKind(out);
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
    outSet = TypeEnv::maskForKind(out);
    auto& mutableNone = const_cast<ast::NoneLiteral&>(n); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableNone.setType(out);
    mutableNone.setCanonicalKey("none");
  }
  void visit(const ast::StringLiteral& n) override {
    out = Type::Str;
    outSet = TypeEnv::maskForKind(out);
    auto& mutableString = const_cast<ast::StringLiteral&>(n); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableString.setType(out);
    mutableString.setCanonicalKey(std::string("s:") + std::to_string(n.value.size()));
  }
  // NOLINTNEXTLINE(readability-function-size)
  void visit(const ast::ObjectLiteral& obj) override {
    out = Type::NoneType;
    for (const auto& field : obj.fields) {
      if (field) {
        ExpressionTyper sub{*env, *sigs, *retParamIdxs, *diags};
        field->accept(sub);
        if (!sub.ok) { ok = false; return; }
      }
    }
    // Treat object as opaque pointer; no concrete type mapping yet
    auto& m = const_cast<ast::ObjectLiteral&>(obj); // NOLINT
    m.setType(Type::NoneType);
    m.setCanonicalKey("obj");
  }
  void visit(const ast::Name& n) override {
    const uint32_t maskVal = env->getSet(n.id);
    if (maskVal == 0U) {
      // Empty set: contradictory refinements (e.g., excluded the only possible kind)
      // Treat as an error at the point of use.
      addDiag(*diags, std::string("contradictory type for name: ") + n.id, &n);
      ok = false; return;
    }
    if (maskVal != 0U) {
      outSet = maskVal;
      if (TypeEnv::isSingleMask(maskVal)) { out = TypeEnv::kindFromMask(maskVal); }
    }
    auto resolvedType = env->get(n.id);
    if (!resolvedType && outSet == 0U) { addDiag(*diags, std::string("undefined name: ") + n.id, &n); ok = false; return; }
    if (outSet == 0U && resolvedType) { out = *resolvedType; outSet = TypeEnv::maskForKind(out); }
    if (TypeEnv::isSingleMask(outSet)) { out = TypeEnv::kindFromMask(outSet); }
    auto& mutableName = const_cast<ast::Name&>(n); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableName.setType(out); mutableName.setCanonicalKey(std::string("n:") + n.id);
  }
  // NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
  void visit(const ast::Unary& unaryNode) override {
    const ast::Expr* operandExpr = unaryNode.operand.get();
    if (operandExpr == nullptr) { addDiag(*diags, "null operand", &unaryNode); ok = false; return; }
    ExpressionTyper sub{*env, *sigs, *retParamIdxs, *diags};
    operandExpr->accept(sub); if (!sub.ok) { ok = false; return; }
    const uint32_t iMask = TypeEnv::maskForKind(Type::Int);
    const uint32_t fMask = TypeEnv::maskForKind(Type::Float);
    const uint32_t bMask = TypeEnv::maskForKind(Type::Bool);
    auto isSubset = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
    const uint32_t maskVal = (sub.outSet != 0U) ? sub.outSet : TypeEnv::maskForKind(sub.out);
    if (unaryNode.op == ast::UnaryOperator::Neg) {
      if (isSubset(maskVal, iMask)) { out = Type::Int; outSet = iMask; }
      else if (isSubset(maskVal, fMask)) { out = Type::Float; outSet = fMask; }
      else { addDiag(*diags, "unary '-' requires int or float", &unaryNode); ok = false; return; }
      auto& mutableUnary = const_cast<ast::Unary&>(unaryNode); // NOLINT
      mutableUnary.setType(out);
      if (unaryNode.operand) { const auto& can = unaryNode.operand->canonical(); if (can) { mutableUnary.setCanonicalKey("u:neg:(" + *can + ")"); } }
      return;
    }
    // 'not'
    if (!isSubset(maskVal, bMask)) { addDiag(*diags, "'not' requires bool", &unaryNode); ok = false; return; }
    out = Type::Bool; outSet = bMask;
    auto& mutableUnary2 = const_cast<ast::Unary&>(unaryNode); // NOLINT
    mutableUnary2.setType(out);
    if (unaryNode.operand) { const auto& can2 = unaryNode.operand->canonical(); if (can2) { mutableUnary2.setCanonicalKey("u:not:(" + *can2 + ")"); } }
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
      const uint32_t iMask = TypeEnv::maskForKind(Type::Int);
      const uint32_t fMask = TypeEnv::maskForKind(Type::Float);
      const uint32_t lMask = (lhsTyper.outSet != 0U) ? lhsTyper.outSet : TypeEnv::maskForKind(lhsTyper.out);
      const uint32_t rMask = (rhsTyper.outSet != 0U) ? rhsTyper.outSet : TypeEnv::maskForKind(rhsTyper.out);
      if (lMask == iMask && rMask == iMask) { out = Type::Int; outSet = iMask; return; }
      if (binaryNode.op != ast::BinaryOperator::Mod && lMask == fMask && rMask == fMask) { out = Type::Float; outSet = fMask; return; }
      const uint32_t numMask = iMask | fMask;
      auto subMask = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
      if (subMask(lMask, numMask) && subMask(rMask, numMask)) { addDiag(*diags, "ambiguous numeric types; both operands must be int or both float", &binaryNode); ok = false; return; }
      addDiag(*diags, "arithmetic operands must both be int or both be float (mod only for int)", &binaryNode); ok = false; return;
    }
    // Comparisons
    if (binaryNode.op == ast::BinaryOperator::Eq || binaryNode.op == ast::BinaryOperator::Ne || binaryNode.op == ast::BinaryOperator::Lt || binaryNode.op == ast::BinaryOperator::Le || binaryNode.op == ast::BinaryOperator::Gt || binaryNode.op == ast::BinaryOperator::Ge) {
      // Allow eq/ne None comparisons regardless of other type
      if ((binaryNode.op == ast::BinaryOperator::Eq || binaryNode.op == ast::BinaryOperator::Ne) &&
          (binaryNode.lhs->kind == ast::NodeKind::NoneLiteral || binaryNode.rhs->kind == ast::NodeKind::NoneLiteral)) {
        out = Type::Bool; auto& mutableBinary = const_cast<ast::Binary&>(binaryNode); // NOLINT(cppcoreguidelines-pro-type-const-cast)
        mutableBinary.setType(out);
        if (binaryNode.lhs && binaryNode.rhs) {
          const auto& lcan = binaryNode.lhs->canonical();
          const auto& rcan = binaryNode.rhs->canonical();
          if (lcan && rcan) { mutableBinary.setCanonicalKey("cmp_none:(" + *lcan + "," + *rcan + ")"); }
        }
        return;
      }
      const uint32_t iMask = TypeEnv::maskForKind(Type::Int);
      const uint32_t fMask = TypeEnv::maskForKind(Type::Float);
      const uint32_t lMask = (lhsTyper.outSet != 0U) ? lhsTyper.outSet : TypeEnv::maskForKind(lhsTyper.out);
      const uint32_t rMask = (rhsTyper.outSet != 0U) ? rhsTyper.outSet : TypeEnv::maskForKind(rhsTyper.out);
      const bool bothInt = (lMask == iMask) && (rMask == iMask);
      const bool bothFloat = (lMask == fMask) && (rMask == fMask);
      if (!(bothInt || bothFloat)) { addDiag(*diags, "comparison operands must both be int or both be float", &binaryNode); ok = false; return; }
      out = Type::Bool; auto& mutableBinary = const_cast<ast::Binary&>(binaryNode); // NOLINT(cppcoreguidelines-pro-type-const-cast)
      mutableBinary.setType(out);
      if (binaryNode.lhs && binaryNode.rhs) {
        const auto& lcan = binaryNode.lhs->canonical();
        const auto& rcan = binaryNode.rhs->canonical();
        if (lcan && rcan) { mutableBinary.setCanonicalKey("cmp:(" + *lcan + "," + *rcan + ")"); }
      }
      return;
    }
    // Logical
    if (binaryNode.op == ast::BinaryOperator::And || binaryNode.op == ast::BinaryOperator::Or) {
      const uint32_t bMask = TypeEnv::maskForKind(Type::Bool);
      const uint32_t lMask = (lhsTyper.outSet != 0U) ? lhsTyper.outSet : TypeEnv::maskForKind(lhsTyper.out);
      const uint32_t rMask = (rhsTyper.outSet != 0U) ? rhsTyper.outSet : TypeEnv::maskForKind(rhsTyper.out);
      auto isSubset = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
      if (!isSubset(lMask, bMask) || !isSubset(rMask, bMask)) { addDiag(*diags, "logical operands must be bool", &binaryNode); ok = false; return; }
      out = Type::Bool; outSet = bMask; auto& mutableBinary2 = const_cast<ast::Binary&>(binaryNode); // NOLINT
      mutableBinary2.setType(out);
      if (binaryNode.lhs && binaryNode.rhs) {
        const auto& lcan = binaryNode.lhs->canonical();
        const auto& rcan = binaryNode.rhs->canonical();
        if (lcan && rcan) { mutableBinary2.setCanonicalKey("log:(" + *lcan + "," + *rcan + ")"); }
      }
      return;
    }
    // Arithmetic (typed) — set canonical for safe recognition
    if ((binaryNode.op == ast::BinaryOperator::Add || binaryNode.op == ast::BinaryOperator::Sub || binaryNode.op == ast::BinaryOperator::Mul || binaryNode.op == ast::BinaryOperator::Div || binaryNode.op == ast::BinaryOperator::Mod) && ( (typeIsInt(lhsTyper.out)&&typeIsInt(rhsTyper.out)) || (typeIsFloat(lhsTyper.out)&&typeIsFloat(rhsTyper.out)) )) {
      auto& mutableBinary3 = const_cast<ast::Binary&>(binaryNode); // NOLINT(cppcoreguidelines-pro-type-const-cast)
      mutableBinary3.setType(typeIsInt(lhsTyper.out) ? Type::Int : Type::Float);
      if (binaryNode.lhs && binaryNode.rhs) {
        const auto& lcan = binaryNode.lhs->canonical();
        const auto& rcan = binaryNode.rhs->canonical();
        if (lcan && rcan) {
          const char* opStr = "?";
          switch (binaryNode.op) {
            case ast::BinaryOperator::Add: opStr = "+"; break;
            case ast::BinaryOperator::Sub: opStr = "-"; break;
            case ast::BinaryOperator::Mul: opStr = "*"; break;
            case ast::BinaryOperator::Div: opStr = "/"; break;
            case ast::BinaryOperator::Mod: opStr = "%"; break;
            default: break;
          }
          mutableBinary3.setCanonicalKey(std::string("bin:") + opStr + ":(" + *lcan + "," + *rcan + ")");
        }
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
  void visit(const ast::IfExpr& ife) override {
    // test must be bool
    ExpressionTyper testTyper{*env, *sigs, *retParamIdxs, *diags};
    if (ife.test) { ife.test->accept(testTyper); } else { addDiag(*diags, "if-expression missing condition", &ife); ok = false; return; }
    if (!testTyper.ok) { ok = false; return; }
    const uint32_t bMask = TypeEnv::maskForKind(Type::Bool);
    const uint32_t tMask = (testTyper.outSet != 0U) ? testTyper.outSet : TypeEnv::maskForKind(testTyper.out);
    auto isSubset = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
    if (!isSubset(tMask, bMask)) { addDiag(*diags, "if-expression condition must be bool", &ife); ok = false; return; }
    // then and else must match
    ExpressionTyper thenTyper{*env, *sigs, *retParamIdxs, *diags};
    ExpressionTyper elseTyper{*env, *sigs, *retParamIdxs, *diags};
    if (!ife.body || !ife.orelse) { addDiag(*diags, "if-expression requires both arms", &ife); ok = false; return; }
    ife.body->accept(thenTyper); if (!thenTyper.ok) { ok = false; return; }
    ife.orelse->accept(elseTyper); if (!elseTyper.ok) { ok = false; return; }
    if (thenTyper.out != elseTyper.out) { addDiag(*diags, "if-expression branches must have same type", &ife); ok = false; return; }
    out = thenTyper.out;
    outSet = (thenTyper.outSet != 0U) ? thenTyper.outSet : TypeEnv::maskForKind(out);
    auto& m = const_cast<ast::IfExpr&>(ife);
    m.setType(out);
  }
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  void visit(const ast::Call& callNode) override {
    if (!callNode.callee || callNode.callee->kind != ast::NodeKind::Name) { addDiag(*diags, "unsupported callee expression", &callNode); ok = false; return; }
    const auto* nameNode = static_cast<const ast::Name*>(callNode.callee.get());
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
      out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); // NOLINT(cppcoreguidelines-pro-type-const-cast) treat as string pointer result
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
      auto retIdxIt = retParamIdxs->find(cname->id);
      if (retIdxIt != retParamIdxs->end()) {
        const int idx = retIdxIt->second;
        if (idx >= 0 && static_cast<size_t>(idx) < callNode.args.size()) {
          const auto& arg = callNode.args[idx];
          if (arg) { const auto& can = arg->canonical(); if (can) { mutableCall.setCanonicalKey(*can); } }
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

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
bool Sema::check(ast::Module& mod, std::vector<Diagnostic>& diags) {
  std::unordered_map<std::string, Sig> sigs;
  for (const auto& func : mod.functions) {
    Sig sig; sig.ret = func->returnType;
    for (const auto& param : func->params) { sig.params.push_back(param.type); }
    sigs[func->name] = std::move(sig);
  }
  // Build a trivial interprocedural summary: which function consistently returns a specific parameter index
  std::unordered_map<std::string, int> retParamIdxs; // func -> param index
  struct RetIdxVisitor : public ast::VisitorBase {
    const ast::FunctionDef* fn{nullptr};
    int retIdx{-1}; bool hasReturn{false}; bool consistent{true};
    void visit(const ast::ReturnStmt& ret) override {
      if (!consistent) { return; }
      hasReturn = true;
      if (!(ret.value && ret.value->kind == ast::NodeKind::Name)) { consistent = false; return; }
      const auto* nameNode = static_cast<const ast::Name*>(ret.value.get());
      int idxFound = -1;
      for (size_t i = 0; i < fn->params.size(); ++i) { if (fn->params[i].name == nameNode->id) { idxFound = static_cast<int>(i); break; } }
      if (idxFound < 0) { consistent = false; return; }
      if (retIdx < 0) { retIdx = idxFound; }
      else if (retIdx != idxFound) { consistent = false; }
    }
    void visit(const ast::IfStmt& iff) override {
      for (const auto& stmt : iff.thenBody) { stmt->accept(*this); }
      for (const auto& stmt : iff.elseBody) { stmt->accept(*this); }
    }
    // No-ops for others
    void visit(const ast::Module& module) override { (void)module; }
    void visit(const ast::FunctionDef& functionDef) override { (void)functionDef; }
    void visit(const ast::AssignStmt& assign) override { (void)assign; }
    void visit(const ast::ExprStmt& exprStmt) override { (void)exprStmt; }
    void visit(const ast::IntLiteral& intLit) override { (void)intLit; }
    void visit(const ast::BoolLiteral& boolLit) override { (void)boolLit; }
    void visit(const ast::FloatLiteral& floatLit) override { (void)floatLit; }
    void visit(const ast::StringLiteral& strLit) override { (void)strLit; }
    void visit(const ast::NoneLiteral& noneLit) override { (void)noneLit; }
    void visit(const ast::Name& name) override { (void)name; }
    void visit(const ast::Call& call) override { (void)call; }
    void visit(const ast::Binary& bin) override { (void)bin; }
    void visit(const ast::Unary& unary) override { (void)unary; }
    void visit(const ast::TupleLiteral& tuple) override { (void)tuple; }
    void visit(const ast::ListLiteral& list) override { (void)list; }
    void visit(const ast::ObjectLiteral& obj) override { (void)obj; }
  };
  for (const auto& func : mod.functions) {
    RetIdxVisitor visitor; visitor.fn = func.get();
    for (const auto& stmt : func->body) { stmt->accept(visitor); if (!visitor.consistent) { break; } }
    if (visitor.hasReturn && visitor.consistent && visitor.retIdx >= 0) { retParamIdxs[func->name] = visitor.retIdx; }
  }

  for (const auto& func : mod.functions) {
    if (!(typeIsInt(func->returnType) || typeIsBool(func->returnType) || typeIsFloat(func->returnType) || typeIsStr(func->returnType) || func->returnType == Type::Tuple)) { Diagnostic diagVar; diagVar.message = "only int/bool/float/str/tuple returns supported"; diags.push_back(std::move(diagVar)); return false; }
    TypeEnv env;
    for (const auto& param : func->params) {
      if (!(typeIsInt(param.type) || typeIsBool(param.type) || typeIsFloat(param.type) || typeIsStr(param.type))) { Diagnostic diagVar; diagVar.message = "only int/bool/float/str params supported"; diags.push_back(std::move(diagVar)); return false; }
      env.define(param.name, param.type, {func->name, 0, 0});
    }
    struct StmtChecker : public ast::VisitorBase {
      StmtChecker(const ast::FunctionDef& fn_, const std::unordered_map<std::string, Sig>& sigs_,
                  const std::unordered_map<std::string, int>& retParamIdxs_, TypeEnv& env_, std::vector<Diagnostic>& diags_)
        : fn(fn_), sigs(sigs_), retParamIdxs(retParamIdxs_), env(env_), diags(diags_) {}
      const ast::FunctionDef& fn; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
      const std::unordered_map<std::string, Sig>& sigs; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
      const std::unordered_map<std::string, int>& retParamIdxs; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
      TypeEnv& env; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
      std::vector<Diagnostic>& diags; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
      bool ok{true};

      bool infer(const ast::Expr* expr, Type& out) { return inferExprType(expr, env, sigs, retParamIdxs, out, diags); }

      void visit(const ast::AssignStmt& assignStmt) override {
        // Infer value with set awareness
        ExpressionTyper valTyper{env, sigs, retParamIdxs, diags};
        if (assignStmt.value) { assignStmt.value->accept(valTyper); } else { ok = false; return; }
        if (!valTyper.ok) { ok = false; return; }
        const Type typeOut = valTyper.out;
        const bool allowed = typeIsInt(typeOut) || typeIsBool(typeOut) || typeIsFloat(typeOut) || typeIsStr(typeOut) || typeOut == Type::List || typeOut == Type::NoneType;
        if (!allowed) {
          addDiag(diags, "only int/bool/float/str/list variables supported", &assignStmt); ok = false; return;
        }
        // Define from set if available, else exact kind
        const uint32_t maskVal = (valTyper.outSet != 0U) ? valTyper.outSet : TypeEnv::maskForKind(typeOut);
        env.defineSet(assignStmt.target, maskVal, {assignStmt.file, assignStmt.line, assignStmt.col});
      }

      // NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
      void visit(const ast::IfStmt& iff) override {
        Type condType{}; if (!infer(iff.cond.get(), condType)) { ok = false; return; }
        if (!typeIsBool(condType)) { addDiag(diags, "if condition must be bool", &iff); ok = false; return; }
        TypeEnv thenL = env;
        TypeEnv elseL = env;
        // Visitor-based condition refinement
        struct ConditionRefiner : public ast::VisitorBase {
          struct Envs { TypeEnv& thenEnv; TypeEnv& elseEnv; };
          TypeEnv& thenEnv; TypeEnv& elseEnv;
          // Track variables refined in each branch and when refinement is via '== None'/'!= None'
          std::vector<std::pair<std::string, uint32_t>> thenRefined;
          std::vector<std::pair<std::string, uint32_t>> elseRefined;
          std::vector<std::string> thenNoneEq;
          std::vector<std::string> elseNoneEq;
          explicit ConditionRefiner(Envs envs) : thenEnv(envs.thenEnv), elseEnv(envs.elseEnv) {}
          static Type typeFromName(const std::string& ident) {
            if (ident == "int") { return Type::Int; }
            if (ident == "bool") { return Type::Bool; }
            if (ident == "float") { return Type::Float; }
            if (ident == "str") { return Type::Str; }
            return Type::NoneType;
          }
          // isinstance(x, T) => then: x:T
          void visit(const ast::Call& call) override {
            if (!(call.callee && call.callee->kind == ast::NodeKind::Name)) { return; }
            const auto* callee = static_cast<const ast::Name*>(call.callee.get());
            if (callee->id != "isinstance" || call.args.size() != 2) { return; }
            if (!(call.args[0] && call.args[0]->kind == ast::NodeKind::Name)) { return; }
            if (!(call.args[1] && call.args[1]->kind == ast::NodeKind::Name)) { return; }
            const auto* var = static_cast<const ast::Name*>(call.args[0].get());
            const auto* tnm = static_cast<const ast::Name*>(call.args[1].get());
            const Type newType = typeFromName(tnm->id);
            if (newType != Type::NoneType) { thenEnv.restrictToKind(var->id, newType); thenEnv.define(var->id, newType, {var->file, var->line, var->col}); }
          }
          // x == None => then x: NoneType; x != None => else x: NoneType
          // NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
          void visit(const ast::Binary& bin) override {
            // Logical approximations:
            // - For A and B: then-branch applies both A and B refinements (safe);
            //   else-branch left unchanged (unspecified which subexpr fails).
            // - For A or B: else-branch would be (!A and !B) which requires negative types;
            //   leave conservative (no refinement) because TypeEnv has no "not None" type.
            if (bin.op == ast::BinaryOperator::And) {
              if (bin.lhs) { bin.lhs->accept(*this); }
              if (bin.rhs) { bin.rhs->accept(*this); }
              return;
            }
            if (bin.op == ast::BinaryOperator::Or) { applyNegExpr(bin.lhs.get()); applyNegExpr(bin.rhs.get()); return; }
            auto refineEq = [&](const ast::Expr* lhs, const ast::Expr* rhs, bool toThen) {
              if (lhs && lhs->kind == ast::NodeKind::Name && rhs && rhs->kind == ast::NodeKind::NoneLiteral) {
                const auto* nameNode = static_cast<const ast::Name*>(lhs);
                auto& envRef = toThen ? thenEnv : elseEnv;
                envRef.define(nameNode->id, Type::NoneType, {nameNode->file, nameNode->line, nameNode->col});
                const uint32_t maskNow = envRef.getSet(nameNode->id);
                if (toThen) thenRefined.emplace_back(nameNode->id, maskNow); else elseRefined.emplace_back(nameNode->id, maskNow);
                if (toThen) thenNoneEq.emplace_back(nameNode->id); else elseNoneEq.emplace_back(nameNode->id);
              }
            };
            if (bin.op == ast::BinaryOperator::Eq) {
              refineEq(bin.lhs.get(), bin.rhs.get(), true);
              refineEq(bin.rhs.get(), bin.lhs.get(), true);
            } else if (bin.op == ast::BinaryOperator::Ne) {
              refineEq(bin.lhs.get(), bin.rhs.get(), false);
              refineEq(bin.rhs.get(), bin.lhs.get(), false);
            }
          }
          // not(<expr>): swap then/else refinements for eq/ne None
          // NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
          void visit(const ast::Unary& unary) override {
            if (unary.op != ast::UnaryOperator::Not || !unary.operand) { return; }
            if (unary.operand->kind == ast::NodeKind::BinaryExpr) {
              const auto* bin = static_cast<const ast::Binary*>(unary.operand.get());
              if (bin->op == ast::BinaryOperator::Eq || bin->op == ast::BinaryOperator::Ne) {
                // Create a temporary refiner with swapped envs
                ConditionRefiner swapped{ConditionRefiner::Envs{elseEnv, thenEnv}};
                unary.operand->accept(swapped);
              }
            } else if (unary.operand->kind == ast::NodeKind::Call) {
              // not isinstance(x,T): then exclude T; else restrict to T
              const auto* call = static_cast<const ast::Call*>(unary.operand.get());
              if (call->callee && call->callee->kind == ast::NodeKind::Name && call->args.size() == 2 && call->args[0] && call->args[0]->kind == ast::NodeKind::Name && call->args[1] && call->args[1]->kind == ast::NodeKind::Name) {
                const auto* callee = static_cast<const ast::Name*>(call->callee.get());
                if (callee->id == "isinstance") {
                  const auto* var = static_cast<const ast::Name*>(call->args[0].get());
                  const auto* tnm = static_cast<const ast::Name*>(call->args[1].get());
                  const Type newType = typeFromName(tnm->id);
                  if (newType != Type::NoneType) {
                    thenEnv.excludeKind(var->id, newType);
                    elseEnv.restrictToKind(var->id, newType);
                    thenRefined.emplace_back(var->id, thenEnv.getSet(var->id));
                    elseRefined.emplace_back(var->id, elseEnv.getSet(var->id));
                  }
                }
              }
            }
          }
          // Apply negation refinement to a subexpression (for else-branch of OR)
          // NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
          void applyNegExpr(const ast::Expr* expr) {
            if (!expr) { return; }
            if (expr->kind == ast::NodeKind::BinaryExpr) {
              const auto* binaryNode = static_cast<const ast::Binary*>(expr);
              if (binaryNode->op == ast::BinaryOperator::Eq) {
                auto setNN = [&](const ast::Expr* lhs, const ast::Expr* rhs) {
                  if (lhs && lhs->kind == ast::NodeKind::Name && rhs && rhs->kind == ast::NodeKind::NoneLiteral) {
                    const auto* nameNode = static_cast<const ast::Name*>(lhs);
                    elseEnv.markNonNone(nameNode->id);
                  }
                };
                setNN(binaryNode->lhs.get(), binaryNode->rhs.get());
                setNN(binaryNode->rhs.get(), binaryNode->lhs.get());
                return;
              }
              if (binaryNode->op == ast::BinaryOperator::Ne) {
                auto setNone = [&](const ast::Expr* lhs, const ast::Expr* rhs) {
                  if (lhs && lhs->kind == ast::NodeKind::Name && rhs && rhs->kind == ast::NodeKind::NoneLiteral) {
                    const auto* nameNode = static_cast<const ast::Name*>(lhs);
                    elseEnv.define(nameNode->id, Type::NoneType, {nameNode->file, nameNode->line, nameNode->col});
                    elseRefined.emplace_back(nameNode->id, elseEnv.getSet(nameNode->id));
                    elseNoneEq.emplace_back(nameNode->id);
                  }
                };
                setNone(binaryNode->lhs.get(), binaryNode->rhs.get());
                setNone(binaryNode->rhs.get(), binaryNode->lhs.get());
                return;
              }
            }
            if (expr->kind == ast::NodeKind::Call) {
              const auto* callNode = static_cast<const ast::Call*>(expr);
              if (callNode->callee && callNode->callee->kind == ast::NodeKind::Name) {
                const auto* cal = static_cast<const ast::Name*>(callNode->callee.get());
                if (cal->id == "isinstance" && callNode->args.size() == 2 && callNode->args[0] && callNode->args[0]->kind == ast::NodeKind::Name && callNode->args[1] && callNode->args[1]->kind == ast::NodeKind::Name) {
                  const auto* var = static_cast<const ast::Name*>(callNode->args[0].get());
                  const auto* tnm = static_cast<const ast::Name*>(callNode->args[1].get());
                  const Type newType2 = typeFromName(tnm->id);
                  if (newType2 != Type::NoneType) { elseEnv.excludeKind(var->id, newType2); }
                  return;
                }
              }
            }
            if (expr->kind == ast::NodeKind::UnaryExpr) {
              const auto* unaryExpr = static_cast<const ast::Unary*>(expr);
              if (unaryExpr->op == ast::UnaryOperator::Not && unaryExpr->operand) { applyNegExpr(unaryExpr->operand.get()); }
            }
          }

          // No-ops
          void visit(const ast::Module& module) override { (void)module; }
          void visit(const ast::FunctionDef& functionDef) override { (void)functionDef; }
          void visit(const ast::ReturnStmt& ret) override { (void)ret; }
          void visit(const ast::AssignStmt& assign) override { (void)assign; }
          void visit(const ast::ExprStmt& exprStmt) override { (void)exprStmt; }
          void visit(const ast::IntLiteral& intLit) override { (void)intLit; }
          void visit(const ast::BoolLiteral& boolLit) override { (void)boolLit; }
          void visit(const ast::FloatLiteral& floatLit) override { (void)floatLit; }
          void visit(const ast::StringLiteral& strLit) override { (void)strLit; }
          void visit(const ast::NoneLiteral& noneLit) override { (void)noneLit; }
          void visit(const ast::Name& name) override { (void)name; }
          void visit(const ast::TupleLiteral& tupleLit) override { (void)tupleLit; }
          void visit(const ast::ListLiteral& listLit) override { (void)listLit; }
          void visit(const ast::ObjectLiteral& objLit) override { (void)objLit; }
          void visit(const ast::IfStmt& ifStmt2) override { (void)ifStmt2; }
        };
        bool skipThen = false;
        bool skipElse = false;
        if (iff.cond) {
          ConditionRefiner ref{ConditionRefiner::Envs{thenL, elseL}};
          iff.cond->accept(ref);
          auto contradictoryNoneEq = [&](const std::vector<std::string>& names, const TypeEnv& branchEnv) {
            for (const auto& nm : names) {
              const uint32_t base = env.getSet(nm);
              const uint32_t bran = branchEnv.getSet(nm);
              if (base != 0U && bran != 0U && ((base & bran) == 0U)) { return true; }
            }
            return false;
          };
          skipThen = contradictoryNoneEq(ref.thenNoneEq, thenL);
          // Do not skip else even if contradictory for current policy;
          // tests expect else-branch errors to be reported for `x != None` cases.
          skipElse = false;
        }
        // then/else branches via a tiny visitor
        struct BranchChecker : public ast::VisitorBase {
          StmtChecker& parent; TypeEnv& envRef; const Type fnRet; bool& okRef;
          BranchChecker(StmtChecker& parentIn, TypeEnv& envIn, Type retType, bool& okIn)
              : parent(parentIn), envRef(envIn), fnRet(retType), okRef(okIn) {}
          bool inferLocal(const ast::Expr* expr, Type& outTy) {
            if (expr == nullptr) { addDiag(parent.diags, "null expression", nullptr); return false; }
            ExpressionTyper v{envRef, parent.sigs, parent.retParamIdxs, parent.diags};
            expr->accept(v);
            if (!v.ok) { return false; }
            outTy = v.out;
            return true;
          }
          void visit(const ast::AssignStmt& assignStmt) override {
            Type tmpType{}; if (!inferLocal(assignStmt.value.get(), tmpType)) { okRef = false; return; }
            envRef.define(assignStmt.target, tmpType, {assignStmt.file, assignStmt.line, assignStmt.col});
          }
          void visit(const ast::ReturnStmt& ret) override {
            Type tmpType{}; if (!inferLocal(ret.value.get(), tmpType)) { okRef = false; return; }
            if (tmpType != fnRet) { addDiag(parent.diags, "return type mismatch in branch", &ret); okRef = false; }
          }
          // No-ops
          void visit(const ast::Module& module) override { (void)module; }
          void visit(const ast::FunctionDef& functionDef) override { (void)functionDef; }
          void visit(const ast::IfStmt& ifStmt2) override { (void)ifStmt2; }
          void visit(const ast::ExprStmt& exprStmt) override { (void)exprStmt; }
          void visit(const ast::IntLiteral& intLit) override { (void)intLit; }
          void visit(const ast::BoolLiteral& boolLit) override { (void)boolLit; }
          void visit(const ast::FloatLiteral& floatLit) override { (void)floatLit; }
          void visit(const ast::StringLiteral& strLit) override { (void)strLit; }
          void visit(const ast::NoneLiteral& noneLit) override { (void)noneLit; }
          void visit(const ast::Name& name) override { (void)name; }
          void visit(const ast::Call& callNode) override { (void)callNode; }
          void visit(const ast::Binary& bin) override { (void)bin; }
          void visit(const ast::Unary& unaryNode) override { (void)unaryNode; }
          void visit(const ast::TupleLiteral& tupleLit) override { (void)tupleLit; }
          void visit(const ast::ListLiteral& listLit) override { (void)listLit; }
          void visit(const ast::ObjectLiteral& objLit) override { (void)objLit; }
        };
        BranchChecker thenChecker{*this, thenL, fn.returnType, ok};
        if (!skipThen) { for (const auto& stmt2 : iff.thenBody) { if (!ok) { break; } stmt2->accept(thenChecker); } }
        BranchChecker elseChecker{*this, elseL, fn.returnType, ok};
        if (!skipElse) { for (const auto& stmt2 : iff.elseBody) { if (!ok) { break; } stmt2->accept(elseChecker); } }
        // Merge back to current env
        if (skipThen && !skipElse) {
          TypeEnv merged; merged.intersectFrom(elseL, elseL); env = merged;
        } else if (skipElse && !skipThen) {
          TypeEnv merged; merged.intersectFrom(thenL, thenL); env = merged;
        } else {
          // Intersect types present in both branches
          env.intersectFrom(thenL, elseL);
        }
      }

      void visit(const ast::ReturnStmt& retStmt) override {
        Type valueType{}; if (!infer(retStmt.value.get(), valueType)) { ok = false; return; }
        if (valueType != fn.returnType) { addDiag(diags, std::string("return type mismatch in function: ") + fn.name, &retStmt); ok = false; }
      }

      void visit(const ast::ExprStmt& stmt) override { if (stmt.value) { Type tmp{}; (void)infer(stmt.value.get(), tmp); } }

      // Unused in stmt context; name parameters for readability
      void visit(const ast::Module& moduleNode) override { (void)moduleNode; }
      void visit(const ast::FunctionDef& functionNode) override { (void)functionNode; }
      void visit(const ast::IntLiteral& intNode) override { (void)intNode; }
      void visit(const ast::BoolLiteral& boolNode) override { (void)boolNode; }
      void visit(const ast::FloatLiteral& floatNode) override { (void)floatNode; }
      void visit(const ast::Name& nameNode) override { (void)nameNode; }
      void visit(const ast::Call& callNode2) override { (void)callNode2; }
      void visit(const ast::Binary& binaryNode2) override { (void)binaryNode2; }
      void visit(const ast::Unary& unaryNode2) override { (void)unaryNode2; }
      void visit(const ast::StringLiteral& strNode) override { (void)strNode; }
      void visit(const ast::TupleLiteral& tupleNode) override { (void)tupleNode; }
      void visit(const ast::ListLiteral& listNode) override { (void)listNode; }
      void visit(const ast::ObjectLiteral& objNode) override { (void)objNode; }
      void visit(const ast::NoneLiteral& noneNode) override { (void)noneNode; }
    };

    StmtChecker checker{*func, sigs, retParamIdxs, env, diags};
    for (const auto& stmt : func->body) {
      stmt->accept(checker);
      if (!checker.ok) { return false; }
    }
  }
  return diags.empty();
}

} // namespace pycc::sema
