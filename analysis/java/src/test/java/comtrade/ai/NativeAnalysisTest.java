package comtrade.ai;

import comtrade.ComtradeRecord;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.EnumSource;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import java.nio.file.*;
import static org.junit.jupiter.api.Assertions.*;

class NativeAnalysisTest {
    @TempDir Path dir;
    @ParameterizedTest @EnumSource(ComtradeRecord.DataType.class)
    void readsEveryDatTypeThroughActualJni(ComtradeRecord.DataType type) throws Exception {
        Path cfg=dir.resolve("record.cfg"),dat=dir.resolve("record.dat");
        try(var record=new ComtradeRecord()) {
            record.setStationAndDevice("private-station","relay",ComtradeRecord.StandardVersion.V2013);
            record.setTimestamps("01/01/2024,00:00:00.000000","01/01/2024,00:00:00.001000");
            record.setDataType(type);
            record.addAnalogChannel(1,"IA","A","A",0.5,1);
            record.addDigitalChannel(1,"trip","",false);
            record.addSampleRate(new ComtradeRecord.SampleRate(1000,2));
            record.addSample(0,new double[]{3},new boolean[]{false});
            record.addSample(1000,new double[]{4},new boolean[]{true});
            record.save(cfg.toString(),dat.toString());
        }
        var analyzer=new ComtradeAiAnalyzer(TestConfig.load(dir,TestConfig.node()),evidence->"Hypothesis based on A1");
        var report=analyzer.analyze(cfg,dat); var e=report.evidence();
        assertEquals(2,e.path("samples").asLong()); assertEquals(type.name(),e.path("dataType").asText());
        assertEquals(Math.sqrt(12.5),e.path("analog").get(0).path("rms").asDouble(),1e-10);
        assertEquals(1,e.path("digitalEventsTotal").asInt()); assertEquals(64,e.path("datSha256").asText().length());
        assertFalse(e.toString().contains("private-station"));
        e.put("samples",99); assertEquals(2,report.evidence().path("samples").asInt());
    }
    @Test void missingInputFailsBeforeModelCall() throws Exception {
        var analyzer=new ComtradeAiAnalyzer(TestConfig.load(dir,TestConfig.node()),e->{fail("must not call model");return "";});
        assertThrows(java.io.IOException.class,()->analyzer.analyze(dir.resolve("missing.cfg"),dir.resolve("missing.dat")));
    }
}
