/**
 * @file
 * @brief AST structural pattern declarations.
 */
#pragma once

#include <memory>
#include <string>
#include <vector>
#include "ast/Node.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct Pattern : Node { using Node::Node; };

struct PatternWildcard final : Pattern, Acceptable<PatternWildcard, NodeKind::PatternWildcard> {
  PatternWildcard() : Pattern(NodeKind::PatternWildcard) {}
};

struct PatternName final : Pattern, Acceptable<PatternName, NodeKind::PatternName> {
  std::string name;
  explicit PatternName(std::string n) : Pattern(NodeKind::PatternName), name(std::move(n)) {}
};

struct PatternLiteral final : Pattern, Acceptable<PatternLiteral, NodeKind::PatternLiteral> {
  // Store literal as expression node for shape flexibility
  std::unique_ptr<Expr> value;
  explicit PatternLiteral(std::unique_ptr<Expr> v)
      : Pattern(NodeKind::PatternLiteral), value(std::move(v)) {}
};

struct PatternOr final : Pattern, Acceptable<PatternOr, NodeKind::PatternOr> {
  std::vector<std::unique_ptr<Pattern>> patterns;
  PatternOr() : Pattern(NodeKind::PatternOr) {}
};

struct PatternAs final : Pattern, Acceptable<PatternAs, NodeKind::PatternAs> {
  std::unique_ptr<Pattern> pattern;
  std::string name;
  PatternAs(std::unique_ptr<Pattern> p, std::string n)
      : Pattern(NodeKind::PatternAs), pattern(std::move(p)), name(std::move(n)) {}
};

struct PatternClass final : Pattern, Acceptable<PatternClass, NodeKind::PatternClass> {
  std::string className; // simple name
  std::vector<std::unique_ptr<Pattern>> args; // positional-only for now (shape)
  std::vector<std::pair<std::string, std::unique_ptr<Pattern>>> kwargs; // keyword patterns
  explicit PatternClass(std::string cn)
      : Pattern(NodeKind::PatternClass), className(std::move(cn)) {}
};

struct PatternSequence final : Pattern, Acceptable<PatternSequence, NodeKind::PatternSequence> {
  bool isList{true}; // true: [], false: ()
  std::vector<std::unique_ptr<Pattern>> elements;
  PatternSequence() : Pattern(NodeKind::PatternSequence) {}
};

struct PatternMapping final : Pattern, Acceptable<PatternMapping, NodeKind::PatternMapping> {
  std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Pattern>>> items;
  bool hasRest{false};
  std::string restName; // valid if hasRest
  PatternMapping() : Pattern(NodeKind::PatternMapping) {}
};

struct PatternStar final : Pattern, Acceptable<PatternStar, NodeKind::PatternStar> {
  std::string name; // may be "_" to discard
  explicit PatternStar(std::string n) : Pattern(NodeKind::PatternStar), name(std::move(n)) {}
};

} // namespace pycc::ast
