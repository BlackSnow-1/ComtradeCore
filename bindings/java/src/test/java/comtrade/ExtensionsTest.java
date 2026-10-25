package comtrade;
import java.nio.file.*;
public final class ExtensionsTest {
    private static void require(boolean value) { if (!value) throw new AssertionError(); }
    public static void main(String[] args) throws Exception {
        Path dir = Files.createTempDirectory("comtrade-java-");
        Path path = dir.resolve("test.cff");
        try {
            for (ComtradeRecord.DataType type : ComtradeRecord.DataType.values()) {
                try (ComtradeRecord record = new ComtradeRecord()) {
                    record.setStationAndDevice("S", "D", ComtradeRecord.StandardVersion.V2013);
                    record.setDataType(type);
                    record.addAnalogChannel(1,"A","A","A",1,0);
                    record.setHeader("operator notes"); record.setInformation("[vendor]\nkey=value");
                    record.addSample(0,new double[]{Double.NaN},new boolean[0]);
                    record.addSample(1000,new double[]{2},new boolean[0]);
                    record.saveCff(path.toString());
                }
                try (ComtradeRecord loaded = new ComtradeRecord()) {
                    loaded.loadCff(path.toString()); require(loaded.getSampleCount()==2);
                    require(Double.isNaN(loaded.getAnalogValue(0,0)));
                    require(loaded.getTimeOffsetNanoseconds(1)==1000000);
                    require(loaded.getWarnings().length > 0);
                    require(loaded.getHeader().contains("operator notes"));
                    loaded.removeSample(0); require(loaded.getTimeOffsetNanoseconds(0)==1000000);
                }
                try (ComtradeStreamReader reader = new ComtradeStreamReader(path.toString())) {
                    require(reader.processCffStream(row -> { if (row.index==1) require(row.analogMissing[0]); })==2);
                    require(reader.getWarnings().length>0);
                    try {
                        Class<?> ai = Class.forName("comtrade.ComtradeAI");
                        String evidence = (String) ai.getMethod("summarizeCff", String.class, String.class)
                                .invoke(null, path.toString(), "{}");
                        require(evidence.contains("parserWarnings") && evidence.contains("\"samples\":2"));
                    } catch (ClassNotFoundException coreOnlyBuild) {
                        // AI is deliberately absent when COMTRADE_BUILD_AI is off.
                    }
                    try { reader.processCffStream(row -> { throw new IllegalStateException("callback"); }); throw new AssertionError(); }
                    catch (IllegalStateException expected) { require(expected.getMessage().equals("callback")); }
                }
            }
            Path cfg=dir.resolve("footer.cfg"), dat=dir.resolve("footer.dat"), exported=dir.resolve("footer.raw");
            byte[] footer=new byte[]{3,0,0,0,(byte)0xd0,7,0,0,99,0};
            try {
                try (ComtradeRecord record=new ComtradeRecord()) {
                    record.setStationAndDevice("S","D",ComtradeRecord.StandardVersion.V2013);
                    record.setDataType(ComtradeRecord.DataType.BINARY);
                    record.addAnalogChannel(1,"A","A","A",1,0);
                    record.addSample(0,new double[]{1},new boolean[0]);
                    record.addSample(1000,new double[]{2},new boolean[0]);
                    record.save(cfg.toString(),dat.toString());
                }
                Files.write(dat,footer,StandardOpenOption.APPEND);
                try (ComtradeRecord loaded=new ComtradeRecord()) {
                    loaded.load(cfg.toString(),dat.toString()); require(loaded.getSampleCount()==2);
                    require(java.util.Arrays.equals(loaded.getDatTail().bytes,footer));
                    require(loaded.saveDatTail(exported.toString()));
                    require(java.util.Arrays.equals(Files.readAllBytes(exported),footer));
                }
                try (ComtradeStreamReader reader=new ComtradeStreamReader(cfg.toString())) {
                    reader.setMaxRetainedDatTailBytes(3);
                    require(reader.processDatStream(dat.toString(),row->{})==2);
                    require(reader.getDatTail().byteCount==10 && reader.getDatTail().truncated && reader.getDatTail().bytes.length==3);
                    reader.setDatTailPolicy(ComtradeRecord.DatTailPolicy.PARSE_TO_END);
                    reader.setTimeAxisMode(ComtradeStreamReader.TimeAxisMode.DAT_TIMESTAMPS);
                    require(reader.processDatStream(dat.toString(),row->{})==3);
                    require(reader.getDatTail().byteCount==0);
                }
            } finally { Files.deleteIfExists(cfg); Files.deleteIfExists(dat); Files.deleteIfExists(exported); }
            try (ComtradeRecord record = new ComtradeRecord()) {
                record.addSample(4294967296L,new double[0],new boolean[0]);
                require(record.getTimestampMicroseconds(0)==4294967296L);
                try { record.addSample(-1,new double[0],new boolean[0]); throw new AssertionError(); }
                catch (IllegalArgumentException expected) {}
            }
            ComtradeRecord closed=new ComtradeRecord(); closed.close();
            try { closed.getSampleCount(); throw new AssertionError(); } catch (IllegalStateException expected) {}
        } finally { Files.deleteIfExists(path); Files.deleteIfExists(dir); }
    }
}
