package comtrade.ai;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import java.io.IOException;
import java.net.URI;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Set;

/** Immutable validated configuration. Secrets are deliberately absent from toString(). */
public final class AiConfig {
    static final ObjectMapper JSON = new ObjectMapper();
    private final ObjectNode root;
    private final URI endpoint;
    private final String token;

    private AiConfig(ObjectNode root) {
        this.root = root.deepCopy();
        try { endpoint = URI.create(required("endpoint")); }
        catch (RuntimeException e) { throw new IllegalArgumentException("Invalid endpoint URL"); }
        String host = endpoint.getHost();
        boolean local = host != null && Set.of("localhost", "127.0.0.1", "[::1]").contains(host);
        if (host == null || endpoint.getUserInfo() != null || endpoint.getFragment() != null || endpoint.getQuery() != null
                || !("https".equals(endpoint.getScheme()) || ("http".equals(endpoint.getScheme()) && local && flag("allowInsecureLocalhost"))))
            throw new IllegalArgumentException("Endpoint requires HTTPS; HTTP is allowed only for explicitly enabled localhost. No URL credentials/query/fragment.");
        required("model");
        flag("includeIdentifiers");
        flag("allowInsecureLocalhost");
        text("instructions", "");
        text("pdfFontPath", "");
        String env = text("tokenEnv", "COMTRADE_AI_TOKEN");
        String supplied = env.isBlank() ? null : System.getenv(env);
        token = supplied == null || supplied.isBlank() ? text("token", "") : supplied;
        if (token.isBlank() || token.contains("\r") || token.contains("\n"))
            throw new IllegalArgumentException("Configure a nonempty token or tokenEnv environment variable");
        integer("timeoutSeconds", 120, 1, 600);
        integer("maxRequestBytes", 262144, 1024, 1048576);
        integer("maxResponseBytes", 131072, 128, 1048576);
        integer("maxChannels", 64, 1, 512);
        integer("maxWindows", 64, 2, 1024);
        integer("windowMillis", 20, 1, 60000);
        integer("maxDigitalEvents", 200, 0, 10000);
        object("headers").fields().forEachRemaining(entry -> {
            if (!entry.getKey().matches("[A-Za-z0-9-]+") || Set.of("authorization", "content-type", "host", "content-length", "connection", "expect", "upgrade").contains(entry.getKey().toLowerCase(java.util.Locale.ROOT))
                    || !entry.getValue().isTextual() || entry.getValue().asText().matches("(?s).*[\r\n].*"))
                throw new IllegalArgumentException("Invalid or reserved custom HTTP header");
        });
        object("requestParameters").fieldNames().forEachRemaining(name -> {
            if (Set.of("model", "messages", "stream", "tools", "tool_choice", "functions", "function_call", "n").contains(name))
                throw new IllegalArgumentException("Reserved request parameter: " + name);
        });
    }

    public static AiConfig load(Path path) throws IOException {
        if (Files.size(path) > 65536) throw new IOException("Configuration exceeds 64 KiB");
        try {
            JsonNode node = JSON.readTree(Files.readAllBytes(path));
            if (!(node instanceof ObjectNode)) throw new IllegalArgumentException("Configuration must be a JSON object");
            return new AiConfig((ObjectNode) node);
        } catch (com.fasterxml.jackson.core.JsonProcessingException e) {
            throw new IOException("Invalid configuration JSON (content hidden)");
        }
    }

    public URI endpoint() { return endpoint; }
    String token() { return token; }
    public String model() { return required("model"); }
    public String text(String key, String fallback) {
        JsonNode value = root.get(key);
        if (value == null) return fallback;
        if (!value.isTextual()) throw new IllegalArgumentException(key + " must be a string");
        return value.textValue();
    }
    public boolean flag(String key) {
        JsonNode value = root.get(key);
        if (value != null && !value.isBoolean()) throw new IllegalArgumentException(key + " must be a boolean");
        return value != null && value.booleanValue();
    }
    public int integer(String key, int fallback, int min, int max) {
        JsonNode value = root.get(key);
        if (value != null && (!value.isIntegralNumber() || !value.canConvertToInt())) throw new IllegalArgumentException("Invalid " + key);
        int number = value == null ? fallback : value.intValue();
        if (number < min || number > max) throw new IllegalArgumentException(key + " out of range");
        return number;
    }
    ObjectNode object(String key) {
        JsonNode value = root.get(key);
        if (value == null) return JSON.createObjectNode();
        if (!(value instanceof ObjectNode)) throw new IllegalArgumentException(key + " must be an object");
        return ((ObjectNode) value).deepCopy();
    }
    private String required(String key) {
        String value = text(key, "");
        if (value.isBlank()) throw new IllegalArgumentException("Missing " + key);
        return value;
    }
    @Override public String toString() { return "AiConfig[credentials and endpoint redacted]"; }
}
