/***
 * Name: pycc::sema::TypeEnv
 * Purpose: Track variable types and provenance for diagnostics.
 */
#pragma once

#include "ast/Nodes.h"
#include <optional>
#include <string>
#include <unordered_map>

namespace pycc::sema {

struct Provenance {
  std::string file;
  int line{0};
  int col{0};
};

class TypeEnv {
 public:
  // Utility helpers for external users (Sema) working with sets
  static uint32_t maskForKind(ast::TypeKind k) { return maskFor(k); }
  static bool isSingleMask(uint32_t m) { return isSingle(m); }
  static ast::TypeKind kindFromMask(uint32_t m) { return kindFor(m); }
  void define(const std::string& name, ast::TypeKind t, Provenance p) {
    types_[name] = t; prov_[name] = std::move(p);
    sets_[name] = maskFor(t);
  }
  // Markers for negative refinements (e.g., not None)
  void markNonNone(const std::string& name) { nonNone_[name] = true; }
  bool isNonNone(const std::string& name) const {
    auto it = nonNone_.find(name);
    return it != nonNone_.end() && it->second;
  }
  // Unions and negation
  void restrictTo(const std::string& name, uint32_t mask) {
    auto& cur = sets_[name];
    if (cur == 0U) { cur = mask; }
    else { cur &= mask; }
    // If narrowed to a single kind, reflect in types_ but avoid erasing prior exact unless conclusive
    if (isSingle(cur)) { types_[name] = kindFor(cur); }
  }
  void restrictToKind(const std::string& name, ast::TypeKind k) { restrictTo(name, maskFor(k)); }
  void excludeKind(const std::string& name, ast::TypeKind k) {
    auto& cur = sets_[name];
    if (cur == 0U) { cur = kAllMask; }
    cur &= ~maskFor(k);
    if (isSingle(cur)) { types_[name] = kindFor(cur); }
    if (k == ast::TypeKind::NoneType) { markNonNone(name); }
  }
  void defineSet(const std::string& name, uint32_t mask, Provenance p) {
    prov_[name] = std::move(p);
    sets_[name] = mask;
    if (isSingle(mask)) { types_[name] = kindFor(mask); }
  }
  std::optional<ast::TypeKind> get(const std::string& name) const {
    auto it = types_.find(name);
    if (it == types_.end()) return std::nullopt; return it->second;
  }
  void defineListElems(const std::string& name, uint32_t elemMask) { listElemSets_[name] = elemMask; }
  uint32_t getListElems(const std::string& name) const {
    auto it = listElemSets_.find(name);
    return (it == listElemSets_.end()) ? 0U : it->second;
  }
  uint32_t getSet(const std::string& name) const {
    auto it = sets_.find(name);
    return (it == sets_.end()) ? 0U : it->second;
  }
  // Intersect current env with two branch envs (then/else): for names present in both, keep common kinds.
  // If the intersection is empty (contradictory), record a zero mask so that use sites will flag an error.
  void intersectFrom(const TypeEnv& a, const TypeEnv& b) {
    for (const auto& kv : a.sets_) {
      const std::string& name = kv.first;
      uint32_t am = kv.second;
      uint32_t bm = b.getSet(name);
      if (am != 0U && bm != 0U) {
        uint32_t inter = am & bm;
        sets_[name] = inter; // may be zero: contradictory
        if (inter != 0U && isSingle(inter)) { types_[name] = kindFor(inter); }
      }
    }
    for (const auto& kv : b.sets_) {
      const std::string& name = kv.first;
      if (a.sets_.find(name) != a.sets_.end()) continue; // already handled
      uint32_t bm = kv.second;
      uint32_t am = a.getSet(name);
      if (am != 0U && bm != 0U) {
        uint32_t inter = am & bm;
        sets_[name] = inter;
        if (inter != 0U && isSingle(inter)) { types_[name] = kindFor(inter); }
      }
    }
  }
  std::optional<Provenance> where(const std::string& name) const {
    auto it = prov_.find(name);
    if (it == prov_.end()) return std::nullopt; return it->second;
  }
 private:
  static constexpr uint32_t kNone  = 1U << 0U;
  static constexpr uint32_t kInt   = 1U << 1U;
  static constexpr uint32_t kBool  = 1U << 2U;
  static constexpr uint32_t kFloat = 1U << 3U;
  static constexpr uint32_t kStr   = 1U << 4U;
  static constexpr uint32_t kList  = 1U << 5U;
  static constexpr uint32_t kAllMask = kNone | kInt | kBool | kFloat | kStr | kList;
  static uint32_t maskFor(ast::TypeKind k) {
    switch (k) {
      case ast::TypeKind::NoneType: return kNone;
      case ast::TypeKind::Int: return kInt;
      case ast::TypeKind::Bool: return kBool;
      case ast::TypeKind::Float: return kFloat;
      case ast::TypeKind::Str: return kStr;
      case ast::TypeKind::List: return kList;
      default: return 0U;
    }
  }
  static bool isSingle(uint32_t m) { return m && ((m & (m - 1U)) == 0U); }
  static ast::TypeKind kindFor(uint32_t m) {
    if (m == kNone) return ast::TypeKind::NoneType;
    if (m == kInt) return ast::TypeKind::Int;
    if (m == kBool) return ast::TypeKind::Bool;
    if (m == kFloat) return ast::TypeKind::Float;
    if (m == kStr) return ast::TypeKind::Str;
    if (m == kList) return ast::TypeKind::List;
    return ast::TypeKind::NoneType;
  }
  std::unordered_map<std::string, ast::TypeKind> types_;
  std::unordered_map<std::string, Provenance> prov_;
  std::unordered_map<std::string, bool> nonNone_;
  std::unordered_map<std::string, uint32_t> sets_;
  std::unordered_map<std::string, uint32_t> listElemSets_;
};

} // namespace pycc::sema
