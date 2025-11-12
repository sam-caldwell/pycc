#pragma once

#include "ast/VisitorBase.h"

namespace pycc::ast {

// CRTP mixin that equips concrete nodes with convenient template accept()
// for ad-hoc visitors. Polymorphic accept(VisitorBase&) is provided by Node.
template <typename Derived, NodeKind K>
struct Acceptable {
    // Template-based accept for ad-hoc visitors when not using VisitorBase
    template <typename Visitor>
    void apply(Visitor& v) { v.visit(static_cast<Derived&>(*this)); }
    template <typename Visitor>
    void apply(Visitor& v) const { v.visit(static_cast<const Derived&>(*this)); }
};

} // namespace pycc::ast
