/**
 * @file config.hpp
 * @brief Configuration for the optional COMTRADE AI analysis module: model endpoint, credentials,
 *        vendor request parameters and the local evidence-summarization limits. Every field is
 *        populated from an untrusted JSON file, so Config::fromJson validates each one and rejects
 *        unknown keys instead of silently ignoring a typo.
 */
#pragma once

#include "detail.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <regex>
#include <set>
#include <string>

namespace comtrade::ai {

using Json = nlohmann::json;

// One Chat-Completions-compatible HTTP(S) endpoint plus the local safety limits applied around it.
// summarize() only needs the limit fields below; analyze()/makeRequest() additionally need a valid
// endpoint and model, which is why validate() takes a `network` flag instead of always requiring them.
struct Config {
    std::string endpoint, model, token, tokenEnv = "COMTRADE_AI_TOKEN", pdfFontPath, caBundle;
    Json headers = Json::object(), requestParameters = Json::object();
    bool allowInsecureLocalhost = false, includeIdentifiers = false, fullWaveform = false;
    int timeoutMs = 60000, maxRequestBytes = 4000000, maxResponseBytes = 1000000;
    int maxChannels = 512, maxWindows = 256, windowMs = 20;
    int maxDigitalEvents = 1000, maxWaveformSamples = 100000;

    static Config fromJson(const Json &json);

    // Convenience for CLI tools and applications that keep their AI configuration in a JSON file
    // (see config/ai-config.example.json): reads and parses it, then delegates to fromJson() so both
    // paths share the exact same validation.
    static Config fromFile(const std::filesystem::path &path);

    void validate(bool network = false) const;
};

namespace detail {

struct Endpoint {
    std::string origin, path, host;
    bool secure;
};

// Parses and validates Config::endpoint. A single compiled regex enforces scheme/host/port syntax
// and rejects fragments, embedded credentials and other parts an HTTP client should not silently
// forward to a server.
inline Endpoint parseEndpoint(const Config &c) {
    static const std::regex pattern(
        R"(^(https?)://(\[[0-9a-fA-F:]+\]|[a-zA-Z0-9.-]+)(:[0-9]{1,5})?(/[^#\s]*)?$)");
    std::smatch m;
    require(std::regex_match(c.endpoint, m, pattern), "Invalid endpoint URL");
    std::string host = m[2];
    std::transform(host.begin(), host.end(), host.begin(),
                   [](unsigned char x) { return char(std::tolower(x)); });
    if (m[3].matched) {
        int port = std::stoi(m[3].str().substr(1));
        require(port > 0 && port <= 65535, "Invalid endpoint port");
    }
    bool secure = m[1] == "https";
    require(secure ||
                (c.allowInsecureLocalhost && (host == "localhost" || host == "127.0.0.1" || host == "[::1]")),
            "HTTPS required (except explicitly enabled loopback HTTP)");
    return {m[1].str() + "://" + m[2].str() + m[3].str(), m[4].matched ? m[4].str() : "/", host, secure};
}

} // namespace detail

inline Config Config::fromJson(const Json &j) {
    detail::require(j.is_object(), "Configuration must be an object");
    Config c;
    const std::set<std::string> known{"endpoint",
                                      "model",
                                      "token",
                                      "tokenEnv",
                                      "pdfFontPath",
                                      "caBundle",
                                      "headers",
                                      "requestParameters",
                                      "allowInsecureLocalhost",
                                      "includeIdentifiers",
                                      "waveformMode",
                                      "timeoutMs",
                                      "maxRequestBytes",
                                      "maxResponseBytes",
                                      "maxChannels",
                                      "maxWindows",
                                      "windowMs",
                                      "maxDigitalEvents",
                                      "maxWaveformSamples"};
    for (auto it = j.begin(); it != j.end(); ++it)
        detail::require(known.count(it.key()) != 0, "Unknown configuration field");
    auto string = [&](const char *key, std::string fallback = {}) {
        detail::require(!j.contains(key) || j[key].is_string(), "Expected configuration string");
        return j.value(key, fallback);
    };
    auto integer = [&](const char *key, int fallback) {
        if (!j.contains(key))
            return fallback;
        detail::require(j[key].is_number_integer(), "Expected integer");
        auto v = j[key].get<double>();
        detail::require(v >= 0 && v <= std::numeric_limits<int>::max(), "Integer limit out of range");
        return int(v);
    };
    auto flag = [&](const char *key) {
        detail::require(!j.contains(key) || j[key].is_boolean(), "Expected boolean");
        return j.value(key, false);
    };
    c.endpoint = string("endpoint");
    c.model = string("model");
    c.token = string("token");
    c.tokenEnv = string("tokenEnv", c.tokenEnv);
    c.pdfFontPath = string("pdfFontPath");
    c.caBundle = string("caBundle");
    for (auto key : {"headers", "requestParameters"})
        detail::require(!j.contains(key) || j[key].is_object(), "Expected object");
    c.headers = j.value("headers", Json::object());
    c.requestParameters = j.value("requestParameters", Json::object());
    c.allowInsecureLocalhost = flag("allowInsecureLocalhost");
    c.includeIdentifiers = flag("includeIdentifiers");
    auto mode = string("waveformMode", "summary");
    detail::require(mode == "summary" || mode == "full", "Invalid waveformMode");
    c.fullWaveform = mode == "full";
    c.timeoutMs = integer("timeoutMs", c.timeoutMs);
    c.maxRequestBytes = integer("maxRequestBytes", c.maxRequestBytes);
    c.maxResponseBytes = integer("maxResponseBytes", c.maxResponseBytes);
    c.maxChannels = integer("maxChannels", c.maxChannels);
    c.maxWindows = integer("maxWindows", c.maxWindows);
    c.windowMs = integer("windowMs", c.windowMs);
    c.maxDigitalEvents = integer("maxDigitalEvents", c.maxDigitalEvents);
    c.maxWaveformSamples = integer("maxWaveformSamples", c.maxWaveformSamples);
    c.validate();
    return c;
}

inline Config Config::fromFile(const std::filesystem::path &path) {
    std::ifstream file(path);
    detail::require(bool(file), "Cannot open AI configuration file");
    Json json;
    try {
        file >> json;
    } catch (...) {
        throw std::runtime_error("Invalid AI configuration JSON: " + path.u8string());
    }
    return fromJson(json);
}

inline void Config::validate(bool network) const {
    detail::require(timeoutMs > 0 && maxRequestBytes > 0 && maxResponseBytes > 0 && maxChannels > 0 &&
                        maxWindows >= 2 && windowMs > 0 && maxDigitalEvents >= 0 && maxWaveformSamples > 0,
                    "Invalid configuration limit");
    detail::require(std::int64_t(maxChannels) * maxWindows <= 1000000, "Channel/window allocation too large");
    if (network) {
        detail::parseEndpoint(*this);
        detail::require(model.find_first_not_of(" \t\r\n") != std::string::npos, "Model is required");
    }
    detail::require(detail::isClean(token) && detail::isClean(tokenEnv), "Invalid token configuration");
    detail::require(headers.is_object() && requestParameters.is_object(),
                    "Headers and parameters must be objects");
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        std::string name = it.key();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char x) { return char(std::tolower(x)); });
        detail::require(!name.empty() && it.value().is_string() && detail::isClean(it.value().get<std::string>()),
                        "Invalid header");
        for (unsigned char ch : name)
            detail::require(ch < 128 &&
                                (std::isalnum(ch) || std::string("!#$%&'*+-.^_`|~").find(ch) != std::string::npos),
                            "Invalid header name");
        const std::set<std::string> reserved{"authorization", "content-type",       "content-length",
                                             "host",          "connection",         "transfer-encoding",
                                             "cookie",        "proxy-authorization"};
        detail::require(!reserved.count(name), "Reserved header");
    }
    for (auto key :
         {"model", "messages", "stream", "tools", "tool_choice", "functions", "function_call", "n"})
        detail::require(!requestParameters.contains(key), "Reserved request parameter");
}

} // namespace comtrade::ai
