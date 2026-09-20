/**
 * @file ai_java_api.hpp
 * @brief Java-friendly facade for the optional COMTRADE AI module (include/comtrade/ai.hpp).
 *
 * Compiled only when COMTRADE_JAVA_AI_ENABLED is defined, which CMake sets when both
 * BUILD_JAVA_BINDINGS and COMTRADE_BUILD_AI are ON (see bindings/java/CMakeLists.txt). When it is
 * not built, no AI header, and none of nlohmann/json, cpp-httplib, OpenSSL or libharu, are pulled
 * into the Java bindings at all.
 *
 * Every method takes and returns JSON as a plain std::string, exactly like the comtrade-ai CLI's
 * own file-based workflow (see docs/ai-analysis.md): no nlohmann::json type ever needs to cross the
 * SWIG/JNI boundary, so std_string.i (already included by comtrade.i) is all the type mapping this
 * facade needs.
 */
#pragma once
#ifdef COMTRADE_JAVA_AI_ENABLED

#include "comtrade/ai.hpp"

#include <stdexcept>
#include <string>

class ComtradeNativeAI {
public:
    // Reads the CFG/DAT pair given by cfgPath/datPath and returns the local evidence summary as a
    // JSON string. Performs no network access. configJson is comtrade::ai::Config serialized as JSON.
    static std::string summarize(const std::string& cfgPath, const std::string& datPath,
                                 const std::string& configJson) {
        return comtrade::ai::summarize(std::filesystem::u8path(cfgPath), std::filesystem::u8path(datPath),
                                       parseConfig(configJson))
            .dump();
    }

    // Sends evidenceJson (as produced by summarize()) to the model configured in configJson and
    // returns its validated plain-text interpretation.
    static std::string analyze(const std::string& evidenceJson, const std::string& configJson) {
        return comtrade::ai::analyze(parseJson(evidenceJson), parseConfig(configJson));
    }

    // Renders evidenceJson and the model's interpretation into a new Chinese-capable PDF report at
    // pdfPath, which must not already exist.
    static void exportPdf(const std::string& pdfPath, const std::string& evidenceJson,
                          const std::string& interpretation, const std::string& configJson) {
        comtrade::ai::exportPdf(std::filesystem::u8path(pdfPath), parseJson(evidenceJson), interpretation,
                                parseConfig(configJson));
    }

    // Validates configJson without doing anything else; throws on the first invalid field. Useful to
    // fail fast before calling analyze(), which additionally requires a valid endpoint and model
    // when requireNetwork is true (fromJson()/summarize() only validate the local limit fields).
    static void validateConfig(const std::string& configJson, bool requireNetwork) {
        parseConfig(configJson).validate(requireNetwork);
    }

private:
    static comtrade::ai::Json parseJson(const std::string& text) {
        try {
            return comtrade::ai::Json::parse(text);
        } catch (const std::exception&) {
            throw std::invalid_argument("Invalid JSON text");
        }
    }

    static comtrade::ai::Config parseConfig(const std::string& configJson) {
        return comtrade::ai::Config::fromJson(parseJson(configJson));
    }
};

#endif // COMTRADE_JAVA_AI_ENABLED
