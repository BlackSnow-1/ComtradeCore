package comtrade.examples;
import comtrade.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
public final class AiEvidence {
    // Local-only: no endpoint/token, no network requests and no PDF font required.
    public static void main(String[] args) throws Exception {
        Path out=DemoSupport.output(args,"ai-evidence"); Path cfg=out.resolve("input.cfg"),dat=out.resolve("input.dat"),cff=out.resolve("input.cff");
        try(ComtradeRecord record=DemoSupport.record(ComtradeRecord.DataType.FLOAT32)) {
            record.save(cfg.toString(),dat.toString()); record.saveCff(cff.toString());
        }
        String summary=ComtradeAI.summarize(cfg.toString(),dat.toString(),"{}");
        String full=ComtradeAI.summarizeCff(cff.toString(),"{\"waveformMode\":\"full\"}");
        Files.write(out.resolve("summary.json"),summary.getBytes(StandardCharsets.UTF_8),StandardOpenOption.CREATE_NEW);
        Files.write(out.resolve("full-evidence.json"),full.getBytes(StandardCharsets.UTF_8),StandardOpenOption.CREATE_NEW);
        System.out.println("Local CFG/DAT and CFF evidence written; no model was called.");
    }
}
