#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

namespace comtrade::ai {
using Json = nlohmann::json;
struct Config {
    std::string endpoint, model, token, tokenEnv = "COMTRADE_AI_TOKEN", pdfFontPath, caBundle;
    Json headers = Json::object(), requestParameters = Json::object();
    bool allowInsecureLocalhost = false, includeIdentifiers = false, fullWaveform = false;
    int timeoutMs = 60000, maxRequestBytes = 4000000, maxResponseBytes = 1000000;
    int maxChannels = 512, maxWindows = 256, windowMs = 20;
    int maxDigitalEvents = 1000, maxWaveformSamples = 100000;
    static Config fromJson(const Json &json);
    void validate(bool network = false) const;
};
Json summarize(const std::filesystem::path &cfg, const std::filesystem::path &dat, const Config &config);
std::string makeRequest(const Json &evidence, const Config &config);
std::string parseResponse(const std::string &body);
std::string analyze(const Json &evidence, const Config &config);
void exportPdf(const std::filesystem::path &path, const Json &evidence, const std::string &interpretation,
               const Config &config);
// Exclusive creation: never replaces an existing report.
void writeNewFile(const std::filesystem::path &path, const std::string &bytes);
} // namespace comtrade::ai
