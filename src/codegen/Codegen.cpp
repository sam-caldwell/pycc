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
#include "ast/IntLiteral.h"
#include "ast/ListLiteral.h"
#include "ast/ObjectLiteral.h"
#include "ast/Module.h"
#include "ast/Name.h"
#include "ast/NodeKind.h"
#include "ast/NoneLiteral.h"
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
  result.llPath = emitLL_ ? changeExt(outBase, ".ll") : std::string();
  if (emitLL_) {
    std::ofstream outFile(result.llPath, std::ios::binary);
    if (!outFile) { return "failed to write .ll to " + result.llPath; }
    outFile << irText;
  }

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
  // GC barrier declaration for pointer writes (C ABI)
  irStream << "declare void @pycc_gc_write_barrier(ptr, ptr)\n";
  // Future aggregate runtime calls (scaffold)
  irStream << "declare ptr @pycc_list_new(i64)\n";
  irStream << "declare void @pycc_list_push(ptr, ptr)\n";
  irStream << "declare i64 @pycc_list_len(ptr)\n";
  irStream << "declare ptr @pycc_object_new(i64)\n";
  irStream << "declare void @pycc_object_set(ptr, i64, ptr)\n";
  irStream << "declare ptr @pycc_object_get(ptr, i64)\n\n";
  // Boxing wrappers for primitives
  irStream << "declare ptr @pycc_box_int(i64)\n";
  irStream << "declare ptr @pycc_box_float(double)\n";
  irStream << "declare ptr @pycc_box_bool(i1)\n\n";

  // Pre-scan functions to gather signatures
  struct Sig { ast::TypeKind ret{ast::TypeKind::NoneType}; std::vector<ast::TypeKind> params; };
  std::unordered_map<std::string, Sig> sigs;
  for (const auto& funcSig : mod.functions) {
    Sig sig; sig.ret = funcSig->returnType;
    for (const auto& param : funcSig->params) { sig.params.push_back(param.type); }
    sigs[funcSig->name] = std::move(sig);
  }

  // Lightweight interprocedural summary: functions that consistently return the same parameter index
  std::unordered_map<std::string, int> retParamIdxs; // func -> param index
  for (const auto& f : mod.functions) {
    int retIdx = -1; bool hasReturn = false; bool consistent = true;
    for (const auto& st : f->body) {
      if (!consistent) break;
      if (st->kind == ast::NodeKind::ReturnStmt) {
        hasReturn = true;
        const auto* r = dynamic_cast<const ast::ReturnStmt*>(st.get());
        if (!(r && r->value && r->value->kind == ast::NodeKind::Name)) { consistent = false; break; }
        const auto* n = dynamic_cast<const ast::Name*>(r->value.get());
        if (!n) { consistent = false; break; }
        int idxFound = -1;
        for (size_t i = 0; i < f->params.size(); ++i) { if (f->params[i].name == n->id) { idxFound = static_cast<int>(i); break; } }
        if (idxFound < 0) { consistent = false; break; }
        if (retIdx < 0) retIdx = idxFound; else if (retIdx != idxFound) { consistent = false; break; }
      }
    }
    if (hasReturn && consistent && retIdx >= 0) { retParamIdxs[f->name] = retIdx; }
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
  };

  {
    StrCollector collector{&strGlobals, hash64};
    mod.accept(collector);
  }

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
        // Analyze function body for a tuple literal return to infer element types
        const ast::TupleLiteral* firstTup = nullptr;
        for (const auto& st : func->body) {
          if (st->kind == ast::NodeKind::ReturnStmt) {
            const auto* r = dynamic_cast<const ast::ReturnStmt*>(st.get());
            if ((r != nullptr) && r->value && r->value->kind == ast::NodeKind::TupleLiteral) {
              firstTup = dynamic_cast<const ast::TupleLiteral*>(r->value.get());
              break;
            }
          }
        }
        size_t arity = (firstTup != nullptr) ? firstTup->elements.size() : 2;
        if (arity == 0) { arity = 2; } // fallback
        for (size_t i = 0; i < arity; ++i) {
          std::string ty = "i32";
          if (firstTup != nullptr) {
            const auto* e = firstTup->elements[i].get();
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
    irStream << ") {\n";
    irStream << "entry:\n";

    enum class ValKind : std::uint8_t { I32, I1, F64, Ptr };
    enum class PtrTag : std::uint8_t { Unknown, Str, List, Object };
    struct Slot { std::string ptr; ValKind kind{}; PtrTag tag{PtrTag::Unknown}; };
    std::unordered_map<std::string, Slot> slots; // var -> slot
    int temp = 0;

    // Parameter allocas
    for (const auto& param : func->params) {
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
        Slot s{ptr, ValKind::Ptr}; s.tag = PtrTag::Str; slots[param.name] = s;
      } else {
        throw std::runtime_error("unsupported param type");
      }
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
        const size_t n = s.value.size() + 1; // include NUL
        std::ostringstream reg; reg << "%t" << temp++;
        // Allow toggling between opaque-pointer and typed-pointer GEP styles
#ifdef PYCC_USE_OPAQUE_PTR_GEP
        ir << "  " << reg.str() << " = getelementptr inbounds ([" << n << " x i8], ptr @" << gname.str() << ", i64 0, i64 0)\n";
#else
        // Typed-pointer form for broad clang compatibility
        ir << "  " << reg.str() << " = getelementptr inbounds [" << n << " x i8], [" << n << " x i8]* @" << gname.str() << ", i64 0, i64 0\n";
#endif
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
            std::ostringstream w, w2; w << "%t" << temp++; w2 << "%t" << temp++;
            ir << "  " << w.str() << " = sext i32 " << v.s << " to i64\n";
            ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << w.str() << ")\n";
            valPtr = w2.str();
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
            std::ostringstream w, w2; w << "%t" << temp++; w2 << "%t" << temp++;
            ir << "  " << w.str() << " = sext i32 " << v.s << " to i64\n";
            ir << "  " << w2.str() << " = call ptr @pycc_box_int(i64 " << w.str() << ")\n";
            elemPtr = w2.str();
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
        if (call.callee == nullptr || call.callee->kind != ast::NodeKind::Name) {
          throw std::runtime_error("unsupported callee expression");
        }
        const auto* nmCall = dynamic_cast<const ast::Name*>(call.callee.get());
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
        } else {
          if (V.k != ValKind::I1) throw std::runtime_error("'not' requires bool");
          std::ostringstream reg; reg << "%t" << temp++;
          ir << "  " << reg.str() << " = xor i1 " << V.s << ", true\n";
          out = Value{reg.str(), ValKind::I1};
        }
      }
      void visit(const ast::Binary& b) override {
        // Handle None comparisons to constants if possible
        bool isCmp = (b.op == ast::BinaryOperator::Eq || b.op == ast::BinaryOperator::Ne ||
                      b.op == ast::BinaryOperator::Lt || b.op == ast::BinaryOperator::Le ||
                      b.op == ast::BinaryOperator::Gt || b.op == ast::BinaryOperator::Ge);
        if (isCmp && (b.lhs->kind == ast::NodeKind::NoneLiteral || b.rhs->kind == ast::NodeKind::NoneLiteral)) {
          bool bothNone = (b.lhs->kind == ast::NodeKind::NoneLiteral && b.rhs->kind == ast::NodeKind::NoneLiteral);
          bool eq = (b.op == ast::BinaryOperator::Eq);
          if (bothNone) { out = Value{ eq ? std::string("true") : std::string("false"), ValKind::I1 }; return; }
          const ast::Expr* other = (b.lhs->kind == ast::NodeKind::NoneLiteral) ? b.rhs.get() : b.lhs.get();
          if (other && other->type() && *other->type() != ast::TypeKind::NoneType) {
            out = Value{ eq ? std::string("false") : std::string("true"), ValKind::I1 }; return;
          }
          // Unknown types: conservatively treat Eq as false, Ne as true
          out = Value{ eq ? std::string("false") : std::string("true"), ValKind::I1 }; return;
        }
        auto LV = run(*b.lhs);
        auto RV = run(*b.rhs);
        // Comparisons
        isCmp = (b.op == ast::BinaryOperator::Eq || b.op == ast::BinaryOperator::Ne ||
                      b.op == ast::BinaryOperator::Lt || b.op == ast::BinaryOperator::Le ||
                      b.op == ast::BinaryOperator::Gt || b.op == ast::BinaryOperator::Ge);
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
              default: break;
            }
            ir << "  " << r1.str() << " = fcmp " << pred << " double " << LV.s << ", " << RV.s << "\n";
          } else {
            throw std::runtime_error("mismatched types in comparison");
          }
          out = Value{r1.str(), ValKind::I1};
          return;
        }
        if (b.op == ast::BinaryOperator::And || b.op == ast::BinaryOperator::Or) {
          if (LV.k != ValKind::I1) throw std::runtime_error("logical LHS must be bool");
          static int scCounter = 0; int id = scCounter++;
          if (b.op == ast::BinaryOperator::And) {
            std::string rhsLbl = std::string("and.rhs") + std::to_string(id);
            std::string falseLbl = std::string("and.false") + std::to_string(id);
            std::string endLbl = std::string("and.end") + std::to_string(id);
            ir << "  br i1 " << LV.s << ", label %" << rhsLbl << ", label %" << falseLbl << "\n";
            ir << rhsLbl << ":\n";
            auto RV2 = run(*b.rhs);
            if (RV2.k != ValKind::I1) throw std::runtime_error("logical RHS must be bool");
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
            if (RV2.k != ValKind::I1) throw std::runtime_error("logical RHS must be bool");
            ir << "  br label %" << endLbl << "\n";
            ir << endLbl << ":\n";
            std::ostringstream rphi; rphi << "%t" << temp++;
            ir << "  " << rphi.str() << " = phi i1 [ true, %" << trueLbl << " ], [ " << RV2.s << ", %" << rhsLbl << " ]\n";
            out = Value{rphi.str(), ValKind::I1};
          }
          return;
        }
        // Arithmetic
        std::ostringstream reg; reg << "%t" << temp++;
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

    struct StmtEmitter : public ast::VisitorBase {
      StmtEmitter(std::ostringstream& ir_, int& temp_, int& ifCounter_, std::unordered_map<std::string, Slot>& slots_, const ast::FunctionDef& fn_, std::function<Value(const ast::Expr*)> eval_, std::string& retStructTy_, std::vector<std::string>& tupleElemTys_, const std::unordered_map<std::string, Sig>& sigs_, const std::unordered_map<std::string, int>& retParamIdxs_)
        : ir(ir_), temp(temp_), ifCounter(ifCounter_), slots(slots_), fn(fn_), eval(std::move(eval_)), retStructTyRef(retStructTy_), tupleElemTysRef(tupleElemTys_), sigs(sigs_), retParamIdxs(retParamIdxs_) {}
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

      void visit(const ast::AssignStmt& asg) override {
        auto val = eval(asg.value.get());
        auto it = slots.find(asg.target);
        if (it == slots.end()) {
          std::string ptr = "%" + asg.target + ".addr";
          if (val.k == ValKind::I32) ir << "  " << ptr << " = alloca i32\n";
          else if (val.k == ValKind::I1) ir << "  " << ptr << " = alloca i1\n";
          else if (val.k == ValKind::F64) ir << "  " << ptr << " = alloca double\n";
          else ir << "  " << ptr << " = alloca ptr\n";
          slots[asg.target] = Slot{ptr, val.k};
          it = slots.find(asg.target);
        }
        if (it->second.kind != val.k) throw std::runtime_error("assignment type changed for variable");
        if (val.k == ValKind::I32) ir << "  store i32 " << val.s << ", ptr " << it->second.ptr << "\n";
        else if (val.k == ValKind::I1) ir << "  store i1 " << val.s << ", ptr " << it->second.ptr << "\n";
        else if (val.k == ValKind::F64) ir << "  store double " << val.s << ", ptr " << it->second.ptr << "\n";
        else {
          ir << "  store ptr " << val.s << ", ptr " << it->second.ptr << "\n";
          ir << "  call void @pycc_gc_write_barrier(ptr " << it->second.ptr << ", ptr " << val.s << ")\n";
        }
        if (val.k == ValKind::Ptr && asg.value) {
          // Tag from literal kinds
          if (asg.value->kind == ast::NodeKind::ListLiteral) { it->second.tag = PtrTag::List; }
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
                  else if (itSig->second.ret == ast::TypeKind::Dict) { it->second.tag = PtrTag::Object; }
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
            ir << "  " << nx.str() << " = insertvalue " << retStructTyRef << " " << cur << ", " << valTy << vi.s << ", " << idx << "\n";
            cur = nx.str();
          }
          ir << "  ret " << retStructTyRef << " " << cur << "\n";
          returned = true; return;
        }
        auto val = eval(r.value.get());
        const char* retStr = (fn.returnType == ast::TypeKind::Int) ? "i32" : (fn.returnType == ast::TypeKind::Bool) ? "i1" : (fn.returnType == ast::TypeKind::Float) ? "double" : "ptr";
        ir << "  ret " << retStr << " " << val.s << "\n";
        returned = true;
      }

      void visit(const ast::IfStmt& iff) override {
        auto c = eval(iff.cond.get());
        std::string cond = c.s;
        if (c.k == ValKind::I32) {
          std::ostringstream c1; c1 << "%t" << temp++;
          ir << "  " << c1.str() << " = icmp ne i32 " << c.s << ", 0\n";
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
        ir << "  br i1 " << cond << ", label %" << thenLbl.str() << ", label %" << elseLbl.str() << "\n";
        ir << thenLbl.str() << ":\n";
        bool thenR = emitStmtList(iff.thenBody);
        if (!thenR) ir << "  br label %" << endLbl.str() << "\n";
        ir << elseLbl.str() << ":\n";
        bool elseR = emitStmtList(iff.elseBody);
        if (!elseR) ir << "  br label %" << endLbl.str() << "\n";
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
      void visit(const ast::ExprStmt&) override {}
      void visit(const ast::TupleLiteral&) override {}
      void visit(const ast::ListLiteral&) override {}
      void visit(const ast::ObjectLiteral&) override {}

      bool emitStmtList(const std::vector<std::unique_ptr<ast::Stmt>>& stmts) {
        bool brReturned = false;
        for (const auto& st : stmts) {
          StmtEmitter child{ir, temp, ifCounter, slots, fn, eval, retStructTyRef, tupleElemTysRef, sigs, retParamIdxs};
          st->accept(child);
          if (child.returned) brReturned = true;
        }
        return brReturned;
      }
    };

    StmtEmitter root{irStream, temp, ifCounter, slots, *func, evalExpr, retStructTy, tupleElemTys, sigs, retParamIdxs};
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
  // NOLINTEND
  return irStream.str();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool Codegen::runCmd(const std::string& cmd, std::string& outErr) { // NOLINT(concurrency-mt-unsafe)
  // Simple wrapper around std::system; capture only exit code.
  // For Milestone 1 simplicity, we don't capture stdout/stderr streams.
  const int statusCode = std::system(cmd.c_str()); // NOLINT(concurrency-mt-unsafe)
  if (statusCode != 0) {
    outErr = "command failed: " + cmd + ", rc=" + std::to_string(statusCode);
    return false;
  }
  return true;
}

} // namespace pycc::codegen
