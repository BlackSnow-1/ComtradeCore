package comtrade;

import java.util.Objects;

/**
 * Idiomatic Java entry point for reading and writing an in-memory COMTRADE record.
 *
 * <p>The class owns native memory and should therefore be used in a
 * try-with-resources block.</p>
 */
public final class ComtradeRecord implements AutoCloseable {
    /** Supported IEEE COMTRADE revision years. */
    public enum StandardVersion {
        V1991(1991),
        V1999(1999),
        V2013(2013);

        private final int year;

        StandardVersion(int year) {
            this.year = year;
        }
    }

    static {
        System.loadLibrary("ComtradeCoreJava");
    }

    private ComtradeNativeRecord nativeRecord = new ComtradeNativeRecord();

    public void clear() {
        nativeRecord().clear();
    }

    public void setStationAndDevice(String station, String device, StandardVersion version) {
        nativeRecord().setStationAndDevice(
                Objects.requireNonNull(station, "station"),
                Objects.requireNonNull(device, "device"),
                Objects.requireNonNull(version, "version").year);
    }

    /**
     * Sets CFG timestamps in COMTRADE form, for example
     * {@code 24/10/2019,04:59:47.123456789}.
     */
    public void setTimestamps(String startTime, String triggerTime) {
        nativeRecord().setTimestamps(
                Objects.requireNonNull(startTime, "startTime"),
                Objects.requireNonNull(triggerTime, "triggerTime"));
    }

    public void setTimeMultiplier(double multiplier) {
        nativeRecord().setTimeMultiplier(multiplier);
    }

    public void setTimestampFractionalDigits(int digits) {
        nativeRecord().setTimestampFractionalDigits(digits);
    }

    public void set2013TimeMetadata(
            String timeCode,
            String localCode,
            String timeQualityCode,
            int leapSecond) {
        nativeRecord().set2013TimeMetadata(
                Objects.requireNonNull(timeCode, "timeCode"),
                Objects.requireNonNull(localCode, "localCode"),
                Objects.requireNonNull(timeQualityCode, "timeQualityCode"),
                leapSecond);
    }

    public void addAnalogChannel(
            int index,
            String id,
            String phase,
            String unit,
            double scale,
            double offset) {
        nativeRecord().addAnalogChannel(
                index,
                Objects.requireNonNull(id, "id"),
                Objects.requireNonNull(phase, "phase"),
                Objects.requireNonNull(unit, "unit"),
                scale,
                offset);
    }

    public void addDigitalChannel(int index, String id, String phase, boolean normalState) {
        nativeRecord().addDigitalChannel(
                index,
                Objects.requireNonNull(id, "id"),
                Objects.requireNonNull(phase, "phase"),
                normalState ? 1 : 0);
    }

    /** Adds one sample. The array lengths must exactly match the declared channel counts. */
    public void addSample(long timestampMicroseconds, double[] analogValues, boolean[] digitalValues) {
        Objects.requireNonNull(analogValues, "analogValues");
        Objects.requireNonNull(digitalValues, "digitalValues");

        DoubleVector analog = new DoubleVector();
        IntVector digital = new IntVector();
        try {
            for (double value : analogValues) {
                analog.add(value);
            }
            for (boolean value : digitalValues) {
                digital.add(value ? 1 : 0);
            }
            nativeRecord().addSample(timestampMicroseconds, analog, digital);
        } finally {
            analog.delete();
            digital.delete();
        }
    }

    /** Loads an ASCII CFG/DAT pair, replacing the current record only after both files parse. */
    public void load(String cfgPath, String datPath) {
        if (!nativeRecord().load(
                Objects.requireNonNull(cfgPath, "cfgPath"),
                Objects.requireNonNull(datPath, "datPath"))) {
            throw new IllegalStateException("Failed to load COMTRADE CFG/DAT files");
        }
    }

    /** Saves the current record as an ASCII CFG/DAT pair. */
    public void save(String cfgPath, String datPath) {
        if (!nativeRecord().save(
                Objects.requireNonNull(cfgPath, "cfgPath"),
                Objects.requireNonNull(datPath, "datPath"))) {
            throw new IllegalStateException("Failed to save COMTRADE CFG/DAT files");
        }
    }

    public String getStationName() {
        return nativeRecord().stationName();
    }

    public String getDeviceId() {
        return nativeRecord().deviceId();
    }

    public int getVersionYear() {
        return nativeRecord().versionYear();
    }

    public int getAnalogChannelCount() {
        return nativeRecord().analogChannelCount();
    }

    public int getDigitalChannelCount() {
        return nativeRecord().digitalChannelCount();
    }

    public long getSampleCount() {
        return nativeRecord().sampleCount();
    }

    public long getTimestampMicroseconds(long sampleIndex) {
        return nativeRecord().timestampAt(sampleIndex);
    }

    public double getAnalogValue(int channelIndex, long sampleIndex) {
        return nativeRecord().analogValueAt(channelIndex, sampleIndex);
    }

    public boolean getDigitalValue(int channelIndex, long sampleIndex) {
        return nativeRecord().digitalValueAt(channelIndex, sampleIndex);
    }

    @Override
    public void close() {
        if (nativeRecord != null) {
            nativeRecord.delete();
            nativeRecord = null;
        }
    }

    private ComtradeNativeRecord nativeRecord() {
        if (nativeRecord == null) {
            throw new IllegalStateException("ComtradeRecord is already closed");
        }
        return nativeRecord;
    }
}
