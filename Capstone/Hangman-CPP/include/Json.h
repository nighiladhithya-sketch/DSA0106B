// Json.h - tiny JSON object builder (overloaded add() for each value type)
#pragma once
#include <cstdio>
#include <string>
#include <vector>

class Json {
public:
    static std::string escape(const std::string &s) {
        std::string out = "\"";
        for (unsigned char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
                    else out += static_cast<char>(c);
            }
        }
        return out + "\"";
    }

    Json &add(const std::string &k, const std::string &v) { return raw(k, escape(v)); }
    Json &add(const std::string &k, const char *v)        { return raw(k, escape(v)); }
    Json &add(const std::string &k, int v)                { return raw(k, std::to_string(v)); }
    Json &add(const std::string &k, bool v)               { return raw(k, v ? "true" : "false"); }
    Json &add(const std::string &k, double v) {
        char buf[32]; std::snprintf(buf, sizeof buf, "%.2f", v); return raw(k, buf);
    }
    Json &add(const std::string &k, const std::vector<std::string> &items) {
        std::string arr = "[";
        for (std::size_t i = 0; i < items.size(); ++i) arr += (i ? "," : "") + escape(items[i]);
        return raw(k, arr + "]");
    }
    Json &raw(const std::string &k, const std::string &rawJson) {
        body_ += (body_.empty() ? "" : ",") + escape(k) + ":" + rawJson;
        return *this;                       // returning *this enables chaining
    }
    std::string str() const { return "{" + body_ + "}"; }

private:
    std::string body_;
};
