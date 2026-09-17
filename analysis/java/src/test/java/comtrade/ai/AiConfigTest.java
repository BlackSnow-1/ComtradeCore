package comtrade.ai;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import java.nio.file.*;
import static org.junit.jupiter.api.Assertions.*;

class AiConfigTest {
    @TempDir Path dir;
    @Test void secretsDoNotAppearInDescription() throws Exception {
        var config=TestConfig.load(dir,TestConfig.node());
        assertFalse(config.toString().contains("unit-test-secret"));
    }
    @Test void rejectsRemoteHttpAndUrlCredentials() {
        for (String url:new String[]{"http://example.com/x", "https://secret@example.com/x", "https://example.com/x?token=secret"})
            assertThrows(IllegalArgumentException.class,()->TestConfig.load(dir,TestConfig.node().put("endpoint",url)));
    }
    @Test void rejectsMissingTokenAndInvalidLimits() {
        assertThrows(IllegalArgumentException.class,()->TestConfig.load(dir,TestConfig.node().put("token","")));
        assertThrows(IllegalArgumentException.class,()->TestConfig.load(dir,TestConfig.node().put("maxWindows",1)));
        assertThrows(IllegalArgumentException.class,()->TestConfig.load(dir,TestConfig.node().put("timeoutSeconds",1.5)));
    }
    @Test void rejectsReservedHeadersAndRequestOverrides() {
        var config=TestConfig.node(); config.putObject("headers").put("Authorization","other-secret");
        assertThrows(IllegalArgumentException.class,()->TestConfig.load(dir,config));
        var other=TestConfig.node(); other.putObject("requestParameters").put("messages","override");
        assertThrows(IllegalArgumentException.class,()->TestConfig.load(dir,other));
    }
    @Test void invalidJsonDoesNotEchoSecrets() throws Exception {
        Path file=dir.resolve("bad.json"); Files.writeString(file,"{ secret-token-broken");
        Exception e=assertThrows(java.io.IOException.class,()->AiConfig.load(file));
        assertFalse(e.toString().contains("secret-token")); assertNull(e.getCause());
    }
}
