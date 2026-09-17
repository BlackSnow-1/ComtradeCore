package comtrade.ai;

import comtrade.ComtradeStreamReader;
import com.fasterxml.jackson.databind.node.ObjectNode;
import com.fasterxml.jackson.databind.node.ArrayNode;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.HexFormat;

/** Local-only preprocessing. No model or network is involved. */
public final class ComtradeEvidence {
    private ComtradeEvidence() {}
    public static ObjectNode summarize(Path cfg, Path dat, AiConfig config) throws IOException {
        if (!Files.isRegularFile(cfg) || !Files.isRegularFile(dat)) throw new IOException("CFG and DAT must be readable files");
        String cfgHash=sha256(cfg), datHash=sha256(dat);
        try (ComtradeStreamReader reader=new ComtradeStreamReader(cfg.toString())) {
            Long triggerNs=null;
            try {
                var format=new java.time.format.DateTimeFormatterBuilder().appendPattern("dd/MM/uuuu,HH:mm:ss")
                        .appendFraction(java.time.temporal.ChronoField.NANO_OF_SECOND,0,9,true).toFormatter(java.util.Locale.ROOT);
                triggerNs=java.time.Duration.between(java.time.LocalDateTime.parse(reader.getStartTime(),format),
                        java.time.LocalDateTime.parse(reader.getTriggerTime(),format)).toNanos();
            } catch (java.time.DateTimeException | ArithmeticException ignored) { /* Report unavailable partition below. */ }
            WaveformSummary accumulator=new WaveformSummary(reader.getAnalogChannelCount(),reader.getDigitalChannelCount(),config,triggerNs);
            RuntimeException[] failure={null};
            reader.processDatStream(dat.toString(), row -> {
                // Never let a Java exception unwind through a SWIG director/native stack.
                if (failure[0]==null) try { accumulator.accept(row.index,row.timeOffsetNanoseconds,row.analogValues,row.digitalValues); }
                catch (RuntimeException e) { failure[0]=e; }
            });
            if (failure[0]!=null) throw failure[0];
            ObjectNode result=accumulator.finish();
            result.put("schemaVersion",1).put("cfgSha256",cfgHash).put("datSha256",datHash)
                    .put("dataType",reader.getDataType().name()).put("nominalFrequencyHz",reader.getLineFrequency())
                    .put("startTime",reader.getStartTime()).put("triggerTime",reader.getTriggerTime())
                    .put("timeCode",reader.getTimeCode()).put("localCode",reader.getLocalCode()).put("timeQuality",reader.getTimeQualityCode());
            boolean identifiers=config.flag("includeIdentifiers");
            if (identifiers) result.put("station",reader.getStationName()).put("device",reader.getDeviceId());
            ArrayNode channels=result.putArray("channels");
            for (int i=0;i<reader.getAnalogChannelCount();i++) {
                var c=reader.getAnalogChannel(i);
                ObjectNode channel=channels.addObject().put("channel","A"+(i+1)).put("unit",c.unit).put("phase",c.phase)
                        .put("primary",c.primary).put("secondary",c.secondary).put("primarySecondary",c.primarySecondary).put("skew",c.skew);
                if (identifiers) channel.put("name",c.id);
            }
            for (int i=0;i<reader.getDigitalChannelCount();i++) {
                var c=reader.getDigitalChannel(i);
                ObjectNode channel=channels.addObject().put("channel","D"+(i+1)).put("normalState",c.normalState);
                if (identifiers) channel.put("name",c.id);
            }
            ArrayNode rates=result.putArray("sampleRates");
            long expected=0;
            for (int i=0;i<reader.getSampleRateCount();i++) {
                var rate=reader.getSampleRate(i); expected=rate.endSample;
                rates.addObject().put("samplesPerSecond",rate.samplesPerSecond).put("endSample",rate.endSample);
            }
            result.put("cfgExpectedSamples",expected);
            ArrayNode warnings=result.putArray("warnings");
            warnings.add("Before/from-trigger statistics partition at CFG trigger time; trigger is not necessarily fault inception. Channel skew is metadata only, not time-compensated.");
            if (triggerNs==null) warnings.add("Could not derive trigger offset; trigger-relative partitions are unavailable.");
            else if (triggerNs<result.path("firstTimeNs").asLong() || triggerNs>result.path("lastTimeNs").asLong()) warnings.add("CFG trigger lies outside sampled time range.");
            warnings.add("Parser supplies valid rows only; malformed rows/trailing partial binary records may be skipped. Summary does not certify file completeness.");
            warnings.add("No phasors, frequency estimation, harmonics, protection settings or fault-distance calculation. Do not infer these from RMS windows.");
            warnings.add("Timestamps come from DAT with TIMEMULT; repeated/zero timestamps are not reconstructed from sample rates.");
            if (expected>0 && expected!=result.path("samples").asLong()) warnings.add("CFG sample count differs from parsed DAT sample count.");
            if (result.path("indexDiscontinuities").asLong()>0 || result.path("nonIncreasingTimes").asLong()>0) warnings.add("Sample sequence or time is discontinuous; event timing may be unreliable.");
            if (result.path("nonFiniteValues").asLong()>0) warnings.add("Non-finite analog values excluded from statistics.");
            if (result.path("digitalEventsOmitted").asLong()>0) warnings.add("Digital event list truncated; total transition counts remain available.");
            if (!cfgHash.equals(sha256(cfg)) || !datHash.equals(sha256(dat))) throw new IOException("Input changed during analysis; retry with immutable copies");
            return result;
        }
    }
    private static String sha256(Path file) throws IOException {
        try {
            MessageDigest digest=MessageDigest.getInstance("SHA-256");
            try (InputStream in=Files.newInputStream(file)) { byte[] buffer=new byte[65536]; int n; while ((n=in.read(buffer))!=-1) digest.update(buffer,0,n); }
            return HexFormat.of().formatHex(digest.digest());
        } catch (NoSuchAlgorithmException e) { throw new IllegalStateException(e); }
    }
}
