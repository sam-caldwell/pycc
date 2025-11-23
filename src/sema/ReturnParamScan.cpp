/***
 * Name: computeReturnParamIdxs (definition)
 * Purpose: Build a map of functions that consistently return one of their
 *          parameters, mapping function name -> returned parameter index.
 */
#include "sema/detail/ReturnParamScan.h"
#include "sema/detail/checks/ReturnParamInfer.h"

namespace pycc::sema {
    std::unordered_map<std::string, int> computeReturnParamIdxs(const ast::Module &mod) {
        std::unordered_map<std::string, int> retParamIdxs;
        for (const auto &func: mod.functions) {
            if (auto idx = detail::inferReturnParamIdx(*func)) retParamIdxs[func->name] = *idx;
        }
        return retParamIdxs;
    }
} // namespace pycc::sema
