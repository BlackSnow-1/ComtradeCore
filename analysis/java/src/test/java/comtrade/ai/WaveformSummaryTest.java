package comtrade.ai;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import java.nio.file.Path;
import static org.junit.jupiter.api.Assertions.*;

class WaveformSummaryTest {
    @TempDir Path dir;
    @Test void computesExactRmsPeaksAndTransitions() throws Exception {
        var s=new WaveformSummary(1,1,TestConfig.load(dir,TestConfig.node()));
        s.accept(1,0,new double[]{3},new boolean[]{false});
        s.accept(2,1_000_000,new double[]{-4},new boolean[]{true});
        var out=s.finish(); var a=out.path("analog").get(0);
        assertEquals(Math.sqrt(12.5),a.path("rms").asDouble(),1e-12);
        assertEquals(-4,a.path("min").asDouble()); assertEquals(-0.5,a.path("mean").asDouble());
        assertEquals(1_000_000,a.path("minTimeNs").asLong());
        assertEquals(1,out.path("digitalEventsTotal").asInt());
    }
    @Test void boundsWindowsAndEventsWithoutLosingPeaks() throws Exception {
        var s=new WaveformSummary(1,1,TestConfig.load(dir,TestConfig.node().put("maxWindows",4).put("maxDigitalEvents",2)));
        for(int i=0;i<10000;i++) s.accept(i+1,i*20_000_000L,new double[]{i==333?999:1},new boolean[]{i%2==0});
        var out=s.finish();
        assertTrue(out.path("windows").size()<=4); assertEquals(2,out.path("digitalEvents").size());
        assertEquals(9997,out.path("digitalEventsOmitted").asInt());
        double peak=0; long samples=0;
        for(var w:out.path("windows")) { peak=Math.max(peak,w.path("analog").get(0).path("max").asDouble()); samples+=w.path("samples").asLong(); }
        assertEquals(999,peak); assertEquals(10000,samples);
    }
    @Test void flagsBadTimesGapsAndNonFiniteValues() throws Exception {
        var s=new WaveformSummary(1,0,TestConfig.load(dir,TestConfig.node()));
        s.accept(1,0,new double[]{Double.NaN},new boolean[]{});
        s.accept(3,0,new double[]{2},new boolean[]{});
        var out=s.finish();
        assertEquals(1,out.path("nonIncreasingTimes").asInt()); assertEquals(1,out.path("indexDiscontinuities").asInt());
        assertEquals(1,out.path("nonFiniteValues").asInt()); assertEquals(2,out.path("analog").get(0).path("rms").asDouble());
    }
    @Test void rmsDoesNotOverflowWithLargeFiniteValues() throws Exception {
        var s=new WaveformSummary(1,0,TestConfig.load(dir,TestConfig.node()));
        for(int i=0;i<10;i++) s.accept(i+1,i,new double[]{1e300},new boolean[]{});
        assertEquals(1e300,s.finish().path("analog").get(0).path("rms").asDouble(),1e285);
    }
    @Test void rejectsEmptyAndExcessChannels() throws Exception {
        var cfg=TestConfig.load(dir,TestConfig.node());
        assertThrows(IllegalArgumentException.class,()->new WaveformSummary(65,0,cfg));
        assertThrows(IllegalStateException.class,()->new WaveformSummary(1,0,cfg).finish());
    }
}
