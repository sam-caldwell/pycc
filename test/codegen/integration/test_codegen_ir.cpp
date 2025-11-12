/***
 * Name: test_codegen_ir
 * Purpose: Integration tests verifying LLVM IR patterns for assignment and call.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, AssignLiteral_ReturnName) {
  const char* src =
      "def main() -> int:\n"
      "  x = 5\n"
      "  return x\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // With variable model: alloca/store/load pattern present
  ASSERT_NE(ir.find("alloca i32"), std::string::npos);
  ASSERT_NE(ir.find("store i32 5"), std::string::npos);
  ASSERT_NE(ir.find("load i32, ptr"), std::string::npos);
  ASSERT_NE(ir.find("ret i32"), std::string::npos);
}

TEST(CodegenIR, CallNoArgs) {
  const char* src =
      "def add() -> int:\n"
      "  return 5\n"
      "def main() -> int:\n"
      "  x = add()\n"
      "  return x\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // Expect a call instruction and using it in ret
  ASSERT_NE(ir.find("call i32 @add()"), std::string::npos);
}

TEST(CodegenIR, CallWithArgs_Supported) {
  const char* src =
      "def add(a: int, b: int) -> int:\n"
      "  return a\n"
      "def main() -> int:\n"
      "  x = add(2, 3)\n"
      "  return x\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("define i32 @add(i32 %a, i32 %b)"), std::string::npos);
  ASSERT_NE(ir.find("call i32 @add(i32 2, i32 3)"), std::string::npos);
}

TEST(CodegenIR, CallArityMismatch_Throws) {
  const char* src =
      "def add(a: int, b: int) -> int:\n"
      "  return a\n"
      "def main() -> int:\n"
      "  x = add(1)\n"
      "  return x\n";
  auto mod = parseSrc(src);
  EXPECT_THROW({ (void)codegen::Codegen::generateIR(*mod); }, std::exception);
}

TEST(CodegenIR, ArithmeticPrecedenceAndParens) {
  const char* src =
      "def main() -> int:\n"
      "  y = (2 + 3) * 4\n"
      "  return y\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  auto posAdd = ir.find("add i32 2, 3");
  auto posMul = ir.find("mul i32 %t0, 4");
  ASSERT_NE(posAdd, std::string::npos);
  ASSERT_NE(posMul, std::string::npos);
  ASSERT_LT(posAdd, posMul);
  ASSERT_NE(ir.find("alloca i32"), std::string::npos);
  ASSERT_NE(ir.find("store i32"), std::string::npos);
  ASSERT_NE(ir.find("load i32, ptr"), std::string::npos);
}

TEST(CodegenIR, ParamAllocaStoreAndLoad) {
  const char* src =
      "def add(a: int, b: int) -> int:\n"
      "  c = a + b\n"
      "  return c\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("define i32 @add(i32 %a, i32 %b)"), std::string::npos);
  ASSERT_NE(ir.find("%a.addr = alloca i32"), std::string::npos);
  ASSERT_NE(ir.find("store i32 %a, ptr %a.addr"), std::string::npos);
  ASSERT_NE(ir.find("%b.addr = alloca i32"), std::string::npos);
  ASSERT_NE(ir.find("store i32 %b, ptr %b.addr"), std::string::npos);
  ASSERT_NE(ir.find("add i32"), std::string::npos);
}

TEST(CodegenIR, UnaryMinusAndIfElse) {
  const char* src =
      "def main() -> int:\n"
      "  if True:\n"
      "    return -5\n"
      "  else:\n"
      "    x = 3\n"
      "    return x * 2\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // Expect direct branch on i1 true
  ASSERT_NE(ir.find("br i1 true"), std::string::npos);
  ASSERT_NE(ir.find("ret i32 -5"), std::string::npos);
  ASSERT_NE(ir.find("mul i32"), std::string::npos);
}

TEST(CodegenIR, ComparisonsNeLeGe) {
  const char* src =
      "def main() -> int:\n"
      "  a = (2 != 3)\n"
      "  b = (2 <= 3)\n"
      "  c = (3 >= 3)\n"
      "  return a\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("icmp ne i32 2, 3"), std::string::npos);
  ASSERT_NE(ir.find("icmp sle i32 2, 3"), std::string::npos);
  ASSERT_NE(ir.find("icmp sge i32 3, 3"), std::string::npos);
}

TEST(CodegenIR, LogicalAndOrNot) {
  const char* src =
      "def main() -> int:\n"
      "  x = True and False\n"
      "  y = not x\n"
      "  z = x or y\n"
      "  return z\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // not compiled via xor i1 ... , true
  ASSERT_NE(ir.find("xor i1"), std::string::npos);
  // short-circuit should produce phi
  ASSERT_NE(ir.find("phi i1"), std::string::npos);
}

TEST(CodegenIR, FloatArithmeticAndComparisons) {
  const char* src =
      "def addf(a: float, b: float) -> float:\n"
      "  return a + b\n"
      "def main() -> int:\n"
      "  x = addf(1.5, 2.25)\n"
      "  y = 3.0 * 2.0\n"
      "  z = (y > x)\n"
      "  return z\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("define double @addf(double %a, double %b)"), std::string::npos);
  ASSERT_NE(ir.find("fadd double"), std::string::npos);
  ASSERT_NE(ir.find("fmul double 3"), std::string::npos);
  ASSERT_NE(ir.find("fcmp ogt double"), std::string::npos);
}

TEST(CodegenIR, ShortCircuitAndOrPhi) {
  const char* src =
      "def main() -> bool:\n"
      "  a = True\n"
      "  b = False\n"
      "  c = a and b\n"
      "  d = a or b\n"
      "  return d\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("and.end"), std::string::npos);
  ASSERT_NE(ir.find("or.end"), std::string::npos);
  ASSERT_NE(ir.find("phi i1"), std::string::npos);
}
