/***
 * Name: pycc::obs::IRDiff (impl)
 * Purpose: Diff two LLVM IR texts while optionally ignoring comments/debug.
 */
#include "observability/IRDiff.h"
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

namespace pycc::obs {
    static inline std::string trim_left(std::string s) {
        s.erase(
            s.begin(),
            std::ranges::find_if(
                s,
                [](const unsigned char c) { return !std::isspace(c); }
            )
        );
        return s;
    }

    static inline bool is_ignored_line(const std::string &line, const bool ignoreComments, const bool ignoreDebug) {
        if (line.empty()) return true;
        if (ignoreComments && !line.empty() && line[0] == ';') return true;
        if (ignoreDebug) {
            if (!line.empty() && line[0] == '!') return true; // pure metadata lines
            // Standalone metadata-only lines can also begin with whitespace then '!'
            if (const auto trimmed = trim_left(line); !trimmed.empty() && trimmed[0] == '!') return true;
            if (line.contains("!DILocation(")) return true;
            if (line.contains("!DICompileUnit(")) return true;
            if (line.contains("!DISubprogram(")) return true;
            if (line.contains("!DIFile(")) return true;
            if (line.contains("!DIBasicType(")) return true;
            if (line.contains("!DIExpression(")) return true;
        }
        return false;
    }

    static std::vector<std::string> filter_lines(const std::string &s, bool ignoreComments, const bool ignoreDebug) {
        std::istringstream in(s);
        std::vector<std::string> out;
        std::string line;
        while (std::getline(in, line)) {
            if (ignoreDebug) {
                // Strip inline debug attachments like ", !dbg !123"
                if (const auto pos = line.find("!dbg "); pos != std::string::npos) {
                    // find preceding comma that starts the attachment
                    if (const auto comma = line.rfind(',', pos); comma != std::string::npos) {
                        line.erase(comma);
                    }
                }
            }
            if (is_ignored_line(line, ignoreComments, ignoreDebug)) continue;
            out.push_back(trim_left(line));
        }
        return out;
    }

    std::string diffIR(const std::string &a, const std::string &b, bool ignoreComments, bool ignoreDebug) {
        const auto A = filter_lines(a, ignoreComments, ignoreDebug);
        const auto B = filter_lines(b, ignoreComments, ignoreDebug);
        std::ostringstream oss;
        const size_t na = A.size();
        const size_t nb = B.size();
        const size_t nmin = std::min(na, nb);
        for (size_t i = 0; i < nmin; ++i) {
            if (A[i] != B[i]) {
                oss << "- " << A[i] << "\n";
                oss << "+ " << B[i] << "\n";
            }
        }
        for (size_t i = nmin; i < na; ++i) { oss << "- " << A[i] << "\n"; }
        for (size_t i = nmin; i < nb; ++i) { oss << "+ " << B[i] << "\n"; }
        return oss.str();
    }
} // namespace pycc::obs
