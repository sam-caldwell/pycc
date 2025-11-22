/***
 * @file
 * @brief Internal Sema shared types (signatures, class info, polymorphic maps).
 */
#pragma once

#include "ast/TypeKind.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pycc::sema {

using Type = ast::TypeKind;

/***
 * @brief Parameter metadata for a function signature used by Sema.
 */
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

/***
 * @brief Compact function signature (return + param kinds) with extended params.
 */
struct Sig {
  Type ret{Type::NoneType};
  std::vector<Type> params;
  std::vector<SigParam> full;
};

/***
 * @brief Minimal per-class info for method signatures and base classes.
 */
struct ClassInfo {
  std::vector<std::string> bases;
  std::unordered_map<std::string, Sig> methods; // method name -> signature
};

/***
 * @brief Polymorphic call target maps grouped to avoid parameter swapping.
 */
struct PolyPtrs {
  const std::unordered_map<std::string, std::unordered_set<std::string>>* vars{nullptr};
  const std::unordered_map<std::string, std::unordered_set<std::string>>* attrs{nullptr};
};

/***
 * @brief Mutable references to polymorphic call/attribute target maps.
 */
struct PolyRefs {
  std::unordered_map<std::string, std::unordered_set<std::string>>& vars;
  std::unordered_map<std::string, std::unordered_set<std::string>>& attrs;
};

} // namespace pycc::sema

