#include "san_l5_regression/scenario_report.hpp"

#include <sstream>

namespace san_l5_regression {

const char* outcomeToString(Outcome o) {
    switch (o) {
        case Outcome::PASS:    return "PASS";
        case Outcome::FAIL:    return "FAIL";
        case Outcome::TIMEOUT: return "TIMEOUT";
        default:               return "ERROR";
    }
}

namespace {

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<int>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

}  // namespace

int ScenarioReportWriter::countPass() const {
    int n = 0;
    for (const auto& r : reports_) if (r.outcome == Outcome::PASS) ++n;
    return n;
}

int ScenarioReportWriter::countFail() const {
    int n = 0;
    for (const auto& r : reports_)
        if (r.outcome == Outcome::FAIL || r.outcome == Outcome::ERROR) ++n;
    return n;
}

int ScenarioReportWriter::countTimeout() const {
    int n = 0;
    for (const auto& r : reports_) if (r.outcome == Outcome::TIMEOUT) ++n;
    return n;
}

bool ScenarioReportWriter::allPassed() const {
    if (reports_.empty()) return false;
    return countFail() == 0 && countTimeout() == 0;
}

std::string ScenarioReportWriter::renderJson() const {
    std::ostringstream o;
    o << "{\n  \"summary\": {"
      << " \"pass\": "    << countPass()
      << ", \"fail\": "   << countFail()
      << ", \"timeout\": " << countTimeout()
      << ", \"total\": "  << reports_.size()
      << " },\n  \"scenarios\": [\n";
    for (std::size_t i = 0; i < reports_.size(); ++i) {
        const auto& r = reports_[i];
        o << "    {"
          << " \"id\": \""          << jsonEscape(r.id)          << "\""
          << ", \"description\": \"" << jsonEscape(r.description) << "\""
          << ", \"outcome\": \""     << outcomeToString(r.outcome) << "\""
          << ", \"deadline_ms\": "   << r.deadline_ms;
        if (r.elapsed_ms) o << ", \"elapsed_ms\": " << *r.elapsed_ms;
        else              o << ", \"elapsed_ms\": null";
        if (!r.fail_reason.empty())
            o << ", \"fail_reason\": \"" << jsonEscape(r.fail_reason) << "\"";
        if (!r.attributes.empty()) {
            o << ", \"attributes\": {";
            std::size_t j = 0;
            for (const auto& [k, v] : r.attributes) {
                if (j++) o << ", ";
                o << "\"" << jsonEscape(k) << "\": \""
                  << jsonEscape(v) << "\"";
            }
            o << "}";
        }
        o << " }";
        if (i + 1 < reports_.size()) o << ",";
        o << "\n";
    }
    o << "  ]\n}\n";
    return o.str();
}

std::string ScenarioReportWriter::renderMarkdown() const {
    std::ostringstream o;
    o << "# SAN v1.4 L5 regression report\n\n"
      << "Summary: **" << countPass() << " PASS** / "
      << countFail() << " FAIL / "
      << countTimeout() << " TIMEOUT / total "
      << reports_.size() << "\n\n";
    o << "| Scenario | Outcome | Elapsed | Deadline | Notes |\n"
      << "|---|---|---|---|---|\n";
    for (const auto& r : reports_) {
        o << "| **" << r.id << "** &mdash; " << r.description
          << " | " << outcomeToString(r.outcome)
          << " | ";
        if (r.elapsed_ms) o << *r.elapsed_ms << " ms";
        else              o << "&mdash;";
        o << " | " << r.deadline_ms << " ms"
          << " | ";
        if (!r.fail_reason.empty()) o << r.fail_reason;
        for (const auto& [k, v] : r.attributes) {
            o << "<br/>`" << k << "=" << v << "`";
        }
        o << " |\n";
    }
    return o.str();
}

}  // namespace san_l5_regression
