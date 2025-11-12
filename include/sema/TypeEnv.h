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
  void define(const std::string& name, ast::TypeKind t, Provenance p) {
    types_[name] = t; prov_[name] = std::move(p);
  }
  std::optional<ast::TypeKind> get(const std::string& name) const {
    auto it = types_.find(name);
    if (it == types_.end()) return std::nullopt; return it->second;
  }
  std::optional<Provenance> where(const std::string& name) const {
    auto it = prov_.find(name);
    if (it == prov_.end()) return std::nullopt; return it->second;
  }
 private:
  std::unordered_map<std::string, ast::TypeKind> types_;
  std::unordered_map<std::string, Provenance> prov_;
};

} // namespace pycc::sema
