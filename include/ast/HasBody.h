#pragma once

#include <memory>
#include <vector>

namespace pycc::ast {

template <typename StmtT>
struct HasBody {
    std::vector<std::unique_ptr<StmtT>> body;
};

} // namespace pycc::ast
