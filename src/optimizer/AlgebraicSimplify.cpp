/***
 * Name: pycc::opt::AlgebraicSimplify (impl)
 */
#include "optimizer/AlgebraicSimplify.h"
#include "ast/BinaryOperator.h"
#include "ast/BoolLiteral.h"
#include "ast/Expr.h"
#include "ast/FloatLiteral.h"
#include "ast/IntLiteral.h"
#include "ast/NodeKind.h"
#include "ast/Stmt.h"
#include "ast/StringLiteral.h"
#include "ast/TypeKind.h"
#include "ast/UnaryOperator.h"
#include "ast/Unary.h"
#include "ast/VisitorBase.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pycc::opt {
using namespace pycc::ast;

// Literal classification helpers at file scope
static inline bool isZero(const Expr* expr) {
  if (expr == nullptr) { return false; }
  if (expr->kind == NodeKind::IntLiteral) { return static_cast<const IntLiteral*>(expr)->value == 0; }
  if (expr->kind == NodeKind::FloatLiteral) { return static_cast<const FloatLiteral*>(expr)->value == 0.0; }
  return false;
}
static inline bool isOne(const Expr* expr) {
  if (expr == nullptr) { return false; }
  if (expr->kind == NodeKind::IntLiteral) { return static_cast<const IntLiteral*>(expr)->value == 1; }
  if (expr->kind == NodeKind::FloatLiteral) { return static_cast<const FloatLiteral*>(expr)->value == 1.0; }
  return false;
}
// Treat a unary-negated float literal as a float participant in arithmetic simplifications.
static inline bool isFloatLike(const Expr* expr) {
  if (expr == nullptr) return false;
  if (expr->kind == NodeKind::FloatLiteral) return true;
  if (expr->kind == NodeKind::UnaryExpr) {
    const auto* u = static_cast<const Unary*>(expr);
    return u->op == UnaryOperator::Neg && u->operand && u->operand->kind == NodeKind::FloatLiteral;
  }
  return false;
}

static inline const BoolLiteral* asBool(const Expr* e) {
  return (e && e->kind == NodeKind::BoolLiteral) ? static_cast<const BoolLiteral*>(e) : nullptr;
}
static inline const IntLiteral* asInt(const Expr* e) {
  return (e && e->kind == NodeKind::IntLiteral) ? static_cast<const IntLiteral*>(e) : nullptr;
}

// Shallow structural equality for a small subset of expressions where
// canonical keys may not be available yet: names and literals.
static bool shallowEqualSimple(const Expr* a, const Expr* b) {
  if (a == nullptr || b == nullptr) { return false; }
  if (a->kind != b->kind) { return false; }
  switch (a->kind) {
    case NodeKind::Name: {
      const auto* na = static_cast<const Name*>(a);
      const auto* nb = static_cast<const Name*>(b);
      return na->id == nb->id;
    }
    case NodeKind::IntLiteral: {
      return static_cast<const IntLiteral*>(a)->value == static_cast<const IntLiteral*>(b)->value;
    }
    case NodeKind::FloatLiteral: {
      return static_cast<const FloatLiteral*>(a)->value == static_cast<const FloatLiteral*>(b)->value;
    }
    case NodeKind::BoolLiteral: {
      return static_cast<const BoolLiteral*>(a)->value == static_cast<const BoolLiteral*>(b)->value;
    }
    default: break;
  }
  // Fall back to canonical keys when both present
  const auto& ca = a->canonical();
  const auto& cb = b->canonical();
  return ca && cb && *ca == *cb;
}

namespace {

struct SimplifyVisitor : public ast::VisitorBase {
  std::unordered_map<std::string, uint64_t>& stats;
  size_t& changes;
  std::unique_ptr<Expr>* slot{nullptr};
  std::unique_ptr<Stmt>* stmtSlot{nullptr};

  explicit SimplifyVisitor(std::unordered_map<std::string, uint64_t>& statsMap, size_t& changeCount)
      : stats(statsMap), changes(changeCount) {}

  void rewrite(std::unique_ptr<Expr>& expr) {
    if (!expr) { return; }
    // debug (removed)
    slot = &expr;
    expr->accept(*this);
    // debug (removed)
  }

  void touch(std::unique_ptr<Stmt>& stmt) {
    if (!stmt) { return; }
    stmtSlot = &stmt;
    stmt->accept(*this);
  }
  void walkBlock(std::vector<std::unique_ptr<Stmt>>& body) {
    for (auto& stmt : body) { touch(stmt); }
  }

  // Expr visitors
  // NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
  void visit(const Binary& binary) override {
    (void)binary;
    auto* bin = static_cast<Binary*>(slot->get());
    auto* exprSlot = slot;
    rewrite(bin->lhs);
    rewrite(bin->rhs);

    std::unique_ptr<Expr> replacement;
    const Expr* left = bin->lhs.get();
    const Expr* right = bin->rhs.get();
    if (left && right) {
      // First, shape-based rewrites that are type-agnostic and always safe
      // for numeric domains. These do not depend on literal presence:
      //   x + (-y) -> x - y
      //   (-x) + y -> y - x
      //   x - (-y) -> x + y
      if (bin->op == BinaryOperator::Add) {
        if (right->kind == NodeKind::UnaryExpr) {
          auto* ur = static_cast<Unary*>(bin->rhs.get());
          if (ur->op == UnaryOperator::Neg && ur->operand) {
            auto sub = std::make_unique<Binary>(BinaryOperator::Sub, std::move(bin->lhs), std::move(ur->operand));
            ++changes; stats["algebraic_shape"]++;
            slot = exprSlot; *slot = std::move(sub);
            return;
          }
        }
        if (left->kind == NodeKind::UnaryExpr) {
          auto* ul = static_cast<Unary*>(bin->lhs.get());
          if (ul->op == UnaryOperator::Neg && ul->operand) {
            auto sub = std::make_unique<Binary>(BinaryOperator::Sub, std::move(bin->rhs), std::move(ul->operand));
            ++changes; stats["algebraic_shape"]++;
            slot = exprSlot; *slot = std::move(sub);
            return;
          }
        }
      } else if (bin->op == BinaryOperator::Sub) {
        if (right->kind == NodeKind::UnaryExpr) {
          auto* ur = static_cast<Unary*>(bin->rhs.get());
          if (ur->op == UnaryOperator::Neg && ur->operand) {
            auto add = std::make_unique<Binary>(BinaryOperator::Add, std::move(bin->lhs), std::move(ur->operand));
            ++changes; stats["algebraic_shape"]++;
            slot = exprSlot; *slot = std::move(add);
            return;
          }
        }
      }
      // Debug: trace ops in this test path
      // std::cerr << "visit Binary op=" << static_cast<int>(bin->op) << "\n";
      // Int rules
      if (left->kind == NodeKind::IntLiteral || right->kind == NodeKind::IntLiteral) {
        switch (bin->op) {
          case BinaryOperator::Add:
            if (isZero(left)) { replacement = std::move(bin->rhs); }
            else if (isZero(right)) { replacement = std::move(bin->lhs); }
            else if (right->kind == NodeKind::UnaryExpr) {
              auto* ur = static_cast<Unary*>(bin->rhs.get());
              if (ur->op == UnaryOperator::Neg && ur->operand) {
                // x + (-y) -> x - y
                auto sub = std::make_unique<Binary>(BinaryOperator::Sub, std::move(bin->lhs), std::move(ur->operand));
                replacement = std::move(sub);
              }
            } else if (left->kind == NodeKind::UnaryExpr) {
              auto* ul = static_cast<Unary*>(bin->lhs.get());
              if (ul->op == UnaryOperator::Neg && ul->operand) {
                // (-x) + y -> y - x
                auto sub = std::make_unique<Binary>(BinaryOperator::Sub, std::move(bin->rhs), std::move(ul->operand));
                replacement = std::move(sub);
              }
            }
            if (replacement) { ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Sub:
            if (isZero(right)) { replacement = std::move(bin->lhs); ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            if (isZero(left)) {
              auto neg = std::make_unique<Unary>(UnaryOperator::Neg, std::move(bin->rhs));
              replacement = std::move(neg);
              ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return;
            }
            if (right->kind == NodeKind::UnaryExpr) {
              auto* ur = static_cast<Unary*>(bin->rhs.get());
              if (ur->op == UnaryOperator::Neg && ur->operand) {
                // x - (-y) -> x + y
                auto add = std::make_unique<Binary>(BinaryOperator::Add, std::move(bin->lhs), std::move(ur->operand));
                replacement = std::move(add);
                ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return;
              }
            }
            break;
          case BinaryOperator::Mul:
            if (isZero(left) || isZero(right)) { replacement = std::make_unique<IntLiteral>(0); /*std::cerr << "replace mul int zero->0\n";*/ }
            else if (isOne(left)) { replacement = std::move(bin->rhs); }
            else if (isOne(right)) { replacement = std::move(bin->lhs); }
            else {
              const IntLiteral* li = asInt(left);
              const IntLiteral* ri = asInt(right);
              if (li && li->value == -1) { auto neg = std::make_unique<Unary>(UnaryOperator::Neg, std::move(bin->rhs)); replacement = std::move(neg); }
              else if (ri && ri->value == -1) { auto neg = std::make_unique<Unary>(UnaryOperator::Neg, std::move(bin->lhs)); replacement = std::move(neg); }
            }
            if (replacement) { ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Div:
            if (isOne(right)) { replacement = std::move(bin->lhs); ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            {
              const IntLiteral* ri = asInt(right);
              if (ri && ri->value == -1) { auto neg = std::make_unique<Unary>(UnaryOperator::Neg, std::move(bin->lhs)); replacement = std::move(neg); }
            }
            if (replacement) { ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            break;
          default: break;
        }
      }
      // Float rules (includes unary-negated float literal)
      if (isFloatLike(left) || isFloatLike(right)) {
        switch (bin->op) {
          case BinaryOperator::Add:
            if (isZero(left)) { replacement = std::move(bin->rhs); }
            else if (isZero(right)) { replacement = std::move(bin->lhs); }
            else if (right->kind == NodeKind::UnaryExpr) {
              auto* ur = static_cast<Unary*>(bin->rhs.get());
              if (ur->op == UnaryOperator::Neg && ur->operand) {
                // x + (-y) -> x - y
                auto sub = std::make_unique<Binary>(BinaryOperator::Sub, std::move(bin->lhs), std::move(ur->operand));
                replacement = std::move(sub);
              }
            } else if (left->kind == NodeKind::UnaryExpr) {
              auto* ul = static_cast<Unary*>(bin->lhs.get());
              if (ul->op == UnaryOperator::Neg && ul->operand) {
                // (-x) + y -> y - x
                auto sub = std::make_unique<Binary>(BinaryOperator::Sub, std::move(bin->rhs), std::move(ul->operand));
                replacement = std::move(sub);
              }
            }
            if (replacement) { ++changes; stats["algebraic_float"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Sub:
            if (isZero(right)) { replacement = std::move(bin->lhs); ++changes; stats["algebraic_float"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            if (isZero(left)) {
              auto neg = std::make_unique<Unary>(UnaryOperator::Neg, std::move(bin->rhs));
              replacement = std::move(neg);
              ++changes; stats["algebraic_float"]++; slot = exprSlot; *slot = std::move(replacement); return;
            }
            if (right->kind == NodeKind::UnaryExpr) {
              auto* ur = static_cast<Unary*>(bin->rhs.get());
              if (ur->op == UnaryOperator::Neg && ur->operand) {
                // x - (-y) -> x + y
                auto add = std::make_unique<Binary>(BinaryOperator::Add, std::move(bin->lhs), std::move(ur->operand));
                replacement = std::move(add);
                ++changes; stats["algebraic_float"]++; slot = exprSlot; *slot = std::move(replacement); return;
              }
            }
            break;
          case BinaryOperator::Mul:
            if (isZero(left) || isZero(right)) { replacement = std::make_unique<FloatLiteral>(0.0); }
            else if (isOne(left)) { replacement = std::move(bin->rhs); }
            else if (isOne(right)) { replacement = std::move(bin->lhs); }
            else {
              const auto* lf = dynamic_cast<const FloatLiteral*>(left);
              const auto* rf = dynamic_cast<const FloatLiteral*>(right);
              if (lf && lf->value == -1.0) { auto neg = std::make_unique<Unary>(UnaryOperator::Neg, std::move(bin->rhs)); replacement = std::move(neg); }
              else if (rf && rf->value == -1.0) { auto neg = std::make_unique<Unary>(UnaryOperator::Neg, std::move(bin->lhs)); replacement = std::move(neg); }
              else if (left->kind == NodeKind::UnaryExpr) {
                const auto* ul = static_cast<const Unary*>(left);
                if (ul->op == UnaryOperator::Neg && ul->operand && ul->operand->kind == NodeKind::FloatLiteral) {
                  const auto* one = static_cast<const FloatLiteral*>(ul->operand.get());
                  if (one->value == 1.0) { auto neg = std::make_unique<Unary>(UnaryOperator::Neg, std::move(bin->rhs)); replacement = std::move(neg); }
                }
              } else if (right->kind == NodeKind::UnaryExpr) {
                const auto* ur = static_cast<const Unary*>(right);
                if (ur->op == UnaryOperator::Neg && ur->operand && ur->operand->kind == NodeKind::FloatLiteral) {
                  const auto* one = static_cast<const FloatLiteral*>(ur->operand.get());
                  if (one->value == 1.0) { auto neg = std::make_unique<Unary>(UnaryOperator::Neg, std::move(bin->lhs)); replacement = std::move(neg); }
                }
              }
            }
            if (replacement) { ++changes; stats["algebraic_float"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Div:
            if (isOne(right)) { replacement = std::move(bin->lhs); ++changes; stats["algebraic_float"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            {
              const auto* rf = dynamic_cast<const FloatLiteral*>(right);
              if (rf && rf->value == -1.0) { auto neg = std::make_unique<Unary>(UnaryOperator::Neg, std::move(bin->lhs)); replacement = std::move(neg); }
              else if (right->kind == NodeKind::UnaryExpr) {
                const auto* ur = static_cast<const Unary*>(right);
                if (ur->op == UnaryOperator::Neg && ur->operand && ur->operand->kind == NodeKind::FloatLiteral) {
                  const auto* one = static_cast<const FloatLiteral*>(ur->operand.get());
                  if (one->value == 1.0) { auto neg = std::make_unique<Unary>(UnaryOperator::Neg, std::move(bin->lhs)); replacement = std::move(neg); }
                }
              }
            }
            if (replacement) { ++changes; stats["algebraic_float"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            break;
          default: break;
        }
      }
      // Canonical x - x -> 0 (typed)
      if (bin->op == BinaryOperator::Sub) {
        const auto& canonLhs = bin->lhs->canonical();
        const auto& canonRhs = bin->rhs->canonical();
        if (canonLhs && canonRhs && *canonLhs == *canonRhs) {
          auto typeL = bin->lhs->type(); auto typeR = bin->rhs->type();
          if (typeL && typeR && *typeL == *typeR) {
            if (*typeL == TypeKind::Int) { replacement = std::make_unique<IntLiteral>(0); ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            if (*typeL == TypeKind::Float) { replacement = std::make_unique<FloatLiteral>(0.0); ++changes; stats["algebraic_float"]++; slot = exprSlot; *slot = std::move(replacement); return; }
          }
        }
      }
      // Boolean algebra (requires literal on either side)
      if (bin->op == BinaryOperator::And || bin->op == BinaryOperator::Or) {
        const BoolLiteral* lb = asBool(left);
        const BoolLiteral* rb = asBool(right);
        if (bin->op == BinaryOperator::And) {
          if (lb) { replacement = lb->value ? std::move(bin->rhs) : std::make_unique<BoolLiteral>(false); }
          else if (rb) { replacement = rb->value ? std::move(bin->lhs) : std::make_unique<BoolLiteral>(false); }
        } else { // Or
          if (lb) { replacement = lb->value ? std::make_unique<BoolLiteral>(true) : std::move(bin->rhs); }
          else if (rb) { replacement = rb->value ? std::make_unique<BoolLiteral>(true) : std::move(bin->lhs); }
        }
        if (replacement) { ++changes; stats["algebraic_bool"]++; slot = exprSlot; *slot = std::move(replacement); return; }
      }
      // Bitwise identities for ints
      if (bin->op == BinaryOperator::BitAnd || bin->op == BinaryOperator::BitOr || bin->op == BinaryOperator::BitXor) {
        const IntLiteral* li = asInt(left);
        const IntLiteral* ri = asInt(right);
        // x & 0 -> 0 ; 0 & x -> 0
        if (bin->op == BinaryOperator::BitAnd) {
          if ((li && li->value == 0) || (ri && ri->value == 0)) {
            replacement = std::make_unique<IntLiteral>(0);
          } else if ((li && li->value == -1)) {
            replacement = std::move(bin->rhs);
          } else if ((ri && ri->value == -1)) {
            replacement = std::move(bin->lhs);
          } else if (shallowEqualSimple(left, right)) {
            // x & x -> x
            replacement = std::move(bin->lhs);
          }
        }
        // x | 0 -> x ; 0 | x -> x ; x | x -> x
        else if (bin->op == BinaryOperator::BitOr) {
          if ((li && li->value == 0)) { replacement = std::move(bin->rhs); }
          else if ((ri && ri->value == 0)) { replacement = std::move(bin->lhs); }
          else if ((li && li->value == -1) || (ri && ri->value == -1)) { replacement = std::make_unique<IntLiteral>(-1); }
          else if (shallowEqualSimple(left, right)) { replacement = std::move(bin->lhs); }
        }
        // x ^ 0 -> x ; 0 ^ x -> x ; x ^ x -> 0
        else if (bin->op == BinaryOperator::BitXor) {
          if ((li && li->value == 0)) { replacement = std::move(bin->rhs); }
          else if ((ri && ri->value == 0)) { replacement = std::move(bin->lhs); }
          else if (li && li->value == -1) { replacement = std::make_unique<Unary>(UnaryOperator::BitNot, std::move(bin->rhs)); }
          else if (ri && ri->value == -1) { replacement = std::make_unique<Unary>(UnaryOperator::BitNot, std::move(bin->lhs)); }
          else if (shallowEqualSimple(left, right)) { replacement = std::make_unique<IntLiteral>(0); }
        }
        if (replacement) { ++changes; stats["algebraic_bitwise"]++; slot = exprSlot; *slot = std::move(replacement); return; }
      }
      // Shifts by zero: x << 0 -> x; x >> 0 -> x
      if (bin->op == BinaryOperator::LShift || bin->op == BinaryOperator::RShift) {
        const IntLiteral* ri = asInt(right);
        if (ri && ri->value == 0) { replacement = std::move(bin->lhs); ++changes; stats["algebraic_shift"]++; slot = exprSlot; *slot = std::move(replacement); return; }
        const IntLiteral* li = asInt(left);
        if (li && li->value == 0) { replacement = std::make_unique<IntLiteral>(0); ++changes; stats["algebraic_shift"]++; slot = exprSlot; *slot = std::move(replacement); return; }
      }
      // Exponentiation identity: x ** 1 -> x (safe for int/float)
      if (bin->op == BinaryOperator::Pow) {
        if (isOne(right)) { replacement = std::move(bin->lhs); ++changes; stats["algebraic_pow"]++; slot = exprSlot; *slot = std::move(replacement); return; }
        // 1 ** x -> 1; 1.0 ** x -> 1.0
        if (left->kind == NodeKind::IntLiteral) {
          const auto* li2 = static_cast<const IntLiteral*>(left);
          if (li2->value == 1) { replacement = std::make_unique<IntLiteral>(1); ++changes; stats["algebraic_pow"]++; slot = exprSlot; *slot = std::move(replacement); return; }
        }
        if (left->kind == NodeKind::FloatLiteral) {
          const auto* lf2 = static_cast<const FloatLiteral*>(left);
          if (lf2->value == 1.0) { replacement = std::make_unique<FloatLiteral>(1.0); ++changes; stats["algebraic_pow"]++; slot = exprSlot; *slot = std::move(replacement); return; }
        }
        // x ** 0 -> 1 (typed)
        if (right->kind == NodeKind::IntLiteral) {
          const auto* ri0 = static_cast<const IntLiteral*>(right);
          if (ri0->value == 0) {
            if (left->kind == NodeKind::FloatLiteral) { replacement = std::make_unique<FloatLiteral>(1.0); }
            else { replacement = std::make_unique<IntLiteral>(1); }
            ++changes; stats["algebraic_pow"]++; slot = exprSlot; *slot = std::move(replacement); return; }
        }
        if (right->kind == NodeKind::FloatLiteral) {
          const auto* rf0 = static_cast<const FloatLiteral*>(right);
          if (rf0->value == 0.0) { replacement = std::make_unique<FloatLiteral>(1.0); ++changes; stats["algebraic_pow"]++; slot = exprSlot; *slot = std::move(replacement); return; }
        }
      }
      // FloorDiv/Mod identities (int only)
      if (bin->op == BinaryOperator::FloorDiv || bin->op == BinaryOperator::Mod) {
        const IntLiteral* ri = asInt(right);
        const IntLiteral* li = asInt(left);
        if (bin->op == BinaryOperator::FloorDiv && ri && ri->value == 1) {
          replacement = std::move(bin->lhs); ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return;
        }
        if (bin->op == BinaryOperator::Mod) {
          if (ri && ri->value == 1) { replacement = std::make_unique<IntLiteral>(0); ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return; }
          if (li && li->value == 0) { replacement = std::make_unique<IntLiteral>(0); ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return; }
        }
      }
    }
  }

  void visit(const Unary& unary) override {
    (void)unary;
    auto* unaryNode = static_cast<Unary*>(slot->get());
    auto* exprSlot = slot;
    if (unaryNode->op == UnaryOperator::Neg && unaryNode->operand && unaryNode->operand->kind == NodeKind::UnaryExpr) {
      auto* inner = static_cast<Unary*>(unaryNode->operand.get());
      if (inner->op == UnaryOperator::Neg && inner->operand) {
        slot = exprSlot; *slot = std::move(inner->operand); ++changes; stats["double_neg"]++;
        return;
      }
    }
    if (unaryNode->op == UnaryOperator::Not && unaryNode->operand && unaryNode->operand->kind == NodeKind::UnaryExpr) {
      auto* inner = static_cast<Unary*>(unaryNode->operand.get());
      if (inner->op == UnaryOperator::Not && inner->operand) {
        slot = exprSlot; *slot = std::move(inner->operand); ++changes; stats["double_not"]++;
        return;
      }
    }
    rewrite(unaryNode->operand);
  }

  void visit(const Call& callExpr) override {
    (void)callExpr;
    auto* call = static_cast<Call*>(slot->get());
    if (call->callee) { rewrite(call->callee); }
    for (auto& arg : call->args) { rewrite(arg); }
  }

  // Stmt visitors
  void visit(const AssignStmt& assignStmt) override {
    (void)assignStmt;
    auto* assignNode = static_cast<AssignStmt*>(stmtSlot->get());
    rewrite(assignNode->value);
  }
  void visit(const ReturnStmt& retStmt) override {
    (void)retStmt;
    auto* retNode = static_cast<ReturnStmt*>(stmtSlot->get());
    // debug (removed)
    rewrite(retNode->value);
    // debug (removed)
  }
  void visit(const IfStmt& ifStmt) override {
    (void)ifStmt;
    auto* ifNode = static_cast<IfStmt*>(stmtSlot->get());
    rewrite(ifNode->cond);
    walkBlock(ifNode->thenBody);
    walkBlock(ifNode->elseBody);
  }

  // No-ops for other nodes
  void visit(const Module& module) override { (void)module; }
  void visit(const FunctionDef& functionDef) override { (void)functionDef; }
  void visit(const ExprStmt& exprStmt) override { (void)exprStmt; }
  void visit(const IntLiteral& intLiteral) override { (void)intLiteral; }
  void visit(const BoolLiteral& boolLiteral) override { (void)boolLiteral; }
  void visit(const FloatLiteral& floatLiteral) override { (void)floatLiteral; }
  void visit(const StringLiteral& stringLiteral) override { (void)stringLiteral; }
  void visit(const NoneLiteral& noneLiteral) override { (void)noneLiteral; }
  void visit(const Name& name) override { (void)name; }
  void visit(const TupleLiteral& tupleLiteral) override { (void)tupleLiteral; }
  void visit(const ListLiteral& listLiteral) override { (void)listLiteral; }
  void visit(const ObjectLiteral& objectLiteral) override { (void)objectLiteral; }
};

} // namespace


size_t AlgebraicSimplify::run(Module& module) {
  size_t changes = 0;
  stats_.clear();
  SimplifyVisitor vis(stats_, changes);
  for (auto& func : module.functions) { vis.walkBlock(func->body); }
  return changes;
}

} // namespace pycc::opt
