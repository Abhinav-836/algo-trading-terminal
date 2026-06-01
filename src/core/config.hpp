#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Config — parses the project's config.yaml format:
//
//   section:
//     key: value          ← dotted key  "section.key"
//     list_key:           ← next lines starting with "- item" are list items
//       - item1
//       - item2
//     subsection:
//       subkey: value     ← dotted key  "section.subsection.subkey"
//
// Inline bracket lists [a, b, c] on the same line are also supported.
// Comment lines (# ...) and blank lines are skipped.
// ─────────────────────────────────────────────────────────────────────────────

class Config {
private:
    std::unordered_map<std::string, std::string> settings;
    std::vector<std::string> trading_symbols;
    std::vector<std::string> active_strategies;

    // ── string helpers ────────────────────────────────────────────────────────

    static std::string trim(const std::string& s) {
        const char* ws = " \t\r\n";
        size_t start = s.find_first_not_of(ws);
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(ws);
        return s.substr(start, end - start + 1);
    }

    static std::string strip_comment(const std::string& s) {
        // Remove everything from the first unquoted '#'
        bool in_quote = false;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '"') in_quote = !in_quote;
            if (!in_quote && s[i] == '#') return trim(s.substr(0, i));
        }
        return s;
    }

    static std::string strip_quotes(const std::string& s) {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
            return s.substr(1, s.size() - 2);
        return s;
    }

    // Count leading spaces to determine indent level
    static int indent_of(const std::string& raw_line) {
        int n = 0;
        for (char c : raw_line) {
            if (c == ' ')       n++;
            else if (c == '\t') n += 2;
            else break;
        }
        return n;
    }

    // Parse an inline bracket list:  [AAPL, MSFT, GOOGL]
    static std::vector<std::string> parse_inline_list(const std::string& s) {
        std::vector<std::string> items;
        size_t start = s.find('[');
        size_t end   = s.find(']');
        if (start == std::string::npos || end == std::string::npos || end <= start)
            return items;
        std::stringstream ss(s.substr(start + 1, end - start - 1));
        std::string item;
        while (std::getline(ss, item, ',')) {
            item = trim(strip_quotes(trim(item)));
            if (!item.empty()) items.push_back(item);
        }
        return items;
    }

public:
    Config() = default;

    bool load_from_file(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open " << filename << ", using defaults\n";
            load_defaults();
            return false;
        }

        // Read all lines with their raw indentation preserved
        struct RawLine { int indent; std::string content; };
        std::vector<RawLine> lines;

        std::string raw;
        while (std::getline(file, raw)) {
            // Normalise Windows CRLF
            if (!raw.empty() && raw.back() == '\r') raw.pop_back();

            int ind     = indent_of(raw);
            std::string content = trim(strip_comment(raw));
            if (content.empty()) continue;          // blank / comment-only
            lines.push_back({ind, content});
        }
        file.close();

        // ── Two-pass parse ────────────────────────────────────────────────
        // We track a section stack keyed by indent level.
        // e.g.  indent 0 → "trading"
        //        indent 2 → "weights"   gives full prefix "trading.weights"

        // Map from indent → section name component at that level
        std::unordered_map<int, std::string> section_at_indent;
        // The "pending list key" — if we see "key:" with nothing after, the
        // next "- item" lines belong to it.
        std::string pending_list_key;
        int         pending_list_indent = -1;

        auto current_prefix = [&](int ind) -> std::string {
            // Build dotted prefix from all section components with indent < ind
            std::string prefix;
            // Collect in order of indent
            std::vector<std::pair<int,std::string>> parts(
                section_at_indent.begin(), section_at_indent.end());
            std::sort(parts.begin(), parts.end());
            for (auto& [level, name] : parts) {
                if (level < ind) {
                    if (!prefix.empty()) prefix += '.';
                    prefix += name;
                }
            }
            return prefix;
        };

        for (size_t i = 0; i < lines.size(); i++) {
            int         ind  = lines[i].indent;
            std::string line = lines[i].content;

            // Remove section entries that are at same or deeper indent
            // (we've moved back out of them)
            for (auto it = section_at_indent.begin(); it != section_at_indent.end(); ) {
                if (it->first >= ind) it = section_at_indent.erase(it);
                else                  ++it;
            }

            // ── YAML list item:  "- value" ────────────────────────────────
            if (line.size() >= 2 && line[0] == '-' && line[1] == ' ') {
                std::string value = strip_quotes(trim(line.substr(2)));
                if (value.empty()) continue;

                // Which list does this belong to?
                if (pending_list_indent >= 0 && ind > pending_list_indent) {
                    // It belongs to pending_list_key
                    if (pending_list_key == "trading.symbols") {
                        trading_symbols.push_back(value);
                    } else if (pending_list_key == "strategies.active") {
                        active_strategies.push_back(value);
                    }
                    // Store individual items too for generic access
                    // (append index-keyed if needed — skip for now)
                }
                continue;
            }

            // ── Key : value  or  Key:  (section header or list key) ──────
            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;

            std::string key   = trim(line.substr(0, colon));
            std::string value = trim(line.substr(colon + 1));
            value = strip_comment(value);   // strip any trailing comment
            value = trim(value);
            value = strip_quotes(value);

            if (key.empty()) continue;

            std::string prefix = current_prefix(ind);
            std::string full_key = prefix.empty() ? key : prefix + '.' + key;

            if (value.empty()) {
                // Could be a section header or a pending list key
                // Check if next line is a list item or indented key
                if (i + 1 < lines.size()) {
                    std::string next = lines[i + 1].content;
                    if (!next.empty() && next[0] == '-') {
                        // This key introduces a list
                        pending_list_key    = full_key;
                        pending_list_indent = ind;
                    } else {
                        // This key introduces a sub-section
                        section_at_indent[ind] = key;
                        pending_list_indent = -1;
                    }
                } else {
                    section_at_indent[ind] = key;
                }
                continue;
            }

            // Inline bracket list on same line:  key: [a, b, c]
            if (value.front() == '[') {
                auto items = parse_inline_list(value);
                if (full_key == "trading.symbols") {
                    trading_symbols = items;
                } else if (full_key == "strategies.active") {
                    active_strategies = items;
                }
                continue;
            }

            // Regular scalar value
            settings[full_key] = value;
            pending_list_indent = -1;
        }

        // ── Apply defaults for anything missing ───────────────────────────
        if (settings.find("trading.mode")             == settings.end()) settings["trading.mode"]             = "paper";
        if (settings.find("trading.exchange")          == settings.end()) settings["trading.exchange"]          = "alpaca";
        if (settings.find("trading.initial_capital")   == settings.end()) settings["trading.initial_capital"]   = "100000";
        if (trading_symbols.empty())   trading_symbols   = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"};
        if (active_strategies.empty()) active_strategies = {"momentum"};

        return true;
    }

    void load_defaults() {
        settings["trading.mode"]           = "paper";
        settings["trading.exchange"]       = "alpaca";
        settings["trading.initial_capital"]= "100000";
        trading_symbols   = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"};
        active_strategies = {"momentum"};
    }

    // ── Getters ───────────────────────────────────────────────────────────────

    std::string get(const std::string& key, const std::string& default_val = "") const {
        auto it = settings.find(key);
        return it != settings.end() ? it->second : default_val;
    }

    double get_double(const std::string& key, double default_val = 0.0) const {
        try { return std::stod(get(key, std::to_string(default_val))); }
        catch (...) { return default_val; }
    }

    int get_int(const std::string& key, int default_val = 0) const {
        try { return std::stoi(get(key, std::to_string(default_val))); }
        catch (...) { return default_val; }
    }

    bool get_bool(const std::string& key, bool default_val = false) const {
        std::string val = get(key);
        if (val == "true"  || val == "1" || val == "yes" || val == "on")  return true;
        if (val == "false" || val == "0" || val == "no"  || val == "off") return false;
        return default_val;
    }

    std::vector<std::string> get_symbols()           const { return trading_symbols;   }
    std::vector<std::string> get_active_strategies() const { return active_strategies; }

    double get_strategy_param(const std::string& strategy,
                              const std::string& param,
                              double default_val) const {
        return get_double("strategies." + strategy + "." + param, default_val);
    }

    int get_strategy_param_int(const std::string& strategy,
                               const std::string& param,
                               int default_val) const {
        return get_int("strategies." + strategy + "." + param, default_val);
    }

    bool is_strategy_enabled(const std::string& strategy) const {
        return std::find(active_strategies.begin(), active_strategies.end(), strategy)
               != active_strategies.end();
    }

    void print_config() const {
        std::cout << "\n📋 Current Configuration:\n";
        std::cout << "═══════════════════════════════════════════════════════════\n";
        std::cout << "  Mode:            " << get("trading.mode")     << "\n";
        std::cout << "  Exchange:        " << get("trading.exchange")  << "\n";
        std::cout << "  Initial Capital: $" << get_double("trading.initial_capital", 100000) << "\n";

        std::cout << "  Trading Symbols: ";
        for (size_t i = 0; i < trading_symbols.size(); i++) {
            std::cout << trading_symbols[i];
            if (i < trading_symbols.size() - 1) std::cout << ", ";
        }
        std::cout << "\n";

        std::cout << "  Active Strategies: ";
        for (size_t i = 0; i < active_strategies.size(); i++) {
            std::cout << active_strategies[i];
            if (i < active_strategies.size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
        std::cout << "═══════════════════════════════════════════════════════════\n\n";
    }

    // Debug: dump all parsed keys (useful during development)
    void dump_all() const {
        std::cout << "\n[Config dump — all parsed keys]\n";
        std::vector<std::pair<std::string,std::string>> sorted(settings.begin(), settings.end());
        std::sort(sorted.begin(), sorted.end());
        for (auto& [k, v] : sorted)
            std::cout << "  " << k << " = " << v << "\n";
        std::cout << "  trading_symbols:   ";
        for (auto& s : trading_symbols) std::cout << s << " ";
        std::cout << "\n  active_strategies: ";
        for (auto& s : active_strategies) std::cout << s << " ";
        std::cout << "\n\n";
    }
};