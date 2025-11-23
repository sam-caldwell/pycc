/***
 * Name: pycc::sema::Sig
 * Purpose: Compact function signature (return + param kinds) with extended params.
 */
#pragma once

#include <vector>
#include "sema/detail/types/TypeAlias.h"
#include "sema/detail/types/SigParam.h"

namespace pycc::sema {

struct Sig {
  Type ret{Type::NoneType};
  std::vector<Type> params;
  std::vector<SigParam> full;
};

} // namespace pycc::sema

