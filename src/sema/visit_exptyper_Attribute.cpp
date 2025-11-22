/***
 * Name: ExpressionTyper::visit(Attribute)
 * Purpose: Type attribute access; uses env attribute set when base is Name.
 */
#include "sema/detail/ExpressionTyper.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Attribute& attr) {
  if (attr.value) {
    ExpressionTyper v{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers};
    attr.value->accept(v); if (!v.ok) { ok = false; return; }
  }
  out = ast::TypeKind::NoneType; outSet = 0U;
  if (attr.value && attr.value->kind == ast::NodeKind::Name) {
    const auto* base = static_cast<const ast::Name*>(attr.value.get());
    const uint32_t msk = env->getAttr(base->id, attr.attr);
    if (msk != 0U) { outSet = msk; if (TypeEnv::isSingleMask(msk)) out = TypeEnv::kindFromMask(msk); }
  }
  auto& m = const_cast<ast::Attribute&>(attr); m.setType(out);
}

