package comtrade.examples;
import comtrade.*;
import java.nio.file.*;
final class DemoSupport {
    static Path output(String[] args,String name) throws Exception {
        if(args.length>1) throw new IllegalArgumentException("Usage: class [NEW_OUTPUT_DIRECTORY]");
        Path out=(args.length==1?Paths.get(args[0]):Paths.get("example-output","java-"+name,Long.toString(System.nanoTime()))).toAbsolutePath();
        if(Files.exists(out)) throw new IllegalArgumentException("Choose a new output directory");
        Files.createDirectories(out); System.out.println("Output: "+out); return out;
    }
    static void require(boolean value,String message) { if(!value) throw new IllegalStateException(message); }
    static void warnings(String[] values) { for(String warning:values) System.out.println("Warning: "+warning); }
    static ComtradeRecord record(ComtradeRecord.DataType type) {
        ComtradeRecord r=new ComtradeRecord();
        r.setStationAndDevice("ExampleStation","ExampleRelay",ComtradeRecord.StandardVersion.V2013);
        r.setDataType(type); r.setTimeMultiplier(.001); r.setTimestampFractionalDigits(9);
        r.set2013TimeMetadata("+8h00","+8h00","0",0);
        r.setTimestamps("25/10/2026,08:00:00.123456789","25/10/2026,08:00:00.123956789");
        r.addAnalogChannel(1,"VA","A","V",.1,0);
        r.addDigitalChannel(1,"TRIP","",false);
        r.addSampleRate(new ComtradeRecord.SampleRate(4000,5));
        for(int i=0;i<5;++i) r.addSample(i*250,new double[]{220+i},new boolean[]{i>=2});
        return r;
    }
}
