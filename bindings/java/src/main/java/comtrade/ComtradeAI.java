package comtrade;

import java.util.Objects;

/**
 * Java facade for the optional COMTRADE AI analysis module ({@code comtrade::ai}, see
 * {@code include/comtrade/ai.hpp} and {@code docs/ai-analysis.md}).
 *
 * <p>This class only exists in {@code comtrade-core-java.jar} when the native library was built
 * with both {@code BUILD_JAVA_BINDINGS=ON} and {@code COMTRADE_BUILD_AI=ON}; it is not present at
 * all otherwise (see {@code bindings/java/CMakeLists.txt}), so a build that does not need AI never
 * pulls in nlohmann/json, cpp-httplib, OpenSSL, or libharu.
 *
 * <p>Every method takes and returns JSON as a plain {@code String}, exactly like the
 * {@code comtrade-ai} command-line tool's own file-based workflow: {@code configJson} is the same
 * JSON object documented in {@code config/ai-config.example.json}, and {@code evidenceJson} is the
 * JSON object returned by {@link #summarize}. All methods are static; nothing here holds native
 * state, so there is no object to close.
 */
public final class ComtradeAI {
    static { System.loadLibrary("ComtradeCoreJava"); }

    private ComtradeAI() {}

    /**
     * Reads the CFG/DAT pair at {@code cfgPath}/{@code datPath} and returns the local evidence
     * summary as a JSON string. Performs no network access; {@code configJson} only needs to
     * satisfy the local limit fields (endpoint/model are not required for this call).
     */
    public static String summarize(String cfgPath, String datPath, String configJson) {
        return ComtradeNativeAI.summarize(Objects.requireNonNull(cfgPath, "cfgPath"),
                Objects.requireNonNull(datPath, "datPath"), Objects.requireNonNull(configJson, "configJson"));
    }

    /**
     * Sends {@code evidenceJson} (as produced by {@link #summarize}) to the model configured in
     * {@code configJson} over HTTPS and returns its validated plain-text interpretation. Throws if
     * the model refuses, truncates, requests tools, or returns malformed JSON.
     */
    public static String analyze(String evidenceJson, String configJson) {
        return ComtradeNativeAI.analyze(Objects.requireNonNull(evidenceJson, "evidenceJson"),
                Objects.requireNonNull(configJson, "configJson"));
    }

    /**
     * Renders {@code evidenceJson} and the model's {@code interpretation} into a new Chinese-capable
     * PDF report at {@code pdfPath}, which must not already exist.
     */
    public static void exportPdf(String pdfPath, String evidenceJson, String interpretation, String configJson) {
        ComtradeNativeAI.exportPdf(Objects.requireNonNull(pdfPath, "pdfPath"),
                Objects.requireNonNull(evidenceJson, "evidenceJson"),
                Objects.requireNonNull(interpretation, "interpretation"),
                Objects.requireNonNull(configJson, "configJson"));
    }

    /**
     * Validates {@code configJson} without doing anything else; throws on the first invalid field.
     * Useful to fail fast before calling {@link #analyze}, which additionally requires a valid
     * endpoint and model when {@code requireNetwork} is {@code true}.
     */
    public static void validateConfig(String configJson, boolean requireNetwork) {
        ComtradeNativeAI.validateConfig(Objects.requireNonNull(configJson, "configJson"), requireNetwork);
    }
}
