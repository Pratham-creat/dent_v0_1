#include "dent/io/lp_parser.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace dent {
namespace {

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    return s.substr(a, b-a);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

struct Term { double coefficient; std::string variable; };

std::vector<std::string> tokenize(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::vector<Term> parse_expression(const std::string& expression) {
    std::string s = expression;
    for (char& c : s) if (c == '*') c = ' ';
    std::vector<Term> terms;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i >= s.size()) break;

        double sign = 1.0;
        if (s[i] == '+') { ++i; }
        else if (s[i] == '-') { sign = -1.0; ++i; }

        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        std::size_t start = i;
        while (i < s.size() &&
               (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.' ||
                s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) {
            if ((s[i] == '+' || s[i] == '-') && i != start &&
                s[i-1] != 'e' && s[i-1] != 'E') break;
            ++i;
        }

        double coefficient = sign;
        if (i > start) {
            coefficient *= std::stod(s.substr(start, i-start));
        }

        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        std::size_t vstart = i;
        while (i < s.size() &&
               !std::isspace(static_cast<unsigned char>(s[i])) &&
               s[i] != '+' && s[i] != '-') ++i;
        if (i == vstart) throw std::runtime_error("LP parser: expected variable name.");
        terms.push_back({coefficient, s.substr(vstart, i-vstart)});
    }
    return terms;
}

struct ParsedConstraint {
    std::string name;
    ConstraintSense sense;
    double rhs;
    std::vector<Term> terms;
};

} // namespace

Problem LPParser::parse_file(const std::string& filename) {
    std::ifstream input(filename);
    if (!input) throw std::runtime_error("LP parser: cannot open file: " + filename);

    enum class Section { None, Objective, Constraints, Bounds, Binary, Integer };
    Section section = Section::None;
    ObjectiveSense objective_sense = ObjectiveSense::Minimize;
    std::vector<Term> objective_terms;
    std::vector<ParsedConstraint> constraints;
    std::unordered_map<std::string, std::pair<double,double>> bounds;
    std::vector<std::string> binaries, integers;
    std::string pending_name;

    std::string line;
    while (std::getline(input, line)) {
        auto comment = line.find("\\");
        if (comment != std::string::npos) line.resize(comment);
        line = trim(line);
        if (line.empty()) continue;

        const std::string h = lower(line);
        if (h == "minimize" || h == "minimum" || h == "min") {
            section = Section::Objective; objective_sense = ObjectiveSense::Minimize; continue;
        }
        if (h == "maximize" || h == "maximum" || h == "max") {
            section = Section::Objective; objective_sense = ObjectiveSense::Maximize; continue;
        }
        if (h == "subject to" || h == "such that" || h == "st" || h == "s.t.") {
            section = Section::Constraints; continue;
        }
        if (h == "bounds" || h == "bound") { section = Section::Bounds; continue; }
        if (h == "binary" || h == "binaries") { section = Section::Binary; continue; }
        if (h == "general" || h == "generals" || h == "integer" || h == "integers") {
            section = Section::Integer; continue;
        }
        if (h == "end") break;

        if (section == Section::Objective) {
            const auto colon = line.find(':');
            if (colon != std::string::npos) line = trim(line.substr(colon + 1));
            const auto terms = parse_expression(line);
            objective_terms.insert(objective_terms.end(), terms.begin(), terms.end());
            continue;
        }

        if (section == Section::Constraints) {
            const auto colon = line.find(':');
            std::string name = "c" + std::to_string(constraints.size() + 1);
            std::string body = line;
            if (colon != std::string::npos) {
                name = trim(line.substr(0, colon));
                body = trim(line.substr(colon + 1));
            }

            ConstraintSense sense;
            std::size_t pos = std::string::npos;
            std::string op;
            for (const char* candidate : {"<=", ">=", "="}) {
                auto p = body.find(candidate);
                if (p != std::string::npos && (pos == std::string::npos || p < pos)) {
                    pos = p; op = candidate;
                }
            }
            if (pos == std::string::npos)
                throw std::runtime_error("LP parser: constraint has no relation: " + line);

            if (op == "<=") sense = ConstraintSense::LessEqual;
            else if (op == ">=") sense = ConstraintSense::GreaterEqual;
            else sense = ConstraintSense::Equal;

            const std::string lhs = trim(body.substr(0, pos));
            const std::string rhs_text = trim(body.substr(pos + op.size()));
            double rhs = std::stod(rhs_text);
            constraints.push_back({name, sense, rhs, parse_expression(lhs)});
            continue;
        }

        if (section == Section::Bounds) {
            std::string s = lower(line);
            const auto toks = tokenize(s);
            if (toks.size() == 3 && toks[1] == "<=") {
                bounds[toks[2]] = {std::stod(toks[0]), 0.0};
            } else if (toks.size() == 3 && toks[1] == ">=") {
                bounds[toks[0]] = {std::stod(toks[2]), 0.0};
            } else if (toks.size() == 5 && toks[1] == "<=" && toks[3] == "<=") {
                bounds[toks[2]] = {std::stod(toks[0]), std::stod(toks[4])};
            } else if (toks.size() == 4 && toks[1] == "<=" && toks[2] == "<=") {
                bounds[toks[0]] = {0.0, std::stod(toks[3])};
            } else if (toks.size() == 3 && toks[1] == "=") {
                const double v = std::stod(toks[2]);
                bounds[toks[0]] = {v, v};
            } else {
                throw std::runtime_error("LP parser: unsupported bounds line: " + line);
            }
            continue;
        }

        if (section == Section::Binary) {
            for (const auto& v : tokenize(line)) binaries.push_back(v);
            continue;
        }
        if (section == Section::Integer) {
            for (const auto& v : tokenize(line)) integers.push_back(v);
            continue;
        }
    }

    std::unordered_map<std::string, int> index;
    std::vector<std::string> names;
    auto register_var = [&](const std::string& name) {
        if (!index.count(name)) {
            index[name] = static_cast<int>(names.size());
            names.push_back(name);
        }
    };
    for (const auto& t : objective_terms) register_var(t.variable);
    for (const auto& c : constraints) for (const auto& t : c.terms) register_var(t.variable);
    for (const auto& [v, _] : bounds) register_var(v);
    for (const auto& v : binaries) register_var(v);
    for (const auto& v : integers) register_var(v);

    Problem problem(objective_sense);
    std::unordered_map<std::string, VariableType> types;
    for (const auto& v : integers) types[v] = VariableType::Integer;
    for (const auto& v : binaries) types[v] = VariableType::Binary;

    for (const auto& name : names) {
        double lo = 0.0, hi = 0.0;
        auto it = bounds.find(name);
        if (it != bounds.end()) { lo = it->second.first; hi = it->second.second; }
        VariableType type = VariableType::Continuous;
        auto tt = types.find(name);
        if (tt != types.end()) type = tt->second;
        if (type == VariableType::Binary) { lo = 0.0; hi = 1.0; }
        problem.add_variable(name, lo, hi, type);
    }

    for (const auto& t : objective_terms)
        problem.set_objective_coefficient(index.at(t.variable), t.coefficient);

    for (const auto& c : constraints) {
        const int row = problem.add_constraint(c.name, c.sense, c.rhs);
        for (const auto& t : c.terms)
            problem.set_constraint_coefficient(row, index.at(t.variable), t.coefficient);
    }
    return problem;
}

} // namespace dent
