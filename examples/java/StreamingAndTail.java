package comtrade.examples;
import comtrade.*;
import java.nio.file.*;
public final class StreamingAndTail {
    public static void main(String[] args) throws Exception {
        Path out=DemoSupport.output(args,"stream-tail");
        Path cfg=out.resolve("stream.cfg"),dat=out.resolve("stream.dat");
        final int count=20000;
        try(ComtradeRecord configuration=DemoSupport.record(ComtradeRecord.DataType.BINARY)) {
            configuration.setTimeMultiplier(1); // Keep five seconds within the binary uint32 tick range.
            configuration.clearSamples(); configuration.clearSampleRates();
            configuration.addSampleRate(new ComtradeRecord.SampleRate(4000,count));
            DemoSupport.require(configuration.saveCfg(cfg.toString()),"Cannot save CFG");
            try(ComtradeStreamWriter writer=configuration.openStreamWriter(dat.toString())) {
                for(int i=0;i<count;++i) writer.pushRow(i*250L,new double[]{220+Math.sin(i*.05)},new boolean[]{i>=10000});
            }
        }
        // Opaque vendor bytes: NUL and non-UTF-8 must survive unchanged.
        byte[] tail=new byte[]{'V','E','N','D','O','R',0,(byte)255,13,10};
        Files.write(dat,tail,StandardOpenOption.APPEND);
        try(ComtradeStreamReader reader=new ComtradeStreamReader(cfg.toString())) {
            reader.setMaxRetainedDatTailBytes(4);
            double[] sum={0};
            long processed=reader.processDatStream(dat.toString(),row->{
                sum[0]+=row.analogValues[0];
                if(row.index<=3) System.out.println("Sample="+row.index+", voltage="+row.analogValues[0]);
                // Java arrays are independent copies and can be retained after the callback.
            });
            DemoSupport.require(processed==count,"Tail was counted as waveform");
            System.out.println("Samples="+processed+", mean="+sum[0]/processed);
            ComtradeRecord.DatTail preview=reader.getDatTail();
            System.out.println("Tail="+preview.byteCount+" bytes, retained="+preview.bytes.length+", truncated="+preview.truncated);
            DemoSupport.warnings(reader.getWarnings());
        }
        try(ComtradeRecord loaded=new ComtradeRecord()) {
            loaded.load(cfg.toString(),dat.toString());
            DemoSupport.require(java.util.Arrays.equals(tail,loaded.getDatTail().bytes),"Vendor bytes changed");
            DemoSupport.require(loaded.saveDatTail(out.resolve("vendor-tail.bin").toString()),"Incomplete tail export");
            DemoSupport.require(loaded.saveDat(out.resolve("standard.dat").toString()),"Cannot save standard DAT");
            // Copies bytes only: vendor checksums/offsets are not recomputed.
            DemoSupport.require(loaded.saveDat(out.resolve("copied-tail.dat").toString(),true),"Cannot copy tail");
            DemoSupport.warnings(loaded.getWarnings());
        }
    }
}
