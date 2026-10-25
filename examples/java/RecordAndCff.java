package comtrade.examples;
import comtrade.*;
import java.nio.file.*;
public final class RecordAndCff {
    public static void main(String[] args) throws Exception {
        Path out=DemoSupport.output(args,"record-cff");
        for(ComtradeRecord.DataType type:ComtradeRecord.DataType.values()) {
            Path cfg=out.resolve(type+".cfg"),dat=out.resolve(type+".dat"),cff=out.resolve(type+".cff");
            try(ComtradeRecord record=DemoSupport.record(type)) {
                record.setAnalogValue(0,2,Double.NaN);
                record.setHeader("Operator notes\n"); record.setInformation("[vendor]\nsource=example\n");
                record.save(cfg.toString(),dat.toString()); record.saveCff(cff.toString());
            }
            try(ComtradeRecord loaded=new ComtradeRecord()) {
                loaded.load(cfg.toString(),dat.toString());
                DemoSupport.require(loaded.getSampleCount()==5,"Incomplete DAT");
                DemoSupport.require(Double.isNaN(loaded.getAnalogValue(0,2)),"Missing value not preserved");
                loaded.setAnalogValue(0,0,230);
                loaded.save(out.resolve(type+"_edited.cfg").toString(),out.resolve(type+"_edited.dat").toString());
                DemoSupport.warnings(loaded.getWarnings());
            }
            try(ComtradeRecord loaded=new ComtradeRecord();ComtradeStreamReader reader=new ComtradeStreamReader(cff.toString())) {
                loaded.loadCff(cff.toString());
                System.out.println(type+": "+loaded.getSampleCount()+" samples, HDR="+loaded.getHeader().trim());
                DemoSupport.require(reader.processCffStream(row->{
                    if(row.index==3) System.out.println("Missing="+row.analogMissing[0]+", offset="+row.timeOffsetNanoseconds+" ns");
                })==5,"Incomplete CFF");
                DemoSupport.warnings(reader.getWarnings());
            }
        }
    }
}
