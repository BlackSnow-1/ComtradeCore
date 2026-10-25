package comtrade.examples;
import comtrade.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
public final class TimeAndRecovery {
    public static void main(String[] args) throws Exception {
        Path out=DemoSupport.output(args,"time-recovery"); Path cfg=out.resolve("rates.cfg"),dat=out.resolve("rates.dat");
        try(ComtradeRecord configuration=DemoSupport.record(ComtradeRecord.DataType.ASCII)) {
            configuration.clearSampleRates(); configuration.addSampleRate(new ComtradeRecord.SampleRate(1000,2));
            configuration.addSampleRate(new ComtradeRecord.SampleRate(2000,5));
            DemoSupport.require(configuration.saveCfg(cfg.toString()),"Cannot save CFG");
        }
        Files.write(dat,("1,0,2200,0\r\n2,0,2210,0\r\n3,,2220,1\r\nbad-row\r\n4,0,2230,1\r\n5,0,2240,1\r\n").getBytes(StandardCharsets.US_ASCII));
        for(ComtradeStreamReader.TimeAxisMode mode:ComtradeStreamReader.TimeAxisMode.values()) {
            try(ComtradeStreamReader reader=new ComtradeStreamReader(cfg.toString())) {
                reader.setTimeAxisMode(mode);
                System.out.println("Mode="+mode+", start="+reader.getStartTime()+", timeCode="+reader.getTimeCode());
                reader.processDatStream(dat.toString(),row->System.out.println(
                    "sample="+row.index+", offset="+row.timeOffsetNanoseconds+" ns, segment="+row.segmentIndex+" ("+row.segmentSampleRate+" Hz)"));
                DemoSupport.warnings(reader.getWarnings());
            }
        }
        // Use only after confirming that CFG's sample count is wrong; not for arbitrary binary trailers.
        try(ComtradeStreamReader reader=new ComtradeStreamReader(cfg.toString())) {
            reader.setDatTailPolicy(ComtradeRecord.DatTailPolicy.PARSE_TO_END);
            reader.processDatStream(dat.toString(),row->{}); DemoSupport.warnings(reader.getWarnings());
        }
    }
}
