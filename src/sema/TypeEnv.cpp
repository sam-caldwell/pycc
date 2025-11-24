/***
 * Name: TypeEnv (definitions)
 * Purpose: Implement methods for tracking type sets, provenance, and shapes.
 */
#include "sema/TypeEnv.h"
#include "sema/detail/types/IntersectOps.h"

namespace pycc::sema {
    /*** Name: TypeEnv::maskForKind */
    uint32_t TypeEnv::maskForKind(ast::TypeKind k) { return maskFor(k); }

    /*** Name: TypeEnv::isSingleMask */
    bool TypeEnv::isSingleMask(uint32_t m) { return isSingle(m); }

    /*** Name: TypeEnv::kindFromMask */
    ast::TypeKind TypeEnv::kindFromMask(uint32_t m) { return kindFor(m); }

    /*** Name: TypeEnv::define */
    void TypeEnv::define(const std::string &name, ast::TypeKind t, Provenance p) {
        types_[name] = t;
        prov_[name] = std::move(p);
        sets_[name] = maskFor(t);
    }

    /*** Name: TypeEnv::markNonNone */
    void TypeEnv::markNonNone(const std::string &name) { nonNone_[name] = true; }

    /*** Name: TypeEnv::isNonNone */
    bool TypeEnv::isNonNone(const std::string &name) const {
        const auto it = nonNone_.find(name);
        return it != nonNone_.end() && it->second;
    }

    /*** Name: TypeEnv::restrictTo */
    void TypeEnv::restrictTo(const std::string &name, uint32_t mask) {
        auto &cur = sets_[name];
        if (cur == 0U) { cur = mask; } else { cur &= mask; }
        if (isSingle(cur)) { types_[name] = kindFor(cur); }
    }

    /*** Name: TypeEnv::restrictToKind */
    void TypeEnv::restrictToKind(const std::string &name, ast::TypeKind k) { restrictTo(name, maskFor(k)); }

    /*** Name: TypeEnv::excludeKind */
    void TypeEnv::excludeKind(const std::string &name, ast::TypeKind k) {
        auto &cur = sets_[name];
        if (cur == 0U) { cur = kAllMask; }
        cur &= ~maskFor(k);
        if (isSingle(cur)) { types_[name] = kindFor(cur); }
        if (k == ast::TypeKind::NoneType) { markNonNone(name); }
    }

    /*** Name: TypeEnv::defineSet */
    void TypeEnv::defineSet(const std::string &name, uint32_t mask, Provenance p) {
        prov_[name] = std::move(p);
        sets_[name] = mask;
        if (isSingle(mask)) { types_[name] = kindFor(mask); }
    }

    /*** Name: TypeEnv::unionSet */
    void TypeEnv::unionSet(const std::string &name, uint32_t mask, Provenance p) {
        if (!sets_.contains(name)) {
            defineSet(name, mask, std::move(p));
            return;
        }
        sets_[name] |= mask;
        if (isSingle(sets_[name])) { types_[name] = kindFor(sets_[name]); }
    }

    /*** Name: TypeEnv::defineInstanceOf */
    void TypeEnv::defineInstanceOf(const std::string &name, const std::string_view &className) {
        instances_[name] = className;
    }

    /*** Name: TypeEnv::instanceOf */
    std::optional<std::string> TypeEnv::instanceOf(const std::string &name) const {
        const auto it = instances_.find(name);
        if (it == instances_.end()) return std::nullopt;
        return it->second;
    }

    /*** Name: TypeEnv::get */
    std::optional<ast::TypeKind> TypeEnv::get(const std::string &name) const {
        const auto it = types_.find(name);
        if (it == types_.end()) return std::nullopt;
        return it->second;
    }

    /*** Name: TypeEnv::defineListElems */
    void TypeEnv::defineListElems(const std::string &name, const uint32_t elemMask) { listElemSets_[name] = elemMask; }

    /*** Name: TypeEnv::getListElems */
    uint32_t TypeEnv::getListElems(const std::string &name) const {
        const auto it = listElemSets_.find(name);
        return (it == listElemSets_.end()) ? 0U : it->second;
    }

    /*** Name: TypeEnv::defineTupleElems */
    void TypeEnv::defineTupleElems(const std::string &name, std::vector<uint32_t> elemMasks) {
        tupleElemSets_[name] = std::move(elemMasks);
    }

    /*** Name: TypeEnv::getTupleElemAt */
    uint32_t TypeEnv::getTupleElemAt(const std::string &name, size_t idx) const {
        const auto it = tupleElemSets_.find(name);
        if (it == tupleElemSets_.end()) return 0U;
        const auto &v = it->second;
        if (idx >= v.size()) return 0U;
        return v[idx];
    }

    /*** Name: TypeEnv::unionOfTupleElems */
    uint32_t TypeEnv::unionOfTupleElems(const std::string &name) const {
        const auto it = tupleElemSets_.find(name);
        if (it == tupleElemSets_.end()) return 0U;
        uint32_t acc = 0U;
        for (auto m: it->second) acc |= m;
        return acc;
    }

    /*** Name: TypeEnv::defineDictKeyVals */
    void TypeEnv::defineDictKeyVals(const std::string &name, uint32_t keyMask, uint32_t valMask) {
        dictKeySets_[name] = keyMask;
        dictValSets_[name] = valMask;
    }

    /*** Name: TypeEnv::getDictKeys */
    uint32_t TypeEnv::getDictKeys(const std::string &name) const {
        const auto it = dictKeySets_.find(name);
        return (it == dictKeySets_.end()) ? 0U : it->second;
    }

    /*** Name: TypeEnv::getDictVals */
    uint32_t TypeEnv::getDictVals(const std::string &name) const {
        const auto it = dictValSets_.find(name);
        return (it == dictValSets_.end()) ? 0U : it->second;
    }

    /*** Name: TypeEnv::defineAttr */
    void TypeEnv::defineAttr(const std::string &base, const std::string &attr, uint32_t mask) {
        attrSets_[base][attr] = mask;
    }

    /*** Name: TypeEnv::getAttr */
    uint32_t TypeEnv::getAttr(const std::string &base, const std::string &attr) const {
        const auto it = attrSets_.find(base);
        if (it == attrSets_.end()) return 0U;
        const auto it2 = it->second.find(attr);
        return (it2 == it->second.end()) ? 0U : it2->second;
    }

    /*** Name: TypeEnv::getSet */
    uint32_t TypeEnv::getSet(const std::string &name) const {
        const auto it = sets_.find(name);
        return (it == sets_.end()) ? 0U : it->second;
    }

    /*** Name: TypeEnv::intersectFrom */
    void TypeEnv::intersectFrom(const TypeEnv &a, const TypeEnv &b) {
        detail::IntersectOps::setsAndTypes(*this, a, b);
        detail::IntersectOps::listElems(*this, a, b);
        detail::IntersectOps::tupleElems(*this, a, b);
        detail::IntersectOps::dictKeyVals(*this, a, b);
    }

    /*** Name: TypeEnv::where */
    std::optional<Provenance> TypeEnv::where(const std::string &name) const {
        const auto it = prov_.find(name);
        if (it == prov_.end()) return std::nullopt;
        return it->second;
    }

    /*** Name: TypeEnv::applyMerged */
    void TypeEnv::applyMerged(const TypeEnv &src) {
        // Scalar sets and singular kinds
        for (const auto &kv: src.sets_) { sets_[kv.first] = kv.second; }
        for (const auto &kv: src.types_) { types_[kv.first] = kv.second; }
        // Container shapes
        for (const auto &kv: src.listElemSets_) { listElemSets_[kv.first] = kv.second; }
        for (const auto &kv: src.tupleElemSets_) { tupleElemSets_[kv.first] = kv.second; }
        for (const auto &kv: src.dictKeySets_) { dictKeySets_[kv.first] = kv.second; }
        for (const auto &kv: src.dictValSets_) { dictValSets_[kv.first] = kv.second; }
    }

    /*** Name: TypeEnv::maskFor */
    uint32_t TypeEnv::maskFor(ast::TypeKind k) {
        switch (k) {
            using enum pycc::ast::TypeKind;
            case NoneType: return kNone;
        case Int: return kInt;
            case Bool: return kBool;
            case Float: return kFloat;
        case Str: return kStr;
        case Bytes: return kBytes;
            case List: return kList;
            case Tuple: return kTuple;
            case Dict: return kDict;
            default: return 0U;
        }
    }

    /*** Name: TypeEnv::isSingle */
    bool TypeEnv::isSingle(uint32_t m) { return m && ((m & (m - 1U)) == 0U); }

    /*** Name: TypeEnv::kindFor */
    ast::TypeKind TypeEnv::kindFor(const uint32_t m) {
        if (m == kNone) return ast::TypeKind::NoneType;
        if (m == kInt) return ast::TypeKind::Int;
        if (m == kBool) return ast::TypeKind::Bool;
        if (m == kFloat) return ast::TypeKind::Float;
        if (m == kStr) return ast::TypeKind::Str;
        if (m == kBytes) return ast::TypeKind::Bytes;
        if (m == kList) return ast::TypeKind::List;
        if (m == kTuple) return ast::TypeKind::Tuple;
        if (m == kDict) return ast::TypeKind::Dict;
        return ast::TypeKind::NoneType;
    }
} // namespace pycc::sema
