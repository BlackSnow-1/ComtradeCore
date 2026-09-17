package comtrade.ai;

import com.fasterxml.jackson.databind.node.ObjectNode;
import java.io.IOException;

/** Implement to support a provider with a different protocol, or to use an offline test double. */
@FunctionalInterface
public interface ModelClient {
    String analyze(ObjectNode evidence) throws IOException, InterruptedException;
}
