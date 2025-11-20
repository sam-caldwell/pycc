/***
 * Name: pycc::sema::Sema (impl)
 * Purpose: Minimal semantic checks with basic type env and source spans.
 */
#include "sema/Sema.h"
#include "sema/TypeEnv.h"
#include <cstddef>
#include <cstdint>
#include <ios>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ast/AssignStmt.h"
#include "ast/Attribute.h"
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

struct SigParam {
  std::string name;
  Type type{Type::NoneType};
  bool isVarArg{false};
  bool isKwVarArg{false};
  bool isKwOnly{false};
  bool isPosOnly{false};
  bool hasDefault{false};
  // Rich annotation info
  uint32_t unionMask{0U};     // allowed argument kinds (bitmask). 0 => just 'type'
  uint32_t listElemMask{0U};  // if type==List and list[T] was annotated, mask of T
};
struct Sig { Type ret{Type::NoneType}; std::vector<Type> params; std::vector<SigParam> full; };

// Simple class info for semantic method and dunder checks
struct ClassInfo {
  std::vector<std::string> bases;
  std::unordered_map<std::string, Sig> methods; // method name -> signature
};

namespace {
// Group polymorphic target maps to avoid adjacent, easily-swappable parameters.
struct PolyPtrs {
  const std::unordered_map<std::string, std::unordered_set<std::string>>* vars{nullptr};
  const std::unordered_map<std::string, std::unordered_set<std::string>>* attrs{nullptr};
};
struct PolyRefs {
  std::unordered_map<std::string, std::unordered_set<std::string>>& vars;
  std::unordered_map<std::string, std::unordered_set<std::string>>& attrs;
};
} // namespace

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
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  // Back-compat constructor without classes map
  ExpressionTyper(const TypeEnv& env_, const std::unordered_map<std::string, Sig>& sigs_,
                  const std::unordered_map<std::string, int>& retParamIdxs_, std::vector<Diagnostic>& diags_,
                  PolyPtrs polyIn = {}, const std::vector<const TypeEnv*>* outerScopes_ = nullptr)
    : env(&env_), sigs(&sigs_), retParamIdxs(&retParamIdxs_), diags(&diags_), polyTargets(polyIn), outers(outerScopes_), classes(nullptr) {}
  // Extended constructor with classes map
  ExpressionTyper(const TypeEnv& env_, const std::unordered_map<std::string, Sig>& sigs_,
                  const std::unordered_map<std::string, int>& retParamIdxs_, std::vector<Diagnostic>& diags_,
                  PolyPtrs polyIn, const std::vector<const TypeEnv*>* outerScopes_,
                  const std::unordered_map<std::string, ClassInfo>* classes_)
    : env(&env_), sigs(&sigs_), retParamIdxs(&retParamIdxs_), diags(&diags_), polyTargets(polyIn), outers(outerScopes_), classes(classes_) {}
  const TypeEnv* env{nullptr};
  const std::unordered_map<std::string, Sig>* sigs{nullptr};
  const std::unordered_map<std::string, int>* retParamIdxs{nullptr};
  std::vector<Diagnostic>* diags{nullptr};
  PolyPtrs polyTargets{};
  const std::vector<const TypeEnv*>* outers{nullptr};
  const std::unordered_map<std::string, ClassInfo>* classes{nullptr};
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
  void visit(const ast::Attribute& attr) override {
    if (attr.value) {
      ExpressionTyper v{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers};
      attr.value->accept(v); if (!v.ok) { ok = false; return; }
    }
    // If base is a simple name and we have a recorded attribute type, use it; else keep opaque
    out = Type::NoneType; outSet = 0U;
    if (attr.value && attr.value->kind == ast::NodeKind::Name) {
      const auto* base = static_cast<const ast::Name*>(attr.value.get());
      const uint32_t msk = env->getAttr(base->id, attr.attr);
      if (msk != 0U) { outSet = msk; if (TypeEnv::isSingleMask(msk)) out = TypeEnv::kindFromMask(msk); }
    }
    auto& m = const_cast<ast::Attribute&>(attr); m.setType(out);
  }
  void visit(const ast::Subscript& sub) override {
    // value[index] typing for list/str/tuple/dict; sets are not subscriptable
    if (!sub.value) { addDiag(*diags, "null subscript", &sub); ok = false; return; }
    // Reject set literals directly
    if (sub.value->kind == ast::NodeKind::SetLiteral) { addDiag(*diags, "set is not subscriptable", &sub); ok = false; return; }
    ExpressionTyper v{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers};
    sub.value->accept(v); if (!v.ok) { ok = false; return; }
    const uint32_t vMask = (v.outSet != 0U) ? v.outSet : TypeEnv::maskForKind(v.out);
    auto isSubset = [](uint32_t m, uint32_t allow){ return m && ((m & ~allow)==0U); };
    const uint32_t iMask = TypeEnv::maskForKind(Type::Int);
    const uint32_t strMask = TypeEnv::maskForKind(Type::Str);
    const uint32_t listMask = TypeEnv::maskForKind(Type::List);
    const uint32_t tupMask = TypeEnv::maskForKind(Type::Tuple);
    const uint32_t dictMask = TypeEnv::maskForKind(Type::Dict);
    // String indexing -> str; index must be int
    if (vMask == strMask) {
      if (sub.slice) { ExpressionTyper s{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; sub.slice->accept(s); if (!s.ok) { ok = false; return; } const uint32_t sMask = (s.outSet != 0U) ? s.outSet : TypeEnv::maskForKind(s.out); if (!isSubset(sMask, iMask)) { addDiag(*diags, "subscript index must be int", &sub); ok = false; return; } }
      out = Type::Str; outSet = strMask; auto& m = const_cast<ast::Subscript&>(sub); m.setType(out); return;
    }
    // List indexing -> element mask; index must be int
    if (vMask == listMask) {
      if (sub.slice) { ExpressionTyper s{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; sub.slice->accept(s); if (!s.ok) { ok = false; return; } const uint32_t sMask = (s.outSet != 0U) ? s.outSet : TypeEnv::maskForKind(s.out); if (!isSubset(sMask, iMask)) { addDiag(*diags, "subscript index must be int", &sub); ok = false; return; } }
      uint32_t elemMask = 0U;
      if (sub.value->kind == ast::NodeKind::Name) {
        const auto* nm = static_cast<const ast::Name*>(sub.value.get());
        elemMask = env->getListElems(nm->id);
      } else if (sub.value->kind == ast::NodeKind::ListLiteral) {
        const auto* lst = static_cast<const ast::ListLiteral*>(sub.value.get());
        for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; el->accept(et); if (!et.ok) { ok = false; return; } elemMask |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
      }
      if (elemMask != 0U) { outSet = elemMask; if (TypeEnv::isSingleMask(elemMask)) out = TypeEnv::kindFromMask(elemMask); }
      else { out = Type::NoneType; outSet = 0U; }
      auto& m = const_cast<ast::Subscript&>(sub); m.setType(out); return;
    }
    // Tuple indexing -> element type at index, or union when unknown; index must be int
    if (vMask == tupMask || sub.value->kind == ast::NodeKind::TupleLiteral) {
      if (sub.slice) { ExpressionTyper s{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; sub.slice->accept(s); if (!s.ok) { ok = false; return; } const uint32_t sMask = (s.outSet != 0U) ? s.outSet : TypeEnv::maskForKind(s.out); if (!isSubset(sMask, iMask)) { addDiag(*diags, "subscript index must be int", &sub); ok = false; return; } }
      uint32_t elemMask = 0U;
      size_t idxVal = static_cast<size_t>(-1);
      bool haveConstIdx = false;
      if (sub.slice && sub.slice->kind == ast::NodeKind::IntLiteral) { const auto* lit = static_cast<const ast::IntLiteral*>(sub.slice.get()); if (lit->value >= 0) { idxVal = static_cast<size_t>(lit->value); haveConstIdx = true; } }
      if (sub.value->kind == ast::NodeKind::Name) {
        const auto* nm = static_cast<const ast::Name*>(sub.value.get());
        if (haveConstIdx) { elemMask = env->getTupleElemAt(nm->id, idxVal); }
        if (elemMask == 0U) { elemMask = env->unionOfTupleElems(nm->id); }
      } else if (sub.value->kind == ast::NodeKind::TupleLiteral) {
        const auto* tup = static_cast<const ast::TupleLiteral*>(sub.value.get());
        if (haveConstIdx && idxVal < tup->elements.size()) {
          if (tup->elements[idxVal]) { ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; tup->elements[idxVal]->accept(et); if (!et.ok) { ok = false; return; } elemMask = (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
        } else {
          for (const auto& el : tup->elements) { if (!el) continue; ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; el->accept(et); if (!et.ok) { ok = false; return; } elemMask |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
        }
      }
      if (elemMask != 0U) { outSet = elemMask; if (TypeEnv::isSingleMask(elemMask)) out = TypeEnv::kindFromMask(elemMask); }
      else { out = Type::NoneType; outSet = 0U; }
      auto& m = const_cast<ast::Subscript&>(sub); m.setType(out); return;
    }
    // Dict indexing -> value mask; key type must match known key set when available
    if (vMask == dictMask || sub.value->kind == ast::NodeKind::DictLiteral) {
      uint32_t keyMask = 0U;
      uint32_t valMask = 0U;
      if (sub.value->kind == ast::NodeKind::Name) {
        const auto* nm = static_cast<const ast::Name*>(sub.value.get());
        keyMask = env->getDictKeys(nm->id);
        valMask = env->getDictVals(nm->id);
      } else if (sub.value->kind == ast::NodeKind::DictLiteral) {
        const auto* dl = static_cast<const ast::DictLiteral*>(sub.value.get());
        for (const auto& kv : dl->items) {
          if (kv.first) { ExpressionTyper kt{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; kv.first->accept(kt); if (!kt.ok) { ok = false; return; } keyMask |= (kt.outSet != 0U) ? kt.outSet : TypeEnv::maskForKind(kt.out); }
          if (kv.second) { ExpressionTyper vt{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; kv.second->accept(vt); if (!vt.ok) { ok = false; return; } valMask |= (vt.outSet != 0U) ? vt.outSet : TypeEnv::maskForKind(vt.out); }
        }
      }
      if (sub.slice) { ExpressionTyper s{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; sub.slice->accept(s); if (!s.ok) { ok = false; return; } const uint32_t sMask = (s.outSet != 0U) ? s.outSet : TypeEnv::maskForKind(s.out); if (keyMask != 0U && !isSubset(sMask, keyMask)) { addDiag(*diags, "dict key type mismatch", &sub); ok = false; return; } }
      if (valMask != 0U) { outSet = valMask; if (TypeEnv::isSingleMask(valMask)) out = TypeEnv::kindFromMask(valMask); }
      else { out = Type::NoneType; outSet = 0U; }
      auto& m = const_cast<ast::Subscript&>(sub); m.setType(out); return;
    }
    addDiag(*diags, "unsupported subscript target type", &sub); ok = false; return;
  }
  // NOLINTNEXTLINE(readability-function-size)
  void visit(const ast::ObjectLiteral& obj) override {
    out = Type::NoneType;
    for (const auto& field : obj.fields) {
      if (field) {
        ExpressionTyper sub{*env, *sigs, *retParamIdxs, *diags, polyTargets};
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
    uint32_t maskVal = env->getSet(n.id);
    if (maskVal == 0U && outers != nullptr) {
      for (const auto* o : *outers) { if (!o) continue; const uint32_t m = o->getSet(n.id); if (m != 0U) { maskVal = m; break; } }
    }
    if (maskVal == 0U) {
      // Try resolving from outer scopes only (free-variable reads)
      if (outers != nullptr) {
        uint32_t outerMask = 0U;
        for (const auto* o : *outers) { if (!o) continue; const uint32_t m = o->getSet(n.id); if (m != 0U) { outerMask = m; break; } }
        if (outerMask != 0U) { outSet = outerMask; if (TypeEnv::isSingleMask(outerMask)) out = TypeEnv::kindFromMask(outerMask); auto& nm = const_cast<ast::Name&>(n); nm.setType(out); nm.setCanonicalKey(std::string("n:") + n.id); return; }
        // Fall back to exact type in an outer scope when available
        std::optional<Type> oty;
        for (const auto* o : *outers) { if (!o) continue; auto t = o->get(n.id); if (t) { oty = t; break; } }
        if (oty) { out = *oty; outSet = TypeEnv::maskForKind(out); auto& nm = const_cast<ast::Name&>(n); nm.setType(out); nm.setCanonicalKey(std::string("n:") + n.id); return; }
      }
      // Contradictory or undefined at this scope
      addDiag(*diags, std::string("contradictory type for name: ") + n.id, &n);
      ok = false; return;
    }
    if (maskVal != 0U) {
      outSet = maskVal;
      if (TypeEnv::isSingleMask(maskVal)) { out = TypeEnv::kindFromMask(maskVal); }
    }
    auto resolvedType = env->get(n.id);
    if (!resolvedType && outSet == 0U) { addDiag(*diags, std::string("undefined name: ") + n.id, &n); ok = false; return; }
    if (TypeEnv::isSingleMask(outSet)) { out = TypeEnv::kindFromMask(outSet); }
    auto& mutableName = const_cast<ast::Name&>(n); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mutableName.setType(out); mutableName.setCanonicalKey(std::string("n:") + n.id);
  }
  // NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
  void visit(const ast::Unary& unaryNode) override {
    const ast::Expr* operandExpr = unaryNode.operand.get();
    if (operandExpr == nullptr) { addDiag(*diags, "null operand", &unaryNode); ok = false; return; }
    ExpressionTyper sub{*env, *sigs, *retParamIdxs, *diags, polyTargets};
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
    if (unaryNode.op == ast::UnaryOperator::BitNot) {
      if (!isSubset(maskVal, iMask)) { addDiag(*diags, "bitwise '~' requires int", &unaryNode); ok = false; return; }
      out = Type::Int; outSet = iMask;
      auto& mutableUnaryBN = const_cast<ast::Unary&>(unaryNode);
      mutableUnaryBN.setType(out);
      if (unaryNode.operand) { const auto& can = unaryNode.operand->canonical(); if (can) { mutableUnaryBN.setCanonicalKey("u:bitnot:(" + *can + ")"); } }
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
    ExpressionTyper lhsTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets};
    ExpressionTyper rhsTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets};
    binaryNode.lhs->accept(lhsTyper);
    if (!lhsTyper.ok) { ok = false; return; }
    binaryNode.rhs->accept(rhsTyper);
    if (!rhsTyper.ok) { ok = false; return; }
    // Arithmetic (incl. floor-div and pow)
    if (binaryNode.op == ast::BinaryOperator::Add || binaryNode.op == ast::BinaryOperator::Sub || binaryNode.op == ast::BinaryOperator::Mul || binaryNode.op == ast::BinaryOperator::Div || binaryNode.op == ast::BinaryOperator::Mod || binaryNode.op == ast::BinaryOperator::FloorDiv || binaryNode.op == ast::BinaryOperator::Pow) {
      const uint32_t iMask = TypeEnv::maskForKind(Type::Int);
      const uint32_t fMask = TypeEnv::maskForKind(Type::Float);
      const uint32_t sMask = TypeEnv::maskForKind(Type::Str);
      const uint32_t lMask = (lhsTyper.outSet != 0U) ? lhsTyper.outSet : TypeEnv::maskForKind(lhsTyper.out);
      const uint32_t rMask = (rhsTyper.outSet != 0U) ? rhsTyper.outSet : TypeEnv::maskForKind(rhsTyper.out);
      // String concatenation: only allow '+' for str + str
      if (binaryNode.op == ast::BinaryOperator::Add && lMask == sMask && rMask == sMask) {
        out = Type::Str; outSet = sMask; auto& mb = const_cast<ast::Binary&>(binaryNode); mb.setType(out);
        return;
      }
      if (lMask == iMask && rMask == iMask) { out = Type::Int; outSet = iMask; return; }
      if (binaryNode.op != ast::BinaryOperator::Mod && lMask == fMask && rMask == fMask) { out = Type::Float; outSet = fMask; return; }
      const uint32_t numMask = iMask | fMask;
      auto subMask = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
      if (subMask(lMask, numMask) && subMask(rMask, numMask)) { addDiag(*diags, "ambiguous numeric types; both operands must be int or both float", &binaryNode); ok = false; return; }
      addDiag(*diags, "arithmetic operands must both be int or both be float (mod only for int)", &binaryNode); ok = false; return;
    }
    // Bitwise and shifts: require ints
    if (binaryNode.op == ast::BinaryOperator::BitAnd || binaryNode.op == ast::BinaryOperator::BitOr || binaryNode.op == ast::BinaryOperator::BitXor || binaryNode.op == ast::BinaryOperator::LShift || binaryNode.op == ast::BinaryOperator::RShift) {
      const uint32_t iMask = TypeEnv::maskForKind(Type::Int);
      const uint32_t lMask = (lhsTyper.outSet != 0U) ? lhsTyper.outSet : TypeEnv::maskForKind(lhsTyper.out);
      const uint32_t rMask = (rhsTyper.outSet != 0U) ? rhsTyper.outSet : TypeEnv::maskForKind(rhsTyper.out);
      if (lMask == iMask && rMask == iMask) { out = Type::Int; outSet = iMask; return; }
      addDiag(*diags, "bitwise/shift operands must be int", &binaryNode); ok = false; return;
    }
    // Comparisons
    if (binaryNode.op == ast::BinaryOperator::Eq || binaryNode.op == ast::BinaryOperator::Ne || binaryNode.op == ast::BinaryOperator::Lt || binaryNode.op == ast::BinaryOperator::Le || binaryNode.op == ast::BinaryOperator::Gt || binaryNode.op == ast::BinaryOperator::Ge || binaryNode.op == ast::BinaryOperator::Is || binaryNode.op == ast::BinaryOperator::IsNot) {
      // Allow eq/ne None comparisons regardless of other type
      if ((binaryNode.op == ast::BinaryOperator::Eq || binaryNode.op == ast::BinaryOperator::Ne || binaryNode.op == ast::BinaryOperator::Is || binaryNode.op == ast::BinaryOperator::IsNot) &&
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
      const uint32_t sMask = TypeEnv::maskForKind(Type::Str);
      const uint32_t lMask = (lhsTyper.outSet != 0U) ? lhsTyper.outSet : TypeEnv::maskForKind(lhsTyper.out);
      const uint32_t rMask = (rhsTyper.outSet != 0U) ? rhsTyper.outSet : TypeEnv::maskForKind(rhsTyper.out);
      const bool bothInt = (lMask == iMask) && (rMask == iMask);
      const bool bothFloat = (lMask == fMask) && (rMask == fMask);
      const bool bothStr = (lMask == sMask) && (rMask == sMask);
      // Identity comparisons always type to bool in this subset
      if (binaryNode.op == ast::BinaryOperator::Is || binaryNode.op == ast::BinaryOperator::IsNot) {
        out = Type::Bool; auto& mb = const_cast<ast::Binary&>(binaryNode); mb.setType(out); return;
      }
      if (bothStr) {
        // Allow all standard comparisons for strings (Eq/Ne/Lt/Le/Gt/Ge)
        if (binaryNode.op == ast::BinaryOperator::Eq || binaryNode.op == ast::BinaryOperator::Ne || binaryNode.op == ast::BinaryOperator::Lt || binaryNode.op == ast::BinaryOperator::Le || binaryNode.op == ast::BinaryOperator::Gt || binaryNode.op == ast::BinaryOperator::Ge) {
          out = Type::Bool; auto& mb = const_cast<ast::Binary&>(binaryNode); mb.setType(out); return;
        }
      }
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
    // Membership tests: lhs in rhs -> bool, with limited type validation
    if (binaryNode.op == ast::BinaryOperator::In || binaryNode.op == ast::BinaryOperator::NotIn) {
      const uint32_t sMask = TypeEnv::maskForKind(Type::Str);
      const uint32_t lMaskList = TypeEnv::maskForKind(Type::List);
      const uint32_t lMask = (lhsTyper.outSet != 0U) ? lhsTyper.outSet : TypeEnv::maskForKind(lhsTyper.out);
      const uint32_t rMask = (rhsTyper.outSet != 0U) ? rhsTyper.outSet : TypeEnv::maskForKind(rhsTyper.out);
      auto isSubset = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
      if (rMask == sMask) {
        if (!isSubset(lMask, sMask)) { addDiag(*diags, "left operand must be str when right is str for 'in'", &binaryNode); ok = false; return; }
      } else if (rMask == lMaskList) {
        // If RHS is a name bound to a list with known element set, enforce lhs within element set
        uint32_t elemMask = 0U;
        if (binaryNode.rhs->kind == ast::NodeKind::Name) {
          const auto* nm = static_cast<const ast::Name*>(binaryNode.rhs.get());
          elemMask = env->getListElems(nm->id);
        } else if (binaryNode.rhs->kind == ast::NodeKind::ListLiteral) {
          const auto* lst = static_cast<const ast::ListLiteral*>(binaryNode.rhs.get());
          for (const auto& el : lst->elements) {
            if (!el) continue; ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; el->accept(et); if (!et.ok) { ok = false; return; }
            const uint32_t em = (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out);
            elemMask |= em;
          }
        }
        if (elemMask != 0U) {
          if (!isSubset(lMask, elemMask)) { addDiag(*diags, "left operand not permitted for membership in list", &binaryNode); ok = false; return; }
        }
      } else {
        // Relax: allow unknown RHS (esp. names) and treat as bool; but fail for obvious literal mismatches
        if (binaryNode.rhs->kind == ast::NodeKind::IntLiteral || binaryNode.rhs->kind == ast::NodeKind::FloatLiteral) {
          addDiag(*diags, "right operand of 'in' must be str or list", &binaryNode); ok = false; return;
        }
      }
      out = Type::Bool; outSet = TypeEnv::maskForKind(Type::Bool);
      auto& mut = const_cast<ast::Binary&>(binaryNode); mut.setType(out);
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
    if ((binaryNode.op == ast::BinaryOperator::Add || binaryNode.op == ast::BinaryOperator::Sub || binaryNode.op == ast::BinaryOperator::Mul || binaryNode.op == ast::BinaryOperator::Div || binaryNode.op == ast::BinaryOperator::Mod || binaryNode.op == ast::BinaryOperator::FloorDiv || binaryNode.op == ast::BinaryOperator::Pow) && ( (typeIsInt(lhsTyper.out)&&typeIsInt(rhsTyper.out)) || (typeIsFloat(lhsTyper.out)&&typeIsFloat(rhsTyper.out)) )) {
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
      ExpressionTyper elemTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets};
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
      ExpressionTyper elemTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets};
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
  void visit(const ast::SetLiteral& setLiteral) override {
    // Validate element expressions; type set conservatively as List in this subset
    for (const auto& element : setLiteral.elements) {
      if (!element) continue;
      ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets};
      element->accept(et); if (!et.ok) { ok = false; return; }
    }
    out = Type::List; outSet = TypeEnv::maskForKind(out);
  }
  void visit(const ast::DictLiteral& dictLiteral) override {
    // Validate keys/values and type as Dict
    for (const auto& kv : dictLiteral.items) {
      if (kv.first) { ExpressionTyper kt{*env, *sigs, *retParamIdxs, *diags, polyTargets}; kv.first->accept(kt); if (!kt.ok) { ok = false; return; } }
      if (kv.second) { ExpressionTyper vt{*env, *sigs, *retParamIdxs, *diags, polyTargets}; kv.second->accept(vt); if (!vt.ok) { ok = false; return; } }
    }
    for (const auto& up : dictLiteral.unpacks) { if (up) { ExpressionTyper ut{*env, *sigs, *retParamIdxs, *diags, polyTargets}; up->accept(ut); if (!ut.ok) { ok = false; return; } } }
    out = Type::Dict; outSet = TypeEnv::maskForKind(out);
  }
  void visit(const ast::ListComp& lc) override {
    // Evaluate in a local env so comp targets do not leak; guards must be bool.
    TypeEnv local = *env;
    auto inferElemMask = [&](const ast::Expr* it) -> uint32_t {
      if (!it) return 0U;
      if (it->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(it); const uint32_t e = local.getListElems(nm->id); if (e != 0U) return e; }
      if (it->kind == ast::NodeKind::ListLiteral) {
        uint32_t em = 0U; const auto* lst = static_cast<const ast::ListLiteral*>(it);
        for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; el->accept(et); if (!et.ok) return 0U; em |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
        return em;
      }
      return 0U;
    };
    const ast::Expr* currentIter = nullptr; // capture the current iter expression for tuple element inference
    std::function<void(const ast::Expr*, uint32_t, int)> bindTarget = [&](const ast::Expr* tgt, uint32_t elemMask, int parentIdx) {
      if (!tgt) return;
      if (tgt->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(tgt); uint32_t m = elemMask; if (m == 0U) m = TypeEnv::maskForKind(Type::Int); local.defineSet(nm->id, m, {"<comp>", 0, 0}); return; }
      if (tgt->kind == ast::NodeKind::TupleLiteral) {
        const auto* tp = static_cast<const ast::TupleLiteral*>(tgt);
        // If iter is a name with known tuple element masks (for list-of-tuples), use those per index
        const ast::Name* iterName = nullptr;
        if (currentIter && currentIter->kind == ast::NodeKind::Name) { iterName = static_cast<const ast::Name*>(currentIter); }
        std::vector<uint32_t> perIndex;
        if (currentIter && currentIter->kind == ast::NodeKind::ListLiteral) {
          const auto* lst = static_cast<const ast::ListLiteral*>(currentIter);
          size_t arity = tp->elements.size(); perIndex.assign(arity, 0U);
          for (const auto& el : lst->elements) {
            if (!el || el->kind != ast::NodeKind::TupleLiteral) continue;
            const auto* lt = static_cast<const ast::TupleLiteral*>(el.get());
            const ast::TupleLiteral* inner = lt;
            if (parentIdx >= 0 && parentIdx < static_cast<int>(lt->elements.size()) && lt->elements[parentIdx] && lt->elements[parentIdx]->kind == ast::NodeKind::TupleLiteral) {
              inner = static_cast<const ast::TupleLiteral*>(lt->elements[parentIdx].get());
            }
            for (size_t i = 0; i < std::min(arity, inner->elements.size()); ++i) {
              const auto& sub = inner->elements[i]; if (!sub) continue;
              ExpressionTyper set{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes};
              sub->accept(set); if (!set.ok) { ok = false; return; }
              perIndex[i] |= (set.outSet != 0U) ? set.outSet : TypeEnv::maskForKind(set.out);
            }
          }
        }
        for (size_t i = 0; i < tp->elements.size(); ++i) {
          const auto& e = tp->elements[i]; if (!e) continue;
          uint32_t m = elemMask;
          if (iterName != nullptr) {
            const uint32_t mi = local.getTupleElemAt(iterName->id, i);
            if (mi != 0U) { m = mi; }
          } else if (!perIndex.empty() && i < perIndex.size()) {
            if (perIndex[i] != 0U) { m = perIndex[i]; }
          }
          // For nested tuple destructuring, pass current index so inner perIndex resolves against inner tuple
          const int nextParentIdx = (parentIdx >= 0) ? parentIdx : static_cast<int>(i);
          bindTarget(e.get(), m, nextParentIdx);
        }
      }
    };
    for (const auto& f : lc.fors) {
      if (f.iter) { ExpressionTyper it{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; f.iter->accept(it); if (!it.ok) { ok = false; return; } }
      currentIter = f.iter.get();
      uint32_t em = inferElemMask(f.iter.get());
      bindTarget(f.target.get(), em, -1);
      for (const auto& g : f.ifs) { if (g) { ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; g->accept(et); if (!et.ok) { ok = false; return; } if (!typeIsBool(et.out)) { addDiag(*diags, "list comprehension guard must be bool", g.get()); ok = false; return; } } }
    }
    if (lc.elt) { ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; lc.elt->accept(et); if (!et.ok) { ok = false; return; } }
    out = Type::List; outSet = TypeEnv::maskForKind(out);
    auto& m = const_cast<ast::ListComp&>(lc); m.setType(out);
  }
  void visit(const ast::SetComp& sc) override {
    // Treat set comp as List; evaluate in local env and require boolean guards
    TypeEnv local = *env;
    auto inferElemMask = [&](const ast::Expr* it) -> uint32_t {
      if (!it) return 0U;
      if (it->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(it); const uint32_t e = local.getListElems(nm->id); if (e != 0U) return e; }
      if (it->kind == ast::NodeKind::ListLiteral) { uint32_t em = 0U; const auto* lst = static_cast<const ast::ListLiteral*>(it); for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; el->accept(et); if (!et.ok) return 0U; em |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); } return em; } return 0U; };
    const ast::Expr* currentIter = nullptr;
    std::function<void(const ast::Expr*, uint32_t, int)> bindTarget = [&](const ast::Expr* tgt, uint32_t elemMask, int parentIdx) {
      if (!tgt) return;
      if (tgt->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(tgt); uint32_t m = elemMask; if (m == 0U) m = TypeEnv::maskForKind(Type::Int); local.defineSet(nm->id, m, {"<comp>", 0, 0}); }
      else if (tgt->kind == ast::NodeKind::TupleLiteral) {
        const auto* tp = static_cast<const ast::TupleLiteral*>(tgt);
        const ast::Name* iterName = (currentIter && currentIter->kind == ast::NodeKind::Name) ? static_cast<const ast::Name*>(currentIter) : nullptr;
        std::vector<uint32_t> perIndex;
        if (currentIter && currentIter->kind == ast::NodeKind::ListLiteral) {
          const auto* lst = static_cast<const ast::ListLiteral*>(currentIter);
          size_t arity = tp->elements.size(); perIndex.assign(arity, 0U);
          for (const auto& el : lst->elements) {
            if (!el || el->kind != ast::NodeKind::TupleLiteral) continue;
            const auto* lt = static_cast<const ast::TupleLiteral*>(el.get());
            const ast::TupleLiteral* inner = lt;
            if (parentIdx >= 0 && parentIdx < static_cast<int>(lt->elements.size()) && lt->elements[parentIdx] && lt->elements[parentIdx]->kind == ast::NodeKind::TupleLiteral) {
              inner = static_cast<const ast::TupleLiteral*>(lt->elements[parentIdx].get());
            }
            for (size_t i = 0; i < std::min(arity, inner->elements.size()); ++i) {
              const auto& sub = inner->elements[i]; if (!sub) continue;
              ExpressionTyper set{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes};
              sub->accept(set); if (!set.ok) { ok = false; return; }
              perIndex[i] |= (set.outSet != 0U) ? set.outSet : TypeEnv::maskForKind(set.out);
            }
          }
        }
        for (size_t i = 0; i < tp->elements.size(); ++i) {
          const auto& e = tp->elements[i]; if (!e) continue;
          uint32_t m = elemMask;
          if (iterName != nullptr) {
            const uint32_t mi = local.getTupleElemAt(iterName->id, i);
            if (mi != 0U) { m = mi; }
          } else if (!perIndex.empty() && i < perIndex.size()) {
            if (perIndex[i] != 0U) { m = perIndex[i]; }
          }
          const int nextParentIdx = (parentIdx >= 0) ? parentIdx : static_cast<int>(i);
          bindTarget(e.get(), m, nextParentIdx);
        }
      }
    };
    for (const auto& f : sc.fors) {
      if (f.iter) { ExpressionTyper it{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; f.iter->accept(it); if (!it.ok) { ok = false; return; } }
      currentIter = f.iter.get();
      uint32_t em = inferElemMask(f.iter.get()); bindTarget(f.target.get(), em, -1);
      for (const auto& g : f.ifs) { if (g) { ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; g->accept(et); if (!et.ok) { ok = false; return; } if (!typeIsBool(et.out)) { addDiag(*diags, "set comprehension guard must be bool", g.get()); ok = false; return; } } }
    }
    if (sc.elt) { ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; sc.elt->accept(et); if (!et.ok) { ok = false; return; } }
    out = Type::List; outSet = TypeEnv::maskForKind(out);
  }
  void visit(const ast::DictComp& dc) override {
    TypeEnv local = *env;
    auto inferElemMask = [&](const ast::Expr* it) -> uint32_t { if (!it) return 0U; if (it->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(it); const uint32_t e = local.getListElems(nm->id); if (e != 0U) return e; } if (it->kind == ast::NodeKind::ListLiteral) { uint32_t em = 0U; const auto* lst = static_cast<const ast::ListLiteral*>(it); for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; el->accept(et); if (!et.ok) return 0U; em |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); } return em; } return 0U; };
    const ast::Expr* currentIter = nullptr;
    std::function<void(const ast::Expr*, uint32_t, int)> bindTarget = [&](const ast::Expr* tgt, uint32_t elemMask, int parentIdx) {
      if (!tgt) return;
      if (tgt->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(tgt); uint32_t m = elemMask; if (m == 0U) m = TypeEnv::maskForKind(Type::Int); local.defineSet(nm->id, m, {"<comp>", 0, 0}); }
      else if (tgt->kind == ast::NodeKind::TupleLiteral) {
        const auto* tp = static_cast<const ast::TupleLiteral*>(tgt);
        const ast::Name* iterName = (currentIter && currentIter->kind == ast::NodeKind::Name) ? static_cast<const ast::Name*>(currentIter) : nullptr;
        std::vector<uint32_t> perIndex;
        if (currentIter && currentIter->kind == ast::NodeKind::ListLiteral) {
          const auto* lst = static_cast<const ast::ListLiteral*>(currentIter);
          size_t arity = tp->elements.size(); perIndex.assign(arity, 0U);
          for (const auto& el : lst->elements) {
            if (!el || el->kind != ast::NodeKind::TupleLiteral) continue;
            const auto* lt = static_cast<const ast::TupleLiteral*>(el.get());
            const ast::TupleLiteral* inner = lt;
            if (parentIdx >= 0 && parentIdx < static_cast<int>(lt->elements.size()) && lt->elements[parentIdx] && lt->elements[parentIdx]->kind == ast::NodeKind::TupleLiteral) {
              inner = static_cast<const ast::TupleLiteral*>(lt->elements[parentIdx].get());
            }
            for (size_t i = 0; i < std::min(arity, inner->elements.size()); ++i) {
              const auto& sub = inner->elements[i]; if (!sub) continue;
              ExpressionTyper set{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes};
              sub->accept(set); if (!set.ok) { ok = false; return; }
              perIndex[i] |= (set.outSet != 0U) ? set.outSet : TypeEnv::maskForKind(set.out);
            }
          }
        }
        for (size_t i = 0; i < tp->elements.size(); ++i) {
          const auto& e = tp->elements[i]; if (!e) continue;
          uint32_t m = elemMask;
          if (iterName != nullptr) {
            const uint32_t mi = local.getTupleElemAt(iterName->id, i);
            if (mi != 0U) { m = mi; }
          } else if (!perIndex.empty() && i < perIndex.size()) {
            if (perIndex[i] != 0U) { m = perIndex[i]; }
          }
          const int nextParentIdx = (parentIdx >= 0) ? parentIdx : static_cast<int>(i);
          bindTarget(e.get(), m, nextParentIdx);
        }
      }
    };
    for (const auto& f : dc.fors) {
      if (f.iter) { ExpressionTyper it{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; f.iter->accept(it); if (!it.ok) { ok = false; return; } }
      currentIter = f.iter.get();
      uint32_t em = inferElemMask(f.iter.get()); bindTarget(f.target.get(), em, -1);
      for (const auto& g : f.ifs) { if (g) { ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; g->accept(et); if (!et.ok) { ok = false; return; } if (!typeIsBool(et.out)) { addDiag(*diags, "dict comprehension guard must be bool", g.get()); ok = false; return; } } }
    }
    if (dc.key) { ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; dc.key->accept(et); if (!et.ok) { ok = false; return; } }
    if (dc.value) { ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; dc.value->accept(et); if (!et.ok) { ok = false; return; } }
    out = Type::Dict; outSet = TypeEnv::maskForKind(out);
  }
  void visit(const ast::YieldExpr& y) override {
    // Explicitly reject yield in this subset of semantics
    addDiag(*diags, "yield is not supported in this subset", &y);
    ok = false;
  }
  void visit(const ast::AwaitExpr& a) override {
    // Await not supported in this subset
    addDiag(*diags, "await is not supported in this subset", &a);
    ok = false;
  }
  void visit(const ast::GeneratorExpr& ge) override {
    TypeEnv local = *env;
    auto inferElemMask = [&](const ast::Expr* it) -> uint32_t { if (!it) return 0U; if (it->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(it); const uint32_t e = local.getListElems(nm->id); if (e != 0U) return e; } if (it->kind == ast::NodeKind::ListLiteral) { uint32_t em = 0U; const auto* lst = static_cast<const ast::ListLiteral*>(it); for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; el->accept(et); if (!et.ok) return 0U; em |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); } return em; } return 0U; };
    const ast::Expr* currentIter = nullptr;
    std::function<void(const ast::Expr*, uint32_t)> bindTarget = [&](const ast::Expr* tgt, uint32_t elemMask) {
      if (!tgt) return;
      if (tgt->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(tgt); uint32_t m = elemMask; if (m == 0U) m = TypeEnv::maskForKind(Type::Int); local.defineSet(nm->id, m, {"<comp>", 0, 0}); }
      else if (tgt->kind == ast::NodeKind::TupleLiteral) {
        const auto* tp = static_cast<const ast::TupleLiteral*>(tgt);
        const ast::Name* iterName = (currentIter && currentIter->kind == ast::NodeKind::Name) ? static_cast<const ast::Name*>(currentIter) : nullptr;
        std::vector<uint32_t> perIndex;
        if (currentIter && currentIter->kind == ast::NodeKind::ListLiteral) {
          const auto* lst = static_cast<const ast::ListLiteral*>(currentIter);
          size_t arity = tp->elements.size(); perIndex.assign(arity, 0U);
          for (const auto& el : lst->elements) {
            if (!el || el->kind != ast::NodeKind::TupleLiteral) continue;
            const auto* lt = static_cast<const ast::TupleLiteral*>(el.get());
            for (size_t i = 0; i < std::min(arity, lt->elements.size()); ++i) {
              const auto& sub = lt->elements[i]; if (!sub) continue;
              ExpressionTyper set{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes};
              sub->accept(set); if (!set.ok) { ok = false; return; }
              perIndex[i] |= (set.outSet != 0U) ? set.outSet : TypeEnv::maskForKind(set.out);
            }
          }
        }
        for (size_t i = 0; i < tp->elements.size(); ++i) {
          const auto& e = tp->elements[i]; if (!e) continue;
          uint32_t m = elemMask;
          if (iterName != nullptr) {
            const uint32_t mi = local.getTupleElemAt(iterName->id, i);
            if (mi != 0U) { m = mi; }
          } else if (!perIndex.empty() && i < perIndex.size()) {
            if (perIndex[i] != 0U) { m = perIndex[i]; }
          }
          bindTarget(e.get(), m);
        }
      }
    };
    for (const auto& f : ge.fors) {
      if (f.iter) { ExpressionTyper it{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; f.iter->accept(it); if (!it.ok) { ok = false; return; } }
      currentIter = f.iter.get();
      uint32_t em = inferElemMask(f.iter.get()); bindTarget(f.target.get(), em);
      for (const auto& g : f.ifs) {
        if (!g) continue;
        ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes};
        g->accept(et); if (!et.ok) { ok = false; return; }
        if (!typeIsBool(et.out)) {
          // Relaxation: allow name-based truthiness over numeric types in generator guards
          if (g->kind == ast::NodeKind::Name) {
            const auto* nm = static_cast<const ast::Name*>(g.get());
            const uint32_t m = local.getSet(nm->id);
            const uint32_t numMask = TypeEnv::maskForKind(Type::Int) | TypeEnv::maskForKind(Type::Float);
            if (m != 0U && (m & ~numMask) == 0U) {
              continue; // accept numeric name as truthy
            }
          }
          addDiag(*diags, "generator guard must be bool", g.get()); ok = false; return;
        }
      }
    }
    if (ge.elt) { ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; ge.elt->accept(et); if (!et.ok) { ok = false; return; } }
    // Treat generator expr as List for typing in this subset
    out = Type::List; outSet = TypeEnv::maskForKind(out);
  }
  void visit(const ast::IfExpr& ife) override {
    // test must be bool
    ExpressionTyper testTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets};
    if (ife.test) { ife.test->accept(testTyper); } else { addDiag(*diags, "if-expression missing condition", &ife); ok = false; return; }
    if (!testTyper.ok) { ok = false; return; }
    const uint32_t bMask = TypeEnv::maskForKind(Type::Bool);
    const uint32_t tMask = (testTyper.outSet != 0U) ? testTyper.outSet : TypeEnv::maskForKind(testTyper.out);
    auto isSubset = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
    if (!isSubset(tMask, bMask)) { addDiag(*diags, "if-expression condition must be bool", &ife); ok = false; return; }
    // then and else must match
    ExpressionTyper thenTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets};
    ExpressionTyper elseTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets};
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
    // Attribute-based call: module.func(...)
    if (callNode.callee && callNode.callee->kind == ast::NodeKind::Attribute) {
      const auto* attr = static_cast<const ast::Attribute*>(callNode.callee.get());
      if (!(attr->value && attr->value->kind == ast::NodeKind::Name)) { addDiag(*diags, "unsupported callee expression", &callNode); ok = false; return; }
      const auto* mod = static_cast<const ast::Name*>(attr->value.get());
      const std::string key = mod->id + std::string(".") + attr->attr;
      const Sig* baseSig = nullptr; Sig polySig{}; bool havePoly = false;
      if (polyTargets.attrs != nullptr) {
        auto it = polyTargets.attrs->find(key);
        if (it != polyTargets.attrs->end() && !it->second.empty()) {
          const Sig* base = nullptr;
          for (const auto& tgt : it->second) {
            auto sIt = sigs->find(tgt);
            if (sIt == sigs->end()) { addDiag(*diags, std::string("monkey patch target not found in known code: ") + tgt, &callNode); ok = false; return; }
            const Sig& sg = sIt->second;
            if (base == nullptr) { base = &sg; polySig = sg; }
            else {
              if (!(sg.ret == base->ret && sg.params.size() == base->params.size())) { addDiag(*diags, std::string("incompatible monkey-patch signatures for: ") + key, &callNode); ok = false; return; }
              for (size_t i = 0; i < sg.params.size(); ++i) { if (sg.params[i] != base->params[i]) { addDiag(*diags, std::string("incompatible monkey-patch signatures for: ") + key, &callNode); ok = false; return; } }
            }
          }
          havePoly = (base != nullptr);
        }
      }
      if (!havePoly) {
        // Try class method lookup via sigs map (ClassName.method)
        auto sIt = sigs->find(key);
        if (sIt != sigs->end()) {
          const Sig& sig = sIt->second; auto& mutableCall = const_cast<ast::Call&>(callNode);
          if (!sig.full.empty()) {
            std::unordered_map<std::string, size_t> nameToIdx; size_t varargIdx = static_cast<size_t>(-1), kwvarargIdx = static_cast<size_t>(-1);
            std::vector<size_t> posIdxs;
            for (size_t i = 0; i < sig.full.size(); ++i) { const auto& sp = sig.full[i]; if (!sp.name.empty()) nameToIdx[sp.name] = i; if (sp.isVarArg) varargIdx = i; if (sp.isKwVarArg) kwvarargIdx = i; if (!sp.isKwOnly && !sp.isVarArg && !sp.isKwVarArg) posIdxs.push_back(i); }
            std::vector<bool> bound(sig.full.size(), false);
          for (size_t i = 0; i < callNode.args.size(); ++i) {
            ExpressionTyper at{*env, *sigs, *retParamIdxs, *diags, polyTargets, nullptr, classes};
            callNode.args[i]->accept(at); if (!at.ok) { ok = false; return; }
            if (i < posIdxs.size()) {
              size_t pidx = posIdxs[i];
              const auto& p = sig.full[pidx];
              bool typeOk = false;
              const uint32_t aMask = TypeEnv::maskForKind(at.out);
              if (p.unionMask != 0U) { typeOk = ((aMask & p.unionMask) != 0U); }
              else if (p.type == Type::List && p.listElemMask != 0U && at.out == Type::List) {
                uint32_t elemMask = 0U;
                if (callNode.args[i] && callNode.args[i]->kind == ast::NodeKind::ListLiteral) {
                  const auto* lst = static_cast<const ast::ListLiteral*>(callNode.args[i].get());
                  for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets}; el->accept(et); if (!et.ok) { ok = false; return; } elemMask |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
                } else if (callNode.args[i] && callNode.args[i]->kind == ast::NodeKind::Name) {
                  const auto* nm = static_cast<const ast::Name*>(callNode.args[i].get()); elemMask = env->getListElems(nm->id);
                }
                if (elemMask != 0U) { typeOk = ((elemMask & ~p.listElemMask) == 0U); } else { typeOk = true; }
              } else { typeOk = (at.out == p.type); }
              if (!typeOk) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; }
              bound[pidx] = true;
            } else if (varargIdx != static_cast<size_t>(-1)) {
              if (sig.full[varargIdx].type != Type::NoneType && at.out != sig.full[varargIdx].type) { addDiag(*diags, "*args element type mismatch", callNode.args[i].get()); ok = false; return; }
            } else { addDiag(*diags, std::string("arity mismatch calling function: ") + key, &callNode); ok = false; return; }
          }
            for (const auto& kw : callNode.keywords) {
              auto itn = nameToIdx.find(kw.name);
              if (itn == nameToIdx.end()) { if (kwvarargIdx == static_cast<size_t>(-1)) { addDiag(*diags, std::string("unknown keyword argument: ") + kw.name, &callNode); ok = false; return; } continue; }
              size_t pidx = itn->second;
              if (sig.full[pidx].isPosOnly) { addDiag(*diags, std::string("positional-only argument passed as keyword: ") + kw.name, &callNode); ok = false; return; }
              if (bound[pidx]) { addDiag(*diags, std::string("multiple values for argument: ") + kw.name, &callNode); ok = false; return; }
              ExpressionTyper kt{*env, *sigs, *retParamIdxs, *diags, polyTargets, nullptr, classes}; if (kw.value) kw.value->accept(kt); if (!kt.ok) { ok = false; return; }
              if (kt.out != sig.full[pidx].type) { addDiag(*diags, std::string("keyword argument type mismatch: ") + kw.name, &callNode); ok = false; return; }
              bound[pidx] = true;
            }
            if (!callNode.starArgs.empty() && varargIdx == static_cast<size_t>(-1)) { addDiag(*diags, "*args provided but callee has no varargs", &callNode); ok = false; return; }
            if (!callNode.kwStarArgs.empty() && kwvarargIdx == static_cast<size_t>(-1)) { addDiag(*diags, "**kwargs provided but callee has no kwvarargs", &callNode); ok = false; return; }
            for (size_t i = 0; i < sig.full.size(); ++i) { const auto& sp = sig.full[i]; if (sp.isVarArg || sp.isKwVarArg) continue; if (!bound[i] && !sp.hasDefault) { addDiag(*diags, std::string(sp.isKwOnly?"missing required keyword-only argument: ":"missing required positional argument: ") + sp.name, &callNode); ok = false; return; } }
            out = sig.ret; mutableCall.setType(out); return;
          }
          if (sig.params.size() != callNode.args.size()) { addDiag(*diags, std::string("arity mismatch calling function: ") + key, &callNode); ok = false; return; }
          for (size_t i = 0; i < callNode.args.size(); ++i) { ExpressionTyper at{*env, *sigs, *retParamIdxs, *diags, polyTargets, nullptr, classes}; callNode.args[i]->accept(at); if (!at.ok) { ok = false; return; } if (at.out != sig.params[i]) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; } }
          out = sig.ret; auto& mutableCall2 = const_cast<ast::Call&>(callNode); mutableCall2.setType(out); return;
        }
        // Attempt instance-binding: base is a variable referring to an instance of a known class
        if (classes != nullptr) {
          auto inst = env->instanceOf(mod->id);
          if (inst) {
            auto cit = classes->find(*inst);
            if (cit != classes->end()) {
              auto mit = cit->second.methods.find(attr->attr);
              if (mit != cit->second.methods.end()) {
                const Sig& sig = mit->second; auto& mutableCall3 = const_cast<ast::Call&>(callNode);
                if (!sig.full.empty()) {
                  std::unordered_map<std::string, size_t> nameToIdx; size_t varargIdx = static_cast<size_t>(-1), kwvarargIdx = static_cast<size_t>(-1);
                  std::vector<size_t> posIdxs;
                  for (size_t i = 0; i < sig.full.size(); ++i) { const auto& sp = sig.full[i]; if (!sp.name.empty()) nameToIdx[sp.name] = i; if (sp.isVarArg) varargIdx = i; if (sp.isKwVarArg) kwvarargIdx = i; if (!sp.isKwOnly && !sp.isVarArg && !sp.isKwVarArg) posIdxs.push_back(i); }
                  std::vector<bool> bound(sig.full.size(), false);
          for (size_t i = 0; i < callNode.args.size(); ++i) {
            ExpressionTyper at{*env, *sigs, *retParamIdxs, *diags, polyTargets, nullptr, classes}; callNode.args[i]->accept(at); if (!at.ok) { ok = false; return; }
            if (i < posIdxs.size()) {
              size_t pidx = posIdxs[i]; const auto& p = sig.full[pidx];
              bool typeOk = false; const uint32_t aMask = TypeEnv::maskForKind(at.out);
              if (p.unionMask != 0U) { typeOk = ((aMask & p.unionMask) != 0U); }
              else if (p.type == Type::List && p.listElemMask != 0U && at.out == Type::List) {
                uint32_t elemMask = 0U;
                if (callNode.args[i] && callNode.args[i]->kind == ast::NodeKind::ListLiteral) {
                  const auto* lst = static_cast<const ast::ListLiteral*>(callNode.args[i].get());
                  for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets}; el->accept(et); if (!et.ok) { ok = false; return; } elemMask |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
                } else if (callNode.args[i] && callNode.args[i]->kind == ast::NodeKind::Name) {
                  const auto* nm = static_cast<const ast::Name*>(callNode.args[i].get()); elemMask = env->getListElems(nm->id);
                }
                typeOk = (elemMask == 0U) ? true : ((elemMask & ~p.listElemMask) == 0U);
              } else { typeOk = (at.out == p.type); }
              if (!typeOk) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; }
              bound[pidx] = true;
            } else if (varargIdx != static_cast<size_t>(-1)) {
              if (sig.full[varargIdx].type != Type::NoneType && at.out != sig.full[varargIdx].type) { addDiag(*diags, "*args element type mismatch", callNode.args[i].get()); ok = false; return; }
            } else { addDiag(*diags, std::string("arity mismatch calling function: ") + (*inst + std::string(".") + attr->attr), &callNode); ok = false; return; }
          }
                  for (const auto& kw : callNode.keywords) {
                    auto itn = nameToIdx.find(kw.name);
                    if (itn == nameToIdx.end()) { if (kwvarargIdx == static_cast<size_t>(-1)) { addDiag(*diags, std::string("unknown keyword argument: ") + kw.name, &callNode); ok = false; return; } continue; }
                    size_t pidx = itn->second; if (bound[pidx]) { addDiag(*diags, std::string("multiple values for argument: ") + kw.name, &callNode); ok = false; return; }
                    ExpressionTyper kt{*env, *sigs, *retParamIdxs, *diags, polyTargets, nullptr, classes}; if (kw.value) kw.value->accept(kt); if (!kt.ok) { ok = false; return; }
                    const auto& p = sig.full[pidx]; bool typeOk = false; const uint32_t aMask = TypeEnv::maskForKind(kt.out);
                    if (p.unionMask != 0U) { typeOk = ((aMask & p.unionMask) != 0U); }
                    else if (p.type == Type::List && p.listElemMask != 0U && kt.out == Type::List) {
                      uint32_t elemMask = 0U;
                      if (kw.value && kw.value->kind == ast::NodeKind::ListLiteral) {
                        const auto* lst = static_cast<const ast::ListLiteral*>(kw.value.get());
                        for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets}; el->accept(et); if (!et.ok) { ok = false; return; } elemMask |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
                      } else if (kw.value && kw.value->kind == ast::NodeKind::Name) {
                        const auto* nm = static_cast<const ast::Name*>(kw.value.get()); elemMask = env->getListElems(nm->id);
                      }
                      typeOk = (elemMask == 0U) ? true : ((elemMask & ~p.listElemMask) == 0U);
                    } else { typeOk = (kt.out == p.type); }
                    if (!typeOk) { addDiag(*diags, std::string("keyword argument type mismatch: ") + kw.name, &callNode); ok = false; return; }
                    bound[pidx] = true;
                  }
                  if (!callNode.starArgs.empty() && varargIdx == static_cast<size_t>(-1)) { addDiag(*diags, "*args provided but callee has no varargs", &callNode); ok = false; return; }
                  if (!callNode.kwStarArgs.empty() && kwvarargIdx == static_cast<size_t>(-1)) { addDiag(*diags, "**kwargs provided but callee has no kwvarargs", &callNode); ok = false; return; }
                  for (size_t i = 0; i < sig.full.size(); ++i) { const auto& sp = sig.full[i]; if (sp.isVarArg || sp.isKwVarArg) continue; if (!bound[i] && !sp.hasDefault) { addDiag(*diags, std::string(sp.isKwOnly?"missing required keyword-only argument: ":"missing required positional argument: ") + sp.name, &callNode); ok = false; return; } }
                  out = sig.ret; mutableCall3.setType(out); return;
                }
                if (sig.params.size() != callNode.args.size()) { addDiag(*diags, std::string("arity mismatch calling function: ") + (*inst + std::string(".") + attr->attr), &callNode); ok = false; return; }
                for (size_t i = 0; i < callNode.args.size(); ++i) { ExpressionTyper at{*env, *sigs, *retParamIdxs, *diags, polyTargets, nullptr, classes}; callNode.args[i]->accept(at); if (!at.ok) { ok = false; return; } if (at.out != sig.params[i]) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; } }
                out = sig.ret; auto& mutableCall4 = const_cast<ast::Call&>(callNode); mutableCall4.setType(out); return;
              }
            }
          }
        }
        // Otherwise unknown
        addDiag(*diags, std::string("unknown function: ") + key, &callNode); ok = false; return;
      }
      const Sig& sig = (baseSig != nullptr ? *baseSig : polySig);
      if (sig.params.size() != callNode.args.size()) { addDiag(*diags, std::string("arity mismatch calling function: ") + key, &callNode); ok = false; return; }
      for (size_t i = 0; i < callNode.args.size(); ++i) {
        ExpressionTyper argTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[i]->accept(argTyper); if (!argTyper.ok) { ok = false; return; }
        if (argTyper.out != sig.params[i]) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; }
      }
      out = sig.ret; auto& mutableCall = const_cast<ast::Call&>(callNode); // NOLINT
      mutableCall.setType(out);
      return;
    }
    if (!callNode.callee || callNode.callee->kind != ast::NodeKind::Name) { addDiag(*diags, "unsupported callee expression", &callNode); ok = false; return; }
    const auto* nameNode = static_cast<const ast::Name*>(callNode.callee.get());
    // Builtins: len(x) -> int; isinstance(x, T) -> bool; plus constructors and common utilities
    if (nameNode->id == "eval") { addDiag(*diags, "eval() is not allowed for security reasons", &callNode); ok = false; return; }
    if (nameNode->id == "exec") { addDiag(*diags, "exec() is not allowed for security reasons", &callNode); ok = false; return; }
    if (nameNode->id == "len") {
      if (callNode.args.size() != 1) { addDiag(*diags, "len() takes exactly one argument", &callNode); ok = false; return; }
      ExpressionTyper argTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(argTyper); if (!argTyper.ok) { ok = false; return; }
      const ast::TypeKind k = argTyper.out;
      if (!(k == Type::Str || k == Type::List || k == Type::Tuple || k == Type::Dict)) {
        addDiag(*diags, "len() argument must be str/list/tuple/dict", callNode.args[0].get()); ok = false; return;
      }
      out = Type::Int; const_cast<ast::Call&>(callNode).setType(out); // NOLINT(cppcoreguidelines-pro-type-const-cast)
      return;
    }
    if (nameNode->id == "obj_get") {
      if (callNode.args.size() != 2) { addDiag(*diags, "obj_get() takes two arguments", &callNode); ok = false; return; }
      // First arg is an object pointer (opaque), second must be int index
      ExpressionTyper idxTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[1]->accept(idxTyper); if (!idxTyper.ok) { ok = false; return; }
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
    if (nameNode->id == "int") {
      if (callNode.args.size() < 1 || callNode.args.size() > 2) { addDiag(*diags, "int() takes 1 or 2 arguments", &callNode); ok = false; return; }
      out = Type::Int; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "float") {
      if (callNode.args.size() != 1) { addDiag(*diags, "float() takes exactly 1 argument", &callNode); ok = false; return; }
      out = Type::Float; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "bool") {
      if (callNode.args.size() > 1) { addDiag(*diags, "bool() takes at most 1 argument", &callNode); ok = false; return; }
      out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "str") {
      if (callNode.args.size() > 1) { addDiag(*diags, "str() takes at most 1 argument", &callNode); ok = false; return; }
      out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "list") {
      if (callNode.args.size() > 1) { addDiag(*diags, "list() takes at most 1 argument", &callNode); ok = false; return; }
      out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "tuple") {
      if (callNode.args.size() > 1) { addDiag(*diags, "tuple() takes at most 1 argument", &callNode); ok = false; return; }
      out = Type::Tuple; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "dict") {
      if (callNode.args.size() > 1) { addDiag(*diags, "dict() takes at most 1 argument", &callNode); ok = false; return; }
      out = Type::Dict; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "range") {
      if (callNode.args.empty() || callNode.args.size() > 3) { addDiag(*diags, "range() takes 1 to 3 int arguments", &callNode); ok = false; return; }
      // Validate int-ness when possible
      for (const auto& a : callNode.args) { if (!a) continue; ExpressionTyper at{*env, *sigs, *retParamIdxs, *diags, polyTargets}; a->accept(at); if (!at.ok) { ok = false; return; } if (!(typeIsInt(at.out))) { /* allow subset: only int expected, but keep permissive */ } }
      out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "sum") {
      if (callNode.args.size() != 1) { addDiag(*diags, "sum() takes exactly 1 argument in this subset", &callNode); ok = false; return; }
      // If argument is a list with known element set, choose Int/Float accordingly; default to Int
      Type retT = Type::Int;
      if (callNode.args[0]->kind == ast::NodeKind::Name) {
        const auto* nm = static_cast<const ast::Name*>(callNode.args[0].get());
        const uint32_t es = env->getListElems(nm->id);
        if (es != 0U) {
          if (es == TypeEnv::maskForKind(Type::Float)) retT = Type::Float;
          else if (es == TypeEnv::maskForKind(Type::Int)) retT = Type::Int;
        }
      } else if (callNode.args[0]->kind == ast::NodeKind::ListLiteral) {
        const auto* lst = static_cast<const ast::ListLiteral*>(callNode.args[0].get());
        bool sawFloat = false; for (const auto& el : lst->elements) { if (!el) continue; if (el->kind == ast::NodeKind::FloatLiteral) { sawFloat = true; break; } }
        retT = sawFloat ? Type::Float : Type::Int;
      }
      out = retT; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "map") {
      if (callNode.args.size() != 2) { addDiag(*diags, "map() takes exactly 2 arguments in this subset", &callNode); ok = false; return; }
      out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "print") {
      // Accept any number of arguments; returns NoneType
      out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    auto sigIt = sigs->find(nameNode->id);
    // Polymorphism for monkey-patching: resolve via callTargets if callee is not a declared function
      Sig polySig{}; bool usePoly = false;
    if (sigIt == sigs->end() && polyTargets.vars != nullptr) {
      auto it = polyTargets.vars->find(nameNode->id);
      if (it != polyTargets.vars->end() && !it->second.empty()) {
        const Sig* base = nullptr;
        for (const auto& tgt : it->second) {
          auto sIt = sigs->find(tgt);
          if (sIt == sigs->end()) { addDiag(*diags, std::string("monkey patch target not found in known code: ") + tgt, &callNode); ok = false; return; }
          const Sig& sg = sIt->second;
          if (base == nullptr) { base = &sg; polySig = sg; }
          else {
            if (!(sg.ret == base->ret && sg.params.size() == base->params.size())) { addDiag(*diags, std::string("incompatible monkey-patch signatures for: ") + nameNode->id, &callNode); ok = false; return; }
            for (size_t i = 0; i < sg.params.size(); ++i) { if (sg.params[i] != base->params[i]) { addDiag(*diags, std::string("incompatible monkey-patch signatures for: ") + nameNode->id, &callNode); ok = false; return; } }
          }
        }
        usePoly = (base != nullptr);
      }
    }
    if (sigIt == sigs->end() && !usePoly) {
      // Allow class construction calls: C(...) where C is a known class. Validate against __init__ if present.
      if (classes != nullptr) {
        auto cit = classes->find(nameNode->id);
        if (cit != classes->end()) {
          auto itInit = cit->second.methods.find("__init__");
          if (itInit != cit->second.methods.end()) {
            // For __init__, ignore leading 'self' parameter if present
            Sig eff = itInit->second;
            if (!eff.full.empty() && !eff.full[0].isVarArg && !eff.full[0].isKwVarArg && eff.full[0].name == "self") {
              eff.full.erase(eff.full.begin());
              if (!eff.params.empty()) eff.params.erase(eff.params.begin());
            }
            const Sig& sig = eff; auto& mutableCall = const_cast<ast::Call&>(callNode);
            if (!sig.full.empty()) {
              std::unordered_map<std::string, size_t> nameToIdx; size_t varargIdx = static_cast<size_t>(-1), kwvarargIdx = static_cast<size_t>(-1);
              std::vector<size_t> posIdxs; for (size_t i = 0; i < sig.full.size(); ++i) { const auto& sp = sig.full[i]; if (!sp.name.empty()) nameToIdx[sp.name] = i; if (sp.isVarArg) varargIdx = i; if (sp.isKwVarArg) kwvarargIdx = i; if (!sp.isKwOnly && !sp.isVarArg && !sp.isKwVarArg) posIdxs.push_back(i); }
              std::vector<bool> bound(sig.full.size(), false);
          for (size_t i = 0; i < callNode.args.size(); ++i) {
            ExpressionTyper at{*env, *sigs, *retParamIdxs, *diags, polyTargets, nullptr, classes}; callNode.args[i]->accept(at); if (!at.ok) { ok = false; return; }
            if (i < posIdxs.size()) {
              size_t pidx = posIdxs[i]; const auto& p = sig.full[pidx];
              bool typeOk = false; const uint32_t aMask = TypeEnv::maskForKind(at.out);
              if (p.unionMask != 0U) { typeOk = ((aMask & p.unionMask) != 0U); }
              else if (p.type == Type::List && p.listElemMask != 0U && at.out == Type::List) {
                uint32_t elemMask = 0U;
                if (callNode.args[i] && callNode.args[i]->kind == ast::NodeKind::ListLiteral) {
                  const auto* lst = static_cast<const ast::ListLiteral*>(callNode.args[i].get());
                  for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets}; el->accept(et); if (!et.ok) { ok = false; return; } elemMask |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
                } else if (callNode.args[i] && callNode.args[i]->kind == ast::NodeKind::Name) {
                  const auto* nm = static_cast<const ast::Name*>(callNode.args[i].get()); elemMask = env->getListElems(nm->id);
                }
                typeOk = (elemMask == 0U) ? true : ((elemMask & ~p.listElemMask) == 0U);
              } else { typeOk = (at.out == p.type); }
              if (!typeOk) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; }
              bound[pidx] = true;
            } else if (varargIdx != static_cast<size_t>(-1)) {
              if (sig.full[varargIdx].type != Type::NoneType && at.out != sig.full[varargIdx].type) { addDiag(*diags, "*args element type mismatch", callNode.args[i].get()); ok = false; return; }
            } else { addDiag(*diags, std::string("arity mismatch calling function: ") + (nameNode->id + std::string(".__init__")), &callNode); ok = false; return; }
          }
              for (const auto& kw : callNode.keywords) {
                auto itn = nameToIdx.find(kw.name);
                if (itn == nameToIdx.end()) { if (kwvarargIdx == static_cast<size_t>(-1)) { addDiag(*diags, std::string("unknown keyword argument: ") + kw.name, &callNode); ok = false; return; } }
                else {
                  const size_t pidx = itn->second; if (bound[pidx]) { addDiag(*diags, std::string("multiple values for argument: ") + kw.name, &callNode); ok = false; return; }
                  ExpressionTyper kt{*env, *sigs, *retParamIdxs, *diags, polyTargets, nullptr, classes}; if (kw.value) kw.value->accept(kt); if (!kt.ok) { ok = false; return; }
                  const auto& p = sig.full[pidx]; bool typeOk = false; const uint32_t aMask = TypeEnv::maskForKind(kt.out);
                  if (p.unionMask != 0U) { typeOk = ((aMask & p.unionMask) != 0U); }
                  else if (p.type == Type::List && p.listElemMask != 0U && kt.out == Type::List) {
                    uint32_t elemMask = 0U;
                    if (kw.value && kw.value->kind == ast::NodeKind::ListLiteral) {
                      const auto* lst = static_cast<const ast::ListLiteral*>(kw.value.get());
                      for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets}; el->accept(et); if (!et.ok) { ok = false; return; } elemMask |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
                    } else if (kw.value && kw.value->kind == ast::NodeKind::Name) {
                      const auto* nm = static_cast<const ast::Name*>(kw.value.get()); elemMask = env->getListElems(nm->id);
                    }
                    typeOk = (elemMask == 0U) ? true : ((elemMask & ~p.listElemMask) == 0U);
                  } else { typeOk = (kt.out == p.type); }
                  if (!typeOk) { addDiag(*diags, std::string("keyword argument type mismatch: ") + kw.name, &callNode); ok = false; return; }
                  bound[pidx] = true;
                }
              }
              if (!callNode.starArgs.empty() && varargIdx == static_cast<size_t>(-1)) { addDiag(*diags, "*args provided but callee has no varargs", &callNode); ok = false; return; }
              if (!callNode.kwStarArgs.empty() && kwvarargIdx == static_cast<size_t>(-1)) { addDiag(*diags, "**kwargs provided but callee has no kwvarargs", &callNode); ok = false; return; }
              for (size_t i = 0; i < sig.full.size(); ++i) { const auto& sp = sig.full[i]; if (sp.isVarArg || sp.isKwVarArg) continue; if (!bound[i] && !sp.hasDefault) {
                if (sp.isKwOnly) { addDiag(*diags, std::string("missing required keyword-only argument: ") + sp.name, &callNode); ok = false; return; }
                else { addDiag(*diags, std::string("missing required positional argument: ") + sp.name, &callNode); ok = false; return; }
              } }
              out = Type::NoneType; mutableCall.setType(out); return;
            } else {
              // Use simple positional params
              if (sig.params.size() != callNode.args.size()) { addDiag(*diags, std::string("arity mismatch calling function: ") + (nameNode->id + std::string(".__init__")), &callNode); ok = false; return; }
              for (size_t i = 0; i < callNode.args.size(); ++i) { ExpressionTyper argTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets, nullptr, classes}; callNode.args[i]->accept(argTyper); if (!argTyper.ok) { ok = false; return; } if (argTyper.out != sig.params[i]) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; } }
              out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
            }
          } else {
            // No __init__; only zero-arg construction allowed in this subset
            if (!callNode.args.empty() || !callNode.keywords.empty()) { addDiag(*diags, std::string("arity mismatch calling function: ") + (nameNode->id + std::string(".__init__")), &callNode); ok = false; return; }
            out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
      }
      // Try instance __call__ if callee is an instance of a known class
      if (classes != nullptr) {
        auto inst = env->instanceOf(nameNode->id);
        if (inst) {
          auto cit = classes->find(*inst);
          if (cit != classes->end()) {
            auto itc = cit->second.methods.find("__call__");
            if (itc != cit->second.methods.end()) {
              const Sig& sig = itc->second;
              auto& mutableCall = const_cast<ast::Call&>(callNode);
              if (!sig.full.empty()) {
                std::unordered_map<std::string, size_t> nameToIdx; size_t varargIdx = static_cast<size_t>(-1), kwvarargIdx = static_cast<size_t>(-1);
                std::vector<size_t> posIdxs;
                for (size_t i = 0; i < sig.full.size(); ++i) { const auto& sp = sig.full[i]; if (!sp.name.empty()) nameToIdx[sp.name] = i; if (sp.isVarArg) varargIdx = i; if (sp.isKwVarArg) kwvarargIdx = i; if (!sp.isKwOnly && !sp.isVarArg && !sp.isKwVarArg) posIdxs.push_back(i); }
                std::vector<bool> bound(sig.full.size(), false);
                for (size_t i = 0; i < callNode.args.size(); ++i) { ExpressionTyper at{*env, *sigs, *retParamIdxs, *diags, polyTargets, nullptr, classes}; callNode.args[i]->accept(at); if (!at.ok) { ok = false; return; } if (i < posIdxs.size()) { size_t pidx = posIdxs[i]; if (at.out != sig.full[pidx].type) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; } bound[pidx] = true; } else if (varargIdx != static_cast<size_t>(-1)) { if (sig.full[varargIdx].type != Type::NoneType && at.out != sig.full[varargIdx].type) { addDiag(*diags, "*args element type mismatch", callNode.args[i].get()); ok = false; return; } } else { addDiag(*diags, std::string("arity mismatch calling function: ") + (nameNode->id + std::string(".__call__")), &callNode); ok = false; return; } }
                for (const auto& kw : callNode.keywords) { auto itn = nameToIdx.find(kw.name); if (itn == nameToIdx.end()) { if (kwvarargIdx == static_cast<size_t>(-1)) { addDiag(*diags, std::string("unknown keyword argument: ") + kw.name, &callNode); ok = false; return; } continue; } size_t pidx = itn->second; if (bound[pidx]) { addDiag(*diags, std::string("multiple values for argument: ") + kw.name, &callNode); ok = false; return; } ExpressionTyper kt{*env, *sigs, *retParamIdxs, *diags, polyTargets, nullptr, classes}; if (kw.value) kw.value->accept(kt); if (!kt.ok) { ok = false; return; } if (kt.out != sig.full[pidx].type) { addDiag(*diags, std::string("keyword argument type mismatch: ") + kw.name, &callNode); ok = false; return; } bound[pidx] = true; }
                if (!callNode.starArgs.empty() && varargIdx == static_cast<size_t>(-1)) { addDiag(*diags, "*args provided but callee has no varargs", &callNode); ok = false; return; }
                if (!callNode.kwStarArgs.empty() && kwvarargIdx == static_cast<size_t>(-1)) { addDiag(*diags, "**kwargs provided but callee has no kwvarargs", &callNode); ok = false; return; }
                for (size_t i = 0; i < sig.full.size(); ++i) { const auto& sp = sig.full[i]; if (sp.isVarArg || sp.isKwVarArg) continue; if (!bound[i] && !sp.hasDefault) { addDiag(*diags, std::string(sp.isKwOnly?"missing required keyword-only argument: ":"missing required positional argument: ") + sp.name, &callNode); ok = false; return; } }
                out = sig.ret; mutableCall.setType(out); return;
              }
              if (sig.params.size() != callNode.args.size()) { addDiag(*diags, std::string("arity mismatch calling function: ") + (nameNode->id + std::string(".__call__")), &callNode); ok = false; return; }
              for (size_t i = 0; i < callNode.args.size(); ++i) { ExpressionTyper argTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets, nullptr, classes}; callNode.args[i]->accept(argTyper); if (!argTyper.ok) { ok = false; return; } if (argTyper.out != sig.params[i]) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; } }
              out = sig.ret; const_cast<ast::Call&>(callNode).setType(out); return;
            }
          }
        }
      }
      addDiag(*diags, std::string("unknown function: ") + nameNode->id, &callNode); ok = false; return;
    }
    const auto& sig = usePoly ? polySig : sigIt->second;
    auto& mutableCall = const_cast<ast::Call&>(callNode); // NOLINT
    if (!sig.full.empty()) {
      std::unordered_map<std::string, size_t> nameToIdx;
      size_t varargIdx = static_cast<size_t>(-1);
      size_t kwvarargIdx = static_cast<size_t>(-1);
      std::vector<size_t> posParamIdxs; posParamIdxs.reserve(sig.full.size());
      for (size_t i = 0; i < sig.full.size(); ++i) {
        const auto& sp = sig.full[i];
        if (!sp.name.empty()) nameToIdx[sp.name] = i;
        if (sp.isVarArg) varargIdx = i;
        if (sp.isKwVarArg) kwvarargIdx = i;
        if (!sp.isKwOnly && !sp.isVarArg && !sp.isKwVarArg) posParamIdxs.push_back(i);
      }
      std::vector<bool> bound(sig.full.size(), false);
      // Positional
      for (size_t i = 0; i < callNode.args.size(); ++i) {
        ExpressionTyper at{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[i]->accept(at); if (!at.ok) { ok = false; return; }
        if (i < posParamIdxs.size()) {
          size_t pidx = posParamIdxs[i];
          if (at.out != sig.full[pidx].type) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; }
          bound[pidx] = true;
        } else if (varargIdx != static_cast<size_t>(-1)) {
          if (sig.full[varargIdx].type != Type::NoneType && at.out != sig.full[varargIdx].type) { addDiag(*diags, "*args element type mismatch", callNode.args[i].get()); ok = false; return; }
        } else { addDiag(*diags, std::string("arity mismatch calling function: ") + nameNode->id, &callNode); ok = false; return; }
      }
      // Keywords
      for (const auto& kw : callNode.keywords) {
        auto it = nameToIdx.find(kw.name);
        if (it == nameToIdx.end()) {
          if (kwvarargIdx == static_cast<size_t>(-1)) { addDiag(*diags, std::string("unknown keyword argument: ") + kw.name, &callNode); ok = false; return; }
          continue;
        }
        const size_t pidx = it->second; if (sig.full[pidx].isPosOnly) { addDiag(*diags, std::string("positional-only argument passed as keyword: ") + kw.name, &callNode); ok = false; return; }
        if (bound[pidx]) { addDiag(*diags, std::string("multiple values for argument: ") + kw.name, &callNode); ok = false; return; }
        ExpressionTyper kt{*env, *sigs, *retParamIdxs, *diags, polyTargets}; if (kw.value) kw.value->accept(kt); if (!kt.ok) { ok = false; return; }
        {
          const auto& p = sig.full[pidx];
          bool typeOk = false; const uint32_t aMask = TypeEnv::maskForKind(kt.out);
          if (p.unionMask != 0U) { typeOk = ((aMask & p.unionMask) != 0U); }
          else if (p.type == Type::List && p.listElemMask != 0U && kt.out == Type::List) {
            uint32_t elemMask = 0U;
            if (kw.value && kw.value->kind == ast::NodeKind::ListLiteral) {
              const auto* lst = static_cast<const ast::ListLiteral*>(kw.value.get());
              for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets}; el->accept(et); if (!et.ok) { ok = false; return; } elemMask |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
            } else if (kw.value && kw.value->kind == ast::NodeKind::Name) {
              const auto* nm = static_cast<const ast::Name*>(kw.value.get()); elemMask = env->getListElems(nm->id);
            }
            if (elemMask != 0U) { typeOk = ((elemMask & ~p.listElemMask) == 0U); } else { typeOk = true; }
          } else { typeOk = (kt.out == p.type); }
          if (!typeOk) { addDiag(*diags, std::string("keyword argument type mismatch: ") + kw.name, &callNode); ok = false; return; }
        }
        bound[pidx] = true;
      }
      if (!callNode.starArgs.empty() && varargIdx == static_cast<size_t>(-1)) { addDiag(*diags, "*args provided but callee has no varargs", &callNode); ok = false; return; }
      if (!callNode.kwStarArgs.empty() && kwvarargIdx == static_cast<size_t>(-1)) { addDiag(*diags, "**kwargs provided but callee has no kwvarargs", &callNode); ok = false; return; }
      for (size_t i = 0; i < sig.full.size(); ++i) {
        const auto& sp = sig.full[i]; if (sp.isVarArg || sp.isKwVarArg) continue; if (!bound[i] && !sp.hasDefault) {
          if (sp.isKwOnly) { addDiag(*diags, std::string("missing required keyword-only argument: ") + sp.name, &callNode); ok = false; return; }
          else { addDiag(*diags, std::string("missing required positional argument: ") + sp.name, &callNode); ok = false; return; }
        }
      }
      out = sig.ret; mutableCall.setType(out);
    } else {
      if (sig.params.size() != callNode.args.size()) { addDiag(*diags, std::string("arity mismatch calling function: ") + nameNode->id, &callNode); ok = false; return; }
      for (size_t i = 0; i < callNode.args.size(); ++i) {
        ExpressionTyper argTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[i]->accept(argTyper); if (!argTyper.ok) { ok = false; return; }
        if (argTyper.out != sig.params[i]) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; }
      }
      out = sig.ret; mutableCall.setType(out);
    }
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

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static bool inferExprType(const ast::Expr* expr,
                         const TypeEnv& env,
                         const std::unordered_map<std::string, Sig>& sigs,
                         const std::unordered_map<std::string, int>& retParamIdxs,
                         Type& outType,
                         std::vector<Diagnostic>& diags,
                         PolyPtrs poly = {}, const std::vector<const TypeEnv*>* outers = nullptr,
                         const std::unordered_map<std::string, ClassInfo>* classes = nullptr) {
  if (expr == nullptr) { addDiag(diags, "null expression", nullptr); return false; }
  ExpressionTyper exprTyper{env, sigs, retParamIdxs, diags, poly, outers, classes};
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
    for (const auto& param : func->params) {
      sig.params.push_back(param.type);
      SigParam sp; sp.name = param.name; sp.type = param.type; sp.isVarArg = param.isVarArg; sp.isKwVarArg = param.isKwVarArg; sp.isKwOnly = param.isKwOnly; sp.isPosOnly = param.isPosOnly; sp.hasDefault = (param.defaultValue != nullptr);
      // Build union mask from unionTypes (if any)
      if (!param.unionTypes.empty()) {
        uint32_t m = 0U;
        for (auto ut : param.unionTypes) { m |= TypeEnv::maskForKind(ut); }
        sp.unionMask = m;
      }
      // Generic list element type
      if (param.type == Type::List && param.listElemType != Type::NoneType) {
        sp.listElemMask = TypeEnv::maskForKind(param.listElemType);
      }
      sig.full.push_back(std::move(sp));
    }
    sigs[func->name] = std::move(sig);
  }
  // Collect class method signatures and simple inheritance
  std::unordered_map<std::string, ClassInfo> classes;
  // Pass 1: gather direct methods and bases
  for (const auto& clsPtr : mod.classes) {
    if (!clsPtr) continue;
    ClassInfo ci;
    for (const auto& b : clsPtr->bases) {
      if (b && b->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(b.get()); ci.bases.push_back(nm->id); }
    }
    for (const auto& st : clsPtr->body) {
      if (!st) continue;
      if (st->kind == ast::NodeKind::DefStmt) {
        const auto* ds = static_cast<const ast::DefStmt*>(st.get());
        if (ds->func) {
          const auto* fn = ds->func.get();
          if (fn->name == "__init__" && fn->returnType != Type::NoneType) {
            addDiag(diags, std::string("__init__ must return NoneType in class: ") + clsPtr->name, fn);
          }
          if (fn->name == "__len__" && fn->returnType != Type::Int) {
            addDiag(diags, std::string("__len__ must return int in class: ") + clsPtr->name, fn);
          }
          if (fn->name == "__get__") {
            const size_t n = fn->params.size();
            if (!(n == 2 || n == 3)) { addDiag(diags, std::string("__get__ must take 2 or 3 params in class: ") + clsPtr->name, fn); }
          }
          if (fn->name == "__set__") {
            const size_t n = fn->params.size();
            if (n != 3) { addDiag(diags, std::string("__set__ must take exactly 3 params in class: ") + clsPtr->name, fn); }
          }
          if (fn->name == "__delete__") {
            const size_t n = fn->params.size(); if (n != 2) { addDiag(diags, std::string("__delete__ must take exactly 2 params in class: ") + clsPtr->name, fn); }
          }
          if (fn->name == "__getattr__") {
            const size_t n = fn->params.size(); if (n != 2) { addDiag(diags, std::string("__getattr__ must take exactly 2 params in class: ") + clsPtr->name, fn); }
          }
          if (fn->name == "__getattribute__") {
            const size_t n = fn->params.size(); if (n != 2) { addDiag(diags, std::string("__getattribute__ must take exactly 2 params in class: ") + clsPtr->name, fn); }
          }
          if (fn->name == "__setattr__") {
            const size_t n = fn->params.size(); if (n != 3) { addDiag(diags, std::string("__setattr__ must take exactly 3 params in class: ") + clsPtr->name, fn); }
          }
          if (fn->name == "__delattr__") {
            const size_t n = fn->params.size(); if (n != 2) { addDiag(diags, std::string("__delattr__ must take exactly 2 params in class: ") + clsPtr->name, fn); }
          }
          if (fn->name == "__bool__" && fn->returnType != Type::Bool) {
            addDiag(diags, std::string("__bool__ must return bool in class: ") + clsPtr->name, fn);
          }
          if ((fn->name == "__str__" || fn->name == "__repr__") && fn->returnType != Type::Str) {
            addDiag(diags, std::string(fn->name) + std::string(" must return str in class: ") + clsPtr->name, fn);
          }
          Sig ms; ms.ret = fn->returnType;
          for (const auto& p : fn->params) {
            ms.params.push_back(p.type);
            SigParam sp; sp.name = p.name; sp.type = p.type; sp.isVarArg = p.isVarArg; sp.isKwVarArg = p.isKwVarArg; sp.isKwOnly = p.isKwOnly; sp.isPosOnly = p.isPosOnly; sp.hasDefault = (p.defaultValue != nullptr);
            ms.full.push_back(std::move(sp));
          }
          ci.methods[fn->name] = std::move(ms);
        }
      }
    }
    classes[clsPtr->name] = std::move(ci);
  }
  // Pass 2: propagate base methods along ancestry (left-to-right), depth-first.
  std::function<void(const std::string&, ClassInfo&)> mergeFromBase;
  mergeFromBase = [&](const std::string& baseName, ClassInfo& dest) {
    auto itb = classes.find(baseName);
    if (itb == classes.end()) return;
    // Copy base's methods first so it overrides its own ancestors
    for (const auto& mkv : itb->second.methods) {
      if (dest.methods.find(mkv.first) == dest.methods.end()) { dest.methods[mkv.first] = mkv.second; }
    }
    // Then recurse into base's bases
    for (const auto& bb : itb->second.bases) { mergeFromBase(bb, dest); }
  };
  for (const auto& clsPtr : mod.classes) {
    if (!clsPtr) continue; auto itc = classes.find(clsPtr->name); if (itc == classes.end()) continue;
    for (const auto& bn : itc->second.bases) { mergeFromBase(bn, itc->second); }
  }
  // Publish into global signatures: ClassName.method
  for (const auto& kv : classes) {
    for (const auto& mkv : kv.second.methods) {
      sigs[kv.first + std::string(".") + mkv.first] = mkv.second;
    }
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
      if (!(typeIsInt(param.type) || typeIsBool(param.type) || typeIsFloat(param.type) || typeIsStr(param.type) || param.type == Type::List)) { Diagnostic diagVar; diagVar.message = "only int/bool/float/str/list params supported"; diags.push_back(std::move(diagVar)); return false; }
      // Apply union/optional modeling via type sets
      uint32_t mask = 0U;
      if (!param.unionTypes.empty()) {
        for (auto tk : param.unionTypes) { mask |= TypeEnv::maskForKind(tk); }
      } else {
        mask = TypeEnv::maskForKind(param.type);
      }
      env.defineSet(param.name, mask, {func->name, 0, 0});
      if (param.type == Type::List && param.listElemType != Type::NoneType) {
        env.defineListElems(param.name, TypeEnv::maskForKind(param.listElemType));
      }
    }
    struct StmtChecker : public ast::VisitorBase {
      StmtChecker(const ast::FunctionDef& fn_, const std::unordered_map<std::string, Sig>& sigs_,
                  const std::unordered_map<std::string, int>& retParamIdxs_, TypeEnv& env_, std::vector<Diagnostic>& diags_,
                  PolyRefs polyRefs_, std::vector<TypeEnv*> outerScopes_ = {}, bool inExcept_ = false,
                  const std::unordered_map<std::string, ClassInfo>* classes_ = nullptr)
        : fn(fn_), sigs(sigs_), retParamIdxs(retParamIdxs_), env(env_), diags(diags_), polyRefs(polyRefs_), outerScopes(std::move(outerScopes_)), inExcept(inExcept_), classes(classes_) {}
      const ast::FunctionDef& fn; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
      const std::unordered_map<std::string, Sig>& sigs; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
      const std::unordered_map<std::string, int>& retParamIdxs; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
      TypeEnv& env; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
      std::vector<Diagnostic>& diags; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
      PolyRefs polyRefs; // grouped polymorphic targets
      bool ok{true};
      // global/nonlocal intent (subset): names declared here are not treated as locals
      std::unordered_set<std::string> globals;
      std::unordered_set<std::string> nonlocals;
      std::unordered_map<std::string, TypeEnv*> nonlocalTargets; // nonlocal name -> outer env
      std::vector<TypeEnv*> outerScopes; // nearest first
      bool inExcept{false};
      const std::unordered_map<std::string, ClassInfo>* classes{nullptr};

      bool infer(const ast::Expr* expr, Type& out) {
        std::vector<const TypeEnv*> ovec; ovec.reserve(outerScopes.size());
        for (auto* p : outerScopes) { ovec.push_back(p); }
        return inferExprType(expr, env, sigs, retParamIdxs, out, diags, PolyPtrs{&polyRefs.vars, &polyRefs.attrs}, &ovec, classes);
      }

      void visit(const ast::AssignStmt& assignStmt) override {
        // Monkey-patching as polymorphism: allow aliasing a function name within known code
        bool isPolyAlias = false;
        if (assignStmt.value && assignStmt.value->kind == ast::NodeKind::Name && !assignStmt.target.empty()) {
          const auto* rhs = static_cast<const ast::Name*>(assignStmt.value.get());
          auto it = sigs.find(rhs->id);
          if (it != sigs.end()) {
            polyRefs.vars[assignStmt.target].insert(rhs->id);
            isPolyAlias = true;
          } else {
            // Support alias-of-alias: h = k where k already aliases known functions
            auto itAlias = polyRefs.vars.find(rhs->id);
            if (itAlias != polyRefs.vars.end() && !itAlias->second.empty()) {
              for (const auto& tgt : itAlias->second) {
                if (sigs.find(tgt) != sigs.end()) { polyRefs.vars[assignStmt.target].insert(tgt); }
              }
              isPolyAlias = true;
            }
          }
        }
        // Attribute-based monkey patching: module.attr = function or alias (including alias-of-alias and attr-of-attr)
        if (assignStmt.value && !assignStmt.targets.empty()) {
          if (assignStmt.value->kind == ast::NodeKind::Name) {
            const auto* rhs = static_cast<const ast::Name*>(assignStmt.value.get());
            auto it = sigs.find(rhs->id);
            std::unordered_set<std::string> rhsTargets;
            if (it != sigs.end()) { rhsTargets.insert(rhs->id); }
            else {
              // Pull from variable alias map if present
              auto itAlias = polyRefs.vars.find(rhs->id);
              if (itAlias != polyRefs.vars.end()) { rhsTargets.insert(itAlias->second.begin(), itAlias->second.end()); }
            }
            if (!rhsTargets.empty()) {
              for (const auto& tgt : assignStmt.targets) {
                if (tgt && tgt->kind == ast::NodeKind::Attribute) {
                  const auto* attr = static_cast<const ast::Attribute*>(tgt.get());
                  if (attr->value && attr->value->kind == ast::NodeKind::Name) {
                    const auto* mod = static_cast<const ast::Name*>(attr->value.get());
                    const std::string key = mod->id + std::string(".") + attr->attr;
                    for (const auto& fn : rhsTargets) { polyRefs.attrs[key].insert(fn); }
                    isPolyAlias = true;
                  }
                }
              }
            } else {
              // RHS is not a known function/alias; for attribute patching, disallow
              for (const auto& tgt : assignStmt.targets) {
                if (tgt && tgt->kind == ast::NodeKind::Attribute) {
                  addDiag(diags, std::string("monkey patch target not found in known code: ") + rhs->id, assignStmt.value.get());
                  ok = false; return;
                }
              }
            }
          } else if (assignStmt.value->kind == ast::NodeKind::Attribute) {
            // module.attr = other.attr (copy existing attribute target set)
            const auto* rhsAttr = static_cast<const ast::Attribute*>(assignStmt.value.get());
            if (!(rhsAttr->value && rhsAttr->value->kind == ast::NodeKind::Name)) {
              addDiag(diags, "monkey patch rhs attribute must be module.attr", assignStmt.value.get()); ok = false; return;
            }
            const auto* rhsMod = static_cast<const ast::Name*>(rhsAttr->value.get());
            const std::string rhsKey = rhsMod->id + std::string(".") + rhsAttr->attr;
            auto itSet = polyRefs.attrs.find(rhsKey);
            if (itSet == polyRefs.attrs.end() || itSet->second.empty()) {
              addDiag(diags, std::string("monkey patch source attribute not found: ") + rhsKey, assignStmt.value.get()); ok = false; return;
            }
            for (const auto& tgt : assignStmt.targets) {
              if (tgt && tgt->kind == ast::NodeKind::Attribute) {
                const auto* attr = static_cast<const ast::Attribute*>(tgt.get());
                if (attr->value && attr->value->kind == ast::NodeKind::Name) {
                  const auto* mod = static_cast<const ast::Name*>(attr->value.get());
                  const std::string key = mod->id + std::string(".") + attr->attr;
                  polyRefs.attrs[key].insert(itSet->second.begin(), itSet->second.end());
                  isPolyAlias = true;
                }
              }
            }
          }
        }
        if (isPolyAlias) { return; }
        // Infer value with set awareness for normal assignments
        std::vector<const TypeEnv*> ovec; ovec.reserve(outerScopes.size()); for (auto* p : outerScopes) { ovec.push_back(p); }
        ExpressionTyper valTyper{env, sigs, retParamIdxs, diags, PolyPtrs{&polyRefs.vars, &polyRefs.attrs}, &ovec, classes};
        if (assignStmt.value) { assignStmt.value->accept(valTyper); } else { ok = false; return; }
        if (!valTyper.ok) { ok = false; return; }
        const Type typeOut = valTyper.out;
        const bool allowed = typeIsInt(typeOut) || typeIsBool(typeOut) || typeIsFloat(typeOut) || typeIsStr(typeOut) || typeOut == Type::List || typeOut == Type::Tuple || typeOut == Type::Dict || typeOut == Type::NoneType;
        if (!allowed) {
          addDiag(diags, "only int/bool/float/str/list/tuple/dict variables supported", &assignStmt); ok = false; return;
        }
        // If explicit targets are provided, update each; otherwise use legacy name-only target
        auto defineForName = [&](TypeEnv& tenv, const std::string& name, const ast::Expr* rhs) {
          const uint32_t maskVal = (valTyper.outSet != 0U) ? valTyper.outSet : TypeEnv::maskForKind(typeOut);
          tenv.defineSet(name, maskVal, {assignStmt.file, assignStmt.line, assignStmt.col});
          // Propagate element/key/value metadata for list/tuple/dict literals and aliases
          if (rhs && rhs->kind == ast::NodeKind::ListLiteral) {
            uint32_t elemMask = 0U; const auto* lst = static_cast<const ast::ListLiteral*>(rhs);
            bool allTuples = !lst->elements.empty();
            size_t tupleArity = 0;
            std::vector<uint32_t> perIndexMasks;
            for (const auto& el : lst->elements) {
              if (!el) continue;
              ExpressionTyper et{env, sigs, retParamIdxs, diags, PolyPtrs{&polyRefs.vars, &polyRefs.attrs}, &ovec, classes};
              el->accept(et); if (!et.ok) { ok = false; return; }
              elemMask |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out);
              // Track per-index tuple element masks if each element is a TupleLiteral
              if (el->kind == ast::NodeKind::TupleLiteral) {
                const auto* tp = static_cast<const ast::TupleLiteral*>(el.get());
                if (tupleArity == 0) { tupleArity = tp->elements.size(); perIndexMasks.assign(tupleArity, 0U); }
                if (tp->elements.size() != tupleArity) { allTuples = false; }
                for (size_t i = 0; i < tp->elements.size(); ++i) {
                  const auto& sub = tp->elements[i]; if (!sub) continue;
                  ExpressionTyper set{env, sigs, retParamIdxs, diags, PolyPtrs{&polyRefs.vars, &polyRefs.attrs}, &ovec, classes};
                  sub->accept(set); if (!set.ok) { ok = false; return; }
                  perIndexMasks[i] |= (set.outSet != 0U) ? set.outSet : TypeEnv::maskForKind(set.out);
                }
              } else {
                allTuples = false;
              }
            }
            tenv.defineListElems(name, elemMask);
            if (allTuples && !perIndexMasks.empty()) {
              // Record tuple element masks keyed by the list variable name to aid destructuring in comps
              tenv.defineTupleElems(name, std::move(perIndexMasks));
            }
          } else if (rhs && rhs->kind == ast::NodeKind::TupleLiteral) {
            const auto* tup = static_cast<const ast::TupleLiteral*>(rhs);
            std::vector<uint32_t> elems; elems.reserve(tup->elements.size());
            for (const auto& el : tup->elements) { if (!el) { elems.push_back(0U); continue; } ExpressionTyper et{env, sigs, retParamIdxs, diags, PolyPtrs{&polyRefs.vars, &polyRefs.attrs}, &ovec, classes}; el->accept(et); if (!et.ok) { ok = false; return; } elems.push_back((et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out)); }
            tenv.defineTupleElems(name, std::move(elems));
          } else if (rhs && rhs->kind == ast::NodeKind::DictLiteral) {
            const auto* dl = static_cast<const ast::DictLiteral*>(rhs);
            uint32_t k = 0U;
            uint32_t v = 0U;
            for (const auto& kv : dl->items) {
              if (kv.first) { ExpressionTyper kt{env, sigs, retParamIdxs, diags, PolyPtrs{&polyRefs.vars, &polyRefs.attrs}, &ovec, classes}; kv.first->accept(kt); if (!kt.ok) { ok = false; return; } k |= (kt.outSet != 0U) ? kt.outSet : TypeEnv::maskForKind(kt.out); }
              if (kv.second) { ExpressionTyper vt{env, sigs, retParamIdxs, diags, PolyPtrs{&polyRefs.vars, &polyRefs.attrs}, &ovec, classes}; kv.second->accept(vt); if (!vt.ok) { ok = false; return; } v |= (vt.outSet != 0U) ? vt.outSet : TypeEnv::maskForKind(vt.out); }
            }
            tenv.defineDictKeyVals(name, k, v);
          } else if (rhs && rhs->kind == ast::NodeKind::Name) {
            const auto* rhsn = static_cast<const ast::Name*>(rhs);
            // list elems
            const uint32_t e = env.getListElems(rhsn->id); if (e != 0U) { tenv.defineListElems(name, e); }
            // tuple elems
            {
              const uint32_t unionMask = env.unionOfTupleElems(rhsn->id); if (unionMask != 0U) {
                // We don't have the vector; attempt to copy by probing sequentially until zero (best-effort)
                std::vector<uint32_t> elems;
                for (size_t i = 0; i < 16; ++i) { const uint32_t mi = env.getTupleElemAt(rhsn->id, i); if (mi == 0U && i > 0) break; elems.push_back(mi); }
                if (!elems.empty()) { tenv.defineTupleElems(name, std::move(elems)); }
              }
            }
            // dict key/vals
            {
              const uint32_t k = env.getDictKeys(rhsn->id); const uint32_t v = env.getDictVals(rhsn->id); if (k != 0U || v != 0U) { tenv.defineDictKeyVals(name, k, v); }
            }
          }
        };
        // Define in correct scope
        if (nonlocals.find(assignStmt.target) != nonlocals.end()) {
          auto itN = nonlocalTargets.find(assignStmt.target);
          if (itN != nonlocalTargets.end() && itN->second != nullptr) {
            if (!assignStmt.targets.empty()) {
              for (const auto& tgt : assignStmt.targets) {
                if (!tgt) continue;
                if (tgt->kind == ast::NodeKind::Name) {
                  const auto* nm = static_cast<const ast::Name*>(tgt.get());
                  defineForName(*itN->second, nm->id, assignStmt.value.get());
                  if (assignStmt.value && assignStmt.value->kind == ast::NodeKind::Call && classes) {
                    const auto* c = static_cast<const ast::Call*>(assignStmt.value.get());
                    if (c->callee && c->callee->kind == ast::NodeKind::Name) {
                      const auto* cal = static_cast<const ast::Name*>(c->callee.get());
                      auto itCls = classes->find(cal->id);
                      if (itCls != classes->end()) {
                        itN->second->defineInstanceOf(nm->id, cal->id);
                        for (const auto& mkv : itCls->second.methods) {
                          const std::string instKey = nm->id + std::string(".") + mkv.first;
                          const std::string clsKey = cal->id + std::string(".") + mkv.first;
                          polyRefs.attrs[instKey].insert(clsKey);
                        }
                      }
                    }
                  }
                }
                else if (tgt->kind == ast::NodeKind::Attribute) {
                  const auto* at = static_cast<const ast::Attribute*>(tgt.get()); if (at->value && at->value->kind == ast::NodeKind::Name) {
                    const auto* base = static_cast<const ast::Name*>(at->value.get()); const uint32_t m = (valTyper.outSet != 0U) ? valTyper.outSet : TypeEnv::maskForKind(typeOut); itN->second->defineAttr(base->id, at->attr, m);
                  }
                }
              }
            } else {
              defineForName(*itN->second, assignStmt.target, assignStmt.value.get());
              // Record instance binding for x = C()
              if (assignStmt.value && assignStmt.value->kind == ast::NodeKind::Call && classes) {
                const auto* c = static_cast<const ast::Call*>(assignStmt.value.get());
                if (c->callee && c->callee->kind == ast::NodeKind::Name) {
                  const auto* cal = static_cast<const ast::Name*>(c->callee.get());
                  auto itCls = classes->find(cal->id);
                  if (itCls != classes->end()) {
                    itN->second->defineInstanceOf(assignStmt.target, cal->id);
                    // Also map attribute calls on this instance to the class method signatures (polymorphic attrs)
                    for (const auto& mkv : itCls->second.methods) {
                      const std::string instKey = assignStmt.target + std::string(".") + mkv.first;
                      const std::string clsKey = cal->id + std::string(".") + mkv.first;
                      polyRefs.attrs[instKey].insert(clsKey);
                    }
                  }
                }
              }
            }
          } else { addDiag(diags, std::string("nonlocal target not found in outer scope: ") + assignStmt.target, &assignStmt); ok = false; return; }
        } else if (globals.find(assignStmt.target) == globals.end()) {
          if (!assignStmt.targets.empty()) {
            for (const auto& tgt : assignStmt.targets) {
              if (!tgt) continue;
              if (tgt->kind == ast::NodeKind::Name) {
                const auto* nm = static_cast<const ast::Name*>(tgt.get());
                defineForName(env, nm->id, assignStmt.value.get());
                if (assignStmt.value && assignStmt.value->kind == ast::NodeKind::Call && classes) {
                  const auto* c = static_cast<const ast::Call*>(assignStmt.value.get());
                  if (c->callee && c->callee->kind == ast::NodeKind::Name) {
                    const auto* cal = static_cast<const ast::Name*>(c->callee.get());
                    auto itCls = classes->find(cal->id);
                    if (itCls != classes->end()) {
                      env.defineInstanceOf(nm->id, cal->id);
                      for (const auto& mkv : itCls->second.methods) {
                        const std::string instKey = nm->id + std::string(".") + mkv.first;
                        const std::string clsKey = cal->id + std::string(".") + mkv.first;
                        polyRefs.attrs[instKey].insert(clsKey);
                      }
                    }
                  }
                }
              }
              else if (tgt->kind == ast::NodeKind::Attribute) {
                const auto* at = static_cast<const ast::Attribute*>(tgt.get()); if (at->value && at->value->kind == ast::NodeKind::Name) {
                  const auto* base = static_cast<const ast::Name*>(at->value.get()); const uint32_t m = (valTyper.outSet != 0U) ? valTyper.outSet : TypeEnv::maskForKind(typeOut); env.defineAttr(base->id, at->attr, m);
                }
              }
            }
          } else {
            defineForName(env, assignStmt.target, assignStmt.value.get());
            if (assignStmt.value && assignStmt.value->kind == ast::NodeKind::Call && classes) {
              const auto* c = static_cast<const ast::Call*>(assignStmt.value.get());
              if (c->callee && c->callee->kind == ast::NodeKind::Name) {
                const auto* cal = static_cast<const ast::Name*>(c->callee.get());
                auto itCls = classes->find(cal->id);
                if (itCls != classes->end()) {
                  env.defineInstanceOf(assignStmt.target, cal->id);
                  for (const auto& mkv : itCls->second.methods) {
                    const std::string instKey = assignStmt.target + std::string(".") + mkv.first;
                    const std::string clsKey = cal->id + std::string(".") + mkv.first;
                    polyRefs.attrs[instKey].insert(clsKey);
                  }
                }
              }
            }
          }
        }
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
          // x == None or x is None => then x: NoneType; x != None or x is not None => else x: NoneType
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
            if (bin.op == ast::BinaryOperator::Eq || bin.op == ast::BinaryOperator::Is) {
              refineEq(bin.lhs.get(), bin.rhs.get(), true);
              refineEq(bin.rhs.get(), bin.lhs.get(), true);
            } else if (bin.op == ast::BinaryOperator::Ne || bin.op == ast::BinaryOperator::IsNot) {
              refineEq(bin.lhs.get(), bin.rhs.get(), false);
              refineEq(bin.rhs.get(), bin.lhs.get(), false);
            }
          }
          // not(<expr>): swap then/else refinements for the operand expression (handles Eq/Ne/And/Or/Call)
          // NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
          void visit(const ast::Unary& unary) override {
            if (unary.op != ast::UnaryOperator::Not || !unary.operand) { return; }
            // Special-case: not isinstance(x,T)
            if (unary.operand->kind == ast::NodeKind::Call) {
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
                    return;
                  }
                }
              }
            }
            // General case: swap then/else for the operand
            ConditionRefiner swapped{ConditionRefiner::Envs{elseEnv, thenEnv}};
            unary.operand->accept(swapped);
          }
          // Apply negation refinement to a subexpression (for else-branch of OR)
          // NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
          void applyNegExpr(const ast::Expr* expr) {
            if (!expr) { return; }
            if (expr->kind == ast::NodeKind::BinaryExpr) {
              const auto* binaryNode = static_cast<const ast::Binary*>(expr);
              if (binaryNode->op == ast::BinaryOperator::And) {
                // not (A and B) => not A or not B; apply both negations (conservative accumulation)
                applyNegExpr(binaryNode->lhs.get());
                applyNegExpr(binaryNode->rhs.get());
                return;
              }
              if (binaryNode->op == ast::BinaryOperator::Eq || binaryNode->op == ast::BinaryOperator::Is) {
                auto excludeNone = [&](const ast::Expr* lhs, const ast::Expr* rhs) {
                  if (lhs && lhs->kind == ast::NodeKind::Name && rhs && rhs->kind == ast::NodeKind::NoneLiteral) {
                    const auto* nameNode = static_cast<const ast::Name*>(lhs);
                    elseEnv.excludeKind(nameNode->id, Type::NoneType);
                  }
                };
                excludeNone(binaryNode->lhs.get(), binaryNode->rhs.get());
                excludeNone(binaryNode->rhs.get(), binaryNode->lhs.get());
                return;
              }
              if (binaryNode->op == ast::BinaryOperator::Ne || binaryNode->op == ast::BinaryOperator::IsNot) {
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
              if (unaryExpr->op == ast::UnaryOperator::Not && unaryExpr->operand) {
                // not (E): the negation of a negation on a subexpression means we should apply the positive
                // refinement of E here, not the negation of E.
                const auto* inner = unaryExpr->operand.get();
                if (inner->kind == ast::NodeKind::BinaryExpr) {
                  const auto* innerBin = static_cast<const ast::Binary*>(inner);
                  if (innerBin->op == ast::BinaryOperator::Ne || innerBin->op == ast::BinaryOperator::IsNot) {
                    auto exNone = [&](const ast::Expr* l, const ast::Expr* r) {
                      if (l && l->kind == ast::NodeKind::Name && r && r->kind == ast::NodeKind::NoneLiteral) {
                        const auto* nm = static_cast<const ast::Name*>(l);
                        elseEnv.excludeKind(nm->id, Type::NoneType);
                      }
                    };
                    exNone(innerBin->lhs.get(), innerBin->rhs.get());
                    exNone(innerBin->rhs.get(), innerBin->lhs.get());
                    return;
                  }
                }
                // Fallback: propagate into operand as before
                applyNegExpr(unaryExpr->operand.get());
              }
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
        std::vector<std::pair<std::string, uint32_t>> refinedThenNames;
        std::vector<std::pair<std::string, uint32_t>> refinedElseNames;
        if (iff.cond) {
          ConditionRefiner ref{ConditionRefiner::Envs{thenL, elseL}};
          iff.cond->accept(ref);
          // Fallback simple-pattern refine for not isinstance(x,T) if visitor missed
          if (iff.cond->kind == ast::NodeKind::UnaryExpr) {
            const auto* un = static_cast<const ast::Unary*>(iff.cond.get());
            if (un->op == ast::UnaryOperator::Not && un->operand && un->operand->kind == ast::NodeKind::Call) {
              const auto* call = static_cast<const ast::Call*>(un->operand.get());
              if (call->callee && call->callee->kind == ast::NodeKind::Name && call->args.size() == 2 && call->args[0] && call->args[0]->kind == ast::NodeKind::Name && call->args[1] && call->args[1]->kind == ast::NodeKind::Name) {
                const auto* callee = static_cast<const ast::Name*>(call->callee.get());
                if (callee->id == "isinstance") {
                  const auto* var = static_cast<const ast::Name*>(call->args[0].get());
                  const auto* tnm = static_cast<const ast::Name*>(call->args[1].get());
                  const Type ty = ConditionRefiner::typeFromName(tnm->id);
                  if (ty != Type::NoneType) {
                    thenL.excludeKind(var->id, ty);
                    elseL.restrictToKind(var->id, ty);
                  }
                }
              }
            }
          }
          // Determine whether both branches end in return
          auto endsWithReturn = [](const std::vector<std::unique_ptr<ast::Stmt>>& body) {
            if (body.empty()) return false;
            const auto* last = body.back().get();
            return last && last->kind == ast::NodeKind::ReturnStmt;
          };
          const bool bothReturn = endsWithReturn(iff.thenBody) && endsWithReturn(iff.elseBody);
          // Skip contradictory branch only when both arms return (unreachable branch); otherwise, keep both for merge
          auto isContradictoryThen = [&](){
            for (const auto& nm : ref.thenNoneEq) {
              const uint32_t base = env.getSet(nm);
              const uint32_t bran = thenL.getSet(nm);
              if (base != 0U && bran != 0U && ((base & bran) == 0U)) { return true; }
            }
            return false;
          };
          skipThen = bothReturn && isContradictoryThen();
          skipElse = false;
          // Save refined-name lists for later merge checks
          refinedThenNames = ref.thenRefined;
          refinedElseNames = ref.elseRefined;
        }
        // then/else branches via a tiny visitor
        struct BranchChecker : public ast::VisitorBase {
          StmtChecker& parent; TypeEnv& envRef; const Type fnRet; bool& okRef;
          BranchChecker(StmtChecker& parentIn, TypeEnv& envIn, Type retType, bool& okIn)
              : parent(parentIn), envRef(envIn), fnRet(retType), okRef(okIn) {}
          bool inferLocal(const ast::Expr* expr, Type& outTy) {
            if (expr == nullptr) { addDiag(parent.diags, "null expression", nullptr); return false; }
            std::vector<const TypeEnv*> ovec; ovec.reserve(parent.outerScopes.size()); for (auto* p : parent.outerScopes) { ovec.push_back(p); }
            ExpressionTyper v{envRef, parent.sigs, parent.retParamIdxs, parent.diags, PolyPtrs{&parent.polyRefs.vars, &parent.polyRefs.attrs}, &ovec};
            expr->accept(v);
            if (!v.ok) { return false; }
            outTy = v.out;
            return true;
          }
          void visit(const ast::AssignStmt& assignStmt) override {
            // Mirror polymorphic alias handling within branch
            bool isPolyAlias = false;
            if (assignStmt.value && assignStmt.value->kind == ast::NodeKind::Name && !assignStmt.target.empty()) {
              const auto* rhs = static_cast<const ast::Name*>(assignStmt.value.get());
              auto it = parent.sigs.find(rhs->id);
              if (it != parent.sigs.end()) { parent.polyRefs.vars[assignStmt.target].insert(rhs->id); isPolyAlias = true; }
              else {
                // alias-of-alias in branch
                auto itAlias = parent.polyRefs.vars.find(rhs->id);
                if (itAlias != parent.polyRefs.vars.end() && !itAlias->second.empty()) {
                  for (const auto& tgt : itAlias->second) {
                    if (parent.sigs.find(tgt) != parent.sigs.end()) { parent.polyRefs.vars[assignStmt.target].insert(tgt); }
                  }
                  isPolyAlias = true;
                }
                // Not a function alias; treat as normal variable assignment handled below
              }
            }
            if (assignStmt.value && !assignStmt.targets.empty()) {
              if (assignStmt.value->kind == ast::NodeKind::Name) {
                const auto* rhs = static_cast<const ast::Name*>(assignStmt.value.get());
                std::unordered_set<std::string> rhsTargets;
                auto it = parent.sigs.find(rhs->id);
                if (it != parent.sigs.end()) { rhsTargets.insert(rhs->id); }
                else {
                  auto itAlias = parent.polyRefs.vars.find(rhs->id);
                  if (itAlias != parent.polyRefs.vars.end()) { rhsTargets.insert(itAlias->second.begin(), itAlias->second.end()); }
                }
                if (!rhsTargets.empty()) {
                  for (const auto& tgt : assignStmt.targets) {
                    if (tgt && tgt->kind == ast::NodeKind::Attribute) {
                      const auto* attr = static_cast<const ast::Attribute*>(tgt.get());
                      if (attr->value && attr->value->kind == ast::NodeKind::Name) {
                        const auto* mod = static_cast<const ast::Name*>(attr->value.get());
                        const std::string key = mod->id + std::string(".") + attr->attr;
                        for (const auto& fn : rhsTargets) { parent.polyRefs.attrs[key].insert(fn); }
                        isPolyAlias = true;
                      }
                    }
                  }
                } else {
                  for (const auto& tgt : assignStmt.targets) {
                    if (tgt && tgt->kind == ast::NodeKind::Attribute) {
                      addDiag(parent.diags, std::string("monkey patch target not found in known code: ") + rhs->id, assignStmt.value.get());
                      okRef = false; return;
                    }
                  }
                }
              } else if (assignStmt.value->kind == ast::NodeKind::Attribute) {
                const auto* rhsAttr = static_cast<const ast::Attribute*>(assignStmt.value.get());
                if (!(rhsAttr->value && rhsAttr->value->kind == ast::NodeKind::Name)) {
                  addDiag(parent.diags, "monkey patch rhs attribute must be module.attr", assignStmt.value.get()); okRef = false; return;
                }
                const auto* rhsMod = static_cast<const ast::Name*>(rhsAttr->value.get());
                const std::string rhsKey = rhsMod->id + std::string(".") + rhsAttr->attr;
                auto itSet = parent.polyRefs.attrs.find(rhsKey);
                if (itSet == parent.polyRefs.attrs.end() || itSet->second.empty()) {
                  addDiag(parent.diags, std::string("monkey patch source attribute not found: ") + rhsKey, assignStmt.value.get()); okRef = false; return;
                }
                for (const auto& tgt : assignStmt.targets) {
                  if (tgt && tgt->kind == ast::NodeKind::Attribute) {
                    const auto* attr = static_cast<const ast::Attribute*>(tgt.get());
                    if (attr->value && attr->value->kind == ast::NodeKind::Name) {
                      const auto* mod = static_cast<const ast::Name*>(attr->value.get());
                      const std::string key = mod->id + std::string(".") + attr->attr;
                      parent.polyRefs.attrs[key].insert(itSet->second.begin(), itSet->second.end());
                      isPolyAlias = true;
                    }
                  }
                }
              }
            }
            if (isPolyAlias) { return; }
            Type tmpType{}; if (!inferLocal(assignStmt.value.get(), tmpType)) { okRef = false; return; }
            if (parent.nonlocals.find(assignStmt.target) != parent.nonlocals.end()) {
              auto itN = parent.nonlocalTargets.find(assignStmt.target);
              if (itN != parent.nonlocalTargets.end() && itN->second != nullptr) {
                itN->second->define(assignStmt.target, tmpType, {assignStmt.file, assignStmt.line, assignStmt.col});
              } else { okRef = false; return; }
            } else {
              envRef.define(assignStmt.target, tmpType, {assignStmt.file, assignStmt.line, assignStmt.col});
            }
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
          // Do not eagerly flag contradictions here; they will be reported on use sites.
        }
      }

      void visit(const ast::ReturnStmt& retStmt) override {
        Type valueType{}; if (!infer(retStmt.value.get(), valueType)) { ok = false; return; }
        if (valueType != fn.returnType) { addDiag(diags, std::string("return type mismatch in function: ") + fn.name, &retStmt); ok = false; }
      }

      void visit(const ast::ExprStmt& stmt) override { if (stmt.value) { Type tmp{}; (void)infer(stmt.value.get(), tmp); } }

      void visit(const ast::GlobalStmt& gs) override {
        for (const auto& n : gs.names) { globals.insert(n); }
      }
      void visit(const ast::NonlocalStmt& ns) override {
        for (const auto& n : ns.names) {
          bool found = false;
          for (auto* o : outerScopes) {
            if (!o) continue;
            if (o->getSet(n) != 0U) { nonlocals.insert(n); nonlocalTargets[n] = o; found = true; break; }
          }
          if (!found) { addDiag(diags, std::string("nonlocal name not found in enclosing scope: ") + n, &ns); ok = false; return; }
        }
      }

      // Conservative loop handling: check body/else but do not change outer env
      void visit(const ast::WhileStmt& ws) override {
        Type condType{}; if (!(ws.cond && infer(ws.cond.get(), condType))) { ok = false; return; }
        if (!typeIsBool(condType)) { addDiag(diags, "while condition must be bool", &ws); ok = false; return; }
        // Snapshot environment before loop
        const TypeEnv before = env;
        // Evaluate loop body on a throwaway env (do not leak)
        {
          TypeEnv bodyEnv = before;
          StmtChecker inner{fn, sigs, retParamIdxs, bodyEnv, diags, polyRefs, outerScopes, false, classes};
          for (const auto& st : ws.thenBody) { if (!inner.ok) break; st->accept(inner); }
          if (!inner.ok) { ok = false; return; }
        }
        // Evaluate else on its own env; else executes only when loop terminates normally
        TypeEnv elseEnv = before;
        if (!ws.elseBody.empty()) {
          StmtChecker inner{fn, sigs, retParamIdxs, elseEnv, diags, polyRefs, outerScopes, false, classes};
          for (const auto& st : ws.elseBody) { if (!inner.ok) break; st->accept(inner); }
          if (!inner.ok) { ok = false; return; }
        }
        // Merge the two possible continuations conservatively: either break path (before) or normal (elseEnv)
        TypeEnv merged; merged.intersectFrom(before, elseEnv); env = merged;
      }

      void visit(const ast::ForStmt& fs) override {
        // Check iterable expression for now
        if (fs.iterable) { Type tmp{}; (void)infer(fs.iterable.get(), tmp); }
        // Snapshot before
        const TypeEnv before = env;
        // Evaluate body (throwaway env)
        {
          TypeEnv bodyEnv = before;
          StmtChecker inner{fn, sigs, retParamIdxs, bodyEnv, diags, polyRefs, outerScopes, false, classes};
          for (const auto& st : fs.thenBody) { if (!inner.ok) break; st->accept(inner); }
          if (!inner.ok) { ok = false; return; }
        }
        // Evaluate else on its own env
        TypeEnv elseEnv = before;
        if (!fs.elseBody.empty()) {
          StmtChecker inner{fn, sigs, retParamIdxs, elseEnv, diags, polyRefs, outerScopes, false, classes};
          for (const auto& st : fs.elseBody) { if (!inner.ok) break; st->accept(inner); }
          if (!inner.ok) { ok = false; return; }
        }
        // Merge conservatively: either break path (before) or normal termination (elseEnv)
        TypeEnv merged; merged.intersectFrom(before, elseEnv); env = merged;
      }

      void visit(const ast::TryStmt& ts) override {
        // Evaluate try body
        TypeEnv tryEnv = env;
        {
          StmtChecker inner{fn, sigs, retParamIdxs, tryEnv, diags, polyRefs, {}, false, classes};
          for (const auto& st : ts.body) { if (!inner.ok) break; st->accept(inner); }
          if (!inner.ok) { ok = false; return; }
        }
        // Evaluate except handlers (validate types, respect shadowing by exception hierarchy)
        std::vector<TypeEnv> handlerEnvs;
        std::vector<int> seenRanks;
        for (const auto& ehPtr : ts.handlers) {
          if (!ehPtr) continue;
          // Validate handler type expression
          int minRank = 100;
          if (ehPtr->type) {
            std::vector<int> ranks;
            if (!collectExceptionRanks(ehPtr->type.get(), ranks)) { addDiag(diags, "except handler type must be exception or tuple of exceptions", ehPtr->type.get()); ok = false; return; }
            for (const int r : ranks) { minRank = std::min(minRank, r); }
          } else {
            minRank = 0; // bare except
          }
          // Shadowing detection: if a broader (lower rank) was seen earlier and current is a subtype, it's shadowed
          bool shadowed = false;
          for (const int pr : seenRanks) { if (pr <= minRank) { shadowed = true; break; } }
          if (shadowed) {
            addDiag(diags, "except handler shadowed by broader previous handler", ehPtr.get());
            ok = false; // treat as error in this subset
            continue;
          }
          if (minRank != 100) { seenRanks.push_back(minRank); }
          TypeEnv he = env; // start from original env
          // Bind exception name if present ("except X as name:") in the handler's scope
          if (!ehPtr->name.empty()) {
            // Use a wide type-set (any of the known kinds) since we do not model exception types explicitly.
            uint32_t anyMask = 0U;
            anyMask |= TypeEnv::maskForKind(Type::NoneType);
            anyMask |= TypeEnv::maskForKind(Type::Int);
            anyMask |= TypeEnv::maskForKind(Type::Bool);
            anyMask |= TypeEnv::maskForKind(Type::Float);
            anyMask |= TypeEnv::maskForKind(Type::Str);
            anyMask |= TypeEnv::maskForKind(Type::List);
            anyMask |= TypeEnv::maskForKind(Type::Tuple);
            anyMask |= TypeEnv::maskForKind(Type::Dict);
            he.defineSet(ehPtr->name, anyMask, {fn.name, 0, 0});
          }
          StmtChecker inner{fn, sigs, retParamIdxs, he, diags, polyRefs, {}, true, classes};
          for (const auto& st : ehPtr->body) { if (!inner.ok) break; st->accept(inner); }
          if (!inner.ok) { ok = false; return; }
          handlerEnvs.push_back(he);
        }
        // Optional else suite (runs if no exception)
        TypeEnv elseEnv = tryEnv;
        if (!ts.orelse.empty()) {
          StmtChecker inner{fn, sigs, retParamIdxs, elseEnv, diags, polyRefs, {}, false, classes};
          for (const auto& st : ts.orelse) { if (!inner.ok) break; st->accept(inner); }
          if (!inner.ok) { ok = false; return; }
        }
        // Merge all possible continuation envs conservatively: intersect across try, else, handlers
        TypeEnv merged;
        bool first = true;
        auto mergeWith = [&](const TypeEnv& next) {
          if (first) { merged.intersectFrom(next, next); first = false; }
          else { TypeEnv tmp; tmp.intersectFrom(merged, next); merged = tmp; }
        };
        mergeWith(tryEnv);
        if (!ts.orelse.empty()) { mergeWith(elseEnv); }
        for (const auto& he : handlerEnvs) { mergeWith(he); }
        // Finally suite: evaluate but do not leak new bindings (conservative); could add checks here
        if (!ts.finalbody.empty()) {
          TypeEnv finEnv = merged;
          StmtChecker inner{fn, sigs, retParamIdxs, finEnv, diags, polyRefs, outerScopes, false, classes};
          for (const auto& st : ts.finalbody) { if (!inner.ok) break; st->accept(inner); }
          if (!inner.ok) { ok = false; return; }
        }
        env = merged;
      }

      void visit(const ast::WithStmt& ws) override {
        // Analyze each with-item context expression; bind 'as' name if present using the inferred type set.
        for (const auto& it : ws.items) {
          if (!it) continue;
          if (it->context) {
            Type ctxTy{}; if (!infer(it->context.get(), ctxTy)) { ok = false; return; }
            // If an alias name is provided, bind it in current env with the context's type set
            if (!it->asName.empty()) {
              // Determine type mask from current env resolution of the context expression when it's a name; otherwise use exact kind
              uint32_t mask = 0U;
              if (it->context->kind == ast::NodeKind::Name) {
                const auto* nm = static_cast<const ast::Name*>(it->context.get());
                mask = env.getSet(nm->id);
              }
              if (mask == 0U) { mask = TypeEnv::maskForKind(ctxTy); }
              env.defineSet(it->asName, mask, {ws.file, ws.line, ws.col});
            }
          } else {
            addDiag(diags, "with-item missing context expression", &ws);
            ok = false; return;
          }
        }
        // Body executes in the same scope (no special block scope in this subset)
        for (const auto& st : ws.body) { if (!ok) break; st->accept(*this); }
      }

      // Pattern matching helpers
      void bindNameToSubject(TypeEnv& tenv, const std::string& name, const ast::Expr* /*subject*/, Type subjType) {
        if (name == "_") return;
        uint32_t mask = TypeEnv::maskForKind(subjType);
        tenv.defineSet(name, mask, {fn.name, 0, 0});
      }

      bool bindPattern(const ast::Pattern* pat, const ast::Expr* subject, Type subjType, TypeEnv& tenv) {
        using NK = ast::NodeKind;
        if (!pat) return true;
        switch (pat->kind) {
          case NK::PatternWildcard: return true;
          case NK::PatternName: {
            const auto* pn = static_cast<const ast::PatternName*>(pat);
            bindNameToSubject(tenv, pn->name, subject, subjType); return true;
          }
          case NK::PatternLiteral: {
            const auto* pl = static_cast<const ast::PatternLiteral*>(pat);
            if (!pl->value) return true;
            Type litT{}; if (!infer(pl->value.get(), litT)) return false;
            if (litT != subjType) { addDiag(diags, "pattern literal type mismatch", pl); return false; }
            return true;
          }
          case NK::PatternAs: {
            const auto* pa = static_cast<const ast::PatternAs*>(pat);
            if (pa->pattern && !bindPattern(pa->pattern.get(), subject, subjType, tenv)) return false;
            bindNameToSubject(tenv, pa->name, subject, subjType); return true;
          }
          case NK::PatternOr: {
            const auto* por = static_cast<const ast::PatternOr*>(pat);
            for (const auto& alt : por->patterns) {
              TypeEnv tmp = tenv; if (!bindPattern(alt.get(), subject, subjType, tmp)) return false;
            }
            return true;
          }
          case NK::PatternSequence: {
            const auto* ps = static_cast<const ast::PatternSequence*>(pat);
            if (ps->isList && subjType != Type::List) { addDiag(diags, "sequence pattern requires list subject", ps); return false; }
            if (!ps->isList && subjType != Type::Tuple) { addDiag(diags, "sequence pattern requires tuple subject", ps); return false; }
            uint32_t elemMask = 0U;
            if (subject && subject->kind == ast::NodeKind::Name) {
              const auto* nm = static_cast<const ast::Name*>(subject);
              if (ps->isList) { elemMask = tenv.getListElems(nm->id); }
            }
            for (size_t i = 0; i < ps->elements.size(); ++i) {
              const auto& el = ps->elements[i]; if (!el) continue;
              if (el->kind == NK::PatternStar) {
                const auto* st = static_cast<const ast::PatternStar*>(el.get());
                if (st->name != "_") { tenv.defineSet(st->name, TypeEnv::maskForKind(Type::List), {fn.name, 0, 0}); }
              } else {
                Type elType = subjType;
                if (!ps->isList && subject && subject->kind == ast::NodeKind::Name) {
                  const auto* nm = static_cast<const ast::Name*>(subject);
                  const uint32_t mi = tenv.getTupleElemAt(nm->id, i);
                  if (mi != 0U && TypeEnv::isSingleMask(mi)) elType = TypeEnv::kindFromMask(mi);
                } else if (ps->isList && elemMask != 0U && TypeEnv::isSingleMask(elemMask)) {
                  elType = TypeEnv::kindFromMask(elemMask);
                }
                if (!bindPattern(el.get(), subject, elType, tenv)) return false;
              }
            }
            return true;
          }
          case NK::PatternMapping: {
            const auto* pm = static_cast<const ast::PatternMapping*>(pat);
            if (subjType != Type::Dict) { addDiag(diags, "mapping pattern requires dict subject", pm); return false; }
            if (pm->hasRest && pm->restName != "_") { tenv.defineSet(pm->restName, TypeEnv::maskForKind(Type::Dict), {fn.name, 0, 0}); }
            uint32_t valMask = 0U;
            if (subject && subject->kind == ast::NodeKind::Name) {
              const auto* nm = static_cast<const ast::Name*>(subject);
              valMask = tenv.getDictVals(nm->id);
            }
            for (const auto& kv : pm->items) {
              Type vType = subjType; if (valMask != 0U && TypeEnv::isSingleMask(valMask)) vType = TypeEnv::kindFromMask(valMask);
              if (!bindPattern(kv.second.get(), subject, vType, tenv)) return false;
            }
            return true;
          }
          case NK::PatternClass: {
            const auto* pc = static_cast<const ast::PatternClass*>(pat);
            bool okInst = true;
            if (subject && subject->kind == ast::NodeKind::Name) {
              const auto* nm = static_cast<const ast::Name*>(subject);
              auto inst = tenv.instanceOf(nm->id);
              if (inst && *inst != pc->className) { okInst = false; }
            }
            if (!okInst) { addDiag(diags, "class pattern requires instance of class", pc); return false; }
            for (const auto& ap : pc->args) { if (!bindPattern(ap.get(), subject, subjType, tenv)) return false; }
            for (const auto& kp : pc->kwargs) { if (!bindPattern(kp.second.get(), subject, subjType, tenv)) return false; }
            return true;
          }
          default: return true;
        }
      }

      void visit(const ast::MatchStmt& ms) override {
        Type subjT{}; if (!(ms.subject && infer(ms.subject.get(), subjT))) { ok = false; return; }
        for (const auto& cs : ms.cases) {
          if (!cs) continue;
          TypeEnv caseEnv = env;
          if (cs->pattern && !bindPattern(cs->pattern.get(), ms.subject.get(), subjT, caseEnv)) { ok = false; return; }
          if (cs->guard) {
            // Evaluate guard in caseEnv so captures are visible
            ExpressionTyper gty{caseEnv, sigs, retParamIdxs, diags, PolyPtrs{&polyRefs.vars, &polyRefs.attrs}, nullptr, classes};
            cs->guard->accept(gty); if (!gty.ok) { ok = false; return; }
            if (!typeIsBool(gty.out)) { addDiag(diags, "match guard must be bool", cs->guard.get()); ok = false; return; }
          }
          StmtChecker inner{fn, sigs, retParamIdxs, caseEnv, diags, polyRefs, outerScopes, inExcept, classes};
          for (const auto& st : cs->body) { if (!inner.ok) break; st->accept(inner); }
          if (!inner.ok) { ok = false; return; }
        }
      }

      void visit(const ast::RaiseStmt& rs) override {
        // raise [exc] [from cause]
        auto isExceptionExpr = [&](const ast::Expr* e) -> bool {
          if (!e) return false;
          if (e->kind == ast::NodeKind::Name) {
            // A bare name could be an exception class or a bound instance (e.g., "except X as e")
            const auto* n = static_cast<const ast::Name*>(e);
            if (exceptionRankOfName(n->id) != 100) { return true; }
            // Treat any bound local name as a valid exception value in this subset
            if (env.getSet(n->id) != 0U || env.get(n->id).has_value()) { return true; }
            return false;
          }
          if (e->kind == ast::NodeKind::NoneLiteral) { return true; }
          if (e->kind == ast::NodeKind::TupleLiteral) {
            std::vector<int> ranks; return collectExceptionRanks(e, ranks);
          }
          if (e->kind == ast::NodeKind::Call) {
            const auto* c = static_cast<const ast::Call*>(e);
            if (c->callee && c->callee->kind == ast::NodeKind::Name) {
              const auto* n = static_cast<const ast::Name*>(c->callee.get()); return exceptionRankOfName(n->id) != 100;
            }
            return false;
          }
          return false;
        };
        if (!rs.exc) {
          if (!inExcept) { addDiag(diags, "bare raise outside except handler", &rs); ok = false; return; }
        } else {
          // disallow raising None and non-exception literals
          if (!isExceptionExpr(rs.exc.get()) || rs.exc->kind == ast::NodeKind::NoneLiteral) { addDiag(diags, "raise target must be exception type or instance", rs.exc.get()); ok = false; return; }
        }
        if (rs.cause) {
          // Allow 'from <name>' when the name is bound in scope, or any recognized exception, or None
          if (rs.cause->kind == ast::NodeKind::NoneLiteral) { return; }
          if (rs.cause->kind == ast::NodeKind::Name) {
            const auto* n = static_cast<const ast::Name*>(rs.cause.get());
            if (env.getSet(n->id) != 0U || env.get(n->id).has_value()) { return; }
          }
          if (!isExceptionExpr(rs.cause.get())) { addDiag(diags, "raise cause must be exception or None", rs.cause.get()); ok = false; return; }
        }
      }

      void visit(const ast::ClassDef& cls) override {
        // Class body has its own scope; names do not leak to enclosing function scope.
        // Methods are FunctionDef nodes; analyze them as nested functions but do not provide classEnv as an outer scope so
        // that free-variable lookups do not capture class-local names.
        // Evaluate bases and decorators expressions
        for (const auto& b : cls.bases) { if (b) { Type tmp{}; (void)infer(b.get(), tmp); } }
        for (const auto& d : cls.decorators) { if (d) { Type tmp{}; (void)infer(d.get(), tmp); } }
        // Collect simple assigned names inside the class to ensure they don't leak
        std::unordered_set<std::string> classLocalNames;
        auto collectFromExpr = [&](const ast::Expr* e, auto&& selfRef) -> void {
          if (!e) return;
          using NK = ast::NodeKind;
          switch (e->kind) {
            case NK::Name: classLocalNames.insert(static_cast<const ast::Name*>(e)->id); break;
            case NK::TupleLiteral: {
              const auto* t = static_cast<const ast::TupleLiteral*>(e);
              for (const auto& el : t->elements) { selfRef(el.get(), selfRef); }
              break;
            }
            case NK::ListLiteral: {
              const auto* l = static_cast<const ast::ListLiteral*>(e);
              for (const auto& el : l->elements) { selfRef(el.get(), selfRef); }
              break;
            }
            default: break;
          }
        };
        for (const auto& st : cls.body) {
          if (!st) continue;
          if (st->kind == ast::NodeKind::AssignStmt) {
            const auto* as = static_cast<const ast::AssignStmt*>(st.get());
            if (!as->targets.empty()) { for (const auto& t : as->targets) { collectFromExpr(t.get(), collectFromExpr); } }
            else if (!as->target.empty()) { classLocalNames.insert(as->target); }
          }
        }

        // Visit class body statements in an isolated environment so names do not leak
        TypeEnv classEnv;
        StmtChecker classChecker{fn, sigs, retParamIdxs, classEnv, diags, polyRefs, outerScopes};
        for (const auto& st : cls.body) {
          if (!ok) break;
          if (st->kind == ast::NodeKind::DefStmt) {
            auto* def = static_cast<ast::DefStmt*>(st.get());
            if (def && def->func) {
              auto* inner = def->func.get();
              TypeEnv childEnv;
              for (const auto& p : inner->params) { childEnv.define(p.name, p.type, {inner->name, 0, 0}); }
              std::vector<TypeEnv*> out; // do not include classEnv
              out.push_back(&env);
              for (auto* p : outerScopes) out.push_back(p);
              std::unordered_map<std::string, std::unordered_set<std::string>> poly; std::unordered_map<std::string, std::unordered_set<std::string>> polyAttr;
              StmtChecker nested{*inner, sigs, retParamIdxs, childEnv, diags, PolyRefs{poly, polyAttr}, std::move(out), false, classes};
              for (const auto& s2 : inner->body) { if (!nested.ok) break; s2->accept(nested); }
              if (!nested.ok) { ok = false; return; }
            }
          } else {
            st->accept(classChecker);
            if (!classChecker.ok) { ok = false; return; }
          }
        }
        // Ensure any names assigned in class scope are not read from outer scope
        for (const auto& nm : classLocalNames) { env.defineSet(nm, 0U, {cls.name, cls.line, cls.col}); }
      }

      // Unused in stmt context; name parameters for readability
      void visit(const ast::Module& moduleNode) override { (void)moduleNode; }
      void visit(const ast::FunctionDef& innerFn) override {
        // Analyze nested function with access to outer scopes for nonlocal/read-only captures
        TypeEnv childEnv;
        for (const auto& p : innerFn.params) { childEnv.define(p.name, p.type, {innerFn.name, 0, 0}); }
        std::vector<TypeEnv*> childOuters; childOuters.push_back(&env); for (auto* p : outerScopes) childOuters.push_back(p);
        std::unordered_map<std::string, std::unordered_set<std::string>> poly; std::unordered_map<std::string, std::unordered_set<std::string>> polyAttr;
        StmtChecker nested{innerFn, sigs, retParamIdxs, childEnv, diags, PolyRefs{poly, polyAttr}, std::move(childOuters), false, classes};
        for (const auto& st : innerFn.body) { if (!nested.ok) break; st->accept(nested); }
        if (!nested.ok) { ok = false; }
      }
      void visit(const ast::DefStmt& ds) override {
        if (!ds.func) return;
        const auto& innerFn = *ds.func;
        // Pre-scan inner function for nonlocal statements and validate against outer scopes
        struct NLScan : public ast::VisitorBase {
          StmtChecker& parent; bool ok{true};
          explicit NLScan(StmtChecker& p) : parent(p) {}
          void visit(const ast::NonlocalStmt& ns) override {
            for (const auto& n : ns.names) {
              bool found = false;
              // search current env's outers (parent.env and its outers)
              if (parent.env.getSet(n) != 0U) { found = true; }
              if (!found) {
                for (auto* o : parent.outerScopes) { if (o && o->getSet(n) != 0U) { found = true; break; } }
              }
              if (!found) { addDiag(parent.diags, std::string("nonlocal name not found in enclosing scope: ") + n, &ns); ok = false; return; }
            }
          }
          // No-op overrides
          void visit(const ast::Module& /*module*/) override {}
          void visit(const ast::FunctionDef& /*fn*/) override {}
          void visit(const ast::ReturnStmt& /*ret*/) override {}
          void visit(const ast::AssignStmt& /*as*/) override {}
          void visit(const ast::IfStmt& /*iff*/) override {}
          void visit(const ast::ExprStmt& /*es*/) override {}
          void visit(const ast::Literal<long long, ast::NodeKind::IntLiteral>& /*lit*/) override {}
          void visit(const ast::Literal<bool, ast::NodeKind::BoolLiteral>& /*lit*/) override {}
          void visit(const ast::Literal<double, ast::NodeKind::FloatLiteral>& /*lit*/) override {}
          void visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral>& /*lit*/) override {}
          void visit(const ast::NoneLiteral& /*nil*/) override {}
          void visit(const ast::Name& /*name*/) override {}
          void visit(const ast::Call& /*call*/) override {}
          void visit(const ast::Binary& /*bin*/) override {}
          void visit(const ast::Unary& /*un*/) override {}
          void visit(const ast::TupleLiteral& /*tup*/) override {}
          void visit(const ast::ListLiteral& /*lst*/) override {}
          void visit(const ast::ObjectLiteral& /*obj*/) override {}
        };
        NLScan scanner{*this};
        for (const auto& st : innerFn.body) { if (!scanner.ok) break; st->accept(scanner); }
        if (!scanner.ok) { ok = false; }
      }
      void visit(const ast::IntLiteral& intNode) override { (void)intNode; }
      void visit(const ast::BoolLiteral& boolNode) override { (void)boolNode; }
      void visit(const ast::FloatLiteral& floatNode) override { (void)floatNode; }
      void visit(const ast::Name& nameNode) override { (void)nameNode; }
      void visit(const ast::Call& callNode2) override { (void)callNode2; }
      static int exceptionRankOfName(const std::string& nm) {
        // Broad base classes
        if (nm == "BaseException") return 0;
        if (nm == "Exception") return 1;
        // Common intermediate bases
        if (nm == "ArithmeticError" || nm == "LookupError" || nm == "RuntimeError") return 2;
        if (nm == "OSError" || nm == "EnvironmentError" || nm == "IOError") return 2;
        if (nm == "ConnectionError") return 2;
        // Specific built-ins
        if (nm == "ValueError" || nm == "TypeError" || nm == "KeyError" || nm == "IndexError" || nm == "ZeroDivisionError" || nm == "AttributeError" || nm == "EOFError" || nm == "AssertionError" || nm == "SystemError" || nm == "MemoryError" || nm == "NameError" || nm == "UnboundLocalError" || nm == "ImportError" || nm == "ModuleNotFoundError" || nm == "NotImplementedError" || nm == "RecursionError") return 3;
        // OSError family
        if (nm == "BlockingIOError" || nm == "ChildProcessError" || nm == "FileExistsError" || nm == "FileNotFoundError" || nm == "BrokenPipeError" ||
            nm == "InterruptedError" || nm == "IsADirectoryError" || nm == "NotADirectoryError" || nm == "PermissionError" ||
            nm == "ProcessLookupError" || nm == "TimeoutError") return 3;
        // ConnectionError family
        if (nm == "ConnectionAbortedError" || nm == "ConnectionRefusedError" || nm == "ConnectionResetError") return 3;
        // UnicodeError family
        if (nm == "UnicodeError" || nm == "UnicodeDecodeError" || nm == "UnicodeEncodeError" || nm == "UnicodeTranslateError") return 3;
        return 100; // unknown
      }
      static bool collectExceptionRanks(const ast::Expr* e, std::vector<int>& outRanks) {
        if (!e) return false;
        if (e->kind == ast::NodeKind::Name) {
          const auto* n = static_cast<const ast::Name*>(e);
          const int r = exceptionRankOfName(n->id);
          if (r == 100) return false;
          outRanks.push_back(r); return true;
        }
        if (e->kind == ast::NodeKind::TupleLiteral) {
          const auto* t = static_cast<const ast::TupleLiteral*>(e);
          bool okLocal = true; for (const auto& el : t->elements) { if (!el) continue; okLocal &= collectExceptionRanks(el.get(), outRanks); }
          return okLocal && !outRanks.empty();
        }
        return false;
      }
      void visit(const ast::Binary& binaryNode2) override { (void)binaryNode2; }
      void visit(const ast::Unary& unaryNode2) override { (void)unaryNode2; }
      void visit(const ast::StringLiteral& strNode) override { (void)strNode; }
      void visit(const ast::TupleLiteral& tupleNode) override { (void)tupleNode; }
      void visit(const ast::ListLiteral& listNode) override { (void)listNode; }
      void visit(const ast::ObjectLiteral& objNode) override { (void)objNode; }
      void visit(const ast::NoneLiteral& noneNode) override { (void)noneNode; }
    };

    std::unordered_map<std::string, std::unordered_set<std::string>> poly; // per-function polymorphic call targets
    std::unordered_map<std::string, std::unordered_set<std::string>> polyAttr; // per-function polymorphic attribute call targets
    // Evaluate function decorators/annotations expressions for basic correctness
    for (const auto& dec : func->decorators) {
      if (!dec) continue;
      Type tmp{};
      (void)inferExprType(dec.get(), env, sigs, retParamIdxs, tmp, diags, {});
      if (!diags.empty()) { return false; }
    }
    StmtChecker checker{*func, sigs, retParamIdxs, env, diags, PolyRefs{poly, polyAttr}, {}, false, &classes};
    for (const auto& stmt : func->body) {
      stmt->accept(checker);
      if (!checker.ok) { return false; }
    }
  }
  return diags.empty();
}

} // namespace pycc::sema
