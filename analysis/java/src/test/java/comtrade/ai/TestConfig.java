package comtrade.ai;

import java.nio.file.*;
import com.fasterxml.jackson.databind.node.ObjectNode;

final class TestConfig {
    static ObjectNode node() {
        return AiConfig.JSON.createObjectNode().put("endpoint","https://example.invalid/v1/chat/completions")
                .put("tokenEnv", "").put("token","unit-test-secret-123").put("model","mock-model");
    }
    static AiConfig load(Path dir, ObjectNode node) throws Exception {
        Path file=Files.createTempFile(dir,"config-",".json");
        Files.writeString(file,node.toString()); return AiConfig.load(file);
    }
}
