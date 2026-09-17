package comtrade.ai;

import com.sun.net.httpserver.HttpServer;
import org.junit.jupiter.api.*;
import org.junit.jupiter.api.io.TempDir;
import java.nio.file.Path;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicReference;
import static org.junit.jupiter.api.Assertions.*;

class ChatCompletionsClientTest {
    @TempDir Path dir;
    HttpServer server;
    AtomicReference<String> request=new AtomicReference<>(), authorization=new AtomicReference<>();
    volatile String body="{\"choices\":[{\"finish_reason\":\"stop\",\"message\":{\"content\":\"Observation: A1 peak.\"}}]}";
    volatile int status=200, delay=0;
    @BeforeEach void start() throws Exception {
        server=HttpServer.create(new InetSocketAddress("127.0.0.1",0),0);
        server.createContext("/chat",exchange->{
            request.set(new String(exchange.getRequestBody().readAllBytes(),StandardCharsets.UTF_8));
            authorization.set(exchange.getRequestHeaders().getFirst("Authorization"));
            try { Thread.sleep(delay); } catch(InterruptedException e) { Thread.currentThread().interrupt(); }
            byte[] bytes=body.getBytes(StandardCharsets.UTF_8);
            try { exchange.sendResponseHeaders(status,bytes.length); exchange.getResponseBody().write(bytes); }
            finally { exchange.close(); }
        }); server.start();
    }
    @AfterEach void stop() { server.stop(0); }
    private com.fasterxml.jackson.databind.node.ObjectNode config() {
        return TestConfig.node().put("endpoint","http://127.0.0.1:"+server.getAddress().getPort()+"/chat").put("allowInsecureLocalhost",true);
    }
    @Test void sendsConfiguredRequestAndParsesAnswer() throws Exception {
        var cfg=config(); cfg.putObject("requestParameters").put("temperature",0.2);
        var client=new ChatCompletionsClient(TestConfig.load(dir,cfg));
        assertEquals("Observation: A1 peak.",client.analyze(AiConfig.JSON.createObjectNode().put("samples",7)));
        var sent=AiConfig.JSON.readTree(request.get());
        assertEquals("mock-model",sent.path("model").asText()); assertEquals(0.2,sent.path("temperature").asDouble());
        assertFalse(sent.path("stream").asBoolean()); assertEquals("Bearer unit-test-secret-123",authorization.get());
        assertFalse(request.get().contains("unit-test-secret"));
    }
    @Test void hidesErrorBodyAndDoesNotRetry() throws Exception {
        status=401; body="secret server error unit-test-secret-123";
        var e=assertThrows(java.io.IOException.class,()->new ChatCompletionsClient(TestConfig.load(dir,config())).analyze(AiConfig.JSON.createObjectNode()));
        assertTrue(e.getMessage().contains("401")); assertFalse(e.toString().contains("secret"));
    }
    @Test void rejectsOversizedBodyAndOversizedRequest() throws Exception {
        body="x".repeat(200);
        assertThrows(java.io.IOException.class,()->new ChatCompletionsClient(TestConfig.load(dir,config().put("maxResponseBytes",128))).analyze(AiConfig.JSON.createObjectNode()));
        request.set(null);
        assertThrows(java.io.IOException.class,()->new ChatCompletionsClient(TestConfig.load(dir,config().put("maxRequestBytes",1024))).analyze(AiConfig.JSON.createObjectNode().put("huge","x".repeat(2048))));
        assertNull(request.get());
    }
    @Test void rejectsTruncatedOrMalformedAnswer() throws Exception {
        var client=new ChatCompletionsClient(TestConfig.load(dir,config()));
        body="not-json"; assertThrows(java.io.IOException.class,()->client.analyze(AiConfig.JSON.createObjectNode()));
        body="{\"choices\":[{\"finish_reason\":\"length\",\"message\":{\"content\":\"partial\"}}]}";
        assertThrows(java.io.IOException.class,()->client.analyze(AiConfig.JSON.createObjectNode()));
    }
    @Test void enforcesTotalTimeout() throws Exception {
        delay=1800;
        assertThrows(java.io.IOException.class,()->new ChatCompletionsClient(TestConfig.load(dir,config().put("timeoutSeconds",1))).analyze(AiConfig.JSON.createObjectNode()));
    }
}
