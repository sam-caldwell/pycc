#include "compiler/Compiler.h"
#include "sema/Sema.h"
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>

extern "C" long write(int, const void *, unsigned long);

namespace pycc {
    static void write_str(const char *str) {
        if (str == nullptr) { return; }
        const auto len = std::strlen(str);
        (void) write(2, str, static_cast<unsigned long>(len));
    }

    static void write_str(const std::string_view strView) {
        (void) write(2, strView.data(), static_cast<unsigned long>(strView.size()));
    }

    static void write_int(const int value) {
        const auto str = std::to_string(value);
        (void) write(2, str.c_str(), static_cast<unsigned long>(str.size()));
    }

    // ANSI fragments
    static constexpr std::string_view kRed = "\033[31m";
    static constexpr std::string_view kBold = "\033[1m";
    static constexpr std::string_view kReset = "\033[0m";

    static void print_header(const sema::Diagnostic &diag, const bool color) {
        if (diag.file.empty()) { return; }
        if (color) { write_str(kBold); }
        write_str(diag.file);
        write_str(":");
        write_int(diag.line);
        write_str(":");
        write_int(diag.col);
        write_str(": ");
        if (color) { write_str(kReset); }
    }

    static void print_label(const bool color) {
        if (color) {
            write_str(kRed);
            write_str("error: ");
            write_str(kReset);
        } else { write_str("error: "); }
    }

    static void print_source_with_caret(const sema::Diagnostic &diag) {
        if (diag.file.empty() || diag.line <= 0 || diag.col <= 0) { return; }
        std::ifstream input(diag.file);
        if (!input) { return; }
        std::string lineStr;
        int curLine = 1;
        while (curLine < diag.line && std::getline(input, lineStr)) { ++curLine; }
        if (curLine != diag.line || !std::getline(input, lineStr)) { return; }
        write_str("  ");
        write_str(lineStr.c_str());
        write_str("\n");
        write_str("  ");
        for (int i = 1; i < diag.col; ++i) { write_str(" "); }
        write_str("^\n");
    }

    static void print_context(const int context) {
        if (context <= 0) { return; }
        write_str("  (context lines: ");
        write_int(context);
        write_str(")\n");
    }

    void Compiler::print_error(const sema::Diagnostic &diag, const bool color, const int context) {
        print_header(diag, color);
        print_label(color);
        write_str(diag.message);
        write_str("\n");
        print_source_with_caret(diag);
        print_context(context);
    }
} // namespace pycc
