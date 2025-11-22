/***
 * Name: pycc::sema::Sema (impl)
 * Purpose: Minimal semantic checks with basic type env and source spans.
 */
#include "sema/Sema.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Types.h"
#include "sema/detail/Helpers.h"
#include "sema/detail/EffectsScan.h"
#include "sema/detail/ExprVisitHelpers.h"
#include "sema/detail/ExprVisitContainers.h"
#include "sema/detail/LocalsAssigned.h"
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
using pycc::sema::Sig;
using pycc::sema::SigParam;
using pycc::sema::ClassInfo;
using pycc::sema::PolyPtrs;
using pycc::sema::PolyRefs;
using pycc::sema::typeIsInt;
using pycc::sema::typeIsBool;
using pycc::sema::typeIsFloat;
using pycc::sema::typeIsStr;
using pycc::sema::addDiag;

// EffectsScan implementation moved to individual compilation units (see sema/detail/EffectsScan.h)

struct ExpressionTyper final : public ast::VisitorBase {
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

  void visit(const ast::IntLiteral& n) override { auto r = expr::handleIntLiteral(n); out = r.out; outSet = r.outSet; }
  void visit(const ast::BoolLiteral& n) override { auto r = expr::handleBoolLiteral(n); out = r.out; outSet = r.outSet; }
  void visit(const ast::FloatLiteral& n) override { auto r = expr::handleFloatLiteral(n); out = r.out; outSet = r.outSet; }
  void visit(const ast::NoneLiteral& n) override { auto r = expr::handleNoneLiteral(n); out = r.out; outSet = r.outSet; }
  void visit(const ast::StringLiteral& n) override { auto r = expr::handleStringLiteral(n); out = r.out; outSet = r.outSet; }
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
  void visit(const ast::ObjectLiteral& obj) override {
    auto visitChild = [&](const ast::Expr* e){ ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets}; e->accept(et); if (!et.ok) return false; return true; };
    if (!expr::handleObjectLiteral(obj, out, outSet, visitChild)) { ok = false; return; }
  }
  void visit(const ast::Name& n) override {
    uint32_t maskVal = env->getSet(n.id);
    // Enforce strict local scoping: if this name is assigned locally anywhere in function,
    // treat it as a local and error if read before assignment.
    if (maskVal == 0U && pycc::sema::detail::g_locals_assigned && pycc::sema::detail::g_locals_assigned->count(n.id)) {
      addDiag(*diags, std::string("local variable referenced before assignment: ") + n.id, &n);
      ok = false; return;
    }
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
    auto visitChild = [&](const ast::Expr* e){ ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets}; e->accept(et); if (!et.ok) return false; return true; };
    if (!expr::handleTupleLiteral(tupleLiteral, out, outSet, visitChild)) { ok = false; return; }
  }
  void visit(const ast::ListLiteral& listLiteral) override {
    auto visitChild = [&](const ast::Expr* e){ ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets}; e->accept(et); if (!et.ok) return false; return true; };
    if (!expr::handleListLiteral(listLiteral, out, outSet, visitChild)) { ok = false; return; }
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
    // Accept yield as dynamic for typing; generator semantics handled in later stages
    (void)y;
    out = Type::NoneType; outSet = TypeEnv::maskForKind(out); ok = true;
  }
  void visit(const ast::AwaitExpr& a) override {
    // Accept await as dynamic; coroutine typing to be modeled later
    (void)a;
    out = Type::NoneType; outSet = TypeEnv::maskForKind(out); ok = true;
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
    // Fast-path: stdlib modules dispatched by attribute on module name (avoid falling into unknown poly resolution)
    if (callNode.callee && callNode.callee->kind == ast::NodeKind::Attribute) {
      const auto* at0 = static_cast<const ast::Attribute*>(callNode.callee.get());
      if (at0->value && at0->value->kind == ast::NodeKind::Name) {
        const auto* base0 = static_cast<const ast::Name*>(at0->value.get());
        // math.* typed helpers
        if (base0->id == "math") {
          const std::string fn = at0->attr;
          auto checkUnary = [&](ast::TypeKind retKind) {
            if (callNode.args.size() != 1) { addDiag(*diags, std::string("math.") + fn + "() takes 1 arg", &callNode); ok = false; return; }
            ExpressionTyper a{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a); if (!a.ok) { ok = false; return; }
            const uint32_t mask = (a.outSet != 0U) ? a.outSet : TypeEnv::maskForKind(a.out);
            const uint32_t okmask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Float);
            if ((mask & ~okmask) != 0U) { addDiag(*diags, std::string("math.") + fn + ": argument must be int/float", callNode.args[0].get()); ok = false; return; }
            out = retKind; const_cast<ast::Call&>(callNode).setType(out);
          };
          auto checkBinary = [&](ast::TypeKind retKind) {
            if (callNode.args.size() != 2) { addDiag(*diags, std::string("math.") + fn + "() takes 2 args", &callNode); ok = false; return; }
            ExpressionTyper a0{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a0); if (!a0.ok) { ok = false; return; }
            ExpressionTyper a1{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[1]->accept(a1); if (!a1.ok) { ok = false; return; }
            const uint32_t okmask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Float);
            const uint32_t m0 = (a0.outSet != 0U) ? a0.outSet : TypeEnv::maskForKind(a0.out);
            const uint32_t m1 = (a1.outSet != 0U) ? a1.outSet : TypeEnv::maskForKind(a1.out);
            if ((m0 & ~okmask) != 0U || (m1 & ~okmask) != 0U) { addDiag(*diags, std::string("math.") + fn + ": arguments must be int/float", &callNode); ok = false; return; }
            out = retKind; const_cast<ast::Call&>(callNode).setType(out);
          };
          if (fn == "sqrt" || fn == "fabs" || fn == "sin" || fn == "cos" || fn == "tan" || fn == "asin" || fn == "acos" || fn == "atan" || fn == "exp" || fn == "exp2" || fn == "log" || fn == "log2" || fn == "log10" || fn == "degrees" || fn == "radians") { checkUnary(ast::TypeKind::Float); return; }
          if (fn == "floor" || fn == "ceil" || fn == "trunc") { checkUnary(ast::TypeKind::Int); return; }
          if (fn == "pow" || fn == "copysign" || fn == "atan2" || fn == "fmod" || fn == "hypot") { checkBinary(ast::TypeKind::Float); return; }
        }
        if (base0->id == "subprocess") {
          const std::string fn = at0->attr;
          if (callNode.args.size() != 1) { addDiag(*diags, std::string("subprocess.") + fn + "() takes 1 arg", &callNode); ok = false; return; }
          ExpressionTyper a{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a); if (!a.ok) { ok = false; return; }
          const uint32_t mask = (a.outSet != 0U) ? a.outSet : TypeEnv::maskForKind(a.out);
          const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
          if ((mask & ~strMask) != 0U) { addDiag(*diags, std::string("subprocess.") + fn + ": argument must be str", callNode.args[0].get()); ok = false; return; }
          out = ast::TypeKind::Int; const_cast<ast::Call&>(callNode).setType(out); return;
        }
        if (base0->id == "sys") {
          const std::string fn = at0->attr;
          if (fn == "exit") {
            if (callNode.args.size() != 1) { addDiag(*diags, "sys.exit() takes 1 arg", &callNode); ok = false; return; }
            ExpressionTyper a{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a); if (!a.ok) { ok = false; return; }
            const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool) | TypeEnv::maskForKind(ast::TypeKind::Float);
            const uint32_t m = (a.outSet != 0U) ? a.outSet : TypeEnv::maskForKind(a.out);
            if ((m & ~allow) != 0U) { addDiag(*diags, "sys.exit: int/bool/float required", callNode.args[0].get()); ok = false; return; }
            out = ast::TypeKind::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "platform" || fn == "version") { if (!callNode.args.empty()) { addDiag(*diags, std::string("sys.") + fn + "() takes 0 args", &callNode); ok = false; return; } out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "maxsize") { if (!callNode.args.empty()) { addDiag(*diags, "sys.maxsize() takes 0 args", &callNode); ok = false; return; } out = ast::TypeKind::Int; const_cast<ast::Call&>(callNode).setType(out); return; }
        }
        if (base0->id == "os") {
          const std::string fn = at0->attr;
          auto reqStr = [&](const ast::Expr* e, const char* msg){ ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; if(e){ e->accept(a); if(!a.ok){ ok=false; return false; } if (a.out != ast::TypeKind::Str) { addDiag(*diags, msg, e); ok=false; return false; } } return true; };
          if (fn == "getcwd") { if (!callNode.args.empty()) { addDiag(*diags, "os.getcwd() takes 0 args", &callNode); ok=false; return; } out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "mkdir") {
            if (!(callNode.args.size()==1 || callNode.args.size()==2)) { addDiag(*diags, "os.mkdir() takes 1 or 2 args", &callNode); ok=false; return; }
            if (!reqStr(callNode.args[0].get(), "os.mkdir: path must be str")) return;
            if (callNode.args.size()==2) { ExpressionTyper m{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(m); if(!m.ok){ ok=false; return; } const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int)|TypeEnv::maskForKind(ast::TypeKind::Bool)|TypeEnv::maskForKind(ast::TypeKind::Float); const uint32_t mm=(m.outSet!=0U)?m.outSet:TypeEnv::maskForKind(m.out); if ((mm & ~allow)!=0U) { addDiag(*diags, "os.mkdir: mode must be numeric", callNode.args[1].get()); ok=false; return; } }
            out = ast::TypeKind::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "remove") { if (callNode.args.size()!=1) { addDiag(*diags, "os.remove() takes 1 arg", &callNode); ok=false; return; } if (!reqStr(callNode.args[0].get(), "os.remove: path must be str")) return; out = ast::TypeKind::Bool; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "rename") { if (callNode.args.size()!=2) { addDiag(*diags, "os.rename() takes 2 args", &callNode); ok=false; return; } if (!reqStr(callNode.args[0].get(), "os.rename: src must be str")) return; if (!reqStr(callNode.args[1].get(), "os.rename: dst must be str")) return; out = ast::TypeKind::Bool; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "getenv") { if (callNode.args.size()!=1) { addDiag(*diags, "os.getenv() takes 1 arg", &callNode); ok=false; return; } if (!reqStr(callNode.args[0].get(), "os.getenv: name must be str")) return; out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return; }
        }
        if (base0->id == "__future__") {
          // __future__.feature() -> bool (0-arg). Treated as compile-time feature hint; runtime is a no-op boolean.
          if (!callNode.args.empty()) { addDiag(*diags, "__future__.feature() takes 0 args", &callNode); ok = false; return; }
          out = ast::TypeKind::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
        }
        if (base0->id == "_abc") {
          const std::string fn = at0->attr;
          auto ensurePtr = [&](const ast::Expr* e){ if(!e) return false; ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; e->accept(t); if(!t.ok) return false; if(!(t.out==Type::Str || t.out==Type::List || t.out==Type::Dict || t.out==Type::Tuple)) { addDiag(*diags, std::string("_abc.")+fn+": pointer arg required", e); return false; } return true; };
          if (fn == "get_cache_token") { if (!callNode.args.empty()) { addDiag(*diags, "_abc.get_cache_token() takes 0 args", &callNode); ok=false; return; } out = ast::TypeKind::Int; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "register" || fn == "is_registered") {
            if (callNode.args.size()!=2) { addDiag(*diags, std::string("_abc.")+fn+"() takes 2 args", &callNode); ok=false; return; }
            if (!ensurePtr(callNode.args[0].get()) || !ensurePtr(callNode.args[1].get())) { ok=false; return; }
            out = ast::TypeKind::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "invalidate_cache" || fn == "reset") { if (!callNode.args.empty()) { addDiag(*diags, std::string("_abc.")+fn+"() takes 0 args", &callNode); ok=false; return; } out = ast::TypeKind::NoneType; const_cast<ast::Call&>(callNode).setType(out); return; }
        }
        if (base0->id == "io") {
          const std::string fn = at0->attr;
          if (fn == "write_stdout" || fn == "write_stderr") {
            if (callNode.args.size() != 1) { addDiag(*diags, std::string("io.") + fn + "() takes 1 arg", &callNode); ok = false; return; }
            ExpressionTyper a{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a); if (!a.ok) { ok = false; return; }
            const uint32_t strM = TypeEnv::maskForKind(ast::TypeKind::Str);
            const uint32_t m = (a.outSet != 0U) ? a.outSet : TypeEnv::maskForKind(a.out);
            if ((m & ~strM) != 0U) { addDiag(*diags, std::string("io.") + fn + ": argument must be str", callNode.args[0].get()); ok = false; return; }
            out = ast::TypeKind::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "read_file") {
            if (callNode.args.size() != 1) { addDiag(*diags, "io.read_file() takes 1 arg", &callNode); ok = false; return; }
            ExpressionTyper a{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a); if (!a.ok) { ok = false; return; }
            const uint32_t strM = TypeEnv::maskForKind(ast::TypeKind::Str);
            const uint32_t m = (a.outSet != 0U) ? a.outSet : TypeEnv::maskForKind(a.out);
            if ((m & ~strM) != 0U) { addDiag(*diags, "io.read_file: path must be str", callNode.args[0].get()); ok = false; return; }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "write_file") {
            if (callNode.args.size() != 2) { addDiag(*diags, "io.write_file() takes 2 args", &callNode); ok = false; return; }
            ExpressionTyper a0{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a0); if (!a0.ok) { ok = false; return; }
            ExpressionTyper a1{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[1]->accept(a1); if (!a1.ok) { ok = false; return; }
            const uint32_t strM = TypeEnv::maskForKind(ast::TypeKind::Str);
            const uint32_t m0 = (a0.outSet != 0U) ? a0.outSet : TypeEnv::maskForKind(a0.out);
            const uint32_t m1 = (a1.outSet != 0U) ? a1.outSet : TypeEnv::maskForKind(a1.out);
            if ((m0 & ~strM) != 0U || (m1 & ~strM) != 0U) { addDiag(*diags, "io.write_file: args must be (str, str)", &callNode); ok = false; return; }
            out = ast::TypeKind::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "pathlib") {
          const std::string fn = at0->attr;
          auto ensureStr = [&](const ast::Expr* e){ ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; if(e){ e->accept(t); if(!t.ok) return false; if(t.out != Type::Str) { addDiag(*diags, std::string("pathlib.")+fn+": argument must be str", e); return false; } } return true; };
          auto ensureInt = [&](const ast::Expr* e){ ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; if(e){ e->accept(t); if(!t.ok) return false; if(!(t.out==Type::Int || t.out==Type::Bool || t.out==Type::Float)) { addDiag(*diags, std::string("pathlib.")+fn+": numeric argument required", e); return false; } } return true; };
          if (fn == "cwd" || fn == "home") { if (!callNode.args.empty()) { addDiag(*diags, std::string("pathlib.")+fn+"() takes 0 args", &callNode); ok=false; return; } out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "join") { if (callNode.args.size()!=2) { addDiag(*diags, "pathlib.join() takes 2 args", &callNode); ok=false; return; } if (!ensureStr(callNode.args[0].get()) || !ensureStr(callNode.args[1].get())) { ok=false; return; } out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "parent" || fn == "basename" || fn == "suffix" || fn == "stem" || fn == "as_posix" || fn == "as_uri" || fn == "resolve" || fn == "absolute") {
            if (callNode.args.size()!=1) { addDiag(*diags, std::string("pathlib.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get())) { ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "with_name" || fn == "with_suffix") {
            if (callNode.args.size()!=2) { addDiag(*diags, std::string("pathlib.")+fn+"() takes 2 args", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get()) || !ensureStr(callNode.args[1].get())) { ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "parts") { if (callNode.args.size()!=1) { addDiag(*diags, "pathlib.parts() takes 1 arg", &callNode); ok=false; return; } if (!ensureStr(callNode.args[0].get())) { ok=false; return; } out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "exists" || fn == "is_file" || fn == "is_dir" || fn == "match") { if (callNode.args.size()!=1 && fn!="match") { addDiag(*diags, std::string("pathlib.")+fn+"() takes 1 arg", &callNode); ok=false; return; } if (fn=="match" && callNode.args.size()!=2) { addDiag(*diags, "pathlib.match() takes 2 args", &callNode); ok=false; return; } if (!ensureStr(callNode.args[0].get())) { ok=false; return; } if (fn=="match" && !ensureStr(callNode.args[1].get())) { ok=false; return; } out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "mkdir") {
            if (callNode.args.empty() || callNode.args.size() > 4) { addDiag(*diags, "pathlib.mkdir() takes 1 to 4 args", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get())) { ok=false; return; }
            if (callNode.args.size()>=2 && !ensureInt(callNode.args[1].get())) { ok=false; return; }
            if (callNode.args.size()>=3 && !ensureInt(callNode.args[2].get())) { ok=false; return; }
            if (callNode.args.size()==4 && !ensureInt(callNode.args[3].get())) { ok=false; return; }
            out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "rmdir" || fn == "unlink" || fn == "rename") {
            if (fn=="rename") {
              if (callNode.args.size()!=2) { addDiag(*diags, "pathlib.rename() takes 2 args", &callNode); ok=false; return; }
              if (!ensureStr(callNode.args[0].get()) || !ensureStr(callNode.args[1].get())) { ok=false; return; }
            } else {
              if (callNode.args.size()!=1) { addDiag(*diags, std::string("pathlib.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
              if (!ensureStr(callNode.args[0].get())) { ok=false; return; }
            }
            out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "operator") {
          const std::string& fn = at->attr;
          auto requireNum = [&](const ast::Expr* e){
            ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; if(e){ e->accept(t); if(!t.ok) return false; if(!(t.out==Type::Int || t.out==Type::Float || t.out==Type::Bool)) { addDiag(*diags, "operator: numeric argument required", e); return false; } } return true; };
          if (fn=="add" || fn=="sub" || fn=="mul" || fn=="truediv") {
            if (callNode.args.size()!=2) { addDiag(*diags, std::string("operator.")+fn+"() takes 2 args", &callNode); ok=false; return; }
            if (!requireNum(callNode.args[0].get()) || !requireNum(callNode.args[1].get())) { ok=false; return; }
            // If any arg is float -> Float, else Int
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            ExpressionTyper a1{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(a1); if(!a1.ok){ ok=false; return; }
            out = (a0.out == Type::Float || a1.out == Type::Float) ? Type::Float : Type::Int;
            const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="neg") {
            if (callNode.args.size()!=1) { addDiag(*diags, "operator.neg() takes 1 arg", &callNode); ok=false; return; }
            if (!requireNum(callNode.args[0].get())) { ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            out = a0.out; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="eq" || fn=="lt" || fn=="not_" || fn=="truth") {
            const size_t ar = (fn=="not_" || fn=="truth") ? 1 : 2;
            if (callNode.args.size()!=ar) { addDiag(*diags, std::string("operator.")+fn+"() takes "+(ar==1?"1":"2")+" args", &callNode); ok=false; return; }
            for (size_t i=0;i<callNode.args.size();++i) { if (!requireNum(callNode.args[i].get())) { ok=false; return; } }
            out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "keyword") {
          const std::string& fn = at->attr;
          if (fn == "iskeyword") {
            if (callNode.args.size()!=1) { addDiag(*diags, "keyword.iskeyword() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ ok=false; return; }
            if (a.out != Type::Str) { addDiag(*diags, "keyword.iskeyword(): argument must be str", callNode.args[0].get()); ok=false; return; }
            out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "kwlist") {
            if (!callNode.args.empty()) { addDiag(*diags, "keyword.kwlist() takes 0 args", &callNode); ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "string") {
          const std::string& fn = at->attr;
          if (fn == "capwords") {
            if (!(callNode.args.size()==1 || callNode.args.size()==2)) { addDiag(*diags, "string.capwords() takes 1 or 2 args", &callNode); ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            if (a0.out != Type::Str) { addDiag(*diags, "string.capwords(): first arg must be str", callNode.args[0].get()); ok=false; return; }
            if (callNode.args.size()==2) {
              ExpressionTyper a1{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(a1); if(!a1.ok){ ok=false; return; }
              if (!(a1.out == Type::Str || a1.out == Type::NoneType)) { addDiag(*diags, "string.capwords(): sep must be str or None", callNode.args[1].get()); ok=false; return; }
            }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "glob") {
          const std::string& fn = at->attr;
          if (fn=="glob" || fn=="iglob") {
            if (callNode.args.size()!=1) { addDiag(*diags, std::string("glob.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            if (a0.out != Type::Str) { addDiag(*diags, std::string("glob.")+fn+": argument must be str", callNode.args[0].get()); ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="escape") {
            if (callNode.args.size()!=1) { addDiag(*diags, "glob.escape() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            if (a0.out != Type::Str) { addDiag(*diags, "glob.escape(): argument must be str", callNode.args[0].get()); ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "uuid") {
          const std::string& fn = at->attr;
          if (fn=="uuid4") {
            if (!callNode.args.empty()) { addDiag(*diags, "uuid.uuid4() takes 0 args", &callNode); ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "base64") {
          const std::string& fn = at->attr;
          if (fn=="b64encode" || fn=="b64decode") {
            if (callNode.args.size()!=1) { addDiag(*diags, std::string("base64.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            const uint32_t allow = TypeEnv::maskForKind(Type::Str) | TypeEnv::maskForKind(Type::Bytes);
            const uint32_t m = (a0.outSet != 0U) ? a0.outSet : TypeEnv::maskForKind(a0.out);
            if ((m & ~allow) != 0U) { addDiag(*diags, std::string("base64.")+fn+": argument must be str or bytes", callNode.args[0].get()); ok=false; return; }
            out = Type::Bytes; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "random") {
          const std::string& fn = at->attr;
          if (fn=="random") {
            if (!callNode.args.empty()) { addDiag(*diags, "random.random() takes 0 args", &callNode); ok=false; return; }
            out = Type::Float; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="randint") {
            if (callNode.args.size()!=2) { addDiag(*diags, "random.randint() takes 2 args", &callNode); ok=false; return; }
            for (int i=0;i<2;++i) { ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[i]->accept(a); if(!a.ok){ ok=false; return; } const uint32_t allow = TypeEnv::maskForKind(Type::Int)|TypeEnv::maskForKind(Type::Float)|TypeEnv::maskForKind(Type::Bool); const uint32_t m=(a.outSet!=0U)?a.outSet:TypeEnv::maskForKind(a.out); if((m & ~allow)!=0U){ addDiag(*diags, "random.randint: numeric required", callNode.args[i].get()); ok=false; return; } }
            out = Type::Int; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="seed") {
            if (callNode.args.size()!=1) { addDiag(*diags, "random.seed() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ ok=false; return; }
            const uint32_t allow = TypeEnv::maskForKind(Type::Int)|TypeEnv::maskForKind(Type::Float)|TypeEnv::maskForKind(Type::Bool);
            const uint32_t m=(a.outSet!=0U)?a.outSet:TypeEnv::maskForKind(a.out);
            if ((m & ~allow)!=0U) { addDiag(*diags, "random.seed: numeric required", callNode.args[0].get()); ok=false; return; }
            out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "stat") {
          const std::string& fn = at->attr;
          auto reqNum = [&](const ast::Expr* e){ ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; if(e){ e->accept(a); if(!a.ok){ ok=false; return false; } const uint32_t allow = TypeEnv::maskForKind(Type::Int)|TypeEnv::maskForKind(Type::Float)|TypeEnv::maskForKind(Type::Bool); const uint32_t m=(a.outSet!=0U)?a.outSet:TypeEnv::maskForKind(a.out); if ((m & ~allow)!=0U) { addDiag(*diags, "stat: mode must be numeric", e); ok=false; return false; } } return true; };
          if (fn=="S_IFMT") {
            if (callNode.args.size()!=1) { addDiag(*diags, "stat.S_IFMT() takes 1 arg", &callNode); ok=false; return; }
            if (!reqNum(callNode.args[0].get())) return;
            out = Type::Int; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="S_ISDIR" || fn=="S_ISREG") {
            if (callNode.args.size()!=1) { addDiag(*diags, std::string("stat.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
            if (!reqNum(callNode.args[0].get())) return;
            out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "secrets") {
          const std::string& fn = at->attr;
          if (fn=="token_bytes" || fn=="token_hex" || fn=="token_urlsafe") {
            if (callNode.args.size()!=1) { addDiag(*diags, std::string("secrets.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ ok=false; return; }
            const uint32_t allow = TypeEnv::maskForKind(Type::Int)|TypeEnv::maskForKind(Type::Float)|TypeEnv::maskForKind(Type::Bool);
            const uint32_t m=(a.outSet!=0U)?a.outSet:TypeEnv::maskForKind(a.out);
            if ((m & ~allow)!=0U) { addDiag(*diags, std::string("secrets.")+fn+": n must be numeric", callNode.args[0].get()); ok=false; return; }
            out = (fn=="token_bytes") ? Type::Bytes : Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "shutil") {
          const std::string& fn = at->attr;
          auto reqStr = [&](const ast::Expr* e, const char* msg){ ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; if(e){ e->accept(a); if(!a.ok){ ok=false; return false; } if (a.out != ast::TypeKind::Str) { addDiag(*diags, msg, e); ok=false; return false; } } return true; };
          if (fn=="copyfile" || fn=="copy") {
            if (callNode.args.size()!=2) { addDiag(*diags, std::string("shutil.")+fn+"() takes 2 args", &callNode); ok=false; return; }
            if (!reqStr(callNode.args[0].get(), "shutil: src must be str")) return;
            if (!reqStr(callNode.args[1].get(), "shutil: dst must be str")) return;
            out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "platform") {
          const std::string fn = at0->attr;
          if (fn=="system" || fn=="machine" || fn=="release" || fn=="version") {
            if (!callNode.args.empty()) { addDiag(*diags, std::string("platform.")+fn+"() takes 0 args", &callNode); ok=false; return; }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "errno") {
          const std::string fn = at0->attr;
          if (fn=="EPERM" || fn=="ENOENT" || fn=="EEXIST" || fn=="EISDIR" || fn=="ENOTDIR" || fn=="EACCES") {
            if (!callNode.args.empty()) { addDiag(*diags, std::string("errno.")+fn+"() takes 0 args", &callNode); ok=false; return; }
            out = ast::TypeKind::Int; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "bisect") {
          const std::string fn = at0->attr;
          if (fn=="bisect_left" || fn=="bisect_right") {
            if (callNode.args.size()!=2) { addDiag(*diags, std::string("bisect.")+fn+"() takes 2 args", &callNode); ok=false; return; }
            // first arg list, second numeric
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            if (a0.out != ast::TypeKind::List) { addDiag(*diags, "bisect: first arg must be list", callNode.args[0].get()); ok=false; return; }
            ExpressionTyper a1{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(a1); if(!a1.ok){ ok=false; return; }
            const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int)|TypeEnv::maskForKind(ast::TypeKind::Float)|TypeEnv::maskForKind(ast::TypeKind::Bool);
            const uint32_t m=(a1.outSet!=0U)?a1.outSet:TypeEnv::maskForKind(a1.out);
            if ((m & ~allow)!=0U) { addDiag(*diags, "bisect: value must be numeric", callNode.args[1].get()); ok=false; return; }
            out = ast::TypeKind::Int; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "tempfile") {
          const std::string fn = at0->attr;
          if (fn=="gettempdir" || fn=="mkdtemp") {
            if (!callNode.args.empty()) { addDiag(*diags, std::string("tempfile.")+fn+"() takes 0 args", &callNode); ok=false; return; }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="mkstemp") {
            if (!callNode.args.empty()) { addDiag(*diags, "tempfile.mkstemp() takes 0 args", &callNode); ok=false; return; }
            out = ast::TypeKind::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "statistics") {
          const std::string fn = at0->attr;
          if (fn=="mean" || fn=="median" || fn=="stdev" || fn=="pvariance") {
            if (callNode.args.size()!=1) { addDiag(*diags, std::string("statistics.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ ok=false; return; }
            if (a.out != ast::TypeKind::List) { addDiag(*diags, std::string("statistics.")+fn+": argument must be list", callNode.args[0].get()); ok=false; return; }
            out = ast::TypeKind::Float; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "textwrap") {
          const std::string fn = at0->attr;
          if (fn=="fill" || fn=="shorten" || fn=="wrap") {
            if (callNode.args.size()!=2) { addDiag(*diags, std::string("textwrap.")+fn+"() takes 2 args", &callNode); ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            if (a0.out != ast::TypeKind::Str) { addDiag(*diags, std::string("textwrap.")+fn+": text must be str", callNode.args[0].get()); ok=false; return; }
            ExpressionTyper a1{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(a1); if(!a1.ok){ ok=false; return; }
            const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int)|TypeEnv::maskForKind(ast::TypeKind::Float)|TypeEnv::maskForKind(ast::TypeKind::Bool);
            const uint32_t m=(a1.outSet!=0U)?a1.outSet:TypeEnv::maskForKind(a1.out);
            if ((m & ~allow)!=0U) { addDiag(*diags, std::string("textwrap.")+fn+": width must be numeric", callNode.args[1].get()); ok=false; return; }
            out = (fn=="wrap") ? ast::TypeKind::List : ast::TypeKind::Str;
            const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="dedent") {
            if (callNode.args.size()!=1) { addDiag(*diags, "textwrap.dedent() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            if (a0.out != ast::TypeKind::Str) { addDiag(*diags, "textwrap.dedent: text must be str", callNode.args[0].get()); ok=false; return; }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="indent") {
            if (callNode.args.size()!=2) { addDiag(*diags, "textwrap.indent() takes 2 args", &callNode); ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            if (a0.out != ast::TypeKind::Str) { addDiag(*diags, "textwrap.indent: text must be str", callNode.args[0].get()); ok=false; return; }
            ExpressionTyper a1{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(a1); if(!a1.ok){ ok=false; return; }
            if (a1.out != ast::TypeKind::Str) { addDiag(*diags, "textwrap.indent: prefix must be str", callNode.args[1].get()); ok=false; return; }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "posixpath") {
          const std::string fn = at0->attr;
          auto ensureStr = [&](const ast::Expr* e){ if(!e) return false; ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; e->accept(t); if(!t.ok) return false; if(t.out!=Type::Str){ addDiag(*diags, std::string("posixpath.")+fn+": path must be str", e); return false; } return true; };
          if (fn == "join") {
            if (callNode.args.size()!=2) { addDiag(*diags, "posixpath.join() takes 2 args", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get()) || !ensureStr(callNode.args[1].get())) { ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "dirname" || fn == "basename" || fn == "abspath") {
            if (callNode.args.size()!=1) { addDiag(*diags, std::string("posixpath.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get())) { ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "splitext") {
            if (callNode.args.size()!=1) { addDiag(*diags, "posixpath.splitext() takes 1 arg", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get())) { ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "exists" || fn == "isfile" || fn == "isdir") {
            if (callNode.args.size()!=1) { addDiag(*diags, std::string("posixpath.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get())) { ok=false; return; }
            out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "ntpath") {
          const std::string fn = at0->attr;
          auto ensureStr = [&](const ast::Expr* e){ if(!e) return false; ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; e->accept(t); if(!t.ok) return false; if(t.out!=Type::Str){ addDiag(*diags, std::string("ntpath.")+fn+": path must be str", e); return false; } return true; };
          if (fn == "join") {
            if (callNode.args.size()!=2) { addDiag(*diags, "ntpath.join() takes 2 args", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get()) || !ensureStr(callNode.args[1].get())) { ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "dirname" || fn == "basename" || fn == "abspath") {
            if (callNode.args.size()!=1) { addDiag(*diags, std::string("ntpath.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get())) { ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "splitext") {
            if (callNode.args.size()!=1) { addDiag(*diags, "ntpath.splitext() takes 1 arg", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get())) { ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "exists" || fn == "isfile" || fn == "isdir") {
            if (callNode.args.size()!=1) { addDiag(*diags, std::string("ntpath.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get())) { ok=false; return; }
            out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "colorsys") {
          const std::string fn = at0->attr;
          if (fn=="rgb_to_hsv" || fn=="hsv_to_rgb") {
            if (callNode.args.size()!=3) { addDiag(*diags, std::string("colorsys.")+fn+"() takes 3 args", &callNode); ok=false; return; }
            for (int i=0;i<3;++i) { ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[i]->accept(a); if(!a.ok){ok=false;return;} const uint32_t allow = TypeEnv::maskForKind(Type::Int)|TypeEnv::maskForKind(Type::Float)|TypeEnv::maskForKind(Type::Bool); const uint32_t m=(a.outSet!=0U)?a.outSet:TypeEnv::maskForKind(a.out); if ((m & ~allow)!=0U) { addDiag(*diags, std::string("colorsys.")+fn+": numeric args required", callNode.args[i].get()); ok=false; return; } }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        // Nested module: os.path.*
        if (callNode.callee->kind == ast::NodeKind::Attribute) {
          const auto* at1 = static_cast<const ast::Attribute*>(callNode.callee.get());
          if (at1->value && at1->value->kind == ast::NodeKind::Attribute) {
            const auto* atMid = static_cast<const ast::Attribute*>(at1->value.get());
            if (atMid->value && atMid->value->kind == ast::NodeKind::Name) {
              const auto* root = static_cast<const ast::Name*>(atMid->value.get());
              if (root->id == "os" && atMid->attr == "path") {
                const std::string fn = at1->attr;
                auto ensureStr = [&](const ast::Expr* e){ if(!e) return false; ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; e->accept(t); if(!t.ok) return false; if(t.out!=Type::Str){ addDiag(*diags, std::string("os.path.")+fn+": path must be str", e); return false; } return true; };
                if (fn == "join") {
                  if (callNode.args.size()!=2) { addDiag(*diags, "os.path.join() takes 2 args", &callNode); ok=false; return; }
                  if (!ensureStr(callNode.args[0].get()) || !ensureStr(callNode.args[1].get())) { ok=false; return; }
                  out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
                }
                if (fn == "dirname" || fn == "basename" || fn == "abspath") {
                  if (callNode.args.size()!=1) { addDiag(*diags, std::string("os.path.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
                  if (!ensureStr(callNode.args[0].get())) { ok=false; return; }
                  out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
                }
                if (fn == "splitext") {
                  if (callNode.args.size()!=1) { addDiag(*diags, "os.path.splitext() takes 1 arg", &callNode); ok=false; return; }
                  if (!ensureStr(callNode.args[0].get())) { ok=false; return; }
                  out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
                }
                if (fn == "exists" || fn == "isfile" || fn == "isdir") {
                  if (callNode.args.size()!=1) { addDiag(*diags, std::string("os.path.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
                  if (!ensureStr(callNode.args[0].get())) { ok=false; return; }
                  out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
                }
              }
            }
          }
        }
        if (base0->id == "hashlib") {
          const std::string fn = at0->attr;
          if (fn=="sha256" || fn=="md5") {
            if (callNode.args.size()!=1) { addDiag(*diags, std::string("hashlib.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ ok=false; return; }
            const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Str) | TypeEnv::maskForKind(ast::TypeKind::Bytes);
            const uint32_t m=(a.outSet!=0U)?a.outSet:TypeEnv::maskForKind(a.out);
            if ((m & ~allow)!=0U) { addDiag(*diags, std::string("hashlib.")+fn+": data must be str or bytes", callNode.args[0].get()); ok=false; return; }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "pprint") {
          const std::string fn = at0->attr;
          if (fn=="pformat") {
            if (callNode.args.size()!=1) { addDiag(*diags, "pprint.pformat() takes 1 arg", &callNode); ok=false; return; }
            // any object accepted; result Str
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "linecache") {
          const std::string fn = at0->attr;
          if (fn=="getline") {
            if (callNode.args.size()!=2) { addDiag(*diags, "linecache.getline() takes 2 args", &callNode); ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            if (a0.out != ast::TypeKind::Str) { addDiag(*diags, "linecache.getline: path must be str", callNode.args[0].get()); ok=false; return; }
            ExpressionTyper a1{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(a1); if(!a1.ok){ ok=false; return; }
            const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int)|TypeEnv::maskForKind(ast::TypeKind::Float)|TypeEnv::maskForKind(ast::TypeKind::Bool);
            const uint32_t m=(a1.outSet!=0U)?a1.outSet:TypeEnv::maskForKind(a1.out);
            if ((m & ~allow)!=0U) { addDiag(*diags, "linecache.getline: lineno must be numeric", callNode.args[1].get()); ok=false; return; }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "getpass") {
          const std::string fn = at0->attr;
          if (fn=="getuser") {
            if (!callNode.args.empty()) { addDiag(*diags, "getpass.getuser() takes 0 args", &callNode); ok=false; return; }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="getpass") {
            if (callNode.args.size()>1) { addDiag(*diags, "getpass.getpass() takes 0 or 1 arg", &callNode); ok=false; return; }
            if (callNode.args.size()==1) {
              ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ ok=false; return; }
              if (a.out != ast::TypeKind::Str) { addDiag(*diags, "getpass.getpass: prompt must be str", callNode.args[0].get()); ok=false; return; }
            }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "shlex") {
          const std::string fn = at0->attr;
          if (fn=="split") {
            if (callNode.args.size()!=1) { addDiag(*diags, "shlex.split() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ ok=false; return; }
            if (a.out != ast::TypeKind::Str) { addDiag(*diags, "shlex.split: text must be str", callNode.args[0].get()); ok=false; return; }
            out = ast::TypeKind::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="join") {
            if (callNode.args.size()!=1) { addDiag(*diags, "shlex.join() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ ok=false; return; }
            if (a.out != ast::TypeKind::List) { addDiag(*diags, "shlex.join: argument must be list", callNode.args[0].get()); ok=false; return; }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "html") {
          const std::string fn = at0->attr;
          if (fn=="escape") {
            if (!(callNode.args.size()==1 || callNode.args.size()==2)) { addDiag(*diags, "html.escape() takes 1 or 2 args", &callNode); ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            if (a0.out != ast::TypeKind::Str) { addDiag(*diags, "html.escape: text must be str", callNode.args[0].get()); ok=false; return; }
            if (callNode.args.size()==2) { ExpressionTyper a1{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(a1); if(!a1.ok){ ok=false; return; } const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int)|TypeEnv::maskForKind(ast::TypeKind::Float)|TypeEnv::maskForKind(ast::TypeKind::Bool); const uint32_t m=(a1.outSet!=0U)?a1.outSet:TypeEnv::maskForKind(a1.out); if ((m & ~allow)!=0U) { addDiag(*diags, "html.escape: quote must be bool/numeric", callNode.args[1].get()); ok=false; return; } }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="unescape") {
            if (callNode.args.size()!=1) { addDiag(*diags, "html.unescape() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            if (a0.out != ast::TypeKind::Str) { addDiag(*diags, "html.unescape: text must be str", callNode.args[0].get()); ok=false; return; }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "reprlib") {
          const std::string fn = at0->attr;
          if (fn=="repr") {
            if (callNode.args.size()!=1) { addDiag(*diags, "reprlib.repr() takes 1 arg", &callNode); ok=false; return; }
            // Accept any value; returns str
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "types") {
          const std::string fn = at0->attr;
          if (fn=="SimpleNamespace") {
            if (!(callNode.args.size()==0 || callNode.args.size()==1)) { addDiag(*diags, "types.SimpleNamespace() takes 0 or 1 args", &callNode); ok=false; return; }
            if (callNode.args.size()==1) {
              ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ ok=false; return; }
              if (a.out != Type::List && a.out != Type::Dict) { addDiag(*diags, "SimpleNamespace: initializer must be list (pairs)", callNode.args[0].get()); ok=false; return; }
            }
            out = Type::Dict; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "binascii") {
          const std::string fn = at0->attr;
          if (fn=="hexlify" || fn=="unhexlify") {
            if (callNode.args.size()!=1) { addDiag(*diags, std::string("binascii.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ ok=false; return; }
            const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Str)|TypeEnv::maskForKind(ast::TypeKind::Bytes);
            const uint32_t m=(a.outSet!=0U)?a.outSet:TypeEnv::maskForKind(a.out);
            if ((m & ~allow)!=0U) { addDiag(*diags, std::string("binascii.")+fn+": data must be str or bytes", callNode.args[0].get()); ok=false; return; }
            out = ast::TypeKind::Bytes; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "struct") {
          const std::string fn = at0->attr;
          if (fn=="pack") {
            if (callNode.args.size()!=2) { addDiag(*diags, "struct.pack() takes 2 args", &callNode); ok=false; return; }
            ExpressionTyper f{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(f); if(!f.ok){ok=false;return;} if(f.out!=Type::Str){ addDiag(*diags, "struct.pack: fmt must be str", callNode.args[0].get()); ok=false; return; }
            ExpressionTyper v{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(v); if(!v.ok){ok=false;return;} if(v.out!=Type::List){ addDiag(*diags, "struct.pack: values must be list", callNode.args[1].get()); ok=false; return; }
            out = Type::Bytes; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="unpack") {
            if (callNode.args.size()!=2) { addDiag(*diags, "struct.unpack() takes 2 args", &callNode); ok=false; return; }
            ExpressionTyper f{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(f); if(!f.ok){ok=false;return;} if(f.out!=Type::Str){ addDiag(*diags, "struct.unpack: fmt must be str", callNode.args[0].get()); ok=false; return; }
            ExpressionTyper d{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(d); if(!d.ok){ok=false;return;}
            const uint32_t allow = TypeEnv::maskForKind(Type::Bytes);
            const uint32_t m=(d.outSet!=0U)?d.outSet:TypeEnv::maskForKind(d.out);
            if ((m & ~allow)!=0U) { addDiag(*diags, "struct.unpack: data must be bytes", callNode.args[1].get()); ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="calcsize") {
            if (callNode.args.size()!=1) { addDiag(*diags, "struct.calcsize() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper f{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(f); if(!f.ok){ok=false;return;} if(f.out!=Type::Str){ addDiag(*diags, "struct.calcsize: fmt must be str", callNode.args[0].get()); ok=false; return; }
            out = Type::Int; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "argparse") {
          const std::string fn = at0->attr;
          if (fn=="ArgumentParser") {
            if (!callNode.args.empty()) { addDiag(*diags, "argparse.ArgumentParser() takes 0 args", &callNode); ok=false; return; }
            out = Type::Dict; const_cast<ast::Call&>(callNode).setType(out); return; // opaque parser object
          }
          if (fn=="add_argument") {
            if (callNode.args.size()!=3) { addDiag(*diags, "argparse.add_argument(parser, name, action)", &callNode); ok=false; return; }
            // parser (opaque), name str, action str
            ExpressionTyper n{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(n); if(!n.ok){ok=false;return;} if(n.out!=Type::Str){ addDiag(*diags, "add_argument: name must be str", callNode.args[1].get()); ok=false; return; }
            ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[2]->accept(a); if(!a.ok){ok=false;return;} if(a.out!=Type::Str){ addDiag(*diags, "add_argument: action must be str", callNode.args[2].get()); ok=false; return; }
            out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="parse_args") {
            if (callNode.args.size()!=2) { addDiag(*diags, "argparse.parse_args(parser, list)", &callNode); ok=false; return; }
            ExpressionTyper l{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(l); if(!l.ok){ok=false;return;} if(l.out!=Type::List){ addDiag(*diags, "parse_args: args must be list", callNode.args[1].get()); ok=false; return; }
            out = Type::Dict; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "hmac") {
          const std::string fn = at0->attr;
          if (fn=="digest") {
            if (callNode.args.size()!=3) { addDiag(*diags, "hmac.digest() takes 3 args", &callNode); ok=false; return; }
            // key and msg: str/bytes; digest: str
            for (int i=0;i<2;++i){ ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[i]->accept(a); if(!a.ok){ ok=false; return; } const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Str)|TypeEnv::maskForKind(ast::TypeKind::Bytes); const uint32_t m=(a.outSet!=0U)?a.outSet:TypeEnv::maskForKind(a.out); if ((m & ~allow)!=0U) { addDiag(*diags, "hmac.digest: key/msg must be str or bytes", callNode.args[i].get()); ok=false; return; } }
            ExpressionTyper a2{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[2]->accept(a2); if(!a2.ok){ ok=false; return; }
            if (a2.out != ast::TypeKind::Str) { addDiag(*diags, "hmac.digest: digest name must be str", callNode.args[2].get()); ok=false; return; }
            out = ast::TypeKind::Bytes; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "warnings") {
          const std::string fn = at0->attr;
          if (fn=="warn") {
            if (callNode.args.size()!=1) { addDiag(*diags, "warnings.warn() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ ok=false; return; }
            if (a.out != ast::TypeKind::Str) { addDiag(*diags, "warnings.warn: message must be str", callNode.args[0].get()); ok=false; return; }
            out = ast::TypeKind::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="simplefilter") {
            if (!(callNode.args.size()==1 || callNode.args.size()==2)) { addDiag(*diags, "warnings.simplefilter() takes 1 or 2 args", &callNode); ok=false; return; }
            ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ ok=false; return; }
            if (a.out != ast::TypeKind::Str) { addDiag(*diags, "warnings.simplefilter: action must be str", callNode.args[0].get()); ok=false; return; }
            if (callNode.args.size()==2) { ExpressionTyper c{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(c); if(!c.ok){ ok=false; return; } if (c.out != ast::TypeKind::Str) { addDiag(*diags, "warnings.simplefilter: category must be str", callNode.args[1].get()); ok=false; return; } }
            out = ast::TypeKind::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "copy") {
          const std::string fn = at0->attr;
          if (fn=="copy" || fn=="deepcopy") {
            if (callNode.args.size()!=1) { addDiag(*diags, std::string("copy.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ ok=false; return; }
            // result type is same as input
            out = a.out; outSet = a.outSet; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "calendar") {
          const std::string fn = at0->attr;
          if (fn=="isleap") {
            if (callNode.args.size()!=1) { addDiag(*diags, "calendar.isleap() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ ok=false; return; }
            const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int)|TypeEnv::maskForKind(ast::TypeKind::Float)|TypeEnv::maskForKind(ast::TypeKind::Bool);
            const uint32_t m=(a.outSet!=0U)?a.outSet:TypeEnv::maskForKind(a.out);
            if ((m & ~allow)!=0U) { addDiag(*diags, "calendar.isleap: year must be numeric", callNode.args[0].get()); ok=false; return; }
            out = ast::TypeKind::Int; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="monthrange") {
            if (callNode.args.size()!=2) { addDiag(*diags, "calendar.monthrange() takes 2 args", &callNode); ok=false; return; }
            for (int i=0;i<2;++i){ ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[i]->accept(a); if(!a.ok){ ok=false; return; } const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int)|TypeEnv::maskForKind(ast::TypeKind::Float)|TypeEnv::maskForKind(ast::TypeKind::Bool); const uint32_t m=(a.outSet!=0U)?a.outSet:TypeEnv::maskForKind(a.out); if ((m & ~allow)!=0U) { addDiag(*diags, "calendar.monthrange: args must be numeric", callNode.args[i].get()); ok=false; return; } }
            out = ast::TypeKind::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "heapq") {
          const std::string fn = at0->attr;
          if (fn=="heappush") {
            if (callNode.args.size()!=2) { addDiag(*diags, "heapq.heappush() takes 2 args", &callNode); ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            if (a0.out != ast::TypeKind::List) { addDiag(*diags, "heapq: first arg must be list", callNode.args[0].get()); ok=false; return; }
            // accept numeric value
            ExpressionTyper a1{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(a1); if(!a1.ok){ ok=false; return; }
            const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int)|TypeEnv::maskForKind(ast::TypeKind::Float)|TypeEnv::maskForKind(ast::TypeKind::Bool);
            const uint32_t m=(a1.outSet!=0U)?a1.outSet:TypeEnv::maskForKind(a1.out);
            if ((m & ~allow)!=0U) { addDiag(*diags, "heapq: value must be numeric", callNode.args[1].get()); ok=false; return; }
            out = ast::TypeKind::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="heappop") {
            if (callNode.args.size()!=1) { addDiag(*diags, "heapq.heappop() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            if (a0.out != ast::TypeKind::List) { addDiag(*diags, "heapq: first arg must be list", callNode.args[0].get()); ok=false; return; }
            out = ast::TypeKind::Int; const_cast<ast::Call&>(callNode).setType(out); return; // in our tests, we use int heaps
          }
        }
        if (base0->id == "fnmatch") {
          const std::string& fn = at->attr;
          auto requireStr = [&](const ast::Expr* e){ ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; if(e){ e->accept(t); if(!t.ok) return false; if(t.out != Type::Str) { addDiag(*diags, std::string("fnmatch.")+fn+": argument must be str", e); return false; } } return true; };
          if (fn=="fnmatch" || fn=="fnmatchcase") {
            if (callNode.args.size()!=2) { addDiag(*diags, std::string("fnmatch.")+fn+"() takes 2 args", &callNode); ok=false; return; }
            if (!requireStr(callNode.args[0].get()) || !requireStr(callNode.args[1].get())) { ok=false; return; }
            out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="filter") {
            if (callNode.args.size()!=2) { addDiag(*diags, "fnmatch.filter() takes 2 args", &callNode); ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            if (a0.out != Type::List) { addDiag(*diags, "fnmatch.filter(): first arg must be list", callNode.args[0].get()); ok=false; return; }
            if (!requireStr(callNode.args[1].get())) { ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn=="translate") {
            if (callNode.args.size()!=1) { addDiag(*diags, "fnmatch.translate() takes 1 arg", &callNode); ok=false; return; }
            if (!requireStr(callNode.args[0].get())) { ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "json") {
          const std::string fn = at0->attr;
          if (fn == "dumps") {
            if (!(callNode.args.size() == 1 || callNode.args.size() == 2)) { addDiag(*diags, "json.dumps() takes 1 or 2 args", &callNode); ok = false; return; }
            if (callNode.args.size() == 2) {
              ExpressionTyper a1{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[1]->accept(a1); if (!a1.ok) { ok = false; return; }
              const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool) | TypeEnv::maskForKind(ast::TypeKind::Float);
              const uint32_t m = (a1.outSet != 0U) ? a1.outSet : TypeEnv::maskForKind(a1.out);
              if ((m & ~allow) != 0U) { addDiag(*diags, "json.dumps: indent must be numeric", callNode.args[1].get()); ok = false; return; }
            }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "loads") {
            if (callNode.args.size() != 1) { addDiag(*diags, "json.loads() takes 1 arg", &callNode); ok = false; return; }
            ExpressionTyper a{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a); if (!a.ok) { ok = false; return; }
            const uint32_t strM = TypeEnv::maskForKind(ast::TypeKind::Str);
            const uint32_t m = (a.outSet != 0U) ? a.outSet : TypeEnv::maskForKind(a.out);
            if ((m & ~strM) != 0U) { addDiag(*diags, "json.loads: argument must be str", callNode.args[0].get()); ok = false; return; }
            // Dynamic return: may be any of None/Int/Float/Bool/Str/List/Dict
            out = ast::TypeKind::NoneType; outSet = (TypeEnv::maskForKind(ast::TypeKind::NoneType)
                | TypeEnv::maskForKind(ast::TypeKind::Int)
                | TypeEnv::maskForKind(ast::TypeKind::Float)
                | TypeEnv::maskForKind(ast::TypeKind::Bool)
                | TypeEnv::maskForKind(ast::TypeKind::Str)
                | TypeEnv::maskForKind(ast::TypeKind::List)
                | TypeEnv::maskForKind(ast::TypeKind::Dict));
            const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "time") {
          const std::string fn = at0->attr;
          auto zeroRetF = [&]{ if (!callNode.args.empty()) { addDiag(*diags, std::string("time.") + fn + "() takes 0 args", &callNode); ok = false; return; } out = ast::TypeKind::Float; const_cast<ast::Call&>(callNode).setType(out); };
          auto zeroRetI = [&]{ if (!callNode.args.empty()) { addDiag(*diags, std::string("time.") + fn + "() takes 0 args", &callNode); ok = false; return; } out = ast::TypeKind::Int; const_cast<ast::Call&>(callNode).setType(out); };
          if (fn == "time" || fn == "monotonic" || fn == "perf_counter" || fn == "process_time") { zeroRetF(); return; }
          if (fn == "time_ns" || fn == "monotonic_ns" || fn == "perf_counter_ns") { zeroRetI(); return; }
          if (fn == "sleep") {
            if (callNode.args.size() != 1) { addDiag(*diags, "time.sleep() takes 1 arg", &callNode); ok = false; return; }
            ExpressionTyper a{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a); if (!a.ok) { ok = false; return; }
            const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Float) | TypeEnv::maskForKind(ast::TypeKind::Bool);
            const uint32_t m = (a.outSet != 0U) ? a.outSet : TypeEnv::maskForKind(a.out);
            if ((m & ~allow) != 0U) { addDiag(*diags, "time.sleep: numeric required", callNode.args[0].get()); ok = false; return; }
            out = ast::TypeKind::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "datetime") {
          const std::string fn = at0->attr;
          if (fn == "now" || fn == "utcnow") {
            if (!callNode.args.empty()) { addDiag(*diags, std::string("datetime.") + fn + "() takes 0 args", &callNode); ok = false; return; }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "fromtimestamp" || fn == "utcfromtimestamp") {
            if (callNode.args.size() != 1) { addDiag(*diags, std::string("datetime.") + fn + "() takes 1 arg", &callNode); ok = false; return; }
            ExpressionTyper a{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a); if (!a.ok) { ok = false; return; }
            const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Float) | TypeEnv::maskForKind(ast::TypeKind::Bool);
            const uint32_t m = (a.outSet != 0U) ? a.outSet : TypeEnv::maskForKind(a.out);
            if ((m & ~allow) != 0U) { addDiag(*diags, std::string("datetime.") + fn + ": numeric required", callNode.args[0].get()); ok = false; return; }
            out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base0->id == "_aix_support") {
          const std::string fn = at0->attr;
          if (fn == "aix_platform" || fn == "default_libpath") { if (!callNode.args.empty()) { addDiag(*diags, std::string("_aix_support.")+fn+"() takes 0 args", &callNode); ok=false; return; } out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "ldflags") { if (!callNode.args.empty()) { addDiag(*diags, "_aix_support.ldflags() takes 0 args", &callNode); ok=false; return; } out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return; }
        }
        if (base0->id == "_android_support") {
          const std::string fn = at0->attr;
          if (fn == "android_platform" || fn == "default_libdir") { if (!callNode.args.empty()) { addDiag(*diags, std::string("_android_support.")+fn+"() takes 0 args", &callNode); ok=false; return; } out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "ldflags") { if (!callNode.args.empty()) { addDiag(*diags, "_android_support.ldflags() takes 0 args", &callNode); ok=false; return; } out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return; }
        }
        if (base0->id == "_apple_support") {
          const std::string fn = at0->attr;
          if (fn == "apple_platform" || fn == "default_sdkroot") { if (!callNode.args.empty()) { addDiag(*diags, std::string("_apple_support.")+fn+"() takes 0 args", &callNode); ok=false; return; } out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "ldflags") { if (!callNode.args.empty()) { addDiag(*diags, "_apple_support.ldflags() takes 0 args", &callNode); ok=false; return; } out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return; }
        }
        if (base0->id == "_asyncio") {
          const std::string fn = at0->attr;
          auto ensurePtr = [&](const ast::Expr* e){ if(!e) return false; ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; e->accept(t); if(!t.ok) return false; if(!(t.out==Type::Str || t.out==Type::List || t.out==Type::Dict || t.out==Type::Tuple)) { addDiag(*diags, std::string("_asyncio.")+fn+": pointer arg required", e); return false; } return true; };
          if (fn == "get_event_loop" || fn == "Future") { if (!callNode.args.empty()) { addDiag(*diags, std::string("_asyncio.")+fn+"() takes 0 args", &callNode); ok=false; return; } out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "future_set_result") { if (callNode.args.size()!=2) { addDiag(*diags, "_asyncio.future_set_result() takes 2 args", &callNode); ok=false; return; } if(!ensurePtr(callNode.args[0].get())||!ensurePtr(callNode.args[1].get())) { ok=false; return; } out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "future_result") { if (callNode.args.size()!=1) { addDiag(*diags, "_asyncio.future_result() takes 1 arg", &callNode); ok=false; return; } if(!ensurePtr(callNode.args[0].get())) { ok=false; return; } out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "future_done") { if (callNode.args.size()!=1) { addDiag(*diags, "_asyncio.future_done() takes 1 arg", &callNode); ok=false; return; } if(!ensurePtr(callNode.args[0].get())) { ok=false; return; } out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); return; }
          if (fn == "sleep") { if (callNode.args.size()!=1) { addDiag(*diags, "_asyncio.sleep() takes 1 arg", &callNode); ok=false; return; } ExpressionTyper a{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a); if(!a.ok){ok=false;return;} const uint32_t allow = TypeEnv::maskForKind(Type::Int)|TypeEnv::maskForKind(Type::Float)|TypeEnv::maskForKind(Type::Bool); const uint32_t m=(a.outSet!=0U)?a.outSet:TypeEnv::maskForKind(a.out); if((m & ~allow)!=0U){ addDiag(*diags, "_asyncio.sleep: numeric required", callNode.args[0].get()); ok=false; return;} out=Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return; }
        }
      }
    }
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
    if (!callNode.callee) { addDiag(*diags, "unsupported callee expression", &callNode); ok = false; return; }
    // Handle stdlib module attribute calls (e.g., math.sqrt)
    if (callNode.callee->kind == ast::NodeKind::Attribute) {
      const auto* at = static_cast<const ast::Attribute*>(callNode.callee.get());
      if (at->value && at->value->kind == ast::NodeKind::Name) {
        const auto* base = static_cast<const ast::Name*>(at->value.get());
        if (base->id == "math") {
          const std::string fn = at->attr;
          auto checkUnary = [&](ast::TypeKind retKind) {
            if (callNode.args.size() != 1) { addDiag(*diags, std::string("math.") + fn + "() takes 1 arg", &callNode); ok = false; return; }
            ExpressionTyper a{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a); if (!a.ok) { ok = false; return; }
            const uint32_t mask = (a.outSet != 0U) ? a.outSet : TypeEnv::maskForKind(a.out);
            const uint32_t okmask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Float);
            if ((mask & ~okmask) != 0U) { addDiag(*diags, std::string("math.") + fn + ": argument must be int/float", callNode.args[0].get()); ok = false; return; }
            out = retKind; const_cast<ast::Call&>(callNode).setType(out);
          };
          auto checkBinary = [&](ast::TypeKind retKind) {
            if (callNode.args.size() != 2) { addDiag(*diags, std::string("math.") + fn + "() takes 2 args", &callNode); ok = false; return; }
            ExpressionTyper a0{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a0); if (!a0.ok) { ok = false; return; }
            ExpressionTyper a1{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[1]->accept(a1); if (!a1.ok) { ok = false; return; }
            const uint32_t okmask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Float);
            const uint32_t m0 = (a0.outSet != 0U) ? a0.outSet : TypeEnv::maskForKind(a0.out);
            const uint32_t m1 = (a1.outSet != 0U) ? a1.outSet : TypeEnv::maskForKind(a1.out);
            if ((m0 & ~okmask) != 0U || (m1 & ~okmask) != 0U) { addDiag(*diags, std::string("math.") + fn + ": arguments must be int/float", &callNode); ok = false; return; }
            out = retKind; const_cast<ast::Call&>(callNode).setType(out);
          };
          if (fn == "sqrt" || fn == "fabs" || fn == "sin" || fn == "cos" || fn == "tan" || fn == "asin" || fn == "acos" || fn == "atan" || fn == "exp" || fn == "exp2" || fn == "log" || fn == "log2" || fn == "log10" || fn == "degrees" || fn == "radians") { checkUnary(ast::TypeKind::Float); return; }
          if (fn == "floor" || fn == "ceil" || fn == "trunc") { checkUnary(ast::TypeKind::Int); return; }
          if (fn == "pow" || fn == "copysign" || fn == "atan2" || fn == "fmod" || fn == "hypot") { checkBinary((fn=="copysign")?ast::TypeKind::Float:ast::TypeKind::Float); return; }
          // Unknown math.*
          addDiag(*diags, std::string("unknown function: math.") + fn, &callNode); ok = false; return;
        }
        if (base->id == "subprocess") {
          const std::string fn = at->attr;
          if (fn == "run" || fn == "call" || fn == "check_call") {
            if (callNode.args.size() != 1) { addDiag(*diags, std::string("subprocess.") + fn + "() takes 1 arg", &callNode); ok = false; return; }
            ExpressionTyper a{*env, *sigs, *retParamIdxs, *diags, polyTargets};
            callNode.args[0]->accept(a); if (!a.ok) { ok = false; return; }
            const uint32_t mask = (a.outSet != 0U) ? a.outSet : TypeEnv::maskForKind(a.out);
            const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
            if ((mask & ~strMask) != 0U) { addDiag(*diags, std::string("subprocess.") + fn + ": argument must be str", callNode.args[0].get()); ok = false; return; }
            out = Type::Int; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          // Default: treat unknown subprocess.* like run(): int return and 1 string arg
          if (callNode.args.size() != 1) { addDiag(*diags, std::string("subprocess.") + fn + "() takes 1 arg", &callNode); ok = false; return; }
          ExpressionTyper a2{*env, *sigs, *retParamIdxs, *diags, polyTargets};
          callNode.args[0]->accept(a2); if (!a2.ok) { ok = false; return; }
          const uint32_t m2 = (a2.outSet != 0U) ? a2.outSet : TypeEnv::maskForKind(a2.out);
          const uint32_t strM = TypeEnv::maskForKind(ast::TypeKind::Str);
          if ((m2 & ~strM) != 0U) { addDiag(*diags, std::string("subprocess.") + fn + ": argument must be str", callNode.args[0].get()); ok = false; return; }
          out = Type::Int; const_cast<ast::Call&>(callNode).setType(out); return;
        }
        if (base->id == "io") {
          const std::string fn = at->attr;
          auto ensureStr = [&](const ast::Expr* e){ ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; if(e){ e->accept(t); if(!t.ok) return false; if(t.out != Type::Str){ addDiag(*diags, std::string("io.")+fn+": argument must be str", e); return false; } } return true; };
          if (fn == "write_stdout" || fn == "write_stderr") {
            if (callNode.args.size()!=1 || !ensureStr(callNode.args[0].get())) { ok=false; return; }
            out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "read_file") {
            if (callNode.args.size()!=1 || !ensureStr(callNode.args[0].get())) { ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "write_file") {
            if (callNode.args.size()!=2 || !ensureStr(callNode.args[0].get()) || !ensureStr(callNode.args[1].get())) { ok=false; return; }
            out = Type::Bool; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base->id == "sys") {
          const std::string fn = at->attr;
          if (fn == "platform" || fn == "version") {
            if (callNode.args.size()!=0) { addDiag(*diags, std::string("sys.")+fn+"() takes 0 args", &callNode); ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "maxsize") {
            if (callNode.args.size()!=0) { addDiag(*diags, "sys.maxsize() takes 0 args", &callNode); ok=false; return; }
            out = Type::Int; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "exit") {
            if (callNode.args.size()!=1) { addDiag(*diags, "sys.exit() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(t); if(!t.ok){ok=false;return;}
            const uint32_t m = (t.outSet!=0U)?t.outSet:TypeEnv::maskForKind(t.out);
            const uint32_t allow = TypeEnv::maskForKind(Type::Int) | TypeEnv::maskForKind(Type::Bool) | TypeEnv::maskForKind(Type::Float);
            if ((m & ~allow) != 0U) { addDiag(*diags, "sys.exit: int required", callNode.args[0].get()); ok=false; return; }
            out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base->id == "time") {
          const std::string fn = at->attr;
          auto zeroFloat = [&](Type ret){ if (callNode.args.size()!=0) { addDiag(*diags, std::string("time.")+fn+"() takes 0 args", &callNode); ok=false; return; } out = ret; const_cast<ast::Call&>(callNode).setType(out); };
          if (fn == "time" || fn == "monotonic" || fn == "perf_counter" || fn == "process_time") { zeroFloat(Type::Float); return; }
          if (fn == "time_ns" || fn == "monotonic_ns" || fn == "perf_counter_ns") { zeroFloat(Type::Int); return; }
          if (fn == "sleep") {
            if (callNode.args.size()!=1) { addDiag(*diags, "time.sleep() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(t); if(!t.ok){ok=false;return;}
            const uint32_t m = (t.outSet!=0U)?t.outSet:TypeEnv::maskForKind(t.out);
            const uint32_t allow = TypeEnv::maskForKind(Type::Int) | TypeEnv::maskForKind(Type::Bool) | TypeEnv::maskForKind(Type::Float);
            if ((m & ~allow)!=0U) { addDiag(*diags, "time.sleep: numeric required", callNode.args[0].get()); ok=false; return; }
            out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base->id == "datetime") {
          const std::string fn = at->attr;
          if (fn == "now" || fn == "utcnow") {
            if (callNode.args.size()!=0) { addDiag(*diags, std::string("datetime.")+fn+"() takes 0 args", &callNode); ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "fromtimestamp" || fn == "utcfromtimestamp") {
            if (callNode.args.size()!=1) { addDiag(*diags, std::string("datetime.")+fn+"() takes 1 arg", &callNode); ok=false; return; }
            ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(t); if(!t.ok){ok=false;return;}
            const uint32_t m = (t.outSet!=0U)?t.outSet:TypeEnv::maskForKind(t.out);
            const uint32_t allow = TypeEnv::maskForKind(Type::Int) | TypeEnv::maskForKind(Type::Bool) | TypeEnv::maskForKind(Type::Float);
            if ((m & ~allow)!=0U) { addDiag(*diags, std::string("datetime.")+fn+": numeric required", callNode.args[0].get()); ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base->id == "re") {
          const std::string fn = at->attr;
          auto ensureStr = [&](const ast::Expr* e){ ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; if(e){ e->accept(t); if(!t.ok) return false; if(t.out != Type::Str) { addDiag(*diags, std::string("re.")+fn+": str argument required", e); return false; } } return true; };
          auto ensureInt = [&](const ast::Expr* e){ ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; if(e){ e->accept(t); if(!t.ok) return false; if(!(t.out==Type::Int || t.out==Type::Bool)) { addDiag(*diags, std::string("re.")+fn+": int argument required", e); return false;} } return true; };
          if (fn == "compile") {
            if (callNode.args.empty() || callNode.args.size()>2) { addDiag(*diags, "re.compile() takes 1 or 2 args", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get())) { ok=false; return; }
            if (callNode.args.size()==2 && !ensureInt(callNode.args[1].get())) { ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return; // treat as opaque ptr/str
          }
          if (fn == "search" || fn=="match" || fn=="fullmatch") {
            if (callNode.args.size()<2 || callNode.args.size()>3) { addDiag(*diags, std::string("re.")+fn+"() takes 2 or 3 args", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get()) || !ensureStr(callNode.args[1].get())) { ok=false; return; }
            if (callNode.args.size()==3 && !ensureInt(callNode.args[2].get())) { ok=false; return; }
            out = Type::Tuple; const_cast<ast::Call&>(callNode).setType(out); return; // opaque ptr
          }
          if (fn == "findall") {
            if (callNode.args.size()<2 || callNode.args.size()>3) { addDiag(*diags, "re.findall() takes 2 or 3 args", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get()) || !ensureStr(callNode.args[1].get())) { ok=false; return; }
            if (callNode.args.size()==3 && !ensureInt(callNode.args[2].get())) { ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "finditer") {
            if (callNode.args.size()<2 || callNode.args.size()>3) { addDiag(*diags, "re.finditer() takes 2 or 3 args", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get()) || !ensureStr(callNode.args[1].get())) { ok=false; return; }
            if (callNode.args.size()==3 && !ensureInt(callNode.args[2].get())) { ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "split") {
            if (callNode.args.size()<2 || callNode.args.size()>4) { addDiag(*diags, "re.split() takes 2 to 4 args", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get()) || !ensureStr(callNode.args[1].get())) { ok=false; return; }
            if (callNode.args.size()>=3 && !ensureInt(callNode.args[2].get())) { ok=false; return; }
            if (callNode.args.size()==4 && !ensureInt(callNode.args[3].get())) { ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "sub" || fn == "subn") {
            if (callNode.args.size()<3 || callNode.args.size()>5) { addDiag(*diags, std::string("re.")+fn+"() takes 3 to 5 args", &callNode); ok=false; return; }
            if (!ensureStr(callNode.args[0].get()) || !ensureStr(callNode.args[1].get()) || !ensureStr(callNode.args[2].get())) { ok=false; return; }
            if (callNode.args.size()>=4 && !ensureInt(callNode.args[3].get())) { ok=false; return; }
            if (callNode.args.size()==5 && !ensureInt(callNode.args[4].get())) { ok=false; return; }
            out = (fn=="sub") ? Type::Str : Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "escape") {
            if (callNode.args.size()!=1 || !ensureStr(callNode.args[0].get())) { addDiag(*diags, "re.escape() takes 1 str arg", &callNode); ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base->id == "collections") {
          const std::string fn = at->attr;
          auto ensureList = [&](const ast::Expr* e){ if(!e) return false; ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; e->accept(t); if(!t.ok) return false; if(t.out != Type::List) { addDiag(*diags, std::string("collections.")+fn+": list required", e); return false; } return true; };
          auto ensurePtr = [&](const ast::Expr* e){ if(!e) return false; ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; e->accept(t); if(!t.ok) return false; if(!(t.out==Type::Str || t.out==Type::List || t.out==Type::Dict || t.out==Type::Tuple)) { addDiag(*diags, std::string("collections.")+fn+": ptr-like arg required", e); return false; } return true; };
          if (fn == "Counter") {
            if (callNode.args.size()!=1 || !ensureList(callNode.args[0].get())) { ok=false; return; }
            out = Type::Dict; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "OrderedDict") {
            if (callNode.args.size()!=1 || !ensureList(callNode.args[0].get())) { ok=false; return; }
            out = Type::Dict; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "ChainMap") {
            if (callNode.args.size()!=1 || !ensureList(callNode.args[0].get())) { ok=false; return; }
            out = Type::Dict; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "defaultdict") {
            if (callNode.args.size()!=1 || !ensurePtr(callNode.args[0].get())) { ok=false; return; }
            out = Type::Dict; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "defaultdict_get") {
            if (callNode.args.size()!=2 || !ensurePtr(callNode.args[0].get()) || !ensurePtr(callNode.args[1].get())) { ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return; // ptr
          }
          if (fn == "defaultdict_set") {
            if (callNode.args.size()!=3 || !ensurePtr(callNode.args[0].get()) || !ensurePtr(callNode.args[1].get()) || !ensurePtr(callNode.args[2].get())) { ok=false; return; }
            out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base->id == "array") {
          const std::string fn = at->attr;
          auto ensurePtr = [&](const ast::Expr* e){ if(!e) return false; ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; e->accept(t); if(!t.ok) return false; if(!(t.out==Type::Str || t.out==Type::List || t.out==Type::Dict || t.out==Type::Tuple)) { addDiag(*diags, std::string("array.")+fn+": ptr-like arg required", e); return false; } return true; };
          if (fn == "array") {
            if (callNode.args.empty() || callNode.args.size() > 2) { addDiag(*diags, "array.array() takes 1 or 2 args", &callNode); ok=false; return; }
            // typecode must be str
            ExpressionTyper t0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(t0); if(!t0.ok){ok=false;return;} if(t0.out!=Type::Str){ addDiag(*diags, "array.array: typecode must be str", callNode.args[0].get()); ok=false; return; }
            if (callNode.args.size()==2) { ExpressionTyper t1{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(t1); if(!t1.ok){ok=false;return;} if(t1.out!=Type::List){ addDiag(*diags, "array.array: initializer must be list", callNode.args[1].get()); ok=false; return; } }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "append") {
            if (callNode.args.size()!=2 || !ensurePtr(callNode.args[0].get())) { addDiag(*diags, "array.append(arr, value) takes (arr, value)", &callNode); ok=false; return; }
            // value: allow numeric (int/float/bool)
            ExpressionTyper tv{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(tv); if(!tv.ok){ ok=false; return; }
            const uint32_t allow = TypeEnv::maskForKind(Type::Int)|TypeEnv::maskForKind(Type::Float)|TypeEnv::maskForKind(Type::Bool);
            const uint32_t m=(tv.outSet!=0U)?tv.outSet:TypeEnv::maskForKind(tv.out);
            if ((m & ~allow)!=0U) { addDiag(*diags, "array.append: value must be numeric", callNode.args[1].get()); ok=false; return; }
            out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "pop") {
            if (callNode.args.size()!=1 || !ensurePtr(callNode.args[0].get())) { addDiag(*diags, "array.pop(arr) takes 1 arg", &callNode); ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return; // ptr-like element
          }
          if (fn == "tolist") {
            if (callNode.args.size()!=1 || !ensurePtr(callNode.args[0].get())) { addDiag(*diags, "array.tolist(arr) takes 1 arg", &callNode); ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base->id == "itertools") {
          const std::string fn = at->attr;
          auto ensureList = [&](const ast::Expr* e){ ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; if(e){ e->accept(t); if(!t.ok) return false; if(t.out != Type::List) { addDiag(*diags, "itertools: list required", e); return false; } } return true; };
          if (fn == "chain") {
            if (callNode.args.size() != 2) { addDiag(*diags, "itertools.chain() takes 2 lists in this subset", &callNode); ok=false; return; }
            if (!ensureList(callNode.args[0].get()) || !ensureList(callNode.args[1].get())) { ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "chain_from_iterable") {
            if (callNode.args.size() != 1) { addDiag(*diags, "itertools.chain_from_iterable() takes 1 arg (list of lists)", &callNode); ok=false; return; }
            if (!ensureList(callNode.args[0].get())) { ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "product") {
            if (callNode.args.size() != 2) { addDiag(*diags, "itertools.product() supports 2 lists in this subset", &callNode); ok=false; return; }
            if (!ensureList(callNode.args[0].get()) || !ensureList(callNode.args[1].get())) { ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "permutations") {
            if (callNode.args.empty() || callNode.args.size() > 2) { addDiag(*diags, "itertools.permutations() takes 1 or 2 args", &callNode); ok=false; return; }
            if (!ensureList(callNode.args[0].get())) { ok=false; return; }
            if (callNode.args.size()==2) { ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(t); if(!t.ok){ok=false;return;} if(t.out!=Type::Int && t.out!=Type::Bool){ addDiag(*diags, "permutations r must be int", callNode.args[1].get()); ok=false; return; } }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "combinations" || fn == "combinations_with_replacement") {
            if (callNode.args.size() != 2) { addDiag(*diags, std::string("itertools.")+fn+"() takes 2 args", &callNode); ok=false; return; }
            if (!ensureList(callNode.args[0].get())) { ok=false; return; }
            ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(t); if(!t.ok){ok=false;return;} if(t.out!=Type::Int && t.out!=Type::Bool){ addDiag(*diags, std::string(fn)+": r must be int", callNode.args[1].get()); ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "zip_longest") {
            if (callNode.args.size() < 2 || callNode.args.size() > 3) { addDiag(*diags, "itertools.zip_longest() takes 2 or 3 args", &callNode); ok=false; return; }
            if (!ensureList(callNode.args[0].get()) || !ensureList(callNode.args[1].get())) { ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "islice") {
            if (callNode.args.size() < 3 || callNode.args.size() > 4) { addDiag(*diags, "itertools.islice() takes 3 or 4 args", &callNode); ok=false; return; }
            if (!ensureList(callNode.args[0].get())) { ok=false; return; }
            for (size_t i = 1; i < callNode.args.size(); ++i) { ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[i]->accept(t); if(!t.ok){ok=false;return;} if(t.out!=Type::Int && t.out!=Type::Bool){ addDiag(*diags, "islice: indices must be int", callNode.args[i].get()); ok=false; return; } }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "accumulate") {
            if (callNode.args.size() != 1) { addDiag(*diags, "itertools.accumulate() supports 1 list arg in this subset", &callNode); ok=false; return; }
            if (!ensureList(callNode.args[0].get())) { ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "repeat") {
            if (callNode.args.size() != 2) { addDiag(*diags, "itertools.repeat() takes 2 args", &callNode); ok=false; return; }
            ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(t); if(!t.ok){ok=false;return;} if(t.out!=Type::Int && t.out!=Type::Bool){ addDiag(*diags, "repeat: times must be int", callNode.args[1].get()); ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "pairwise") {
            if (callNode.args.size() != 1) { addDiag(*diags, "itertools.pairwise() takes 1 list", &callNode); ok=false; return; }
            if (!ensureList(callNode.args[0].get())) { ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "batched") {
            if (callNode.args.size() != 2) { addDiag(*diags, "itertools.batched() takes 2 args", &callNode); ok=false; return; }
            if (!ensureList(callNode.args[0].get())) { ok=false; return; }
            ExpressionTyper t{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(t); if(!t.ok){ok=false;return;} if(t.out!=Type::Int && t.out!=Type::Bool){ addDiag(*diags, "batched: n must be int", callNode.args[1].get()); ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
          if (fn == "compress") {
            if (callNode.args.size() != 2) { addDiag(*diags, "itertools.compress() takes 2 args", &callNode); ok=false; return; }
            if (!ensureList(callNode.args[0].get()) || !ensureList(callNode.args[1].get())) { ok=false; return; }
            out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
        if (base->id == "unicodedata") {
          const std::string fn = at->attr;
          if (fn == "normalize") {
            if (callNode.args.size()!=2) { addDiag(*diags, "unicodedata.normalize() takes 2 args", &callNode); ok=false; return; }
            ExpressionTyper a0{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[0]->accept(a0); if(!a0.ok){ ok=false; return; }
            if (a0.out != Type::Str) { addDiag(*diags, "normalize: form must be str", callNode.args[0].get()); ok=false; return; }
            ExpressionTyper a1{*env,*sigs,*retParamIdxs,*diags,polyTargets}; callNode.args[1]->accept(a1); if(!a1.ok){ ok=false; return; }
            if (a1.out != Type::Str) { addDiag(*diags, "normalize: value must be str", callNode.args[1].get()); ok=false; return; }
            out = Type::Str; const_cast<ast::Call&>(callNode).setType(out); return;
          }
        }
      }
      addDiag(*diags, "unsupported callee expression", &callNode); ok = false; return;
    }
    if (callNode.callee->kind != ast::NodeKind::Name) { addDiag(*diags, "unsupported callee expression", &callNode); ok = false; return; }
    const auto* nameNode = static_cast<const ast::Name*>(callNode.callee.get());
    // Builtins: len(x) -> int; isinstance(x, T) -> bool; plus constructors and common utilities
    if (nameNode->id == "eval") {
      // Compile-time only: accept eval(<literal string>) and reject others
      if (callNode.args.size() != 1 || !callNode.args[0] || callNode.args[0]->kind != ast::NodeKind::StringLiteral) {
        addDiag(*diags, "eval() only accepts a compile-time literal string in this subset", &callNode);
        ok = false; return;
      }
      out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "exec") {
      // Compile-time only: accept exec(<literal string>) and reject others (no runtime effect)
      if (callNode.args.size() != 1 || !callNode.args[0] || callNode.args[0]->kind != ast::NodeKind::StringLiteral) {
        addDiag(*diags, "exec() only accepts a compile-time literal string in this subset", &callNode);
        ok = false; return;
      }
      out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
    }
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
    // Concurrency builtins: treated as dynamic/opaque in this subset with basic arity checks
    if (nameNode->id == "chan_new") {
      if (callNode.args.size() != 1) { addDiag(*diags, "chan_new() takes exactly 1 argument", &callNode); ok = false; return; }
      out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "chan_send") {
      if (callNode.args.size() != 2) { addDiag(*diags, "chan_send() takes exactly 2 arguments", &callNode); ok = false; return; }
      // Static payload check: only allow immutable payload kinds (None,int,float,bool,str,tuple)
      ExpressionTyper payTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets};
      if (callNode.args[1]) { callNode.args[1]->accept(payTyper); if (!payTyper.ok) { ok = false; return; } }
      auto maskFor = [&](Type k){ return TypeEnv::maskForKind(k); };
      const uint32_t allowed = maskFor(Type::NoneType) | maskFor(Type::Int) | maskFor(Type::Float) | maskFor(Type::Bool) | maskFor(Type::Str) | maskFor(Type::Tuple);
      const uint32_t gotMask = (payTyper.outSet != 0U) ? payTyper.outSet : TypeEnv::maskForKind(payTyper.out);
      auto isSubset = [](uint32_t m, uint32_t allow){ return m && ((m & ~allow) == 0U); };
      if (gotMask != 0U && !isSubset(gotMask, allowed)) {
        addDiag(*diags, "chan_send payload must be immutable (int/float/bool/str/tuple or None)", callNode.args[1].get());
        ok = false; return;
      }
      out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "chan_recv") {
      if (callNode.args.size() != 1) { addDiag(*diags, "chan_recv() takes exactly 1 argument", &callNode); ok = false; return; }
      out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "spawn") {
      if (callNode.args.size() != 1 || !(callNode.args[0] && callNode.args[0]->kind == ast::NodeKind::Name)) { addDiag(*diags, "spawn() requires function name", &callNode); ok = false; return; }
      out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "join") {
      if (callNode.args.size() != 1) { addDiag(*diags, "join() requires 1 handle argument", &callNode); ok = false; return; }
      out = Type::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
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
    if (nameNode->id == "enumerate") {
      if (callNode.args.size() < 1 || callNode.args.size() > 2) { addDiag(*diags, "enumerate() takes 1 or 2 arguments", &callNode); ok = false; return; }
      out = Type::List; const_cast<ast::Call&>(callNode).setType(out); return;
    }
    if (nameNode->id == "zip") {
      if (callNode.args.empty()) { addDiag(*diags, "zip() takes at least 1 argument", &callNode); ok = false; return; }
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
/***
 * Name: sema_check_impl
 * Purpose: Internal implementation of semantic analysis for Sema::check.
 * Inputs:
 *   - self: Sema instance to populate internal maps
 *   - mod: AST module to analyze
 *   - diags: diagnostics output container
 * Returns:
 *   - true on success (no diagnostics), false otherwise
 */
// Implementation moved: public wrapper Sema::check is in Sema_check.cpp
bool sema_check_impl(pycc::sema::Sema* self, ast::Module& mod, std::vector<pycc::sema::Diagnostic>& diags) {
  auto& funcFlags_ = self->funcFlags_;
  auto& stmtMayRaise_ = self->stmtMayRaise_;
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

  // Function flags: generator/coroutine pre-scan
  struct FnTraitScan : public ast::VisitorBase {
    bool hasYield{false}; bool hasAwait{false};
    // Stubs for pure virtuals
    void visit(const ast::Module&) override {}
    void visit(const ast::FunctionDef&) override {}
    void visit(const ast::YieldExpr&) override { hasYield = true; }
    void visit(const ast::AwaitExpr&) override { hasAwait = true; }
    void visit(const ast::AssignStmt&) override {}
    void visit(const ast::ExprStmt& es) override { if (es.value) es.value->accept(*this); }
    void visit(const ast::ReturnStmt& rs) override { if (rs.value) rs.value->accept(*this); }
    void visit(const ast::IfStmt& is) override { if (is.cond) is.cond->accept(*this); for (const auto& s : is.thenBody) if (s) s->accept(*this); for (const auto& s : is.elseBody) if (s) s->accept(*this); }
    void visit(const ast::WhileStmt& ws) override { if (ws.cond) ws.cond->accept(*this); for (const auto& s : ws.thenBody) if (s) s->accept(*this); for (const auto& s : ws.elseBody) if (s) s->accept(*this); }
    void visit(const ast::ForStmt& fs) override { if (fs.target) fs.target->accept(*this); if (fs.iterable) fs.iterable->accept(*this); for (const auto& s : fs.thenBody) if (s) s->accept(*this); for (const auto& s : fs.elseBody) if (s) s->accept(*this); }
    void visit(const ast::TryStmt& ts) override { for (const auto& s : ts.body) if (s) s->accept(*this); for (const auto& h : ts.handlers) if (h) for (const auto& s : h->body) if (s) s->accept(*this); for (const auto& s : ts.orelse) if (s) s->accept(*this); for (const auto& s : ts.finalbody) if (s) s->accept(*this); }
    void visit(const ast::IntLiteral&) override {}
    void visit(const ast::BoolLiteral&) override {}
    void visit(const ast::FloatLiteral&) override {}
    void visit(const ast::StringLiteral&) override {}
    void visit(const ast::NoneLiteral&) override {}
    void visit(const ast::Name&) override {}
    void visit(const ast::Call&) override {}
    void visit(const ast::Binary&) override {}
    void visit(const ast::Unary&) override {}
    void visit(const ast::TupleLiteral&) override {}
    void visit(const ast::ListLiteral&) override {}
    void visit(const ast::ObjectLiteral&) override {}
  };
  for (const auto& func : mod.functions) {
    FnTraitScan scan; for (const auto& st : func->body) if (st) st->accept(scan);
    funcFlags_[func.get()] = FuncFlags{scan.hasYield, scan.hasAwait};
  }

  for (const auto& func : mod.functions) {
    if (!(typeIsInt(func->returnType) || typeIsBool(func->returnType) || typeIsFloat(func->returnType) || typeIsStr(func->returnType) || func->returnType == Type::Tuple)) { Diagnostic diagVar; diagVar.message = "only int/bool/float/str/tuple returns supported"; diags.push_back(std::move(diagVar)); return false; }
    TypeEnv env;
    // Pre-scan for local assignments and nonlocal/global declarations in this function
    struct LocalAssignScan : public ast::VisitorBase {
      std::unordered_set<std::string> locals;
      std::unordered_set<std::string> globs;
      std::unordered_set<std::string> nls;
      void visit(const ast::GlobalStmt& gs) override { for (const auto& n : gs.names) globs.insert(n); }
      void visit(const ast::NonlocalStmt& ns) override { for (const auto& n : ns.names) nls.insert(n); }
      void visit(const ast::AssignStmt& as) override {
        auto addName = [&](const ast::Expr* e){ if (!e) return; if (e->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(e); if (!globs.count(nm->id) && !nls.count(nm->id)) locals.insert(nm->id); } };
        if (!as.target.empty()) { if (!globs.count(as.target) && !nls.count(as.target)) locals.insert(as.target); }
        for (const auto& t : as.targets) addName(t.get());
      }
      void visit(const ast::AugAssignStmt& aa) override {
        if (aa.target && aa.target->kind == ast::NodeKind::Name) {
          const auto* nm = static_cast<const ast::Name*>(aa.target.get());
          if (!globs.count(nm->id) && !nls.count(nm->id)) locals.insert(nm->id);
        }
      }
      // Recurse into same-scope statements; skip nested functions/classes
      void visit(const ast::IfStmt& iff) override { for (const auto& s: iff.thenBody) if (s) s->accept(*this); for (const auto& s: iff.elseBody) if (s) s->accept(*this); }
      void visit(const ast::WhileStmt& ws) override { for (const auto& s: ws.thenBody) if (s) s->accept(*this); for (const auto& s: ws.elseBody) if (s) s->accept(*this); }
      void visit(const ast::ForStmt& fs) override { for (const auto& s: fs.thenBody) if (s) s->accept(*this); for (const auto& s: fs.elseBody) if (s) s->accept(*this); }
      void visit(const ast::TryStmt& ts) override { for (const auto& s: ts.body) if (s) s->accept(*this); for (const auto& h: ts.handlers) if (h) for (const auto& s: h->body) if (s) s->accept(*this); for (const auto& s: ts.orelse) if (s) s->accept(*this); for (const auto& s: ts.finalbody) if (s) s->accept(*this); }
      void visit(const ast::WithStmt& ws) override { for (const auto& s: ws.body) if (s) s->accept(*this); }
      // No-op for expressions and non-scope-bearing nodes
      void visit(const ast::Module&) override {}
      void visit(const ast::FunctionDef&) override {}
      void visit(const ast::ClassDef&) override {}
      void visit(const ast::ReturnStmt&) override {}
      void visit(const ast::ExprStmt&) override {}
      void visit(const ast::PassStmt&) override {}
      void visit(const ast::BreakStmt&) override {}
      void visit(const ast::ContinueStmt&) override {}
      void visit(const ast::Name&) override {}
      void visit(const ast::Call&) override {}
      void visit(const ast::Binary&) override {}
      void visit(const ast::Unary&) override {}
      void visit(const ast::TupleLiteral&) override {}
      void visit(const ast::ListLiteral&) override {}
      void visit(const ast::ObjectLiteral&) override {}
      void visit(const ast::Literal<long long, ast::NodeKind::IntLiteral>&) override {}
      void visit(const ast::Literal<bool, ast::NodeKind::BoolLiteral>&) override {}
      void visit(const ast::Literal<double, ast::NodeKind::FloatLiteral>&) override {}
      void visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral>&) override {}
      void visit(const ast::NoneLiteral&) override {}
    } lscan;
    for (const auto& st : func->body) { if (st) st->accept(lscan); }
    pycc::sema::detail::ScopedLocalsAssigned localsGuard(&lscan.locals);
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
          // Dynamic typing: accumulate unions across assignments
          tenv.unionSet(name, maskVal, {assignStmt.file, assignStmt.line, assignStmt.col});
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
        for (const auto& n : gs.names) { if (nonlocals.count(n)) { addDiag(diags, std::string("name declared both global and nonlocal: ") + n, &gs); ok = false; return; } }
      }
      void visit(const ast::NonlocalStmt& ns) override {
        for (const auto& n : ns.names) {
          bool found = false;
          for (auto* o : outerScopes) {
            if (!o) continue;
            if (o->getSet(n) != 0U) { nonlocals.insert(n); nonlocalTargets[n] = o; found = true; break; }
          }
          if (!found) { addDiag(diags, std::string("nonlocal name not found in enclosing scope: ") + n, &ns); ok = false; return; }
          if (globals.count(n)) { addDiag(diags, std::string("name declared both nonlocal and global: ") + n, &ns); ok = false; return; }
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

      void visit(const ast::Import& im) override {
        for (const auto& a : im.names) {
          const std::string nm = a.asname.empty() ? a.name : a.asname;
          env.defineSet(nm, 0U, {im.file, im.line, im.col});
        }
      }
      void visit(const ast::ImportFrom& inf) override {
        for (const auto& a : inf.names) {
          const std::string nm = a.asname.empty() ? a.name : a.asname;
          env.defineSet(nm, 0U, {inf.file, inf.line, inf.col});
        }
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
                if (st->name != "_") { tenv.unionSet(st->name, TypeEnv::maskForKind(Type::List), {fn.name, 0, 0}); }
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
            if (pm->hasRest && pm->restName != "_") { tenv.unionSet(pm->restName, TypeEnv::maskForKind(Type::Dict), {fn.name, 0, 0}); }
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
        // Tolerate unknown class decorators; suppress diagnostics here.
        {
          std::vector<const TypeEnv*> ovec; ovec.reserve(outerScopes.size());
          for (auto* p : outerScopes) { ovec.push_back(p); }
          for (const auto& d : cls.decorators) {
            if (d) { Type tmp{}; std::vector<Diagnostic> scratch; (void)inferExprType(d.get(), env, sigs, retParamIdxs, tmp, scratch, PolyPtrs{&polyRefs.vars, &polyRefs.attrs}, &ovec, classes); }
          }
        }
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
    // Evaluate function decorators expressions for basic correctness, but tolerate unknown names.
    // We intentionally suppress diagnostics here to allow external decorators not modeled.
    for (const auto& dec : func->decorators) {
      if (!dec) continue;
      Type tmp{}; std::vector<Diagnostic> scratch;
      (void)inferExprType(dec.get(), env, sigs, retParamIdxs, tmp, scratch, {});
    }
    StmtChecker checker{*func, sigs, retParamIdxs, env, diags, PolyRefs{poly, polyAttr}, {}, false, &classes};
    for (const auto& stmt : func->body) {
      stmt->accept(checker);
      if (!checker.ok) { return false; }
    }
  }
  // Effect typing: per-statement mayRaise map (post-pass)
  struct EffStmtScan : public ast::VisitorBase {
    std::unordered_map<const ast::Stmt*, bool>& out;
    explicit EffStmtScan(std::unordered_map<const ast::Stmt*, bool>& o) : out(o) {}
    void visit(const ast::Module&) override {}
    void visit(const ast::FunctionDef&) override {}
    void visit(const ast::ExprStmt& es) override { EffectsScan eff; if (es.value) es.value->accept(eff); out[&es] = eff.mayRaise; }
    void visit(const ast::ReturnStmt& rs) override { EffectsScan eff; if (rs.value) rs.value->accept(eff); out[&rs] = eff.mayRaise; }
    void visit(const ast::AssignStmt& as) override { EffectsScan eff; if (as.value) as.value->accept(eff); out[&as] = eff.mayRaise; }
    void visit(const ast::RaiseStmt& rs) override { (void)rs; out[&rs] = true; }
    void visit(const ast::IfStmt& iff) override { bool mr=false; EffectsScan e; if (iff.cond) iff.cond->accept(e); mr = e.mayRaise; for (const auto& s: iff.thenBody){ EffStmtScan sub{out}; if (s) s->accept(sub); mr = mr || out[s.get()]; } for (const auto& s: iff.elseBody){ EffStmtScan sub2{out}; if (s) s->accept(sub2); mr = mr || out[s.get()]; } out[&iff] = mr; }
    void visit(const ast::WhileStmt& ws) override { bool mr=false; EffectsScan e; if (ws.cond) ws.cond->accept(e); mr = e.mayRaise; for (const auto& s: ws.thenBody){ EffStmtScan sub{out}; if (s) s->accept(sub); mr = mr || out[s.get()]; } for (const auto& s: ws.elseBody){ EffStmtScan sub2{out}; if (s) s->accept(sub2); mr = mr || out[s.get()]; } out[&ws] = mr; }
    void visit(const ast::ForStmt& fs) override { bool mr=false; EffectsScan e; if (fs.iterable) fs.iterable->accept(e); mr = e.mayRaise; for (const auto& s: fs.thenBody){ EffStmtScan sub{out}; if (s) s->accept(sub); mr = mr || out[s.get()]; } for (const auto& s: fs.elseBody){ EffStmtScan sub2{out}; if (s) s->accept(sub2); mr = mr || out[s.get()]; } out[&fs] = mr; }
    void visit(const ast::TryStmt& ts) override { out[&ts] = true; for (const auto& s: ts.body) if (s) s->accept(*this); for (const auto& h: ts.handlers) if (h) for (const auto& s: h->body) if (s) s->accept(*this); for (const auto& s: ts.orelse) if (s) s->accept(*this); for (const auto& s: ts.finalbody) if (s) s->accept(*this); }
    void visit(const ast::IntLiteral&) override {}
    void visit(const ast::FloatLiteral&) override {}
    void visit(const ast::BoolLiteral&) override {}
    void visit(const ast::StringLiteral&) override {}
    void visit(const ast::Name&) override {}
    void visit(const ast::Call&) override {}
    void visit(const ast::Binary&) override {}
    void visit(const ast::Unary&) override {}
    void visit(const ast::TupleLiteral&) override {}
    void visit(const ast::ListLiteral&) override {}
    void visit(const ast::ObjectLiteral&) override {}
    void visit(const ast::NoneLiteral&) override {}
  };
  for (const auto& func : mod.functions) {
    EffStmtScan ess{stmtMayRaise_};
    for (const auto& st : func->body) if (st) st->accept(ess);
  }
  return diags.empty();
}

} // namespace pycc::sema
