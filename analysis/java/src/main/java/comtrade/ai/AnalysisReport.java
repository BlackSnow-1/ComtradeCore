package comtrade.ai;

import com.fasterxml.jackson.databind.node.ObjectNode;
import java.time.Instant;

/** Separates reproducible evidence from unverified model interpretation. */
public record AnalysisReport(Instant createdAt, String model, ObjectNode evidence, String interpretation) {
    public AnalysisReport { evidence=evidence.deepCopy(); }
    @Override public ObjectNode evidence() { return evidence.deepCopy(); }
}
