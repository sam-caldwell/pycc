/***
 * Name: test_parser_imports
 * Purpose: Ensure import and from-import statements cover dotted names, aliases, relative levels, star import, and parenthesized lists.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/Import.h"
#include "ast/ImportFrom.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "imp.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserImports, ImportSimpleAndAliases) {
  const char* src =
      "def main() -> int:\n"
      "  import os, sys as s, pkg.sub as ps\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::Import);
  const auto* im = static_cast<const ast::Import*>(fn.body[0].get());
  ASSERT_EQ(im->names.size(), 3u);
  EXPECT_EQ(im->names[0].name, std::string("os"));
  EXPECT_EQ(im->names[1].name, std::string("sys"));
  EXPECT_EQ(im->names[1].asname, std::string("s"));
  EXPECT_EQ(im->names[2].name, std::string("pkg.sub"));
  EXPECT_EQ(im->names[2].asname, std::string("ps"));
}

TEST(ParserImports, FromRelativeAndStar) {
  const char* src =
      "def main() -> int:\n"
      "  from ..pkg.sub import a as b, c\n"
      "  from . import x\n"
      "  from pkg import *\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  {
    ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::ImportFrom);
    const auto* fr = static_cast<const ast::ImportFrom*>(fn.body[0].get());
    EXPECT_EQ(fr->level, 2);
    EXPECT_EQ(fr->module, std::string("pkg.sub"));
    ASSERT_EQ(fr->names.size(), 2u);
    EXPECT_EQ(fr->names[0].name, std::string("a"));
    EXPECT_EQ(fr->names[0].asname, std::string("b"));
    EXPECT_EQ(fr->names[1].name, std::string("c"));
  }
  {
    const auto* fr = static_cast<const ast::ImportFrom*>(fn.body[1].get());
    EXPECT_EQ(fr->level, 1);
    EXPECT_EQ(fr->module, std::string(""));
    ASSERT_EQ(fr->names.size(), 1u);
    EXPECT_EQ(fr->names[0].name, std::string("x"));
  }
  {
    const auto* fr = static_cast<const ast::ImportFrom*>(fn.body[2].get());
    EXPECT_EQ(fr->level, 0);
    EXPECT_EQ(fr->module, std::string("pkg"));
    ASSERT_TRUE(fr->names.empty());
  }
}

TEST(ParserImports, FromParenList) {
  const char* src =
      "def main() -> int:\n"
      "  from pkg import (a, b as c)\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::ImportFrom);
  const auto* fr = static_cast<const ast::ImportFrom*>(fn.body[0].get());
  ASSERT_EQ(fr->names.size(), 2u);
  EXPECT_EQ(fr->names[0].name, std::string("a"));
  EXPECT_EQ(fr->names[1].name, std::string("b"));
  EXPECT_EQ(fr->names[1].asname, std::string("c"));
}

