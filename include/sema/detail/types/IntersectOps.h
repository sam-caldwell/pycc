/**
 * @file
 * @brief Internal helpers to intersect TypeEnv state across branches.
 */
#pragma once

namespace pycc::sema {
    class TypeEnv;
}

namespace pycc::sema::detail {

struct IntersectOps {
    // Intersect scalar type sets and record resulting type where singular
    static void setsAndTypes(TypeEnv& dst, const TypeEnv& a, const TypeEnv& b);
    // Intersect list element sets for names present in both
    static void listElems(TypeEnv& dst, const TypeEnv& a, const TypeEnv& b);
    // Intersect tuple element sets index-wise for names present in both
    static void tupleElems(TypeEnv& dst, const TypeEnv& a, const TypeEnv& b);
    // Intersect dict key/value sets for names present in both
    static void dictKeyVals(TypeEnv& dst, const TypeEnv& a, const TypeEnv& b);
};

} // namespace pycc::sema::detail

