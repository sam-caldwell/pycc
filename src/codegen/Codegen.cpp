/***
 * Name: pycc::codegen::Codegen (impl)
 * Purpose: Emit LLVM IR and drive clang to produce desired artifacts.
 */
#include "codegen/Codegen.h"
#include "ast/AssignStmt.h"
#include "ast/Binary.h"
#include "ast/BoolLiteral.h"
#include "ast/Call.h"
#include "ast/ExprStmt.h"
#include "ast/FloatLiteral.h"
#include "ast/FunctionDef.h"
#include "ast/IfStmt.h"
#include "ast/WhileStmt.h"
#include "ast/ForStmt.h"
#include "ast/TryStmt.h"
#include "ast/AugAssignStmt.h"
#include "ast/BreakStmt.h"
#include "ast/ContinueStmt.h"
#include "ast/Attribute.h"
#include "ast/DictLiteral.h"
#include "ast/NonlocalStmt.h"
#include "ast/Subscript.h"
#include "ast/IntLiteral.h"
#include "ast/ListLiteral.h"
#include "ast/Module.h"
#include "ast/Name.h"
#include "ast/NodeKind.h"
#include "ast/NoneLiteral.h"
#include "ast/ObjectLiteral.h"
#include "ast/ReturnStmt.h"
#include "ast/StringLiteral.h"
#include "ast/TupleLiteral.h"
#include "ast/TypeKind.h"
#include "ast/Unary.h"
#include "ast/VisitorBase.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <ios>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pycc::codegen {

static std::string changeExt(const std::string& base, const std::string& ext) {
  // Replace extension if present, else append
  auto pos = base.find_last_of('.');
  if (pos == std::string::npos) { return base + ext; }
  return base.substr(0, pos) + ext;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::string Codegen::emit(const ast::Module& mod, const std::string& outBase,
                          bool assemblyOnly, bool compileOnly, EmitResult& result) const {
  // 1) Generate IR
  std::string irText;
  try {
    irText = generateIR(mod);
  } catch (const std::exception& ex) { // NOLINT(bugprone-empty-catch)
    return std::string("codegen: ") + ex.what();
  }
  // Prepend original source file content as IR comments when available
  if (const char* srcPath = std::getenv("PYCC_SOURCE_PATH"); srcPath && *srcPath) {
    std::ifstream inSrc(srcPath);
    if (inSrc) {
      std::ostringstream commented;
      commented << "; ---- PY SOURCE: " << srcPath << " ----\n";
      std::string line;
      while (std::getline(inSrc, line)) { commented << "; " << line << "\n"; }
      commented << "; ---- END PY SOURCE ----\n\n";
      commented << irText;
      irText = commented.str();
    }
  }
  result.llPath = emitLL_ ? changeExt(outBase, ".ll") : std::string();
  if (emitLL_) {
    std::ofstream outFile(result.llPath, std::ios::binary);
    if (!outFile) { return "failed to write .ll to " + result.llPath; }
    outFile << irText;
  }

  // Optionally run LLVM IR pass plugin to elide redundant GC barriers on stack writes.
  // This uses the externally built pass plugin and 'opt' tool.
#ifdef PYCC_LLVM_PASS_PLUGIN_PATH
  if (emitLL_) {
    if (const char* k = std::getenv("PYCC_OPT_ELIDE_GCBARRIER"); k && *k) {
      std::string passPluginPath = PYCC_LLVM_PASS_PLUGIN_PATH;
      // Allow overriding plugin path via environment if desired
      if (const char* p = std::getenv("PYCC_LLVM_PASS_PLUGIN_PATH")) { passPluginPath = p; }
      // Produce an optimized .ll alongside the original for readability and debugging
      const std::string optLL = changeExt(outBase, ".opt.ll");
      std::ostringstream optCmd;
      optCmd << "opt -load-pass-plugin \"" << passPluginPath << "\" -passes=\"function(pycc-elide-gcbarrier)\" -S \"" << result.llPath << "\" -o \"" << optLL << "\"";
      std::string err;
      if (runCmd(optCmd.str(), err)) {
        // Use the optimized IR for subsequent compile stages
        result.llPath = optLL;
      } else {
        // Best-effort: if opt fails, continue with unoptimized IR
        (void)err;
      }
    }
  }
#else
  // Fallback: allow fully environment-driven invocation when plugin path macro isn't compiled in.
  if (emitLL_) {
    const char* envEnable = std::getenv("PYCC_OPT_ELIDE_GCBARRIER");
    const char* envPlugin = std::getenv("PYCC_LLVM_PASS_PLUGIN_PATH");
    if (envEnable && *envEnable && envPlugin && *envPlugin) {
      const std::string optLL = changeExt(outBase, ".opt.ll");
      std::ostringstream optCmd;
      optCmd << "opt -load-pass-plugin \"" << envPlugin << "\" -passes=\"function(pycc-elide-gcbarrier)\" -S \"" << result.llPath << "\" -o \"" << optLL << "\"";
      std::string err;
      if (runCmd(optCmd.str(), err)) { result.llPath = optLL; }
      else { (void)err; }
    }
  }
#endif

  // 2) Produce assembly/object/binary using clang
  std::string err;
  if (assemblyOnly) {
    // clang -S in.ll -o <out>
    result.asmPath = outBase;
    std::ostringstream cmd;
    cmd << "clang -S " << (emitLL_ ? result.llPath : std::string("-x ir -")) << " -o " << result.asmPath;
    if (!emitLL_) {
      // Feed IR via stdin if we didn't emit file (not used in milestone 1)
    }
    if (!runCmd(cmd.str(), err)) { return err; }
    return {};
  }

  // Compile to object
  result.objPath = compileOnly ? outBase : changeExt(outBase, ".o");
  {
    std::ostringstream cmd;
    cmd << "clang -c " << (emitLL_ ? result.llPath : std::string("-x ir -")) << " -o " << result.objPath;
    if (std::getenv("PYCC_COVERAGE") || std::getenv("LLVM_PROFILE_FILE")) {
      cmd << " -fprofile-instr-generate -fcoverage-mapping";
    }
    if (!runCmd(cmd.str(), err)) { return err; }
  }

  if (compileOnly) {
    // No link
    return {};
  }

  // Link to binary (use C++ linker to satisfy runtime deps)
  result.binPath = outBase; // user chose exact file
  {
    std::ostringstream cmd;
    cmd << "clang++ " << result.objPath << ' ';
#ifdef PYCC_RUNTIME_LIB_PATH
    cmd << PYCC_RUNTIME_LIB_PATH << ' ';
#endif
    cmd << "-pthread -o " << result.binPath;
    if (std::getenv("PYCC_COVERAGE") || std::getenv("LLVM_PROFILE_FILE")) {
      cmd << " -fprofile-instr-generate -fcoverage-mapping";
    }
    if (!runCmd(cmd.str(), err)) { return err; }
  }

  // Optionally emit ASM if enabled (generate from IR for readability)
  if (emitASM_) {
    result.asmPath = changeExt(outBase, ".asm");
    std::ostringstream cmd;
    cmd << "clang -S " << result.llPath << " -o " << result.asmPath;
    runCmd(cmd.str(), err); // Best-effort
  }

  return {};
}

// NOLINTBEGIN
// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
std::string Codegen::generateIR(const ast::Module& mod) {
  std::ostringstream irStream;
  irStream << "; ModuleID = 'pycc_module'\n";
  irStream << "source_filename = \"pycc\"\n\n";
  // Debug info scaffold: track subprograms and per-instruction locations; emit metadata at end.
  struct DebugSub { std::string name; int id; };
  std::vector<DebugSub> dbgSubs;
  struct DebugLoc { int id; int line; int col; int scope; };
  std::vector<DebugLoc> dbgLocs;
  std::unordered_map<unsigned long long, int> dbgLocKeyToId; // key = (scope<<32) ^ (line<<16|col)
  int nextDbgId = 2; // !0 = CU, !1 = DIFile
  // Basic DI types and DIExpression
  const int diIntId = nextDbgId++;
  const int diBoolId = nextDbgId++;
  const int diDoubleId = nextDbgId++;
  const int diPtrId = nextDbgId++;
  const int diExprId = nextDbgId++;
  struct DbgVar { int id; std::string name; int scope; int line; int col; int typeId; int argIndex; bool isParam; };
  std::vector<DbgVar> dbgVars;
  // GC barrier declaration for pointer writes (C ABI)
  irStream << "declare void @pycc_gc_write_barrier(ptr, ptr)\n";
  // Future aggregate runtime calls (scaffold)
  irStream << "declare ptr @pycc_list_new(i64)\n";
  irStream << "declare void @pycc_list_push(ptr, ptr)\n";
  irStream << "declare i64 @pycc_list_len(ptr)\n";
  irStream << "declare ptr @pycc_list_get(ptr, i64)\n";
  irStream << "declare void @pycc_list_set(ptr, i64, ptr)\n";
  irStream << "declare ptr @pycc_object_new(i64)\n";
  irStream << "declare void @pycc_object_set(ptr, i64, ptr)\n";
  irStream << "declare ptr @pycc_object_get(ptr, i64)\n\n";
  // Dict and attribute helpers
  irStream << "declare ptr @pycc_dict_new(i64)\n";
  irStream << "declare void @pycc_dict_set(ptr, ptr, ptr)\n";
  irStream << "declare ptr @pycc_dict_get(ptr, ptr)\n";
  irStream << "declare i64 @pycc_dict_len(ptr)\n";
  irStream << "declare void @pycc_object_set_attr(ptr, ptr, ptr)\n";
  irStream << "declare ptr @pycc_object_get_attr(ptr, ptr)\n";
  irStream << "declare ptr @pycc_string_new(ptr, i64)\n\n";
  // Debug intrinsics for variable locations and GC roots
  irStream << "declare void @llvm.dbg.declare(metadata, metadata, metadata)\n\n";
  irStream << "declare void @llvm.gcroot(ptr, ptr)\n\n";
  // C++ EH personality (Phase 1 EH)
  irStream << "declare i32 @__gxx_personality_v0(...)\n\n";
  // Boxing wrappers for primitives
  irStream << "declare ptr @pycc_box_int(i64)\n";
  irStream << "declare ptr @pycc_box_float(double)\n";
  irStream << "declare ptr @pycc_box_bool(i1)\n\n";
  // String operations
  irStream << "declare ptr @pycc_string_concat(ptr, ptr)\n";
  irStream << "declare ptr @pycc_string_slice(ptr, i64, i64)\n\n";
  irStream << "declare i1 @pycc_string_contains(ptr, ptr)\n";
  irStream << "declare ptr @pycc_string_repeat(ptr, i64)\n\n";
  // Selected LLVM intrinsics used by codegen
  irStream << "declare double @llvm.powi.f64(double, i32)\n";
  irStream << "declare double @llvm.pow.f64(double, double)\n";
  irStream << "declare double @llvm.floor.f64(double)\n\n";
  // Exceptions and string utils (C ABI)
  irStream << "declare void @pycc_rt_raise(ptr, ptr)\n";
  irStream << "declare i1 @pycc_rt_has_exception()\n";
  irStream << "declare ptr @pycc_rt_current_exception()\n";
  irStream << "declare void @pycc_rt_clear_exception()\n";
  irStream << "declare ptr @pycc_rt_exception_type(ptr)\n";
  irStream << "declare ptr @pycc_rt_exception_message(ptr)\n";
  irStream << "declare i1 @pycc_string_eq(ptr, ptr)\n\n";
  // Dict iteration helpers
  irStream << "declare ptr @pycc_dict_iter_new(ptr)\n";
  irStream << "declare ptr @pycc_dict_iter_next(ptr)\n\n";

  // Pre-scan functions to gather signatures
  struct Sig { ast::TypeKind ret{ast::TypeKind::NoneType}; std::vector<ast::TypeKind> params; };
  std::unordered_map<std::string, Sig> sigs;
  for (const auto& funcSig : mod.functions) {
    Sig sig; sig.ret = funcSig->returnType;
    for (const auto& param : funcSig->params) { sig.params.push_back(param.type); }
    sigs[funcSig->name] = std::move(sig);
  }

  // Lightweight interprocedural summary: functions that consistently return the same parameter index (top-level only)
  std::unordered_map<std::string, int> retParamIdxs; // func -> param index
  struct ReturnParamIdxScan : public ast::VisitorBase {
    const ast::FunctionDef* fn{nullptr};
    int retIdx{-1}; bool hasReturn{false}; bool consistent{true};
    void visit(const ast::ReturnStmt& r) override {
      if (!consistent) { return; }
      hasReturn = true;
      if (!(r.value && r.value->kind == ast::NodeKind::Name)) { consistent = false; return; }
      const auto* n = static_cast<const ast::Name*>(r.value.get());
      int idxFound = -1;
      for (size_t i = 0; i < fn->params.size(); ++i) { if (fn->params[i].name == n->id) { idxFound = static_cast<int>(i); break; } }
      if (idxFound < 0) { consistent = false; return; }
      if (retIdx < 0) retIdx = idxFound; else if (retIdx != idxFound) { consistent = false; }
    }
    // No-op for other nodes (we only scan top-level statements)
    void visit(const ast::Module&) override {}
    void visit(const ast::FunctionDef&) override {}
    void visit(const ast::AssignStmt&) override {}
    void visit(const ast::IfStmt&) override {}
    void visit(const ast::ExprStmt&) override {}
    void visit(const ast::IntLiteral&) override {}
    void visit(const ast::BoolLiteral&) override {}
    void visit(const ast::FloatLiteral&) override {}
    void visit(const ast::StringLiteral&) override {}
    void visit(const ast::NoneLiteral&) override {}
    void visit(const ast::Name&) override {}
    void visit(const ast::Call&) override {}
    void visit(const ast::Binary&) override {}
    void visit(const ast::Unary&) override {}
    void visit(const ast::TupleLiteral&) override {}
    void visit(const ast::ListLiteral&) override {}
    void visit(const ast::ObjectLiteral&) override {}
  };
  for (const auto& f : mod.functions) {
    ReturnParamIdxScan scan; scan.fn = f.get();
    for (const auto& st : f->body) { st->accept(scan); if (!scan.consistent) break; }
    if (scan.hasReturn && scan.consistent && scan.retIdx >= 0) { retParamIdxs[f->name] = scan.retIdx; }
  }

  // Collect string literals to emit as global constants
  auto hash64 = [](const std::string& str) {
    constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
    constexpr uint64_t kFnvPrime = 1099511628211ULL;
    uint64_t hash = kFnvOffsetBasis;
    for (const unsigned char ch : str) { hash ^= ch; hash *= kFnvPrime; }
    return hash;
  };
  auto escapeIR = [](const std::string& text) {
    std::ostringstream out;
    constexpr int kPrintableMin = 32;
    constexpr int kPrintableMaxExclusive = 127;
    for (const unsigned char c : text) {
      switch (c) {
        case '\\': out << "\\5C"; break; // backslash
        case '"': out << "\\22"; break;  // quote
        case '\n': out << "\\0A"; break;
        case '\r': out << "\\0D"; break;
        case '\t': out << "\\09"; break;
        default:
          if (c >= kPrintableMin && c < kPrintableMaxExclusive) { out << static_cast<char>(c); }
          else { out << "\\"; out << std::hex; out.width(2); out.fill('0'); out << static_cast<int>(c); out << std::dec; }
      }
    }
    return out.str();
  };

  std::unordered_map<std::string, std::pair<std::string, size_t>> strGlobals; // content -> (name, N)
  struct StrCollector : public ast::VisitorBase {
    std::unordered_map<std::string, std::pair<std::string, size_t>>* out;
    std::function<uint64_t(const std::string&)> hasher;
    StrCollector(std::unordered_map<std::string, std::pair<std::string, size_t>>* mapPtr,
                 std::function<uint64_t(const std::string&)> h)
      : out(mapPtr), hasher(std::move(h)) {}
    void add(const std::string& str) const {
      if (out->contains(str)) { return; }
      std::ostringstream nm; nm << ".str_" << std::hex << hasher(str);
      out->emplace(str, std::make_pair(nm.str(), str.size() + 1));
    }
    void visit(const ast::Attribute& attr) override {
      add(attr.attr);
      if (attr.value) { attr.value->accept(*this); }
    }
    void visit(const ast::Module& module) override {
      for (const auto& func : module.functions) { func->accept(*this); }
    }
    void visit(const ast::FunctionDef& func) override {
      for (const auto& stmt : func.body) { stmt->accept(*this); }
    }
    void visit(const ast::ReturnStmt& ret) override { if (ret.value) { ret.value->accept(*this); } }
    void visit(const ast::AssignStmt& asg) override { if (asg.value) { asg.value->accept(*this); } }
    void visit(const ast::IfStmt& iff) override {
      if (iff.cond) { iff.cond->accept(*this); }
      for (const auto& stmtThen : iff.thenBody) { stmtThen->accept(*this); }
      for (const auto& stmtElse : iff.elseBody) { stmtElse->accept(*this); }
    }
    void visit(const ast::ExprStmt& expr) override { if (expr.value) { expr.value->accept(*this); } }
    void visit(const ast::RaiseStmt& rs) override {
      // collect type name from raise Type("msg") or raise Type
      if (!rs.exc) return;
      if (rs.exc->kind == ast::NodeKind::Name) {
        const auto* n = static_cast<const ast::Name*>(rs.exc.get());
        add(n->id);
      } else if (rs.exc->kind == ast::NodeKind::Call) {
        const auto* c = static_cast<const ast::Call*>(rs.exc.get());
        if (c->callee && c->callee->kind == ast::NodeKind::Name) {
          const auto* cn = static_cast<const ast::Name*>(c->callee.get());
          add(cn->id);
        }
      }
    }
    void visit(const ast::Literal<long long, ast::NodeKind::IntLiteral>& litInt) override { (void)litInt; }
    void visit(const ast::Literal<bool, ast::NodeKind::BoolLiteral>& litBool) override { (void)litBool; }
    void visit(const ast::Literal<double, ast::NodeKind::FloatLiteral>& litFloat) override { (void)litFloat; }
    void visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral>& litStr) override { add(litStr.value); }
    void visit(const ast::NoneLiteral& none) override { (void)none; }
    void visit(const ast::Name& id) override { (void)id; }
    void visit(const ast::Call& call) override {
      if (call.callee) { call.callee->accept(*this); }
      for (const auto& arg : call.args) { if (arg) { arg->accept(*this); } }
    }
    void visit(const ast::Binary& bin) override {
      if (bin.lhs) { bin.lhs->accept(*this); }
      if (bin.rhs) { bin.rhs->accept(*this); }
    }
    void visit(const ast::Unary& unary) override { if (unary.operand) { unary.operand->accept(*this); } }
    void visit(const ast::TupleLiteral& tuple) override { for (const auto& elem : tuple.elements) { if (elem) { elem->accept(*this); } } }
    void visit(const ast::ListLiteral& list) override { for (const auto& elem : list.elements) { if (elem) { elem->accept(*this); } } }
    void visit(const ast::ObjectLiteral& obj) override { for (const auto& fld : obj.fields) { if (fld) { fld->accept(*this); } } }
    void visit(const ast::TryStmt& ts) override {
      for (const auto& st : ts.body) { if (st) st->accept(*this); }
      for (const auto& h : ts.handlers) {
        if (!h) continue;
        if (h->type && h->type->kind == ast::NodeKind::Name) {
          const auto* n = static_cast<const ast::Name*>(h->type.get()); add(n->id);
        } else if (h->type && h->type->kind == ast::NodeKind::TupleLiteral) {
          const auto* tp = static_cast<const ast::TupleLiteral*>(h->type.get());
          for (const auto& el : tp->elements) { if (el && el->kind == ast::NodeKind::Name) { add(static_cast<const ast::Name*>(el.get())->id); } }
        }
      }
      for (const auto& st : ts.orelse) { if (st) st->accept(*this); }
      for (const auto& st : ts.finalbody) { if (st) st->accept(*this); }
    }
  };

  {
    StrCollector collector{&strGlobals, hash64};
    mod.accept(collector);
  }

  // Ensure common exception strings exist for lowering raise/handlers
  auto ensureStr = [&](const std::string& s) {
    if (!strGlobals.contains(s)) {
      std::ostringstream nm; nm << ".str_" << std::hex << hash64(s);
      strGlobals.emplace(s, std::make_pair(nm.str(), s.size() + 1));
    }
  };
  ensureStr("Exception");
  ensureStr("");

  // Emit global string constants
  for (const auto& [content, info] : strGlobals) {
    const auto& name = info.first; const size_t count = info.second; // includes NUL
    irStream << "@" << name << " = private unnamed_addr constant [" << count << " x i8] c\"" << escapeIR(content) << "\\00\", align 1\n";
  }

  // Declare runtime helpers and C interop
  irStream << "declare i64 @strlen(ptr)\n";
  irStream << "declare i64 @pycc_string_len(ptr)\n\n";

  for (const auto& func : mod.functions) {
    auto typeStr = [&](ast::TypeKind t) -> const char* {
      switch (t) {
        case ast::TypeKind::Int: return "i32";
        case ast::TypeKind::Bool: return "i1";
        case ast::TypeKind::Float: return "double";
        case ast::TypeKind::Str: return "ptr";
        default: return nullptr;
      }
    };
    const char* retStr = typeStr(func->returnType);
    std::string retStructTy;
    std::vector<std::string> tupleElemTys;
    if (retStr == nullptr) {
      if (func->returnType == ast::TypeKind::Tuple) {
        // Analyze function body for a tuple literal return to infer element types (top-level only)
        struct TupleReturnFinder : public ast::VisitorBase {
          const ast::TupleLiteral* found{nullptr};
          void visit(const ast::ReturnStmt& r) override {
            if (!found && r.value && r.value->kind == ast::NodeKind::TupleLiteral) { found = static_cast<const ast::TupleLiteral*>(r.value.get()); }
          }
          // no-ops for other nodes
          void visit(const ast::Module&) override {}
          void visit(const ast::FunctionDef&) override {}
          void visit(const ast::AssignStmt&) override {}
          void visit(const ast::IfStmt&) override {}
          void visit(const ast::ExprStmt&) override {}
          void visit(const ast::IntLiteral&) override {}
          void visit(const ast::BoolLiteral&) override {}
          void visit(const ast::FloatLiteral&) override {}
          void visit(const ast::StringLiteral&) override {}
          void visit(const ast::NoneLiteral&) override {}
          void visit(const ast::Name&) override {}
          void visit(const ast::Call&) override {}
          void visit(const ast::Binary&) override {}
          void visit(const ast::Unary&) override {}
          void visit(const ast::TupleLiteral&) override {}
          void visit(const ast::ListLiteral&) override {}
          void visit(const ast::ObjectLiteral&) override {}
        } finder;
        for (const auto& st : func->body) { st->accept(finder); if (finder.found) break; }
        size_t arity = (finder.found != nullptr) ? finder.found->elements.size() : 2;
        if (arity == 0) { arity = 2; } // fallback
        for (size_t i = 0; i < arity; ++i) {
          std::string ty = "i32";
          if (finder.found != nullptr) {
            const auto* e = finder.found->elements[i].get();
            if (e->kind == ast::NodeKind::FloatLiteral) { ty = "double"; }
            else if (e->kind == ast::NodeKind::BoolLiteral) { ty = "i1"; }
          }
          tupleElemTys.push_back(ty);
        }
        std::ostringstream ts; ts << "{ ";
        for (size_t i = 0; i < tupleElemTys.size(); ++i) { if (i != 0) { ts << ", "; } ts << tupleElemTys[i]; }
        ts << " }"; retStructTy = ts.str();
      } else {
        throw std::runtime_error("unsupported function type");
      }
    }
    irStream << "define " << ((retStr != nullptr) ? retStr : retStructTy.c_str()) << " @" << func->name << "(";
    for (size_t i = 0; i < func->params.size(); ++i) {
      if (i != 0) { irStream << ", "; }
      irStream << typeStr(func->params[i].type) << " %" << func->params[i].name;
    }
    // Attach a simple DISubprogram for function-level debug info
    dbgSubs.push_back(DebugSub{func->name, nextDbgId});
    const int subDbgId = nextDbgId;
    irStream << ") gc \"shadow-stack\" personality ptr @__gxx_personality_v0 !dbg !" << subDbgId << " {\n";
    nextDbgId++;
    irStream << "entry:\n";

    enum class ValKind : std::uint8_t { I32, I1, F64, Ptr };
    enum class PtrTag : std::uint8_t { Unknown, Str, List, Dict, Object };
    struct Slot { std::string ptr; ValKind kind{}; PtrTag tag{PtrTag::Unknown}; };
    std::unordered_map<std::string, Slot> slots; // var -> slot
    int temp = 0;

    // Helper for DI locations in this function
    auto ensureLocId = [&](int line, int col) -> int {
      if (line <= 0) return 0;
      const unsigned long long key = (static_cast<unsigned long long>(static_cast<unsigned int>(subDbgId)) << 32ULL)
        ^ (static_cast<unsigned long long>((static_cast<unsigned int>(line) << 16U) | static_cast<unsigned int>(col)));
      auto it = dbgLocKeyToId.find(key);
      if (it != dbgLocKeyToId.end()) return it->second;
      const int id = nextDbgId++;
      dbgLocKeyToId[key] = id;
      dbgLocs.push_back(DebugLoc{id, line, col, subDbgId});
      return id;
    };

    // Parameter allocas + debug
    std::unordered_map<std::string, int> varMdId; // per-function var->!DILocalVariable id
    for (size_t pidx = 0; pidx < func->params.size(); ++pidx) {
      const auto& param = func->params[pidx];
      const std::string ptr = "%" + param.name + ".addr";
      if (param.type == ast::TypeKind::Int) {
        irStream << "  " << ptr << " = alloca i32\n";
        irStream << "  store i32 %" << param.name << ", ptr " << ptr << "\n";
        slots[param.name] = Slot{ptr, ValKind::I32};
      } else if (param.type == ast::TypeKind::Bool) {
        irStream << "  " << ptr << " = alloca i1\n";
        irStream << "  store i1 %" << param.name << ", ptr " << ptr << "\n";
        slots[param.name] = Slot{ptr, ValKind::I1};
      } else if (param.type == ast::TypeKind::Float) {
        irStream << "  " << ptr << " = alloca double\n";
        irStream << "  store double %" << param.name << ", ptr " << ptr << "\n";
        slots[param.name] = Slot{ptr, ValKind::F64};
      } else if (param.type == ast::TypeKind::Str) {
        irStream << "  " << ptr << " = alloca ptr\n";
        irStream << "  store ptr %" << param.name << ", ptr " << ptr << "\n";
        irStream << "  call void @pycc_gc_write_barrier(ptr " << ptr << ", ptr %" << param.name << ")\n";
        irStream << "  call void @llvm.gcroot(ptr " << ptr << ", ptr null)\n";
        Slot s{ptr, ValKind::Ptr}; s.tag = PtrTag::Str; slots[param.name] = s;
      } else {
        throw std::runtime_error("unsupported param type");
      }
      // Emit DILocalVariable for parameter and dbg.declare
      const int varId = nextDbgId++;
      varMdId[param.name] = varId;
      const int locId = ensureLocId(func->line, func->col);
      const int tyId = (param.type == ast::TypeKind::Int) ? diIntId
                      : (param.type == ast::TypeKind::Bool) ? diBoolId
                      : (param.type == ast::TypeKind::Float) ? diDoubleId
                      : diPtrId;
      dbgVars.push_back(DbgVar{varId, param.name, subDbgId, func->line, func->col, tyId, static_cast<int>(pidx) + 1, true});
      irStream << "  call void @llvm.dbg.declare(metadata ptr " << ptr
               << ", metadata !" << varId << ", metadata !" << diExprId << ")";
      if (locId > 0) irStream << " , !dbg !" << locId;
      irStream << "\n";
    }

    struct Value { std::string s; ValKind k; };

    // NOLINTBEGIN
    struct ExpressionLowerer : public ast::VisitorBase {
      ExpressionLowerer(std::ostringstream& ir_, int& temp_, std::unordered_map<std::string, Slot>& slots_, const std::unordered_map<std::string, Sig>& sigs_, const std::unordered_map<std::string, int>& retParamIdxs_)
        : ir(ir_), temp(temp_), slots(slots_), sigs(sigs_), retParamIdxs(retParamIdxs_) {}
      std::ostringstream& ir; // NOLINT
      int& temp; // NOLINT
      std::unordered_map<std::string, Slot>& slots; // NOLINT
      const std::unordered_map<std::string, Sig>& sigs; // NOLINT
      const std::unordered_map<std::string, int>& retParamIdxs; // NOLINT
      Value out{ { }, ValKind::I32 };

      std::string fneg(const std::string& v) {
        std::ostringstream r; r << "%t" << temp++;
        ir << "  " << r.str() << " = fneg double " << v << "\n";
        return r.str();
      }

      Value run(const ast::Expr& e) {
        e.accept(*this);
        return out;
      }

      void visit(const ast::IntLiteral& lit) override {
        out = Value{std::to_string(static_cast<int>(lit.value)), ValKind::I32};
      }
      void visit(const ast::BoolLiteral& bl) override {
        out = Value{bl.value ? std::string("true") : std::string("false"), ValKind::I1};
      }
      void visit(const ast::FloatLiteral& fl) override {
        std::ostringstream ss; ss.setf(std::ios::fmtflags(0), std::ios::floatfield); ss.precision(17); ss << fl.value;
        out = Value{ss.str(), ValKind::F64};
      }
      void visit(const ast::NoneLiteral& /*unused*/) override { throw std::runtime_error("none literal not supported in expressions"); }
      void visit(const ast::StringLiteral& s) override {
        // Compute the same name used during global emission
        auto hash = [&](const std::string &str) {
          constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
          constexpr uint64_t kFnvPrime = 1099511628211ULL;
          uint64_t hv = kFnvOffsetBasis; for (unsigned char ch : str) { hv ^= ch; hv *= kFnvPrime; } return hv;
        };
        const uint64_t h = hash(s.value);
        std::ostringstream gname; gname << ".str_" << std::hex << h;
        // length is available from global table; compute GEP to start
        std::ostringstream reg; reg << "%t" << temp++;
        // Allow toggling between opaque-pointer and typed-pointer GEP styles
        // Use opaque-pointer friendly GEP form
        ir << "  " << reg.str() << " = getelementptr inbounds i8, ptr @" << gname.str() << ", i64 0\n";
        out = Value{reg.str(), ValKind::Ptr};
      }
      void visit(const ast::Subscript& sub) override {
        if (!sub.value || !sub.slice) { throw std::runtime_error("null subscript"); }
        // Evaluate base
        auto base = run(*sub.value);
        if (base.k != ValKind::Ptr) { throw std::runtime_error("subscript base must be pointer"); }
        // Heuristic: decide between string/list/dict by literal or slot tag
        bool isList = (sub.value->kind == ast::NodeKind::ListLiteral);
        bool isStr = (sub.value->kind == ast::NodeKind::StringLiteral);
        bool isDict = (sub.value->kind == ast::NodeKind::DictLiteral);
        if (!isList && !isStr && !isDict && sub.value->kind == ast::NodeKind::Name) {
          const auto* nm = static_cast<const ast::Name*>(sub.value.get());
          auto it = slots.find(nm->id); if (it != slots.end()) {
            isList = (it->second.tag == PtrTag::List);
            isStr  = (it->second.tag == PtrTag::Str);
            isDict = (it->second.tag == PtrTag::Dict);
          }
        }
        if (isList || isStr) {
          auto idxV = run(*sub.slice);
          std::string idx64;
          if (idxV.k == ValKind::I32) {
            std::ostringstream z; z << "%t" << temp++;
            ir << "  " << z.str() << " = sext i32 " << idxV.s << " to i64\n"; idx64 = z.str();
          } else { throw std::runtime_error("subscript index must be int"); }
          if (isList) {
            std::ostringstream r; r << "%t" << temp++;
            ir << "  " << r.str() << " = call ptr @pycc_list_get(ptr " << base.s << ", i64 " << idx64 << ")\n";
            out = Value{r.str(), ValKind::Ptr}; return;
          }
          // string slice of length 1
          std::ostringstream r; r << "%t" << temp++;
          ir << "  " << r.str() << " = call ptr @pycc_string_slice(ptr " << base.s << ", i64 " << idx64 << ", i64 1)\n";
          out = Value{r.str(), ValKind::Ptr}; return;
        }
        if (isDict) {
          // dict get: key must be ptr; box primitives
          auto key = run(*sub.slice);
          std::string kptr;
          if (key.k == ValKind::Ptr) { kptr = key.s; }
          else if (key.k == ValKind::I32) {
            if (!key.s.empty() && key.s[0] != '%') { std::ostringstream w2; w2 << "%t" << temp++; ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << key.s << ")\n"; kptr = w2.str(); }
            else { std::ostringstream w,w2; w << "%t" << temp++; w2 << "%t" << temp++; ir << "  " << w.str() << " = sext i32 " << key.s << " to i64\n"; ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << w.str() << ")\n"; kptr = w2.str(); }
          } else if (key.k == ValKind::F64) { std::ostringstream w; w << "%t" << temp++; ir << "  " << w.str() << " = call ptr @pycc_box_float(double " << key.s << ")\n"; kptr = w.str(); }
          else if (key.k == ValKind::I1) { std::ostringstream w; w << "%t" << temp++; ir << "  " << w.str() << " = call ptr @pycc_box_bool(i1 " << key.s << ")\n"; kptr = w.str(); }
          else { throw std::runtime_error("unsupported dict key"); }
          std::ostringstream r; r << "%t" << temp++;
          ir << "  " << r.str() << " = call ptr @pycc_dict_get(ptr " << base.s << ", ptr " << kptr << ")\n";
          out = Value{r.str(), ValKind::Ptr}; return;
        }
        throw std::runtime_error("unsupported subscript base");
      }
      void visit(const ast::DictLiteral& d) override { // NOLINT(readability-function-cognitive-complexity)
        const std::size_t n = d.items.size();
        std::ostringstream slot, dict, cap;
        slot << "%t" << temp++;
        dict << "%t" << temp++;
        cap << (n == 0 ? 8 : n * 2);
        ir << "  " << slot.str() << " = alloca ptr\n";
        ir << "  " << dict.str() << " = call ptr @pycc_dict_new(i64 " << cap.str() << ")\n";
        ir << "  store ptr " << dict.str() << ", ptr " << slot.str() << "\n";
        ir << "  call void @pycc_gc_write_barrier(ptr " << slot.str() << ", ptr " << dict.str() << ")\n";
        for (const auto& kv : d.items) {
          if (!kv.first || !kv.second) { continue; }
          auto k = run(*kv.first);
          auto v = run(*kv.second);
          std::string kptr;
          if (k.k == ValKind::Ptr) { kptr = k.s; }
          else if (k.k == ValKind::I32) {
            if (!k.s.empty() && k.s[0] != '%') {
              std::ostringstream w2; w2 << "%t" << temp++;
              ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << k.s << ")\n";
              kptr = w2.str();
            } else {
              std::ostringstream w, w2; w << "%t" << temp++; w2 << "%t" << temp++;
              ir << "  " << w.str() << " = sext i32 " << k.s << " to i64\n";
              ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << w.str() << ")\n";
              kptr = w2.str();
            }
          } else if (k.k == ValKind::F64) {
            std::ostringstream w; w << "%t" << temp++;
            ir << "  " << w.str() << " = call ptr @pycc_box_float(double " << k.s << ")\n";
            kptr = w.str();
          } else if (k.k == ValKind::I1) {
            std::ostringstream w; w << "%t" << temp++;
            ir << "  " << w.str() << " = call ptr @pycc_box_bool(i1 " << k.s << ")\n";
            kptr = w.str();
          } else {
            throw std::runtime_error("unsupported key in dict literal");
          }
          std::string vptr;
          if (v.k == ValKind::Ptr) { vptr = v.s; }
          else if (v.k == ValKind::I32) {
            if (!v.s.empty() && v.s[0] != '%') {
              std::ostringstream w2; w2 << "%t" << temp++;
              ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << v.s << ")\n";
              vptr = w2.str();
            } else {
              std::ostringstream w, w2; w << "%t" << temp++; w2 << "%t" << temp++;
              ir << "  " << w.str() << " = sext i32 " << v.s << " to i64\n";
              ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << w.str() << ")\n";
              vptr = w2.str();
            }
          } else if (v.k == ValKind::F64) {
            std::ostringstream w; w << "%t" << temp++;
            ir << "  " << w.str() << " = call ptr @pycc_box_float(double " << v.s << ")\n";
            vptr = w.str();
          } else if (v.k == ValKind::I1) {
            std::ostringstream w; w << "%t" << temp++;
            ir << "  " << w.str() << " = call ptr @pycc_box_bool(i1 " << v.s << ")\n";
            vptr = w.str();
          } else {
            throw std::runtime_error("unsupported value in dict literal");
          }
          ir << "  call void @pycc_dict_set(ptr " << slot.str() << ", ptr " << kptr << ", ptr " << vptr << ")\n";
        }
        std::ostringstream outReg; outReg << "%t" << temp++;
        ir << "  " << outReg.str() << " = load ptr, ptr " << slot.str() << "\n";
        out = Value{outReg.str(), ValKind::Ptr};
      }
      void visit(const ast::Attribute& attr) override {
        if (!attr.value) { throw std::runtime_error("null attribute base"); }
        auto base = run(*attr.value);
        if (base.k != ValKind::Ptr) { throw std::runtime_error("attribute base must be pointer"); }
        // Build constant pointer to attribute name text using same global emission naming
        auto hash = [&](const std::string &str) {
          constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
          constexpr uint64_t kFnvPrime = 1099511628211ULL;
          uint64_t hv = kFnvOffsetBasis; for (unsigned char ch : str) { hv ^= ch; hv *= kFnvPrime; } return hv;
        };
        const uint64_t h = hash(attr.attr);
        std::ostringstream gname; gname << ".str_" << std::hex << h;
        std::ostringstream dataPtr; dataPtr << "%t" << temp++;
        ir << "  " << dataPtr.str() << " = getelementptr inbounds i8, ptr @" << gname.str() << ", i64 0\n";
        std::ostringstream sobj; sobj << "%t" << temp++;
        ir << "  " << sobj.str() << " = call ptr @pycc_string_new(ptr " << dataPtr.str() << ", i64 " << static_cast<long long>(attr.attr.size()) << ")\n";
        std::ostringstream reg; reg << "%t" << temp++;
        ir << "  " << reg.str() << " = call ptr @pycc_object_get_attr(ptr " << base.s << ", ptr " << sobj.str() << ")\n";
        out = Value{reg.str(), ValKind::Ptr};
      }
      void visit(const ast::ObjectLiteral& obj) override { // NOLINT(readability-function-cognitive-complexity)
        const std::size_t n = obj.fields.size();
        std::ostringstream regObj, nfields;
        regObj << "%t" << temp++;
        nfields << n;
        ir << "  " << regObj.str() << " = call ptr @pycc_object_new(i64 " << nfields.str() << ")\n";
        for (std::size_t i = 0; i < n; ++i) {
          const auto& f = obj.fields[i];
          if (!f) { continue; }
          auto v = run(*f);
          std::string valPtr;
          if (v.k == ValKind::Ptr) {
            valPtr = v.s;
          } else if (v.k == ValKind::I32) {
            if (!v.s.empty() && v.s[0] != '%') {
              std::ostringstream w2; w2 << "%t" << temp++;
              ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << v.s << ")\n";
              valPtr = w2.str();
            } else {
              std::ostringstream w, w2; w << "%t" << temp++; w2 << "%t" << temp++;
              ir << "  " << w.str() << " = sext i32 " << v.s << " to i64\n";
              ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << w.str() << ")\n";
              valPtr = w2.str();
            }
          } else if (v.k == ValKind::F64) {
            std::ostringstream w; w << "%t" << temp++;
            ir << "  " << w.str() << " = call ptr @pycc_box_float(double " << v.s << ")\n";
            valPtr = w.str();
          } else if (v.k == ValKind::I1) {
            std::ostringstream w; w << "%t" << temp++;
            ir << "  " << w.str() << " = call ptr @pycc_box_bool(i1 " << v.s << ")\n";
            valPtr = w.str();
          } else {
            throw std::runtime_error("unsupported field kind in object literal");
          }
          std::ostringstream idx; idx << static_cast<long long>(i);
          ir << "  call void @pycc_object_set(ptr " << regObj.str() << ", i64 " << idx.str() << ", ptr " << valPtr << ")\n";
        }
        out = Value{regObj.str(), ValKind::Ptr};
      }
      void visit(const ast::ListLiteral& list) override { // NOLINT(readability-function-cognitive-complexity)
        const std::size_t n = list.elements.size();
        std::ostringstream slot, lst, cap;
        slot << "%t" << temp++;
        lst << "%t" << temp++;
        cap << n;
        ir << "  " << slot.str() << " = alloca ptr\n";
        ir << "  " << lst.str() << " = call ptr @pycc_list_new(i64 " << cap.str() << ")\n";
        ir << "  store ptr " << lst.str() << ", ptr " << slot.str() << "\n";
        ir << "  call void @pycc_gc_write_barrier(ptr " << slot.str() << ", ptr " << lst.str() << ")\n";
        for (const auto& el : list.elements) {
          if (!el) { continue; }
          auto v = run(*el);
          std::string elemPtr;
          if (v.k == ValKind::Ptr) {
            elemPtr = v.s;
          } else if (v.k == ValKind::I32) {
            if (!v.s.empty() && v.s[0] != '%') {
              std::ostringstream w2; w2 << "%t" << temp++;
              ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << v.s << ")\n";
              elemPtr = w2.str();
            } else {
              std::ostringstream w, w2; w << "%t" << temp++; w2 << "%t" << temp++;
              ir << "  " << w.str() << " = sext i32 " << v.s << " to i64\n";
              ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << w.str() << ")\n";
              elemPtr = w2.str();
            }
          } else if (v.k == ValKind::F64) {
            std::ostringstream w; w << "%t" << temp++;
            ir << "  " << w.str() << " = call ptr @pycc_box_float(double " << v.s << ")\n";
            elemPtr = w.str();
          } else if (v.k == ValKind::I1) {
            std::ostringstream w; w << "%t" << temp++;
            ir << "  " << w.str() << " = call ptr @pycc_box_bool(i1 " << v.s << ")\n";
            elemPtr = w.str();
          } else {
            throw std::runtime_error("unsupported element kind in list literal");
          }
          ir << "  call void @pycc_list_push(ptr " << slot.str() << ", ptr " << elemPtr << ")\n";
        }
        std::ostringstream outReg; outReg << "%t" << temp++;
        ir << "  " << outReg.str() << " = load ptr, ptr " << slot.str() << "\n";
        out = Value{outReg.str(), ValKind::Ptr};
      }
      void visit(const ast::Name& nm) override {
        auto it = slots.find(nm.id);
        if (it == slots.end()) throw std::runtime_error(std::string("undefined name: ") + nm.id);
        std::ostringstream reg; reg << "%t" << temp++;
        if (it->second.kind == ValKind::I32)
          ir << "  " << reg.str() << " = load i32, ptr " << it->second.ptr << "\n";
        else if (it->second.kind == ValKind::I1)
          ir << "  " << reg.str() << " = load i1, ptr " << it->second.ptr << "\n";
        else if (it->second.kind == ValKind::F64)
          ir << "  " << reg.str() << " = load double, ptr " << it->second.ptr << "\n";
        else
          ir << "  " << reg.str() << " = load ptr, ptr " << it->second.ptr << "\n";
        out = Value{reg.str(), it->second.kind};
      }
      void visit(const ast::Call& call) override { // NOLINT(readability-function-cognitive-complexity)
        if (call.callee == nullptr) { throw std::runtime_error("unsupported callee expression"); }
        // Polymorphic list.append(x)
        if (call.callee->kind == ast::NodeKind::Attribute) {
          const auto* at = static_cast<const ast::Attribute*>(call.callee.get());
          if (!at->value) { throw std::runtime_error("null method base"); }
          // identify list base
          bool isList = (at->value->kind == ast::NodeKind::ListLiteral);
          if (!isList && at->value->kind == ast::NodeKind::Name) {
            auto* nm = static_cast<const ast::Name*>(at->value.get());
            auto it = slots.find(nm->id); if (it != slots.end()) isList = (it->second.tag == PtrTag::List);
          }
          if (isList && at->attr == "append") {
            if (call.args.size() != 1) throw std::runtime_error("append() takes one arg");
            auto base = run(*at->value);
            if (base.k != ValKind::Ptr) throw std::runtime_error("append base not ptr");
            auto av = run(*call.args[0]);
            std::string aptr;
            if (av.k == ValKind::Ptr) { aptr = av.s; }
            else if (av.k == ValKind::I32) { if (!av.s.empty() && av.s[0] != '%') { std::ostringstream w2; w2 << "%t" << temp++; ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << av.s << ")\n"; aptr = w2.str(); } else { std::ostringstream w,w2; w << "%t" << temp++; w2 << "%t" << temp++; ir << "  " << w.str() << " = sext i32 " << av.s << " to i64\n"; ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << w.str() << ")\n"; aptr = w2.str(); } }
            else if (av.k == ValKind::F64) { std::ostringstream w; w << "%t" << temp++; ir << "  " << w.str() << " = call ptr @pycc_box_float(double " << av.s << ")\n"; aptr = w.str(); }
            else if (av.k == ValKind::I1) { std::ostringstream w; w << "%t" << temp++; ir << "  " << w.str() << " = call ptr @pycc_box_bool(i1 " << av.s << ")\n"; aptr = w.str(); }
            else { throw std::runtime_error("unsupported append arg"); }
            // create slot for base and push
            std::ostringstream slot; slot << "%t" << temp++;
            ir << "  " << slot.str() << " = alloca ptr\n";
            ir << "  store ptr " << base.s << ", ptr " << slot.str() << "\n";
            ir << "  call void @pycc_list_push(ptr " << slot.str() << ", ptr " << aptr << ")\n";
            out = Value{base.s, ValKind::Ptr};
            return;
          }
          throw std::runtime_error("unsupported attribute call");
        }
        const auto* nmCall = dynamic_cast<const ast::Name*>(call.callee.get());
        if (nmCall == nullptr) { throw std::runtime_error("unsupported callee expression"); }
        // Builtin: len(arg) -> i32 constant for tuple/list literal lengths
        if (nmCall->id == "len") {
          if (call.args.size() != 1) { throw std::runtime_error("len() takes exactly one argument"); }
          auto* arg0 = call.args[0].get();
          if (arg0->kind == ast::NodeKind::TupleLiteral) {
            const auto* tupLit = dynamic_cast<const ast::TupleLiteral*>(arg0);
            out = Value{std::to_string(static_cast<int>(tupLit->elements.size())), ValKind::I32};
            return;
          }
          if (arg0->kind == ast::NodeKind::ListLiteral) {
            const auto* listLit = dynamic_cast<const ast::ListLiteral*>(arg0);
            out = Value{std::to_string(static_cast<int>(listLit->elements.size())), ValKind::I32};
            return;
          }
          if (arg0->kind == ast::NodeKind::StringLiteral) {
            const auto* strLit = dynamic_cast<const ast::StringLiteral*>(arg0);
            out = Value{std::to_string(static_cast<int>(strLit->value.size())), ValKind::I32};
            return;
          }
          if (arg0->kind == ast::NodeKind::BytesLiteral) {
            const auto* byLit = dynamic_cast<const ast::BytesLiteral*>(arg0);
            out = Value{std::to_string(static_cast<int>(byLit->value.size())), ValKind::I32};
            return;
          }
          if (arg0->kind == ast::NodeKind::Call) {
            const auto* c = dynamic_cast<const ast::Call*>(arg0);
            if (c && c->callee && c->callee->kind == ast::NodeKind::Name) {
              const auto* cname = dynamic_cast<const ast::Name*>(c->callee.get());
              if (cname != nullptr) {
                // Evaluate the call to get a pointer result
                auto v = run(*arg0);
                if (v.k != ValKind::Ptr) throw std::runtime_error("len(call): callee did not return pointer");
                std::ostringstream r64, r32; r64 << "%t" << temp++; r32 << "%t" << temp++;
                bool isList = false;
                // First, try interprocedural param-forwarding tag inference
                auto itRet = retParamIdxs.find(cname->id);
                if (itRet != retParamIdxs.end()) {
                  int rp = itRet->second;
                  if (rp >= 0 && static_cast<size_t>(rp) < c->args.size()) {
                    const auto* a = c->args[rp].get();
                    if (a && a->kind == ast::NodeKind::Name) {
                      const auto* an = static_cast<const ast::Name*>(a);
                      auto itn = slots.find(an->id);
                      if (itn != slots.end()) { isList = (itn->second.tag == PtrTag::List); }
                    }
                  }
                }
                // Fallback to return-type based choice
                if (!isList) {
                  auto itSig = sigs.find(cname->id);
                  if (itSig != sigs.end()) { isList = (itSig->second.ret == ast::TypeKind::List); }
                }
                if (isList) {
                  ir << "  " << r64.str() << " = call i64 @pycc_list_len(ptr " << v.s << ")\n";
                } else {
                  ir << "  " << r64.str() << " = call i64 @strlen(ptr " << v.s << ")\n";
                }
                ir << "  " << r32.str() << " = trunc i64 " << r64.str() << " to i32\n";
                out = Value{r32.str(), ValKind::I32};
                return;
              }
            }
          }
          if (arg0->kind == ast::NodeKind::Name) {
            const auto* nmArg = dynamic_cast<const ast::Name*>(arg0);
            auto itn = slots.find(nmArg->id);
            if (itn == slots.end()) throw std::runtime_error(std::string("undefined name: ") + nmArg->id);
            // Load pointer
            std::ostringstream regPtr; regPtr << "%t" << temp++;
            ir << "  " << regPtr.str() << " = load ptr, ptr " << itn->second.ptr << "\n";
            std::ostringstream r64, r32; r64 << "%t" << temp++; r32 << "%t" << temp++;
            if (itn->second.tag == PtrTag::Str || itn->second.tag == PtrTag::Unknown) {
              ir << "  " << r64.str() << " = call i64 @strlen(ptr " << regPtr.str() << ")\n";
            } else {
              ir << "  " << r64.str() << " = call i64 @pycc_list_len(ptr " << regPtr.str() << ")\n";
            }
            ir << "  " << r32.str() << " = trunc i64 " << r64.str() << " to i32\n";
            out = Value{r32.str(), ValKind::I32};
            return;
          }
          // Fallback: unsupported target type
          out = Value{"0", ValKind::I32};
          return;
        }
        if (nmCall->id == "obj_get") {
          if (call.args.size() != 2) { throw std::runtime_error("obj_get() takes exactly two arguments"); }
          auto vObj = run(*call.args[0]);
          auto vIdx = run(*call.args[1]);
          if (vObj.k != ValKind::Ptr) throw std::runtime_error("obj_get: first arg must be object pointer");
          if (vIdx.k != ValKind::I32) throw std::runtime_error("obj_get: index must be int");
          std::ostringstream idx64, reg; idx64 << "%t" << temp++; reg << "%t" << temp++;
          ir << "  " << idx64.str() << " = sext i32 " << vIdx.s << " to i64\n";
          ir << "  " << reg.str() << " = call ptr @pycc_object_get(ptr " << vObj.s << ", i64 " << idx64.str() << ")\n";
          out = Value{reg.str(), ValKind::Ptr};
          return;
        }
        // Builtin: isinstance(x, T) -> i1 const for basic T in {int,bool,float}
        if (nmCall->id == "isinstance") {
          if (call.args.size() != 2) { throw std::runtime_error("isinstance() takes two arguments"); }
          // Determine type of first arg
          auto classify = [&](const ast::Expr* e) -> ValKind {
            if (!e) { throw std::runtime_error("null arg"); }
            if (e->kind == ast::NodeKind::IntLiteral) return ValKind::I32;
            if (e->kind == ast::NodeKind::BoolLiteral) return ValKind::I1;
            if (e->kind == ast::NodeKind::FloatLiteral) return ValKind::F64;
            if (e->kind == ast::NodeKind::Name) {
              auto* n = static_cast<const ast::Name*>(e);
              auto it = slots.find(n->id);
              if (it == slots.end()) { throw std::runtime_error(std::string("unknown name in isinstance: ") + n->id); }
              return it->second.kind;
            }
            // Evaluate and assume int for unknowns
            auto v = run(*e); return v.k;
          };
          ValKind vk = classify(call.args[0].get());
          // Second arg must be a Name for type
          bool match = false;
          if (call.args[1]->kind == ast::NodeKind::Name) {
            auto* tn = static_cast<const ast::Name*>(call.args[1].get());
            if (tn->id == "int") match = (vk == ValKind::I32);
            else if (tn->id == "bool") match = (vk == ValKind::I1);
            else if (tn->id == "float") match = (vk == ValKind::F64);
            else match = false;
          }
          out = Value{ match ? std::string("true") : std::string("false"), ValKind::I1 };
          return;
        }
        auto itS = sigs.find(nmCall->id);
        if (itS == sigs.end()) { throw std::runtime_error(std::string("unknown function: ") + nmCall->id); }
        const auto& ps = itS->second.params;
        if (ps.size() != call.args.size()) { throw std::runtime_error(std::string("arity mismatch calling function: ") + nmCall->id); }
        std::vector<std::string> argsSSA; argsSSA.reserve(call.args.size());
        for (size_t i = 0; i < call.args.size(); ++i) {
          auto v = run(*call.args[i]);
          if ((ps[i] == ast::TypeKind::Int && v.k != ValKind::I32) ||
              (ps[i] == ast::TypeKind::Bool && v.k != ValKind::I1) ||
              (ps[i] == ast::TypeKind::Float && v.k != ValKind::F64) ||
              (ps[i] == ast::TypeKind::Str && v.k != ValKind::Ptr))
            throw std::runtime_error("call argument type mismatch");
          argsSSA.push_back(v.s);
        }
        std::ostringstream reg; reg << "%t" << temp++;
        auto retT = itS->second.ret;
        const char* retStr = (retT == ast::TypeKind::Int) ? "i32" : (retT == ast::TypeKind::Bool) ? "i1" : (retT == ast::TypeKind::Float) ? "double" : "ptr";
        ir << "  " << reg.str() << " = call " << retStr << " @" << nmCall->id << "(";
        for (size_t i = 0; i < argsSSA.size(); ++i) {
          if (i != 0) { ir << ", "; }
          const char* argStr = (ps[i] == ast::TypeKind::Int) ? "i32" : (ps[i] == ast::TypeKind::Bool) ? "i1" : (ps[i] == ast::TypeKind::Float) ? "double" : "ptr";
          ir << argStr << " " << argsSSA[i];
        }
        ir << ")\n";
        ValKind rk = (retT == ast::TypeKind::Int) ? ValKind::I32 : (retT == ast::TypeKind::Bool) ? ValKind::I1 : (retT == ast::TypeKind::Float) ? ValKind::F64 : ValKind::Ptr;
        out = Value{reg.str(), rk};
      }
      void visit(const ast::Unary& u) override {
        auto V = run(*u.operand);
        auto toBool = [&](const Value& vin) -> Value {
          if (vin.k == ValKind::I1) { return vin; }
          std::ostringstream r; r << "%t" << temp++;
          switch (vin.k) {
            case ValKind::I32:
              ir << "  " << r.str() << " = icmp ne i32 " << vin.s << ", 0\n"; return Value{r.str(), ValKind::I1};
            case ValKind::F64:
              ir << "  " << r.str() << " = fcmp one double " << vin.s << ", 0.0\n"; return Value{r.str(), ValKind::I1};
            case ValKind::Ptr:
              ir << "  " << r.str() << " = icmp ne ptr " << vin.s << ", null\n"; return Value{r.str(), ValKind::I1};
            default: throw std::runtime_error("unsupported truthiness conversion");
          }
        };
        if (u.op == ast::UnaryOperator::Neg) {
          if (V.k == ValKind::I32) {
            std::ostringstream reg; reg << "%t" << temp++;
            ir << "  " << reg.str() << " = sub i32 0, " << V.s << "\n";
            out = Value{reg.str(), ValKind::I32};
          } else if (V.k == ValKind::F64) {
            out = Value{fneg(V.s), ValKind::F64};
          } else {
            throw std::runtime_error("unsupported '-' on bool");
          }
        } else if (u.op == ast::UnaryOperator::BitNot) {
          if (V.k != ValKind::I32) { throw std::runtime_error("bitwise '~' requires int"); }
          std::ostringstream reg; reg << "%t" << temp++;
          ir << "  " << reg.str() << " = xor i32 " << V.s << ", -1\n";
          out = Value{reg.str(), ValKind::I32};
        } else {
          auto VB = toBool(V);
          std::ostringstream reg; reg << "%t" << temp++;
          ir << "  " << reg.str() << " = xor i1 " << VB.s << ", true\n";
          out = Value{reg.str(), ValKind::I1};
        }
      }
      void visit(const ast::Binary& b) override {
        // Handle None comparisons to constants if possible (Eq/Ne/Is/IsNot)
        bool isCmp = (b.op == ast::BinaryOperator::Eq || b.op == ast::BinaryOperator::Ne ||
                      b.op == ast::BinaryOperator::Lt || b.op == ast::BinaryOperator::Le ||
                      b.op == ast::BinaryOperator::Gt || b.op == ast::BinaryOperator::Ge ||
                      b.op == ast::BinaryOperator::Is || b.op == ast::BinaryOperator::IsNot);
        if (isCmp && (b.lhs->kind == ast::NodeKind::NoneLiteral || b.rhs->kind == ast::NodeKind::NoneLiteral)) {
          bool bothNone = (b.lhs->kind == ast::NodeKind::NoneLiteral && b.rhs->kind == ast::NodeKind::NoneLiteral);
          bool eq = (b.op == ast::BinaryOperator::Eq || b.op == ast::BinaryOperator::Is);
          if (bothNone) { out = Value{ eq ? std::string("true") : std::string("false"), ValKind::I1 }; return; }
          const ast::Expr* other = (b.lhs->kind == ast::NodeKind::NoneLiteral) ? b.rhs.get() : b.lhs.get();
          if (other && other->type() && *other->type() != ast::TypeKind::NoneType) {
            out = Value{ eq ? std::string("false") : std::string("true"), ValKind::I1 }; return;
          }
          // Unknown types: conservatively treat Eq as false, Ne as true
          out = Value{ eq ? std::string("false") : std::string("true"), ValKind::I1 }; return;
        }
        auto LV = run(*b.lhs);
        // Defer evaluating RHS until we know we need it. Some ops (like 'in' over literal containers)
        // only need to inspect RHS structure without lowering it as a value.
        // Handle membership early to avoid lowering tuple/list RHS as a value.
        if (b.op == ast::BinaryOperator::In || b.op == ast::BinaryOperator::NotIn) {
          // String membership: substring in string
          bool rhsStr = (b.rhs->kind == ast::NodeKind::StringLiteral) ||
                        (b.rhs->kind == ast::NodeKind::Name && [this,&b]() { auto it = slots.find(static_cast<const ast::Name*>(b.rhs.get())->id); return it != slots.end() && it->second.tag == PtrTag::Str; }());
          bool lhsStr = (b.lhs->kind == ast::NodeKind::StringLiteral) ||
                        (b.lhs->kind == ast::NodeKind::Name && [this,&b]() { auto it = slots.find(static_cast<const ast::Name*>(b.lhs.get())->id); return it != slots.end() && it->second.tag == PtrTag::Str; }());
          if (rhsStr && lhsStr) {
            auto H = run(*b.rhs);
            auto N = run(*b.lhs);
            std::ostringstream c; c << "%t" << temp++;
            ir << "  " << c.str() << " = call i1 @pycc_string_contains(ptr " << H.s << ", ptr " << N.s << ")\n";
            if (b.op == ast::BinaryOperator::NotIn) { std::ostringstream nx; nx << "%t" << temp++; ir << "  " << nx.str() << " = xor i1 " << c.str() << ", true\n"; out = Value{nx.str(), ValKind::I1}; }
            else { out = Value{c.str(), ValKind::I1}; }
            return;
          }
          if (b.rhs->kind != ast::NodeKind::ListLiteral && b.rhs->kind != ast::NodeKind::TupleLiteral) {
            out = Value{"false", ValKind::I1};
            return;
          }
          std::vector<const ast::Expr*> elements;
          if (b.rhs->kind == ast::NodeKind::ListLiteral) {
            const auto* lst = static_cast<const ast::ListLiteral*>(b.rhs.get());
            for (const auto& e : lst->elements) if (e) elements.push_back(e.get());
          } else {
            const auto* tp = static_cast<const ast::TupleLiteral*>(b.rhs.get());
            for (const auto& e : tp->elements) if (e) elements.push_back(e.get());
          }
          if (elements.empty()) { out = Value{"false", ValKind::I1}; return; }
          std::string accum;
          for (const auto* ee : elements) {
            auto EV = run(*ee);
            if (EV.k != LV.k) { continue; }
            std::ostringstream c; c << "%t" << temp++;
            if (LV.k == ValKind::I32) {
              ir << "  " << c.str() << " = icmp eq i32 " << LV.s << ", " << EV.s << "\n";
            } else if (LV.k == ValKind::F64) {
              ir << "  " << c.str() << " = fcmp oeq double " << LV.s << ", " << EV.s << "\n";
            } else if (LV.k == ValKind::I1) {
              ir << "  " << c.str() << " = icmp eq i1 " << LV.s << ", " << EV.s << "\n";
            } else if (LV.k == ValKind::Ptr) {
              ir << "  " << c.str() << " = icmp eq ptr " << LV.s << ", " << EV.s << "\n";
            } else { continue; }
            if (accum.empty()) { accum = c.str(); }
            else { std::ostringstream o; o << "%t" << temp++; ir << "  " << o.str() << " = or i1 " << accum << ", " << c.str() << "\n"; accum = o.str(); }
          }
          if (accum.empty()) { out = Value{"false", ValKind::I1}; return; }
          if (b.op == ast::BinaryOperator::NotIn) {
            std::ostringstream n; n << "%t" << temp++;
            ir << "  " << n.str() << " = xor i1 " << accum << ", true\n";
            out = Value{n.str(), ValKind::I1};
          } else {
            out = Value{accum, ValKind::I1};
          }
          return;
        }
        // Comparisons
        isCmp = (b.op == ast::BinaryOperator::Eq || b.op == ast::BinaryOperator::Ne ||
                      b.op == ast::BinaryOperator::Lt || b.op == ast::BinaryOperator::Le ||
                      b.op == ast::BinaryOperator::Gt || b.op == ast::BinaryOperator::Ge ||
                      b.op == ast::BinaryOperator::Is || b.op == ast::BinaryOperator::IsNot);
        auto RV = run(*b.rhs);
        if (isCmp) {
          std::ostringstream r1; r1 << "%t" << temp++;
          if (LV.k == ValKind::I32 && RV.k == ValKind::I32) {
            const char* pred = "eq";
            switch (b.op) {
              case ast::BinaryOperator::Eq: pred = "eq"; break;
              case ast::BinaryOperator::Ne: pred = "ne"; break;
              case ast::BinaryOperator::Lt: pred = "slt"; break;
              case ast::BinaryOperator::Le: pred = "sle"; break;
              case ast::BinaryOperator::Gt: pred = "sgt"; break;
              case ast::BinaryOperator::Ge: pred = "sge"; break;
              case ast::BinaryOperator::Is: pred = "eq"; break;
              case ast::BinaryOperator::IsNot: pred = "ne"; break;
              default: break;
            }
            ir << "  " << r1.str() << " = icmp " << pred << " i32 " << LV.s << ", " << RV.s << "\n";
          } else if (LV.k == ValKind::F64 && RV.k == ValKind::F64) {
            const char* pred = "oeq";
            switch (b.op) {
              case ast::BinaryOperator::Eq: pred = "oeq"; break;
              case ast::BinaryOperator::Ne: pred = "one"; break;
              case ast::BinaryOperator::Lt: pred = "olt"; break;
              case ast::BinaryOperator::Le: pred = "ole"; break;
              case ast::BinaryOperator::Gt: pred = "ogt"; break;
              case ast::BinaryOperator::Ge: pred = "oge"; break;
              case ast::BinaryOperator::Is: pred = "oeq"; break;
              case ast::BinaryOperator::IsNot: pred = "one"; break;
              default: break;
            }
            ir << "  " << r1.str() << " = fcmp " << pred << " double " << LV.s << ", " << RV.s << "\n";
          } else if (LV.k == ValKind::Ptr && RV.k == ValKind::Ptr) {
            const char* pred = (b.op == ast::BinaryOperator::Is || b.op == ast::BinaryOperator::Eq) ? "eq" : (b.op == ast::BinaryOperator::IsNot || b.op == ast::BinaryOperator::Ne) ? "ne" : nullptr;
            if (!pred) throw std::runtime_error("unsupported pointer comparison predicate");
            ir << "  " << r1.str() << " = icmp " << pred << " ptr " << LV.s << ", " << RV.s << "\n";
          } else {
            throw std::runtime_error("mismatched types in comparison");
          }
          out = Value{r1.str(), ValKind::I1};
          return;
        }
        // Bitwise and shifts (ints only)
        if (b.op == ast::BinaryOperator::BitAnd || b.op == ast::BinaryOperator::BitOr || b.op == ast::BinaryOperator::BitXor || b.op == ast::BinaryOperator::LShift || b.op == ast::BinaryOperator::RShift) {
          if (!(LV.k == ValKind::I32 && RV.k == ValKind::I32)) { throw std::runtime_error("bitwise/shift requires int operands"); }
          std::ostringstream r; r << "%t" << temp++;
          const char* op = nullptr;
          switch (b.op) {
            case ast::BinaryOperator::BitAnd: op = "and"; break;
            case ast::BinaryOperator::BitOr:  op = "or";  break;
            case ast::BinaryOperator::BitXor: op = "xor"; break;
            case ast::BinaryOperator::LShift: op = "shl"; break;
            case ast::BinaryOperator::RShift: op = "ashr"; break; // arithmetic right shift
            default: break;
          }
          ir << "  " << r.str() << " = " << op << " i32 " << LV.s << ", " << RV.s << "\n";
          out = Value{r.str(), ValKind::I32};
          return;
        }
        // FloorDiv and Pow
        if (b.op == ast::BinaryOperator::FloorDiv || b.op == ast::BinaryOperator::Pow) {
          // Ints
          if (LV.k == ValKind::I32 && RV.k == ValKind::I32) {
            if (b.op == ast::BinaryOperator::FloorDiv) {
              std::ostringstream r; r << "%t" << temp++;
              ir << "  " << r.str() << " = sdiv i32 " << LV.s << ", " << RV.s << "\n";
              out = Value{r.str(), ValKind::I32};
              return;
            }
            // pow for ints: cast to double, call powi, cast back to i32
            std::ostringstream a, r, back;
            a << "%t" << temp++; r << "%t" << temp++; back << "%t" << temp++;
            ir << "  " << a.str() << " = sitofp i32 " << LV.s << " to double\n";
            ir << "  " << r.str() << " = call double @llvm.powi.f64(double " << a.str() << ", i32 " << RV.s << ")\n";
            ir << "  " << back.str() << " = fptosi double " << r.str() << " to i32\n";
            out = Value{back.str(), ValKind::I32};
            return;
          }
          // Floats
          if (LV.k == ValKind::F64 && (RV.k == ValKind::F64 || RV.k == ValKind::I32)) {
            if (b.op == ast::BinaryOperator::FloorDiv) {
              std::ostringstream q, f; q << "%t" << temp++; f << "%t" << temp++;
              std::string rhsF = RV.s;
              if (RV.k == ValKind::I32) { std::ostringstream c; c << "%t" << temp++; ir << "  " << c.str() << " = sitofp i32 " << RV.s << " to double\n"; rhsF = c.str(); }
              ir << "  " << q.str() << " = fdiv double " << LV.s << ", " << rhsF << "\n";
              ir << "  " << f.str() << " = call double @llvm.floor.f64(double " << q.str() << ")\n";
              out = Value{f.str(), ValKind::F64};
              return;
            }
            std::ostringstream res; res << "%t" << temp++;
            // Ensure base is in an SSA register for consistent intrinsic signature patterns
            std::string base = LV.s;
            if (base.empty() || base[0] != '%') { std::ostringstream bss; bss << "%t" << temp++; ir << "  " << bss.str() << " = fadd double 0.0, " << base << "\n"; base = bss.str(); }
            if (RV.k == ValKind::I32) {
              ir << "  " << res.str() << " = call double @llvm.powi.f64(double " << base << ", i32 " << RV.s << ")\n";
            } else {
              ir << "  " << res.str() << " = call double @llvm.pow.f64(double " << base << ", double " << RV.s << ")\n";
            }
            out = Value{res.str(), ValKind::F64};
            return;
          }
          throw std::runtime_error("unsupported operand types for // or **");
        }
        if (b.op == ast::BinaryOperator::And || b.op == ast::BinaryOperator::Or) {
          auto toBool = [&](const Value& vin) -> Value {
            if (vin.k == ValKind::I1) { return vin; }
            std::ostringstream r; r << "%t" << temp++;
            switch (vin.k) {
              case ValKind::I32:
                ir << "  " << r.str() << " = icmp ne i32 " << vin.s << ", 0\n"; return Value{r.str(), ValKind::I1};
              case ValKind::F64:
                ir << "  " << r.str() << " = fcmp one double " << vin.s << ", 0.0\n"; return Value{r.str(), ValKind::I1};
              case ValKind::Ptr:
                ir << "  " << r.str() << " = icmp ne ptr " << vin.s << ", null\n"; return Value{r.str(), ValKind::I1};
              default: throw std::runtime_error("unsupported truthiness conversion");
            }
          };
          LV = toBool(LV);
          static int scCounter = 0; int id = scCounter++;
          if (b.op == ast::BinaryOperator::And) {
            std::string rhsLbl = std::string("and.rhs") + std::to_string(id);
            std::string falseLbl = std::string("and.false") + std::to_string(id);
            std::string endLbl = std::string("and.end") + std::to_string(id);
            ir << "  br i1 " << LV.s << ", label %" << rhsLbl << ", label %" << falseLbl << "\n";
            ir << rhsLbl << ":\n";
            auto RV2 = run(*b.rhs);
            RV2 = toBool(RV2);
            ir << "  br label %" << endLbl << "\n";
            ir << falseLbl << ":\n  br label %" << endLbl << "\n";
            ir << endLbl << ":\n";
            std::ostringstream rphi; rphi << "%t" << temp++;
            ir << "  " << rphi.str() << " = phi i1 [ " << RV2.s << ", %" << rhsLbl << " ], [ false, %" << falseLbl << " ]\n";
            out = Value{rphi.str(), ValKind::I1};
          } else {
            std::string trueLbl = std::string("or.true") + std::to_string(id);
            std::string rhsLbl = std::string("or.rhs") + std::to_string(id);
            std::string endLbl = std::string("or.end") + std::to_string(id);
            ir << "  br i1 " << LV.s << ", label %" << trueLbl << ", label %" << rhsLbl << "\n";
            ir << trueLbl << ":\n  br label %" << endLbl << "\n";
            ir << rhsLbl << ":\n";
            auto RV2 = run(*b.rhs);
            RV2 = toBool(RV2);
            ir << "  br label %" << endLbl << "\n";
            ir << endLbl << ":\n";
            std::ostringstream rphi; rphi << "%t" << temp++;
            ir << "  " << rphi.str() << " = phi i1 [ true, %" << trueLbl << " ], [ " << RV2.s << ", %" << rhsLbl << " ]\n";
            out = Value{rphi.str(), ValKind::I1};
          }
          return;
        }
        // Membership handled above
        // For remaining operations, ensure RHS has been evaluated when needed (RV already computed for comparisons and used below).
        // Arithmetic and string concatenation
        std::ostringstream reg; reg << "%t" << temp++;
        if (LV.k == ValKind::Ptr && RV.k == ValKind::Ptr) {
          // If both are strings, '+' means concatenate
          bool strL = false, strR = false;
          if (b.lhs->kind == ast::NodeKind::StringLiteral) strL = true;
          else if (b.lhs->kind == ast::NodeKind::Name) { auto it = slots.find(static_cast<const ast::Name*>(b.lhs.get())->id); if (it != slots.end()) strL = (it->second.tag == PtrTag::Str); }
          if (b.rhs->kind == ast::NodeKind::StringLiteral) strR = true;
          else if (b.rhs->kind == ast::NodeKind::Name) { auto it = slots.find(static_cast<const ast::Name*>(b.rhs.get())->id); if (it != slots.end()) strR = (it->second.tag == PtrTag::Str); }
          if (strL && strR && b.op == ast::BinaryOperator::Add) {
            ir << "  " << reg.str() << " = call ptr @pycc_string_concat(ptr " << LV.s << ", ptr " << RV.s << ")\n";
            out = Value{reg.str(), ValKind::Ptr};
            return;
          }
        }
        if (LV.k == ValKind::I32 && RV.k == ValKind::I32) {
          const char* op = "add";
          switch (b.op) {
            case ast::BinaryOperator::Add: op = "add"; break;
            case ast::BinaryOperator::Sub: op = "sub"; break;
            case ast::BinaryOperator::Mul: op = "mul"; break;
            case ast::BinaryOperator::Div: op = "sdiv"; break;
            case ast::BinaryOperator::Mod: op = "srem"; break;
            default: break;
          }
          ir << "  " << reg.str() << " = " << op << " i32 " << LV.s << ", " << RV.s << "\n";
          out = Value{reg.str(), ValKind::I32};
        } else if (LV.k == ValKind::F64 && RV.k == ValKind::F64) {
          if (b.op == ast::BinaryOperator::Mod) throw std::runtime_error("float mod not supported");
          const char* op = "fadd";
          switch (b.op) {
            case ast::BinaryOperator::Add: op = "fadd"; break;
            case ast::BinaryOperator::Sub: op = "fsub"; break;
            case ast::BinaryOperator::Mul: op = "fmul"; break;
            case ast::BinaryOperator::Div: op = "fdiv"; break;
            default: break;
          }
          ir << "  " << reg.str() << " = " << op << " double " << LV.s << ", " << RV.s << "\n";
          out = Value{reg.str(), ValKind::F64};
        } else if ((LV.k == ValKind::Ptr && RV.k == ValKind::I32) || (LV.k == ValKind::I32 && RV.k == ValKind::Ptr)) {
          // String repetition: str * int or int * str
          if (b.op != ast::BinaryOperator::Mul) throw std::runtime_error("unsupported op on str,int");
          std::string strV; std::string intV;
          if (LV.k == ValKind::Ptr) { strV = LV.s; intV = RV.s; }
          else { std::ostringstream z; z << "%t" << temp++; // ensure RHS int in i64
                 ir << "  " << z.str() << " = sext i32 " << LV.s << " to i64\n"; strV = RV.s; intV = z.str(); }
          if (LV.k == ValKind::Ptr && RV.k == ValKind::I32) { std::ostringstream z; z << "%t" << temp++; ir << "  " << z.str() << " = sext i32 " << RV.s << " to i64\n"; intV = z.str(); }
          ir << "  " << reg.str() << " = call ptr @pycc_string_repeat(ptr " << strV << ", i64 " << intV << ")\n";
          out = Value{reg.str(), ValKind::Ptr};
          return;
        } else {
          throw std::runtime_error("arithmetic type mismatch");
        }
      }
      void visit(const ast::ReturnStmt&) override { throw std::runtime_error("internal: return not expr"); }
      void visit(const ast::AssignStmt&) override { throw std::runtime_error("internal: assign not expr"); }
      void visit(const ast::IfStmt&) override { throw std::runtime_error("internal: if not expr"); }
      void visit(const ast::ExprStmt&) override { throw std::runtime_error("internal: exprstmt not expr"); }
      void visit(const ast::TupleLiteral&) override { throw std::runtime_error("internal: tuple not expr"); }
      // handled above
      void visit(const ast::FunctionDef&) override { throw std::runtime_error("internal: fn not expr"); }
      void visit(const ast::Module&) override { throw std::runtime_error("internal: mod not expr"); }
    }; // struct ExpressionLowerer
    // NOLINTEND

    auto evalExpr = [&](const ast::Expr* e) -> Value {
      if (!e) throw std::runtime_error("null expr");
      ExpressionLowerer V{irStream, temp, slots, sigs, retParamIdxs};
      return V.run(*e);
    };

    bool returned = false;
    int ifCounter = 0;

    // Basic capture analysis using 'nonlocal' statements as a starting signal.
    std::vector<std::string> capturedNames;
    struct CaptureScan : public ast::VisitorBase {
      std::vector<std::string>& out;
      explicit CaptureScan(std::vector<std::string>& o) : out(o) {}
      void visit(const ast::NonlocalStmt& ns) override {
        for (const auto& n : ns.names) out.push_back(n);
      }
      // Default traversal for body-containing nodes
      void visit(const ast::FunctionDef&) override {}
      void visit(const ast::Module&) override {}
      void visit(const ast::ReturnStmt&) override {}
      void visit(const ast::AssignStmt&) override {}
      void visit(const ast::IfStmt&) override {}
      void visit(const ast::ExprStmt&) override {}
      void visit(const ast::WhileStmt&) override {}
      void visit(const ast::ForStmt&) override {}
      void visit(const ast::TryStmt&) override {}
      void visit(const ast::IntLiteral&) override {}
      void visit(const ast::BoolLiteral&) override {}
      void visit(const ast::FloatLiteral&) override {}
      void visit(const ast::Name&) override {}
      void visit(const ast::Call&) override {}
      void visit(const ast::Binary&) override {}
      void visit(const ast::Unary&) override {}
      void visit(const ast::StringLiteral&) override {}
      void visit(const ast::NoneLiteral&) override {}
      void visit(const ast::TupleLiteral&) override {}
      void visit(const ast::ListLiteral&) override {}
      void visit(const ast::ObjectLiteral&) override {}
    };
    {
      CaptureScan scan{capturedNames};
      for (const auto& st : func->body) { if (st) st->accept(scan); }
      if (!capturedNames.empty()) {
        // Emit a simple env struct alloca holding pointers to captured variables, if available
        irStream << "  ; env for function '" << func->name << "' captures: ";
        for (size_t i = 0; i < capturedNames.size(); ++i) { if (i) irStream << ", "; irStream << capturedNames[i]; }
        irStream << "\n";
        std::ostringstream envTy; envTy << "{ "; for (size_t i = 0; i < capturedNames.size(); ++i) { if (i) envTy << ", "; envTy << "ptr"; } envTy << " }";
        std::ostringstream env; env << "%env." << func->name;
        irStream << "  " << env.str() << " = alloca " << envTy.str() << "\n";
        for (size_t i = 0; i < capturedNames.size(); ++i) {
          auto it = slots.find(capturedNames[i]); if (it == slots.end()) continue;
          std::ostringstream gep; gep << "%t" << temp++;
          irStream << "  " << gep.str() << " = getelementptr inbounds " << envTy.str() << ", ptr " << env.str() << ", i32 0, i32 " << i << "\n";
          irStream << "  store ptr " << it->second.ptr << ", ptr " << gep.str() << "\n";
        }
      }
    }

    struct StmtEmitter : public ast::VisitorBase {
      StmtEmitter(std::ostringstream& ir_, int& temp_, int& ifCounter_, std::unordered_map<std::string, Slot>& slots_, const ast::FunctionDef& fn_, std::function<Value(const ast::Expr*)> eval_, std::string& retStructTy_, std::vector<std::string>& tupleElemTys_, const std::unordered_map<std::string, Sig>& sigs_, const std::unordered_map<std::string, int>& retParamIdxs_, int subDbgId_, int& nextDbgId_, std::vector<DebugLoc>& dbgLocs_, std::unordered_map<unsigned long long, int>& dbgLocKeyToId_, std::unordered_map<std::string,int>& varMdId_, std::vector<DbgVar>& dbgVars_, int diIntId_, int diBoolId_, int diDoubleId_, int diPtrId_, int diExprId_)
        : ir(ir_), temp(temp_), ifCounter(ifCounter_), slots(slots_), fn(fn_), eval(std::move(eval_)), retStructTyRef(retStructTy_), tupleElemTysRef(tupleElemTys_), sigs(sigs_), retParamIdxs(retParamIdxs_), subDbgId(subDbgId_), nextDbgId(nextDbgId_), dbgLocs(dbgLocs_), dbgLocKeyToId(dbgLocKeyToId_), varMdId(varMdId_), dbgVars(dbgVars_), diIntId(diIntId_), diBoolId(diBoolId_), diDoubleId(diDoubleId_), diPtrId(diPtrId_), diExprId(diExprId_) {}
      std::ostringstream& ir;
      int& temp;
      int& ifCounter;
      std::unordered_map<std::string, Slot>& slots;
      const ast::FunctionDef& fn;
      std::function<Value(const ast::Expr*)> eval;
      bool returned{false};
      std::string& retStructTyRef;
      std::vector<std::string>& tupleElemTysRef;
      const std::unordered_map<std::string, Sig>& sigs;
      const std::unordered_map<std::string, int>& retParamIdxs;
      const int subDbgId;
      int& nextDbgId;
      std::vector<DebugLoc>& dbgLocs;
      std::unordered_map<unsigned long long, int>& dbgLocKeyToId;
      int curLocId{0};
      std::unordered_map<std::string,int>& varMdId;
      std::vector<DbgVar>& dbgVars;
      int diIntId; int diBoolId; int diDoubleId; int diPtrId; int diExprId;
      // Loop label stacks for break/continue
      std::vector<std::string> breakLabels;
      std::vector<std::string> continueLabels;
      // Exception check label for enclosing try (used by raise)
      std::string excCheckLabel;
      // Landingpad label when under try
      std::string lpadLabel;

      // Emit a call that may be turned into invoke (void return)
      void emitCallOrInvokeVoid(const std::string& calleeAndArgs) {
        if (!lpadLabel.empty()) {
          std::ostringstream cont; cont << "inv.cont" << temp++;
          ir << "  invoke void " << calleeAndArgs << " to label %" << cont.str() << " unwind label %" << lpadLabel << "\n";
          ir << cont.str() << ":\n";
        } else {
          ir << "  call void " << calleeAndArgs << "\n";
        }
      }
      // Emit a ptr-returning call that may be turned into invoke; returns the SSA name
      std::string emitCallOrInvokePtr(const std::string& dest, const std::string& calleeAndArgs) {
        if (!lpadLabel.empty()) {
          std::ostringstream cont; cont << "inv.cont" << temp++;
          ir << "  " << dest << " = invoke ptr " << calleeAndArgs << " to label %" << cont.str() << " unwind label %" << lpadLabel << "\n";
          ir << cont.str() << ":\n";
        } else {
          ir << "  " << dest << " = call ptr " << calleeAndArgs << "\n";
        }
        return dest;
      }

      static void emitLoc(std::ostringstream& irOut, const ast::Node& n, const char* kind) {
        irOut << "  ; loc: "
              << (n.file.empty() ? std::string("<unknown>") : n.file)
              << ":" << n.line << ":" << n.col
              << " (" << (kind ? kind : "") << ")\n";
      }

      void visit(const ast::AssignStmt& asg) override {
        emitLoc(ir, asg, "assign");
        if (asg.line > 0) {
          const unsigned long long key = (static_cast<unsigned long long>(static_cast<unsigned int>(subDbgId)) << 32ULL)
                                       ^ (static_cast<unsigned long long>((static_cast<unsigned int>(asg.line) << 16U) | static_cast<unsigned int>(asg.col)));
          auto itDbg = dbgLocKeyToId.find(key);
          if (itDbg != dbgLocKeyToId.end()) curLocId = itDbg->second; else {
            curLocId = nextDbgId++;
            dbgLocKeyToId[key] = curLocId;
            dbgLocs.push_back(DebugLoc{curLocId, asg.line, asg.col, subDbgId});
          }
        } else { curLocId = 0; }
        auto dbg = [this]() -> std::string { return (curLocId > 0) ? (std::string(", !dbg !") + std::to_string(curLocId)) : std::string(); };
        // First, support general target if provided (e.g., subscript store)
        if (!asg.targets.empty()) {
          const ast::Expr* tgtExpr = asg.targets[0].get();
          if (tgtExpr && tgtExpr->kind == ast::NodeKind::Subscript) {
            const auto* sub = static_cast<const ast::Subscript*>(tgtExpr);
            if (!sub->value || !sub->slice) { throw std::runtime_error("null subscript target"); }
            // Evaluate container first
            auto base = eval(sub->value.get());
            if (base.k != ValKind::Ptr) { throw std::runtime_error("subscript base must be pointer"); }
            bool isList = (sub->value->kind == ast::NodeKind::ListLiteral);
            bool isDict = (sub->value->kind == ast::NodeKind::DictLiteral);
            if (!isList && !isDict && sub->value->kind == ast::NodeKind::Name) {
              const auto* nm = static_cast<const ast::Name*>(sub->value.get());
              auto itn = slots.find(nm->id); if (itn != slots.end()) { isList = (itn->second.tag == PtrTag::List); isDict = (itn->second.tag == PtrTag::Dict); }
            }
            if (!isList && !isDict) { throw std::runtime_error("only list/dict subscripting supported in assignment"); }
            // Evaluate RHS and box to ptr if needed
            auto rv = eval(asg.value.get());
            std::string vptr;
            if (rv.k == ValKind::Ptr) { vptr = rv.s; }
            else if (rv.k == ValKind::I32) {
              if (!rv.s.empty() && rv.s[0] != '%') { std::ostringstream w2; w2 << "%t" << temp++; ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << rv.s << ")" << dbg() << "\n"; vptr = w2.str(); }
              else { std::ostringstream w, w2; w << "%t" << temp++; w2 << "%t" << temp++; ir << "  " << w.str() << " = sext i32 " << rv.s << " to i64" << dbg() << "\n"; ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << w.str() << ")" << dbg() << "\n"; vptr = w2.str(); }
            } else if (rv.k == ValKind::F64) { std::ostringstream w; w << "%t" << temp++; ir << "  " << w.str() << " = call ptr @pycc_box_float(double " << rv.s << ")" << dbg() << "\n"; vptr = w.str(); }
            else if (rv.k == ValKind::I1) { std::ostringstream w; w << "%t" << temp++; ir << "  " << w.str() << " = call ptr @pycc_box_bool(i1 " << rv.s << ")" << dbg() << "\n"; vptr = w.str(); }
            else { throw std::runtime_error("unsupported rhs for list store"); }
            if (isList) {
              // list index must be int
              auto idxV = eval(sub->slice.get());
              std::string idx64;
              if (idxV.k == ValKind::I32) { std::ostringstream z; z << "%t" << temp++; ir << "  " << z.str() << " = sext i32 " << idxV.s << " to i64" << dbg() << "\n"; idx64 = z.str(); }
              else { throw std::runtime_error("subscript index must be int"); }
              ir << "  call void @pycc_list_set(ptr " << base.s << ", i64 " << idx64 << ", ptr " << vptr << ")" << dbg() << "\n";
            } else {
              // dict_set takes boxed key and a slot; create a temp slot around base
              // Evaluate and box key as needed
              auto key = eval(sub->slice.get());
              std::string kptr;
              if (key.k == ValKind::Ptr) { kptr = key.s; }
              else if (key.k == ValKind::I32) {
                if (!key.s.empty() && key.s[0] != '%') { std::ostringstream w2; w2 << "%t" << temp++; ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << key.s << ")" << dbg() << "\n"; kptr = w2.str(); }
                else { std::ostringstream w, w2; w << "%t" << temp++; w2 << "%t" << temp++; ir << "  " << w.str() << " = sext i32 " << key.s << " to i64" << dbg() << "\n"; ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << w.str() << ")" << dbg() << "\n"; kptr = w2.str(); }
              } else if (key.k == ValKind::F64) { std::ostringstream w; w << "%t" << temp++; ir << "  " << w.str() << " = call ptr @pycc_box_float(double " << key.s << ")" << dbg() << "\n"; kptr = w.str(); }
              else if (key.k == ValKind::I1) { std::ostringstream w; w << "%t" << temp++; ir << "  " << w.str() << " = call ptr @pycc_box_bool(i1 " << key.s << ")" << dbg() << "\n"; kptr = w.str(); }
              else { throw std::runtime_error("unsupported dict key"); }
              std::ostringstream slot; slot << "%t" << temp++;
              ir << "  " << slot.str() << " = alloca ptr" << dbg() << "\n";
              ir << "  store ptr " << base.s << ", ptr " << slot.str() << dbg() << "\n";
              ir << "  call void @pycc_dict_set(ptr " << slot.str() << ", ptr " << kptr << ", ptr " << vptr << ")" << dbg() << "\n";
            }
            return;
          }
        }
        auto val = eval(asg.value.get());
        auto it = slots.find(asg.target);
        if (it == slots.end()) {
          std::string ptr = "%" + asg.target + ".addr";
          if (val.k == ValKind::I32) ir << "  " << ptr << " = alloca i32\n";
          else if (val.k == ValKind::I1) ir << "  " << ptr << " = alloca i1\n";
          else if (val.k == ValKind::F64) ir << "  " << ptr << " = alloca double\n";
          else { ir << "  " << ptr << " = alloca ptr\n"; ir << "  call void @llvm.gcroot(ptr " << ptr << ", ptr null)\n"; }
          slots[asg.target] = Slot{ptr, val.k};
          it = slots.find(asg.target);
          // Emit local variable debug declaration at first definition
          int varId = 0;
          auto itMd = varMdId.find(asg.target);
          if (itMd == varMdId.end()) { varId = nextDbgId++; varMdId[asg.target] = varId; }
          else { varId = itMd->second; }
          const int tyId = (val.k == ValKind::I32) ? diIntId : (val.k == ValKind::I1) ? diBoolId : (val.k == ValKind::F64) ? diDoubleId : diPtrId;
          dbgVars.push_back(DbgVar{varId, asg.target, subDbgId, asg.line, asg.col, tyId, 0, false});
          ir << "  call void @llvm.dbg.declare(metadata ptr " << ptr << ", metadata !" << varId << ", metadata !" << diExprId << ")" << dbg() << "\n";
        }
        if (it->second.kind != val.k) throw std::runtime_error("assignment type changed for variable");
        if (val.k == ValKind::I32) ir << "  store i32 " << val.s << ", ptr " << it->second.ptr << dbg() << "\n";
        else if (val.k == ValKind::I1) ir << "  store i1 " << val.s << ", ptr " << it->second.ptr << dbg() << "\n";
        else if (val.k == ValKind::F64) ir << "  store double " << val.s << ", ptr " << it->second.ptr << dbg() << "\n";
        else {
          ir << "  store ptr " << val.s << ", ptr " << it->second.ptr << dbg() << "\n";
          { std::ostringstream ca; ca << "@pycc_gc_write_barrier(ptr " << it->second.ptr << ", ptr " << val.s << ")"; emitCallOrInvokeVoid(ca.str()); }
        }
        if (val.k == ValKind::Ptr && asg.value) {
          // Tag from literal kinds
          if (asg.value->kind == ast::NodeKind::ListLiteral) { it->second.tag = PtrTag::List; }
          else if (asg.value->kind == ast::NodeKind::DictLiteral) { it->second.tag = PtrTag::Dict; }
          else if (asg.value->kind == ast::NodeKind::StringLiteral) { it->second.tag = PtrTag::Str; }
          else if (asg.value->kind == ast::NodeKind::ObjectLiteral) { it->second.tag = PtrTag::Object; }
          // Propagate tag from name-to-name assignments
          else if (asg.value->kind == ast::NodeKind::Name) {
            const auto* rhsName = dynamic_cast<const ast::Name*>(asg.value.get());
            if (rhsName != nullptr) {
              auto itSrc = slots.find(rhsName->id);
              if (itSrc != slots.end()) { it->second.tag = itSrc->second.tag; }
            }
          }
          // Simple function-return tag inference based on signature
          else if (asg.value->kind == ast::NodeKind::Call) {
            const auto* c = dynamic_cast<const ast::Call*>(asg.value.get());
            if (c && c->callee && c->callee->kind == ast::NodeKind::Name) {
              const auto* cname = dynamic_cast<const ast::Name*>(c->callee.get());
              if (cname != nullptr) {
                auto itSig = sigs.find(cname->id);
                if (itSig != sigs.end()) {
                  if (itSig->second.ret == ast::TypeKind::Str) { it->second.tag = PtrTag::Str; }
                  else if (itSig->second.ret == ast::TypeKind::List) { it->second.tag = PtrTag::List; }
                  else if (itSig->second.ret == ast::TypeKind::Dict) { it->second.tag = PtrTag::Dict; }
                }
                // Interprocedural propagation: if callee forwards one of its params, take tag from that arg
                auto itRet = retParamIdxs.find(cname->id);
                if (itRet != retParamIdxs.end()) {
                  int rp = itRet->second;
                  if (rp >= 0 && static_cast<size_t>(rp) < c->args.size()) {
                    const auto* a = c->args[rp].get();
                    if (a && a->kind == ast::NodeKind::Name) {
                      const auto* an = static_cast<const ast::Name*>(a);
                      auto itn = slots.find(an->id);
                      if (itn != slots.end()) { it->second.tag = itn->second.tag; }
                    }
                  }
                }
              }
            }
          }
        }
      }

      void visit(const ast::ReturnStmt& r) override {
        emitLoc(ir, r, "return");
        if (r.line > 0) {
          const unsigned long long key = (static_cast<unsigned long long>(static_cast<unsigned int>(subDbgId)) << 32ULL)
                                       ^ (static_cast<unsigned long long>((static_cast<unsigned int>(r.line) << 16U) | static_cast<unsigned int>(r.col)));
          auto itDbg = dbgLocKeyToId.find(key);
          if (itDbg != dbgLocKeyToId.end()) curLocId = itDbg->second; else {
            curLocId = nextDbgId++;
            dbgLocKeyToId[key] = curLocId;
            dbgLocs.push_back(DebugLoc{curLocId, r.line, r.col, subDbgId});
          }
        } else { curLocId = 0; }
        auto dbg = [this]() -> std::string { return (curLocId > 0) ? (std::string(", !dbg !") + std::to_string(curLocId)) : std::string(); };
        // Fast path: constant folding for len of literal aggregates/strings in returns
        if (fn.returnType == ast::TypeKind::Int && r.value && r.value->kind == ast::NodeKind::Call) {
          const auto* c = dynamic_cast<const ast::Call*>(r.value.get());
          if (c && c->callee && c->callee->kind == ast::NodeKind::Name && c->args.size() == 1) {
            const auto* nm = dynamic_cast<const ast::Name*>(c->callee.get());
            if (nm && nm->id == "len") {
              const auto* a0 = c->args[0].get();
              int retConst = -1;
              if (a0->kind == ast::NodeKind::TupleLiteral) {
                retConst = static_cast<int>(static_cast<const ast::TupleLiteral*>(a0)->elements.size());
              } else if (a0->kind == ast::NodeKind::ListLiteral) {
                retConst = static_cast<int>(static_cast<const ast::ListLiteral*>(a0)->elements.size());
              } else if (a0->kind == ast::NodeKind::StringLiteral) {
                retConst = static_cast<int>(static_cast<const ast::StringLiteral*>(a0)->value.size());
              }
              if (retConst >= 0) { ir << "  ret i32 " << retConst << dbg() << "\n"; returned = true; return; }
            }
          }
        }
        if (fn.returnType == ast::TypeKind::Tuple) {
          if (!r.value || r.value->kind != ast::NodeKind::TupleLiteral) throw std::runtime_error("tuple return requires tuple literal");
          const auto* tup = dynamic_cast<const ast::TupleLiteral*>(r.value.get());
          if (tupleElemTysRef.empty()) {
            tupleElemTysRef.reserve(tup->elements.size());
            for (size_t i = 0; i < tup->elements.size(); ++i) {
              const auto* e = tup->elements[i].get();
              if (e->kind == ast::NodeKind::FloatLiteral) tupleElemTysRef.push_back("double");
              else if (e->kind == ast::NodeKind::BoolLiteral) tupleElemTysRef.push_back("i1");
              else tupleElemTysRef.push_back("i32");
            }
          }
          std::ostringstream agg; agg << "%t" << temp++;
          ir << "  " << agg.str() << " = undef " << retStructTyRef << "\n";
          std::string cur = agg.str();
          for (size_t idx = 0; idx < tup->elements.size(); ++idx) {
            auto vi = eval(tup->elements[idx].get());
            const std::string& ety = (idx < tupleElemTysRef.size()) ? tupleElemTysRef[idx] : std::string("i32");
            if ((ety == "i32" && vi.k != ValKind::I32) || (ety == "double" && vi.k != ValKind::F64) || (ety == "i1" && vi.k != ValKind::I1))
              throw std::runtime_error("tuple element type mismatch");
            std::ostringstream nx; nx << "%t" << temp++;
            const char* valTy = (ety == "double") ? "double " : (ety == "i1") ? "i1 " : "i32 ";
            ir << "  " << nx.str() << " = insertvalue " << retStructTyRef << " " << cur << ", " << valTy << vi.s << ", " << idx << dbg() << "\n";
            cur = nx.str();
          }
          ir << "  ret " << retStructTyRef << " " << cur << dbg() << "\n";
          returned = true; return;
        }
        auto val = eval(r.value.get());
        const char* retStr = (fn.returnType == ast::TypeKind::Int) ? "i32" : (fn.returnType == ast::TypeKind::Bool) ? "i1" : (fn.returnType == ast::TypeKind::Float) ? "double" : "ptr";
        ir << "  ret " << retStr << " " << val.s << dbg() << "\n";
        returned = true;
      }

      void visit(const ast::IfStmt& iff) override {
        emitLoc(ir, iff, "if");
        if (iff.line > 0) {
          const unsigned long long key = (static_cast<unsigned long long>(static_cast<unsigned int>(subDbgId)) << 32ULL)
                                       ^ (static_cast<unsigned long long>((static_cast<unsigned int>(iff.line) << 16U) | static_cast<unsigned int>(iff.col)));
        
          auto itDbg = dbgLocKeyToId.find(key);
          if (itDbg != dbgLocKeyToId.end()) curLocId = itDbg->second; else { curLocId = nextDbgId++; dbgLocKeyToId[key] = curLocId; dbgLocs.push_back(DebugLoc{curLocId, iff.line, iff.col, subDbgId}); }
        } else { curLocId = 0; }
        auto dbg = [this]() -> std::string { return (curLocId > 0) ? (std::string(", !dbg !") + std::to_string(curLocId)) : std::string(); };
        auto c = eval(iff.cond.get());
        std::string cond = c.s;
        if (c.k == ValKind::I32) {
          std::ostringstream c1; c1 << "%t" << temp++;
          ir << "  " << c1.str() << " = icmp ne i32 " << c.s << ", 0" << dbg() << "\n";
          cond = c1.str();
        } else if (c.k == ValKind::I1) {
          // ok
        } else {
          throw std::runtime_error("if condition must be bool or int");
        }
        std::ostringstream thenLbl, elseLbl, endLbl;
        thenLbl << "if.then" << ifCounter;
        elseLbl << "if.else" << ifCounter;
        endLbl  << "if.end"  << ifCounter;
        ++ifCounter;
        ir << "  br i1 " << cond << ", label %" << thenLbl.str() << ", label %" << elseLbl.str() << dbg() << "\n";
        ir << thenLbl.str() << ":\n";
        bool thenR = emitStmtList(iff.thenBody);
        if (!thenR) ir << "  br label %" << endLbl.str() << dbg() << "\n";
        ir << elseLbl.str() << ":\n";
        bool elseR = emitStmtList(iff.elseBody);
        if (!elseR) ir << "  br label %" << endLbl.str() << dbg() << "\n";
        ir << endLbl.str() << ":\n";
      }

      void visit(const ast::WhileStmt& ws) override {
        emitLoc(ir, ws, "while");
        if (ws.line > 0) {
          const unsigned long long key = (static_cast<unsigned long long>(static_cast<unsigned int>(subDbgId)) << 32ULL)
                                       ^ (static_cast<unsigned long long>((static_cast<unsigned int>(ws.line) << 16U) | static_cast<unsigned int>(ws.col)));
          auto itDbg = dbgLocKeyToId.find(key);
          if (itDbg != dbgLocKeyToId.end()) curLocId = itDbg->second; else { curLocId = nextDbgId++; dbgLocKeyToId[key] = curLocId; dbgLocs.push_back(DebugLoc{curLocId, ws.line, ws.col, subDbgId}); }
        } else { curLocId = 0; }
        auto dbg = [this]() -> std::string { return (curLocId > 0) ? (std::string(", !dbg !") + std::to_string(curLocId)) : std::string(); };
        // Labels for while: cond, body, end
        std::ostringstream condLbl, bodyLbl, endLbl;
        condLbl << "while.cond" << ifCounter;
        bodyLbl << "while.body" << ifCounter;
        endLbl  << "while.end"  << ifCounter;
        ++ifCounter;
        // Initial branch to condition
        ir << "  br label %" << condLbl.str() << dbg() << "\n";
        ir << condLbl.str() << ":\n";
        auto c = eval(ws.cond.get());
        std::string cond = c.s;
        if (c.k == ValKind::I32) {
          std::ostringstream c1; c1 << "%t" << temp++;
          ir << "  " << c1.str() << " = icmp ne i32 " << c.s << ", 0" << dbg() << "\n";
          cond = c1.str();
        } else if (c.k != ValKind::I1) {
          throw std::runtime_error("while condition must be bool or int");
        }
        ir << "  br i1 " << cond << ", label %" << bodyLbl.str() << ", label %" << endLbl.str() << dbg() << "\n";
        // Body
        ir << bodyLbl.str() << ":\n";
        // Push loop labels for nested break/continue
        breakLabels.push_back(endLbl.str());
        continueLabels.push_back(condLbl.str());
        bool bodyReturned = emitStmtList(ws.thenBody);
        continueLabels.pop_back();
        breakLabels.pop_back();
        if (!bodyReturned) {
          // Re-evaluate condition
          ir << "  br label %" << condLbl.str() << dbg() << "\n";
        }
        // End
        ir << endLbl.str() << ":\n";
        // else-body executes only if loop exits normally (condition false)
        bool elseReturned = emitStmtList(ws.elseBody);
        (void)elseReturned;
      }

      void visit(const ast::BreakStmt& brk) override {
        emitLoc(ir, brk, "break");
        if (!breakLabels.empty()) {
          ir << "  br label %" << breakLabels.back() << "\n";
          returned = true;
        }
      }

      void visit(const ast::ContinueStmt& cont) override {
        emitLoc(ir, cont, "continue");
        if (!continueLabels.empty()) {
          ir << "  br label %" << continueLabels.back() << "\n";
          returned = true;
        }
      }

      void visit(const ast::AugAssignStmt& asg) override {
        emitLoc(ir, asg, "augassign");
        if (!asg.target || asg.target->kind != ast::NodeKind::Name) { return; }
        const auto* tgt = static_cast<const ast::Name*>(asg.target.get());
        auto it = slots.find(tgt->id);
        if (it == slots.end()) throw std::runtime_error("augassign to undefined name");
        std::ostringstream cur; cur << "%t" << temp++;
        if (it->second.kind == ValKind::I32)
          ir << "  " << cur.str() << " = load i32, ptr " << it->second.ptr << "\n";
        else if (it->second.kind == ValKind::F64)
          ir << "  " << cur.str() << " = load double, ptr " << it->second.ptr << "\n";
        else if (it->second.kind == ValKind::I1)
          ir << "  " << cur.str() << " = load i1, ptr " << it->second.ptr << "\n";
        else return;
        auto rhs = eval(asg.value.get());
        std::ostringstream res; res << "%t" << temp++;
        if (it->second.kind == ValKind::I32 && rhs.k == ValKind::I32) {
          const char* op = nullptr;
          switch (asg.op) {
            case ast::BinaryOperator::Add: op = "add"; break;
            case ast::BinaryOperator::Sub: op = "sub"; break;
            case ast::BinaryOperator::Mul: op = "mul"; break;
            case ast::BinaryOperator::Div: op = "sdiv"; break;
            case ast::BinaryOperator::Mod: op = "srem"; break;
            case ast::BinaryOperator::LShift: op = "shl"; break;
            case ast::BinaryOperator::RShift: op = "ashr"; break;
            case ast::BinaryOperator::BitAnd: op = "and"; break;
            case ast::BinaryOperator::BitOr: op = "or"; break;
            case ast::BinaryOperator::BitXor: op = "xor"; break;
            default: throw std::runtime_error("unsupported augassign op for int");
          }
          ir << "  " << res.str() << " = " << op << " i32 " << cur.str() << ", " << rhs.s << "\n";
          ir << "  store i32 " << res.str() << ", ptr " << it->second.ptr << "\n";
        } else if (it->second.kind == ValKind::F64 && rhs.k == ValKind::F64) {
          const char* op = nullptr;
          switch (asg.op) {
            case ast::BinaryOperator::Add: op = "fadd"; break;
            case ast::BinaryOperator::Sub: op = "fsub"; break;
            case ast::BinaryOperator::Mul: op = "fmul"; break;
            case ast::BinaryOperator::Div: op = "fdiv"; break;
            default: throw std::runtime_error("unsupported augassign op for float");
          }
          ir << "  " << res.str() << " = " << op << " double " << cur.str() << ", " << rhs.s << "\n";
          ir << "  store double " << res.str() << ", ptr " << it->second.ptr << "\n";
        } else if (it->second.kind == ValKind::I1 && rhs.k == ValKind::I1) {
          const char* op = nullptr;
          switch (asg.op) {
            case ast::BinaryOperator::BitXor: op = "xor"; break;
            case ast::BinaryOperator::BitOr: op = "or"; break;
            case ast::BinaryOperator::BitAnd: op = "and"; break;
            default: throw std::runtime_error("unsupported augassign op for bool");
          }
          ir << "  " << res.str() << " = " << op << " i1 " << cur.str() << ", " << rhs.s << "\n";
          ir << "  store i1 " << res.str() << ", ptr " << it->second.ptr << "\n";
        } else {
          throw std::runtime_error("augassign type mismatch");
        }
      }

      void visit(const ast::ForStmt& fs) override { // limited lowering: iterate list/tuple literals and dict keys
        emitLoc(ir, fs, "for");
        if (fs.line > 0) {
          const unsigned long long key = (static_cast<unsigned long long>(static_cast<unsigned int>(subDbgId)) << 32ULL)
                                       ^ (static_cast<unsigned long long>((static_cast<unsigned int>(fs.line) << 16U) | static_cast<unsigned int>(fs.col)));
          auto itDbg = dbgLocKeyToId.find(key);
          if (itDbg != dbgLocKeyToId.end()) curLocId = itDbg->second; else { curLocId = nextDbgId++; dbgLocKeyToId[key] = curLocId; dbgLocs.push_back(DebugLoc{curLocId, fs.line, fs.col, subDbgId}); }
        } else { curLocId = 0; }
        auto dbg = [this]() -> std::string { return (curLocId > 0) ? (std::string(", !dbg !") + std::to_string(curLocId)) : std::string(); };
        // Only support simple name target for now
        if (!fs.target || fs.target->kind != ast::NodeKind::Name) {
          // Best-effort: skip body if unsupported
          return;
        }
        const auto* tgt = static_cast<const ast::Name*>(fs.target.get());
        auto ensureSlotFor = [&](const std::string& name, ValKind kind) -> std::string {
          auto it = slots.find(name);
          if (it == slots.end()) {
            std::string ptr = "%" + name + ".addr";
            if (kind == ValKind::I32) ir << "  " << ptr << " = alloca i32\n";
            else if (kind == ValKind::I1) ir << "  " << ptr << " = alloca i1\n";
            else if (kind == ValKind::F64) ir << "  " << ptr << " = alloca double\n";
            else ir << "  " << ptr << " = alloca ptr\n";
            slots[name] = Slot{ptr, kind};
            // Debug declare for loop-target variable on first definition
            int varId = 0; auto itMd = varMdId.find(name);
            if (itMd == varMdId.end()) { varId = nextDbgId++; varMdId[name] = varId; } else { varId = itMd->second; }
            const int tyId = (kind == ValKind::I32) ? diIntId : (kind == ValKind::I1) ? diBoolId : (kind == ValKind::F64) ? diDoubleId : diPtrId;
            dbgVars.push_back(DbgVar{varId, name, subDbgId, fs.line, fs.col, tyId, 0, false});
            ir << "  call void @llvm.dbg.declare(metadata ptr " << ptr << ", metadata !" << varId << ", metadata !" << diExprId << ")" << dbg() << "\n";
            return ptr;
          }
          return it->second.ptr;
        };
        auto emitBodyWithValue = [&](const Value& v) {
          const std::string addr = ensureSlotFor(tgt->id, v.k);
          if (v.k == ValKind::I32) ir << "  store i32 " << v.s << ", ptr " << addr << dbg() << "\n";
          else if (v.k == ValKind::I1) ir << "  store i1 " << v.s << ", ptr " << addr << dbg() << "\n";
          else if (v.k == ValKind::F64) ir << "  store double " << v.s << ", ptr " << addr << dbg() << "\n";
          else { ir << "  store ptr " << v.s << ", ptr " << addr << dbg() << "\n"; { std::ostringstream ca; ca << "@pycc_gc_write_barrier(ptr " << addr << ", ptr " << v.s << ")"; emitCallOrInvokeVoid(ca.str()); } }
          (void)emitStmtList(fs.thenBody);
        };
        if (fs.iterable && fs.iterable->kind == ast::NodeKind::ListLiteral) {
          const auto* lst = static_cast<const ast::ListLiteral*>(fs.iterable.get());
          for (const auto& el : lst->elements) {
            if (!el) continue;
            auto v = eval(el.get());
            emitBodyWithValue(v);
          }
        } else if (fs.iterable && fs.iterable->kind == ast::NodeKind::TupleLiteral) {
          const auto* tp = static_cast<const ast::TupleLiteral*>(fs.iterable.get());
          for (const auto& el : tp->elements) {
            if (!el) continue; auto v = eval(el.get()); emitBodyWithValue(v);
          }
        } else if (fs.iterable && fs.iterable->kind == ast::NodeKind::Name) {
          // If dict, iterate keys using iterator API
          const auto* nm = static_cast<const ast::Name*>(fs.iterable.get());
          auto itn = slots.find(nm->id);
          if (itn != slots.end() && itn->second.kind == ValKind::Ptr && itn->second.tag == PtrTag::Dict) {
            std::ostringstream itv, key, condLbl, bodyLbl, endLbl;
            itv << "%t" << temp++;
            { std::ostringstream args; args << "@pycc_dict_iter_new(ptr " << itn->second.ptr << ")"; emitCallOrInvokePtr(itv.str(), args.str()); }
            condLbl << "for.cond" << ifCounter;
            bodyLbl << "for.body" << ifCounter;
            endLbl << "for.end" << ifCounter;
            ++ifCounter;
            ir << "  br label %" << condLbl.str() << dbg() << "\n";
            ir << condLbl.str() << ":\n";
            key << "%t" << temp++;
            { std::ostringstream args; args << "@pycc_dict_iter_next(ptr " << itv.str() << ")"; emitCallOrInvokePtr(key.str(), args.str()); }
            std::ostringstream test; test << "%t" << temp++;
            ir << "  " << test.str() << " = icmp ne ptr " << key.str() << ", null" << dbg() << "\n";
            ir << "  br i1 " << test.str() << ", label %" << bodyLbl.str() << ", label %" << endLbl.str() << dbg() << "\n";
            ir << bodyLbl.str() << ":\n";
            // bind key to target
            const std::string addr = ensureSlotFor(tgt->id, ValKind::Ptr);
            ir << "  store ptr " << key.str() << ", ptr " << addr << dbg() << "\n";
            { std::ostringstream ca; ca << "@pycc_gc_write_barrier(ptr " << addr << ", ptr " << key.str() << ")"; emitCallOrInvokeVoid(ca.str()); }
            (void)emitStmtList(fs.thenBody);
            ir << "  br label %" << condLbl.str() << dbg() << "\n";
            ir << endLbl.str() << ":\n";
            (void)emitStmtList(fs.elseBody);
            return;
          }
        } else {
          // Unsupported iterator in this subset; no-op
        }
        // for-else executes if loop completed normally (always true in this subset)
        (void)emitStmtList(fs.elseBody);
      }

      void visit(const ast::TryStmt& ts) override {
        emitLoc(ir, ts, "try");
        auto dbg = [this]() -> std::string { return (curLocId > 0) ? (std::string(", !dbg !") + std::to_string(curLocId)) : std::string(); };
        // Labels
        std::ostringstream chkLbl, excLbl, elseLbl, finLbl, endLbl, lpadLbl;
        chkLbl << "try.check" << ifCounter;
        excLbl << "try.except" << ifCounter;
        elseLbl << "try.else" << ifCounter;
        finLbl << "try.finally" << ifCounter;
        endLbl << "try.end" << ifCounter;
        lpadLbl << "try.lpad" << ifCounter;
        ++ifCounter;
        // Emit try body with EH landingpad and raise forwarding to check label
        const std::string prevExc = excCheckLabel;
        const std::string prevLpad = lpadLabel;
        excCheckLabel = chkLbl.str();
        lpadLabel = lpadLbl.str();
        bool bodyReturned = emitStmtList(ts.body);
        lpadLabel = prevLpad;
        excCheckLabel = prevExc;
        if (!bodyReturned) { ir << "  br label %" << chkLbl.str() << dbg() << "\n"; }
        // Landingpad to translate C++ EH into runtime pending-exception path
        ir << lpadLbl.str() << ":\n";
        ir << "  %lp" << temp++ << " = landingpad { ptr, i32 } cleanup\n";
        ir << "  br label %" << excLbl.str() << dbg() << "\n";
        ir << chkLbl.str() << ":\n";
        // Branch on pending exception
        std::ostringstream has; has << "%t" << temp++;
        ir << "  " << has.str() << " = call i1 @pycc_rt_has_exception()" << dbg() << "\n";
        ir << "  br i1 " << has.str() << ", label %" << excLbl.str() << ", label %" << elseLbl.str() << dbg() << "\n";
        // Except dispatch
        ir << excLbl.str() << ":\n";
        std::ostringstream excReg, tyReg; excReg << "%t" << temp++; tyReg << "%t" << temp++;
        ir << "  " << excReg.str() << " = call ptr @pycc_rt_current_exception()" << dbg() << "\n";
        ir << "  " << tyReg.str() << " = call ptr @pycc_rt_exception_type(ptr " << excReg.str() << ")" << dbg() << "\n";
        // Build match chain for handlers
        std::string fallthrough = finLbl.str();
        bool hasBare = false;
        int hidx = 0;
        std::vector<std::string> handlerLabels;
        for (const auto& h : ts.handlers) {
          if (!h) continue;
          std::ostringstream hl; hl << "handler." << hidx++;
          handlerLabels.push_back(hl.str());
          if (!h->type) { hasBare = true; continue; }
          if (h->type->kind == ast::NodeKind::Name) {
            const auto* n = static_cast<const ast::Name*>(h->type.get());
            // materialize handler type String from global cstring
            auto hash = [&](const std::string &str) {
              constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
              constexpr uint64_t kFnvPrime = 1099511628211ULL;
              uint64_t hv = kFnvOffsetBasis; for (unsigned char ch : str) { hv ^= ch; hv *= kFnvPrime; } return hv;
            };
            std::ostringstream gname; gname << ".str_" << std::hex << hash(n->id);
            std::ostringstream dataPtr, sObj, eq;
            dataPtr << "%t" << temp++; sObj << "%t" << temp++; eq << "%t" << temp++;
            ir << "  " << dataPtr.str() << " = getelementptr inbounds i8, ptr @" << gname.str() << ", i64 0" << dbg() << "\n";
            ir << "  " << sObj.str() << " = call ptr @pycc_string_new(ptr " << dataPtr.str() << ", i64 " << (long long)n->id.size() << ")" << dbg() << "\n";
            ir << "  " << eq.str() << " = call i1 @pycc_string_eq(ptr " << tyReg.str() << ", ptr " << sObj.str() << ")" << dbg() << "\n";
            ir << "  br i1 " << eq.str() << ", label %" << handlerLabels.back() << ", label %" << (hasBare ? "handler.bare" : finLbl.str()) << dbg() << "\n";
          } else {
            // Unsupported typed handler: fall through to bare or finally
            ir << "  br label %" << (hasBare ? "handler.bare" : finLbl.str()) << dbg() << "\n";
          }
        }
        if (hasBare) {
          ir << "handler.bare:\n";
          // Treat as match: clear exception and execute bare body
          { std::ostringstream ca; ca << "@pycc_rt_clear_exception()"; emitCallOrInvokeVoid(ca.str()); }
          // Find bare handler body index
          for (size_t i = 0; i < ts.handlers.size(); ++i) {
            const auto& h = ts.handlers[i]; if (!h || h->type) continue;
            // Bind name if provided
            if (!h->name.empty()) {
              std::string ptr = "%" + h->name + ".addr";
              // allocate slot (ptr)
              ir << "  " << ptr << " = alloca ptr\n";
              slots[h->name] = Slot{ptr, ValKind::Ptr};
              // dbg.declare omitted for brevity (could be added similarly to assigns)
              ir << "  store ptr " << excReg.str() << ", ptr " << ptr << dbg() << "\n";
              { std::ostringstream ca; ca << "@pycc_gc_write_barrier(ptr " << ptr << ", ptr " << excReg.str() << ")"; emitCallOrInvokeVoid(ca.str()); }
            }
            (void)emitStmtList(h->body);
            break;
          }
          ir << "  br label %" << finLbl.str() << dbg() << "\n";
        }
        // Typed handlers bodies
        for (size_t i = 0; i < ts.handlers.size(); ++i) {
          const auto& h = ts.handlers[i]; if (!h) continue; if (!h->type) continue;
          ir << handlerLabels[i] << ":\n";
          { std::ostringstream ca; ca << "@pycc_rt_clear_exception()"; emitCallOrInvokeVoid(ca.str()); }
          if (!h->name.empty()) {
            std::string ptr = "%" + h->name + ".addr";
            ir << "  " << ptr << " = alloca ptr\n";
            slots[h->name] = Slot{ptr, ValKind::Ptr};
            ir << "  store ptr " << excReg.str() << ", ptr " << ptr << dbg() << "\n";
            { std::ostringstream ca; ca << "@pycc_gc_write_barrier(ptr " << ptr << ", ptr " << excReg.str() << ")"; emitCallOrInvokeVoid(ca.str()); }
          }
          (void)emitStmtList(h->body);
          ir << "  br label %" << finLbl.str() << dbg() << "\n";
        }
        // Else block when no exception
        ir << elseLbl.str() << ":\n";
        (void)emitStmtList(ts.orelse);
        ir << "  br label %" << finLbl.str() << dbg() << "\n";
        // Finally always
        ir << finLbl.str() << ":\n";
        (void)emitStmtList(ts.finalbody);
        if (!excCheckLabel.empty()) {
          std::ostringstream has2; has2 << "%t" << temp++;
          ir << "  " << has2.str() << " = call i1 @pycc_rt_has_exception()" << dbg() << "\n";
          ir << "  br i1 " << has2.str() << ", label %" << excCheckLabel << ", label %" << endLbl.str() << dbg() << "\n";
        } else {
          ir << "  br label %" << endLbl.str() << dbg() << "\n";
        }
        ir << endLbl.str() << ":\n";
      }

      // Unused here
      void visit(const ast::Module&) override {}
      void visit(const ast::FunctionDef&) override {}
      void visit(const ast::IntLiteral&) override {}
      void visit(const ast::BoolLiteral&) override {}
      void visit(const ast::FloatLiteral&) override {}
      void visit(const ast::Name&) override {}
      void visit(const ast::Call&) override {}
      void visit(const ast::Binary&) override {}
      void visit(const ast::Unary&) override {}
      void visit(const ast::StringLiteral&) override {}
      void visit(const ast::NoneLiteral&) override {}
      void visit(const ast::ExprStmt& es) override { emitLoc(ir, es, "expr"); if (es.value) { (void)eval(es.value.get()); } }
      void visit(const ast::TupleLiteral&) override {}
      void visit(const ast::ListLiteral&) override {}
      void visit(const ast::ObjectLiteral&) override {}
      void visit(const ast::RaiseStmt& rs) override {
        emitLoc(ir, rs, "raise");
        // materialize type name and message strings from globals
        auto hash = [&](const std::string &str) {
          constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
          constexpr uint64_t kFnvPrime = 1099511628211ULL;
          uint64_t hv = kFnvOffsetBasis; for (unsigned char ch : str) { hv ^= ch; hv *= kFnvPrime; } return hv;
        };
        std::string typeName = "Exception";
        std::string msg = "";
        if (rs.exc) {
          if (rs.exc->kind == ast::NodeKind::Name) {
            typeName = static_cast<const ast::Name*>(rs.exc.get())->id;
          } else if (rs.exc->kind == ast::NodeKind::Call) {
            const auto* c = static_cast<const ast::Call*>(rs.exc.get());
            if (c->callee && c->callee->kind == ast::NodeKind::Name) { typeName = static_cast<const ast::Name*>(c->callee.get())->id; }
            if (!c->args.empty() && c->args[0] && c->args[0]->kind == ast::NodeKind::StringLiteral) {
              msg = static_cast<const ast::StringLiteral*>(c->args[0].get())->value;
            }
          }
        }
        std::ostringstream tgl, mgl, tptr, mptr; tgl << ".str_" << std::hex << hash(typeName); mgl << ".str_" << std::hex << hash(msg);
        tptr << "%t" << temp++; mptr << "%t" << temp++;
        ir << "  " << tptr.str() << " = getelementptr inbounds i8, ptr @" << tgl.str() << ", i64 0\n";
        ir << "  " << mptr.str() << " = getelementptr inbounds i8, ptr @" << mgl.str() << ", i64 0\n";
        if (!lpadLabel.empty()) {
          std::ostringstream cont; cont << "raise.cont" << temp++;
          ir << "  invoke void @pycc_rt_raise(ptr " << tptr.str() << ", ptr " << mptr.str() << ") to label %" << cont.str() << " unwind label %" << lpadLabel << "\n";
          ir << cont.str() << ":\n";
          if (!excCheckLabel.empty()) { ir << "  br label %" << excCheckLabel << "\n"; }
        } else {
          ir << "  call void @pycc_rt_raise(ptr " << tptr.str() << ", ptr " << mptr.str() << ")\n";
          if (!excCheckLabel.empty()) { ir << "  br label %" << excCheckLabel << "\n"; }
        }
        returned = true;
      }

      bool emitStmtList(const std::vector<std::unique_ptr<ast::Stmt>>& stmts) {
        bool brReturned = false;
        for (const auto& st : stmts) {
          StmtEmitter child{ir, temp, ifCounter, slots, fn, eval, retStructTyRef, tupleElemTysRef, sigs, retParamIdxs, subDbgId, nextDbgId, dbgLocs, dbgLocKeyToId, varMdId, dbgVars, diIntId, diBoolId, diDoubleId, diPtrId, diExprId};
          child.breakLabels = breakLabels;
          child.continueLabels = continueLabels;
          // Propagate exception/landingpad context into nested emitter
          child.excCheckLabel = excCheckLabel;
          child.lpadLabel = lpadLabel;
          st->accept(child);
          if (child.returned) brReturned = true;
        }
        return brReturned;
      }
    };

    StmtEmitter root{irStream, temp, ifCounter, slots, *func, evalExpr, retStructTy, tupleElemTys, sigs, retParamIdxs,
                     subDbgId, nextDbgId, dbgLocs, dbgLocKeyToId, varMdId, dbgVars, diIntId, diBoolId, diDoubleId,
                     diPtrId, diExprId};
    returned = root.emitStmtList(func->body);
    if (!returned) {
      // default return based on function type
      if (func->returnType == ast::TypeKind::Int) irStream << "  ret i32 0\n";
      else if (func->returnType == ast::TypeKind::Bool) irStream << "  ret i1 false\n";
      else if (func->returnType == ast::TypeKind::Float) irStream << "  ret double 0.0\n";
      else if (func->returnType == ast::TypeKind::Str) irStream << "  ret ptr null\n";
      else if (func->returnType == ast::TypeKind::Tuple) {
        if (retStructTy.empty()) { retStructTy = "{ i32, i32 }"; }
        // Build zero aggregate of appropriate arity with per-element types
        std::ostringstream agg; agg << "%t" << temp++;
        irStream << "  " << agg.str() << " = undef " << retStructTy << "\n";
        std::string cur = agg.str();
        // Count elements in struct by commas
        size_t elems = 1; for (auto c : retStructTy) if (c == ',') ++elems; // rough count
        for (size_t idx = 0; idx < elems; ++idx) {
          std::ostringstream nx; nx << "%t" << temp++;
          const std::string& ety = (idx < tupleElemTys.size()) ? tupleElemTys[idx] : std::string("i32");
          std::string zero = (ety == "double") ? std::string("double 0.0") : (ety == "i1") ? std::string("i1 false") : std::string("i32 0");
          irStream << "  " << nx.str() << " = insertvalue " << retStructTy << " " << cur << ", " << zero << ", " << idx << "\n";
          cur = nx.str();
        }
        irStream << "  ret " << retStructTy << " " << cur << "\n";
      }
    }
    irStream << "}\n\n";
  }
  // Module initialization stub (placeholder for future globals/registrations)
  irStream << "define i32 @pycc_module_init() {\n  ret i32 0\n}\n\n";
  // Emit lightweight debug metadata at end of module
  irStream << "\n!llvm.dbg.cu = !{!0}\n";
  irStream << "!0 = distinct !DICompileUnit(language: DW_LANG_Python, file: !1, producer: \"pycc\", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)\n";
  // Prefer the module's file name if present
  std::string diFileName = mod.file.empty() ? std::string("pycc") : mod.file;
  irStream << "!1 = !DIFile(filename: \"" << diFileName << "\", directory: \".\")\n";
  // Basic types and DIExpression
  irStream << "!" << diIntId << " = !DIBasicType(name: \"int\", size: 32, encoding: DW_ATE_signed)\n";
  irStream << "!" << diBoolId << " = !DIBasicType(name: \"bool\", size: 1, encoding: DW_ATE_boolean)\n";
  irStream << "!" << diDoubleId << " = !DIBasicType(name: \"double\", size: 64, encoding: DW_ATE_float)\n";
  irStream << "!" << diPtrId << " = !DIBasicType(name: \"ptr\", size: 64, encoding: DW_ATE_unsigned)\n";
  irStream << "!" << diExprId << " = !DIExpression()\n";
  for (const auto& ds : dbgSubs) {
    irStream << "!" << ds.id << " = distinct !DISubprogram(name: \"" << ds.name
             << "\", linkageName: \"" << ds.name
             << "\", scope: !1, file: !1, line: 1, unit: !0, spFlags: DISPFlagDefinition)\n";
  }
  for (const auto& dv : dbgVars) {
    irStream << "!" << dv.id << " = !DILocalVariable(name: \"" << dv.name << "\", scope: !" << dv.scope
             << ", file: !1, line: " << dv.line << ", type: !" << dv.typeId;
    if (dv.isParam) { irStream << ", arg: " << dv.argIndex; }
    irStream << ")\n";
  }
  for (const auto& dl : dbgLocs) {
    irStream << "!" << dl.id << " = !DILocation(line: " << dl.line << ", column: " << dl.col << ", scope: !" << dl.scope << ")\n";
  }
  // NOLINTEND
  return irStream.str();
}


bool Codegen::runCmd(const std::string& cmd, std::string& outErr) { // NOLINT(concurrency-mt-unsafe)
  // Simple wrapper around std::system; capture only exit code.
  // For Milestone 1 simplicity, we don't capture stdout/stderr streams.
  int statusCode = 0; statusCode = std::system(cmd.c_str()); // NOLINT(concurrency-mt-unsafe,cppcoreguidelines-init-variables)
  if (statusCode != 0) {
    outErr = "command failed: " + cmd + ", rc=" + std::to_string(statusCode);
    return false; // NOLINT(readability-simplify-boolean-expr)
  }
  return true;
}

} // namespace pycc::codegen
