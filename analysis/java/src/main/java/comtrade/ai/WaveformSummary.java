package comtrade.ai;

import com.fasterxml.jackson.databind.node.ArrayNode;
import com.fasterxml.jackson.databind.node.ObjectNode;
import java.util.ArrayList;
import java.util.List;

/** Bounded streaming statistics. RMS is sample-weighted, not time-weighted or fundamental RMS. */
public final class WaveformSummary {
    private final int analogCount, digitalCount, maxWindows, maxEvents;
    private long windowNs, count, previousIndex, previousTime, firstTime, lastTime, gaps, badTimes, invalid;
    private long eventsSeen;
    private final Stats[] totals;
    private final boolean[] previousDigital;
    private final boolean[] initialDigital;
    private final long[] transitions;
    private final List<Window> windows = new ArrayList<>();
    private final ArrayNode events = AiConfig.JSON.createArrayNode();

    public WaveformSummary(int analogCount, int digitalCount, AiConfig config) {
        if (analogCount < 0 || digitalCount < 0 || (long) analogCount + digitalCount > config.integer("maxChannels", 64, 1, 512))
            throw new IllegalArgumentException("Channel count exceeds configured limit");
        this.analogCount = analogCount; this.digitalCount = digitalCount;
        maxWindows = config.integer("maxWindows", 64, 2, 1024);
        maxEvents = config.integer("maxDigitalEvents", 200, 0, 10000);
        windowNs = config.integer("windowMillis", 20, 1, 60000) * 1_000_000L;
        totals = stats(analogCount); previousDigital = new boolean[digitalCount]; initialDigital = new boolean[digitalCount]; transitions = new long[digitalCount];
    }

    public void accept(long index, long timeNs, double[] analog, boolean[] digital) {
        if (analog.length != analogCount || digital.length != digitalCount || timeNs < 0)
            throw new IllegalArgumentException("Invalid sample shape or time");
        if (count == 0) firstTime = timeNs;
        else {
            if (index != previousIndex + 1) gaps++;
            if (timeNs <= previousTime) badTimes++;
        }
        if (count == 0 && index != 1) gaps++;
        previousIndex = index; previousTime = timeNs; lastTime = timeNs; count++;
        if (windows.isEmpty() || timeNs - windows.get(windows.size()-1).start >= windowNs) {
            if (windows.size() >= maxWindows) compact();
            windows.add(new Window(timeNs, analogCount));
        }
        Window current = windows.get(windows.size()-1);
        current.end = timeNs; current.count++;
        for (int i=0; i<analogCount; i++) {
            if (!Double.isFinite(analog[i])) { invalid++; continue; }
            totals[i].add(analog[i], timeNs); current.values[i].add(analog[i], timeNs);
        }
        for (int i=0; i<digitalCount; i++) {
            if (count == 1) initialDigital[i] = digital[i];
            if (count > 1 && previousDigital[i] != digital[i]) {
                transitions[i]++; eventsSeen++;
                if (events.size() < maxEvents) events.addObject().put("channel", "D"+(i+1)).put("timeNs", timeNs).put("sample", index).put("value", digital[i]);
            }
            previousDigital[i] = digital[i];
        }
    }

    private void compact() {
        List<Window> merged = new ArrayList<>();
        for (int i=0; i<windows.size(); i+=2) {
            Window a = windows.get(i);
            if (i+1 < windows.size()) {
                Window b = windows.get(i+1); a.end = b.end; a.count += b.count;
                for (int c=0; c<analogCount; c++) a.values[c].merge(b.values[c]);
            }
            merged.add(a);
        }
        windows.clear(); windows.addAll(merged);
        windowNs = windowNs > Long.MAX_VALUE / 2 ? Long.MAX_VALUE : windowNs * 2;
    }

    public ObjectNode finish() {
        if (count == 0) throw new IllegalStateException("No valid samples; file is empty, unreadable, or invalid");
        ObjectNode out = AiConfig.JSON.createObjectNode();
        out.put("samples", count).put("firstTimeNs", firstTime).put("lastTimeNs", lastTime)
                .put("indexDiscontinuities", gaps).put("nonIncreasingTimes", badTimes).put("nonFiniteValues", invalid)
                .put("digitalEventsTotal", eventsSeen).put("digitalEventsOmitted", eventsSeen-events.size());
        out.put("rmsDefinition", "Sample-weighted sqrt(mean(x^2)), includes DC; not fundamental or time-weighted RMS. No CT/PT conversion beyond CFG a*x+b.");
        ArrayNode analogs = out.putArray("analog");
        for (int i=0; i<analogCount; i++) analogs.add(totals[i].json().put("channel", "A"+(i+1)));
        ArrayNode digitals = out.putArray("digital");
        for (int i=0; i<digitalCount; i++) digitals.addObject().put("channel", "D"+(i+1)).put("transitions", transitions[i]).put("initialState", initialDigital[i]).put("finalState", previousDigital[i]);
        out.set("digitalEvents", events.deepCopy());
        ArrayNode bins = out.putArray("windows");
        for (Window window : windows) {
            ObjectNode w = bins.addObject().put("startNs", window.start).put("endNs", window.end).put("samples", window.count);
            ArrayNode values = w.putArray("analog");
            for (int i=0; i<analogCount; i++) values.add(window.values[i].json().put("channel", "A"+(i+1)));
        }
        out.put("windowPolicy", "Adjacent windows are merged as needed; peaks retained, timing resolution decreases. Windows are not cycles or fault classifications.");
        return out;
    }
    private static Stats[] stats(int n) { Stats[] s = new Stats[n]; for (int i=0;i<n;i++) s[i]=new Stats(); return s; }
    private static final class Window {
        final long start; long end, count; final Stats[] values;
        Window(long start, int n) { this.start=start; end=start; values=stats(n); }
    }
    private static final class Stats {
        long n, minTime, maxTime; double min=Double.POSITIVE_INFINITY, max=Double.NEGATIVE_INFINITY, mean, scale, squares;
        void add(double x, long t) {
            if (x<min) { min=x; minTime=t; } if (x>max) { max=x; maxTime=t; }
            n++; mean = mean * ((n-1.0)/n) + x/n;
            double a=Math.abs(x);
            if (a>scale) { double r=scale/a; squares=squares*r*r+1; scale=a; }
            else if (a>0) { double r=a/scale; squares+=r*r; }
        }
        void merge(Stats b) {
            if (b.n==0) return;
            if (b.min<min) { min=b.min; minTime=b.minTime; } if (b.max>max) { max=b.max; maxTime=b.maxTime; }
            long total=n+b.n; mean=mean*((double)n/total)+b.mean*((double)b.n/total);
            double s=Math.max(scale,b.scale);
            if (s>0) squares=squares*(scale/s)*(scale/s)+b.squares*(b.scale/s)*(b.scale/s);
            scale=s; n=total;
        }
        ObjectNode json() {
            ObjectNode o=AiConfig.JSON.createObjectNode().put("validSamples",n);
            if (n>0) o.put("min",min).put("max",max).put("minTimeNs",minTime).put("maxTimeNs",maxTime).put("mean",mean).put("rms",scale*Math.sqrt(Math.min(1.0,squares/n)));
            return o;
        }
    }
}
