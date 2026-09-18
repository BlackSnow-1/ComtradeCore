#include <cmath>
#include <comtrade/ai.hpp>
#include <comtrade/ai_statistics.hpp>
int main() {
    auto c = comtrade::ai::Config::fromJson(
        {{"model", "integration"}, {"endpoint", "https://example.invalid/v1/chat/completions"}});
    auto request = comtrade::ai::Json::parse(comtrade::ai::makeRequest({{"samples", 2}}, c));
    comtrade::ai::Statistics stats;
    stats.add(3, 0);
    stats.add(4, 1);
    return request["model"] == "integration" && std::abs(stats.rms() - std::sqrt(12.5)) < 1e-12 ? 0 : 1;
}
