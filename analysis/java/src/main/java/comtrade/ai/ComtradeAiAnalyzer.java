package comtrade.ai;

import com.fasterxml.jackson.databind.node.ObjectNode;
import java.io.IOException;
import java.nio.file.Path;
import java.time.Instant;

/** Java entry point: preview evidence locally, then explicitly send it to the configured provider. */
public final class ComtradeAiAnalyzer {
    private final AiConfig config;
    private final ModelClient client;
    public ComtradeAiAnalyzer(AiConfig config) { this(config,new ChatCompletionsClient(config)); }
    public ComtradeAiAnalyzer(AiConfig config, ModelClient client) { this.config=config; this.client=client; }
    public ObjectNode summarize(Path cfg, Path dat) throws IOException { return ComtradeEvidence.summarize(cfg,dat,config); }
    public AnalysisReport analyze(Path cfg, Path dat) throws IOException, InterruptedException {
        ObjectNode evidence=summarize(cfg,dat);
        return new AnalysisReport(Instant.now(),config.model(),evidence,client.analyze(evidence.deepCopy()));
    }
    public void exportPdf(AnalysisReport report, Path destination) throws IOException {
        String font=config.text("pdfFontPath","");
        if (font.isBlank()) throw new IOException("Set pdfFontPath to a Unicode TrueType font supporting the report language");
        PdfReportExporter.write(report,Path.of(font),destination);
    }
}
