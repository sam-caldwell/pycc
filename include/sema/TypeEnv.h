/***
 * Name: pycc::sema::TypeEnv
 * Purpose: Track variable types and provenance for diagnostics.
 */
#pragma once

#include "ast/Nodes.h"
#include "sema/Provenance.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace pycc::sema {
    namespace detail { struct IntersectOps; }
    /***
     * Name: TypeEnv
     * Purpose: Track variable type sets, provenance, and shape details for containers.
     */
    class TypeEnv {
    public:
        // Utility helpers for external users (Sema) working with sets
        static uint32_t maskForKind(ast::TypeKind k);

        static bool isSingleMask(uint32_t m);

        static ast::TypeKind kindFromMask(uint32_t m);

        void define(const std::string &name, ast::TypeKind t, Provenance p);

        // Markers for negative refinements (e.g., not None)
        void markNonNone(const std::string &name);

        bool isNonNone(const std::string &name) const;

        // Unions and negation
        void restrictTo(const std::string &name, uint32_t mask);

        void restrictToKind(const std::string &name, ast::TypeKind k);

        void excludeKind(const std::string &name, ast::TypeKind k);

        void defineSet(const std::string &name, uint32_t mask, Provenance p);

        // Union in additional kinds for a name (dynamic typing). Records provenance if not present.
        void unionSet(const std::string &name, uint32_t mask, Provenance p);

        // Record that a name is an instance of a known class (by class name string)
        void defineInstanceOf(const std::string &name, const std::string_view &className);

        std::optional<std::string> instanceOf(const std::string &name) const;

        std::optional<ast::TypeKind> get(const std::string &name) const;

        void defineListElems(const std::string &name, uint32_t elemMask);

        uint32_t getListElems(const std::string &name) const;

        // Tuple element masks by index (heterogeneous). Unknown index uses unionOfTupleElems(name).
        void defineTupleElems(const std::string &name, std::vector<uint32_t> elemMasks);

        uint32_t getTupleElemAt(const std::string &name, size_t idx) const;

        uint32_t unionOfTupleElems(const std::string &name) const;

        // Dict key/value masks
        void defineDictKeyVals(const std::string &name, uint32_t keyMask, uint32_t valMask);

        uint32_t getDictKeys(const std::string &name) const;

        uint32_t getDictVals(const std::string &name) const;

        // Attribute typing per base variable name
        void defineAttr(const std::string &base, const std::string &attr, uint32_t mask);

        uint32_t getAttr(const std::string &base, const std::string &attr) const;

        uint32_t getSet(const std::string &name) const;

        // Intersect current env with two branch envs (then/else): for names present in both, keep common kinds.
        // If the intersection is empty (contradictory), record a zero mask so that use sites will flag an error.
        void intersectFrom(const TypeEnv &a, const TypeEnv &b);

        std::optional<Provenance> where(const std::string &name) const;

    private:
        static constexpr uint32_t kNone = 1U << 0U;
        static constexpr uint32_t kInt = 1U << 1U;
        static constexpr uint32_t kBool = 1U << 2U;
        static constexpr uint32_t kFloat = 1U << 3U;
        static constexpr uint32_t kStr = 1U << 4U;
        static constexpr uint32_t kList = 1U << 5U;
        static constexpr uint32_t kTuple = 1U << 6U;
        static constexpr uint32_t kDict = 1U << 7U;
        static constexpr uint32_t kAllMask = kNone | kInt | kBool | kFloat | kStr | kList | kTuple | kDict;

        static uint32_t maskFor(ast::TypeKind k);

        static bool isSingle(uint32_t m);

        static ast::TypeKind kindFor(uint32_t m);

        std::unordered_map<std::string, ast::TypeKind> types_;
        std::unordered_map<std::string, Provenance> prov_;
        std::unordered_map<std::string, bool> nonNone_;
        std::unordered_map<std::string, uint32_t> sets_;
        std::unordered_map<std::string, uint32_t> listElemSets_;
        std::unordered_map<std::string, std::vector<uint32_t> > tupleElemSets_;
        std::unordered_map<std::string, uint32_t> dictKeySets_;
        std::unordered_map<std::string, uint32_t> dictValSets_;
        std::unordered_map<std::string, std::unordered_map<std::string, uint32_t> > attrSets_;
        std::unordered_map<std::string, std::string> instances_;

        // Allow internal intersect helpers to access private maps for efficiency
        friend struct detail::IntersectOps;
    };
} // namespace pycc::sema
