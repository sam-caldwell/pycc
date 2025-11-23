/***
 * Name: pycc::sema::SigParam
 * Purpose: Parameter metadata for a function signature used by Sema.
 */
#pragma once

#include <string>
#include <cstdint>
#include "sema/detail/types/TypeAlias.h"

namespace pycc::sema {

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
  uint32_t listElemMask{0U};  // if type==List and list[T] annotated, mask of T
};

} // namespace pycc::sema

