// FilmNotation.cpp
#include "FilmNotation.h"

#include <cctype>
#include <cstdlib>

namespace ofd {
namespace optics {

namespace {

bool isSymbolChar(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

std::string trim(const std::string &s)
{
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// 層構成の再帰下降パーサ。pos は文字位置。
struct Parser {
    const std::string &s;
    std::size_t pos = 0;
    int maxLayers = 512;
    std::string error;

    explicit Parser(const std::string &text, int maxL) : s(text), maxLayers(maxL) {}

    void skipSpace()
    {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos])))
            ++pos;
    }

    // 数 (係数 / 繰り返し回数)。読めなければ false (pos は動かさない)。
    bool readNumber(double &out)
    {
        skipSpace();
        const std::size_t start = pos;
        while (pos < s.size() &&
               (std::isdigit(static_cast<unsigned char>(s[pos])) || s[pos] == '.'))
            ++pos;
        if (pos == start) return false;
        const std::string tok = s.substr(start, pos - start);
        // "1.2.3" のような入力を弾く (strtod は途中まで読んでしまう)
        char *end = nullptr;
        const double v = std::strtod(tok.c_str(), &end);
        if (!end || *end != '\0') { pos = start; error = "malformed number"; return false; }
        out = v;
        return true;
    }

    // seq := (group | term)*   終端は ')' ']' か文字列末尾
    bool parseSeq(std::vector<NotationLayer> &out, int depth)
    {
        if (depth > 16) { error = "nesting too deep"; return false; }
        for (;;) {
            skipSpace();
            if (pos >= s.size()) return true;
            const char c = s[pos];
            if (c == ')' || c == ']') return true;

            if (c == '(' || c == '[') {
                const char close = (c == '(') ? ')' : ']';
                ++pos;
                std::vector<NotationLayer> inner;
                if (!parseSeq(inner, depth + 1)) return false;
                skipSpace();
                if (pos >= s.size() || s[pos] != close) {
                    error = "unbalanced bracket";
                    return false;
                }
                ++pos;
                skipSpace();
                if (pos >= s.size() || s[pos] != '^') {
                    error = "'^' with a repeat count is required after a group";
                    return false;
                }
                ++pos;
                double rep = 0.0;
                if (!readNumber(rep)) {
                    if (error.empty()) error = "missing repeat count after '^'";
                    return false;
                }
                const int n = static_cast<int>(rep);
                if (rep <= 0.0 || static_cast<double>(n) != rep) {
                    error = "repeat count must be a positive integer";
                    return false;
                }
                if (inner.empty()) { error = "empty group"; return false; }
                // 展開後の総数を先に見積もってから積む (爆発を防ぐ)
                const long long total =
                    static_cast<long long>(out.size()) +
                    static_cast<long long>(inner.size()) * n;
                if (total > maxLayers) { error = "too many layers"; return false; }
                for (int i = 0; i < n; ++i)
                    out.insert(out.end(), inner.begin(), inner.end());
                continue;
            }

            // term := [number] SYMBOL
            double q = 1.0;
            const bool hadNum = readNumber(q);
            if (!hadNum && !error.empty()) return false;
            skipSpace();
            if (pos >= s.size() || !isSymbolChar(s[pos])) {
                if (hadNum)          error = "a layer symbol must follow the coefficient";
                else if (pos >= s.size()) error = "the stack ends unexpectedly";
                else                 error = std::string("unexpected character '")
                                             + s[pos] + "'";
                return false;
            }
            if (hadNum && !(q > 0.0)) {
                error = "the thickness coefficient must be positive";
                return false;
            }
            if (static_cast<int>(out.size()) + 1 > maxLayers) {
                error = "too many layers";
                return false;
            }
            NotationLayer L;
            L.symbol = s[pos];
            L.qwot = q;
            out.push_back(L);
            ++pos;
        }
    }
};

// "@ 1550nm" / "@1.55um" → nm。読めなければ 0。
double parseWavelength(const std::string &tok)
{
    const std::string t = trim(tok);
    if (t.empty()) return 0.0;
    char *end = nullptr;
    const double v = std::strtod(t.c_str(), &end);
    if (!end || end == t.c_str() || !(v > 0.0)) return 0.0;
    std::string unit = trim(std::string(end));
    for (char &c : unit) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (unit.empty() || unit == "nm")           return v;
    if (unit == "um" || unit == "\xc2\xb5m")    return v * 1000.0;   // µm (UTF-8)
    if (unit == "m")                            return v * 1e9;
    return 0.0;
}

} // namespace

NotationResult parseNotation(const std::string &text, int maxLayers)
{
    NotationResult r;
    if (maxLayers < 1) maxLayers = 1;

    // ── `|` で区切る ────────────────────────────────────────────────────────
    std::vector<std::string> parts;
    std::string cur;
    for (char c : text) {
        if (c == '|') { parts.push_back(cur); cur.clear(); }
        else            cur.push_back(c);
    }
    parts.push_back(cur);

    std::string stackExpr, tail;
    if (parts.size() == 1) {
        stackExpr = parts[0];
    } else if (parts.size() == 3) {
        r.incident = trim(parts[0]);
        stackExpr  = parts[1];
        tail       = parts[2];
    } else {
        r.error = "use either no '|' or exactly two (incident | stack | substrate)";
        return r;
    }

    // ── 3 区画目: 基板 + 記号割当 + 設計波長 ────────────────────────────────
    // 空白区切りのトークンに分け、`=` を含むものは割当、`@` 以降は波長、
    // それ以外の最初のトークンを基板名とする。
    {
        std::vector<std::string> toks;
        std::string t;
        for (char c : tail) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!t.empty()) { toks.push_back(t); t.clear(); }
            } else {
                t.push_back(c);
            }
        }
        if (!t.empty()) toks.push_back(t);

        bool wantLambda = false;
        for (const std::string &tok : toks) {
            if (wantLambda) {                       // "@" の次のトークン
                r.lambda0_nm = parseWavelength(tok);
                if (!(r.lambda0_nm > 0.0)) {
                    r.error = "cannot read the design wavelength after '@'";
                    return r;
                }
                wantLambda = false;
                continue;
            }
            if (tok == "@") { wantLambda = true; continue; }
            if (tok.size() > 1 && tok[0] == '@') {  // "@1550nm" のような連結
                r.lambda0_nm = parseWavelength(tok.substr(1));
                if (!(r.lambda0_nm > 0.0)) {
                    r.error = "cannot read the design wavelength after '@'";
                    return r;
                }
                continue;
            }
            const std::size_t eq = tok.find('=');
            if (eq != std::string::npos) {
                if (eq != 1 || !isSymbolChar(tok[0]) || eq + 1 >= tok.size()) {
                    r.error = "a material assignment must be '<letter>=<material>'";
                    return r;
                }
                r.assign[tok[0]] = tok.substr(eq + 1);
                continue;
            }
            if (r.substrate.empty()) r.substrate = tok;
            else { r.error = "unexpected token '" + tok + "' after the substrate"; return r; }
        }
        if (wantLambda) { r.error = "'@' is not followed by a wavelength"; return r; }
    }

    // ── 層構成の展開 ────────────────────────────────────────────────────────
    Parser p(stackExpr, maxLayers);
    if (!p.parseSeq(r.layers, 0)) {
        r.error = p.error.empty() ? "cannot parse the stack" : p.error;
        r.layers.clear();
        return r;
    }
    p.skipSpace();
    if (p.pos < stackExpr.size()) {
        r.error = std::string("unexpected character '") + stackExpr[p.pos] + "'";
        r.layers.clear();
        return r;
    }
    if (r.layers.empty()) {
        r.error = "the stack is empty";
        return r;
    }

    r.ok = true;
    return r;
}

} // namespace optics
} // namespace ofd
