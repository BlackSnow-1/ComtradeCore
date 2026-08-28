package comtrade;

import java.util.Objects;

/**
 * Java facade for the complete in-memory {@code comtrade::Record} API.
 * Native memory is released by {@link #close()}, so use try-with-resources.
 */
public final class ComtradeRecord implements AutoCloseable {
    public enum StandardVersion {
        V1991(1991), V1999(1999), V2013(2013);

        private final int year;

        StandardVersion(int year) { this.year = year; }

        static StandardVersion fromYear(int year) {
            for (StandardVersion value : values()) if (value.year == year) return value;
            throw new IllegalStateException("Unsupported native COMTRADE version: " + year);
        }
    }

    public enum DataType {
        ASCII, BINARY, BINARY32, FLOAT32
    }

    /** Complete value representation of {@code comtrade::AnalogChannel}. */
    public static final class AnalogChannel {
        public final int index;
        public final String id;
        public final String phase;
        public final String ccbm;
        public final String unit;
        public final double scale;
        public final double offset;
        public final double skew;
        public final int minimum;
        public final int maximum;
        public final double primary;
        public final double secondary;
        public final String primarySecondary;

        public AnalogChannel(int index, String id, String phase, String ccbm, String unit,
                             double scale, double offset, double skew, int minimum, int maximum,
                             double primary, double secondary, String primarySecondary) {
            this.index = index;
            this.id = Objects.requireNonNull(id, "id");
            this.phase = Objects.requireNonNull(phase, "phase");
            this.ccbm = Objects.requireNonNull(ccbm, "ccbm");
            this.unit = Objects.requireNonNull(unit, "unit");
            this.scale = scale;
            this.offset = offset;
            this.skew = skew;
            this.minimum = minimum;
            this.maximum = maximum;
            this.primary = primary;
            this.secondary = secondary;
            this.primarySecondary = Objects.requireNonNull(primarySecondary, "primarySecondary");
        }

        public AnalogChannel(int index, String id, String phase, String unit,
                             double scale, double offset) {
            this(index, id, phase, "", unit, scale, offset, 0.0, 0, 0, 1.0, 1.0, "P");
        }
    }

    /** Complete value representation of {@code comtrade::DigitalChannel}. */
    public static final class DigitalChannel {
        public final int index;
        public final String id;
        public final String phase;
        public final String ccbm;
        public final boolean normalState;

        public DigitalChannel(int index, String id, String phase, String ccbm, boolean normalState) {
            this.index = index;
            this.id = Objects.requireNonNull(id, "id");
            this.phase = Objects.requireNonNull(phase, "phase");
            this.ccbm = Objects.requireNonNull(ccbm, "ccbm");
            this.normalState = normalState;
        }

        public DigitalChannel(int index, String id, String phase, boolean normalState) {
            this(index, id, phase, "", normalState);
        }
    }

    /** Complete value representation of {@code comtrade::SampleRate}. */
    public static final class SampleRate {
        public final double samplesPerSecond;
        public final long endSample;

        public SampleRate(double samplesPerSecond, long endSample) {
            this.samplesPerSecond = samplesPerSecond;
            this.endSample = endSample;
        }
    }

    static { System.loadLibrary("ComtradeCoreJava"); }

    private ComtradeNativeRecord nativeRecord = new ComtradeNativeRecord();

    public void clear() { nativeRecord().clear(); }

    public boolean parseCfg(String cfgPath) {
        return nativeRecord().parseCfg(Objects.requireNonNull(cfgPath, "cfgPath"));
    }

    public boolean parseDat(String datPath) {
        return nativeRecord().parseDat(Objects.requireNonNull(datPath, "datPath"));
    }

    public boolean saveCfg(String cfgPath) {
        return nativeRecord().saveCfg(Objects.requireNonNull(cfgPath, "cfgPath"));
    }

    public boolean saveDat(String datPath) {
        return nativeRecord().saveDat(Objects.requireNonNull(datPath, "datPath"));
    }

    /** Transactionally loads a CFG/DAT pair. */
    public void load(String cfgPath, String datPath) {
        if (!nativeRecord().load(Objects.requireNonNull(cfgPath, "cfgPath"),
                                 Objects.requireNonNull(datPath, "datPath"))) {
            throw new IllegalStateException("Failed to load COMTRADE CFG/DAT files");
        }
    }

    public void save(String cfgPath, String datPath) {
        if (!nativeRecord().save(Objects.requireNonNull(cfgPath, "cfgPath"),
                                 Objects.requireNonNull(datPath, "datPath"))) {
            throw new IllegalStateException("Failed to save COMTRADE CFG/DAT files");
        }
    }

    public void setStationAndDevice(String station, String device, StandardVersion version) {
        nativeRecord().setStationAndDevice(Objects.requireNonNull(station, "station"),
                Objects.requireNonNull(device, "device"),
                Objects.requireNonNull(version, "version").year);
    }

    public String getStationName() { return nativeRecord().stationName(); }
    public void setStationName(String value) {
        nativeRecord().setStationName(Objects.requireNonNull(value, "value"));
    }
    public String getDeviceId() { return nativeRecord().deviceId(); }
    public void setDeviceId(String value) {
        nativeRecord().setDeviceId(Objects.requireNonNull(value, "value"));
    }
    public StandardVersion getStandardVersion() {
        return StandardVersion.fromYear(nativeRecord().versionYear());
    }
    public int getVersionYear() { return nativeRecord().versionYear(); }
    public void setStandardVersion(StandardVersion value) {
        nativeRecord().setVersionYear(Objects.requireNonNull(value, "value").year);
    }

    public void setTimestamps(String startTime, String triggerTime) {
        nativeRecord().setTimestamps(Objects.requireNonNull(startTime, "startTime"),
                Objects.requireNonNull(triggerTime, "triggerTime"));
    }
    public String getStartTime() { return nativeRecord().startTime(); }
    public String getTriggerTime() { return nativeRecord().triggerTime(); }

    public double getLineFrequency() { return nativeRecord().lineFrequency(); }
    public void setLineFrequency(double value) { nativeRecord().setLineFrequency(value); }
    public DataType getDataType() { return DataType.valueOf(nativeRecord().dataType()); }
    public void setDataType(DataType value) {
        nativeRecord().setDataType(Objects.requireNonNull(value, "value").name());
    }
    public boolean isVariableSampleRate() { return nativeRecord().variableSampleRate(); }
    public void setVariableSampleRate(boolean value) { nativeRecord().setVariableSampleRate(value); }
    public double getTimeMultiplier() { return nativeRecord().timeMultiplier(); }
    public void setTimeMultiplier(double value) { nativeRecord().setTimeMultiplier(value); }
    public int getTimestampFractionalDigits() { return nativeRecord().timestampFractionalDigits(); }
    public void setTimestampFractionalDigits(int value) {
        nativeRecord().setTimestampFractionalDigits(value);
    }

    public void set2013TimeMetadata(String timeCode, String localCode,
                                    String timeQualityCode, int leapSecond) {
        nativeRecord().set2013TimeMetadata(Objects.requireNonNull(timeCode, "timeCode"),
                Objects.requireNonNull(localCode, "localCode"),
                Objects.requireNonNull(timeQualityCode, "timeQualityCode"), leapSecond);
    }
    public String getTimeCode() { return nativeRecord().timeCode(); }
    public String getLocalCode() { return nativeRecord().localCode(); }
    public String getTimeQualityCode() { return nativeRecord().timeQualityCode(); }
    public int getLeapSecond() { return nativeRecord().leapSecond(); }

    public void addAnalogChannel(AnalogChannel channel) {
        AnalogChannel value = Objects.requireNonNull(channel, "channel");
        nativeRecord().addAnalogChannel(value.index, value.id, value.phase, value.ccbm, value.unit,
                value.scale, value.offset, value.skew, value.minimum, value.maximum,
                value.primary, value.secondary, value.primarySecondary);
    }

    public void addAnalogChannel(int index, String id, String phase, String unit,
                                 double scale, double offset) {
        addAnalogChannel(new AnalogChannel(index, id, phase, unit, scale, offset));
    }

    public AnalogChannel getAnalogChannel(int position) {
        ComtradeNativeRecord value = nativeRecord();
        return new AnalogChannel(value.analogChannelIndex(position), value.analogChannelId(position),
                value.analogChannelPhase(position), value.analogChannelCcbm(position),
                value.analogChannelUnit(position), value.analogChannelScale(position),
                value.analogChannelOffset(position), value.analogChannelSkew(position),
                value.analogChannelMinimum(position), value.analogChannelMaximum(position),
                value.analogChannelPrimary(position), value.analogChannelSecondary(position),
                value.analogChannelPrimarySecondary(position));
    }

    public void setAnalogChannel(int position, AnalogChannel channel) {
        AnalogChannel value = Objects.requireNonNull(channel, "channel");
        nativeRecord().setAnalogChannel(position, value.index, value.id, value.phase, value.ccbm,
                value.unit, value.scale, value.offset, value.skew, value.minimum, value.maximum,
                value.primary, value.secondary, value.primarySecondary);
    }
    public void removeAnalogChannel(int position) { nativeRecord().removeAnalogChannel(position); }
    public void clearAnalogChannels() { nativeRecord().clearAnalogChannels(); }
    public int getAnalogChannelCount() { return nativeRecord().analogChannelCount(); }

    public void addDigitalChannel(DigitalChannel channel) {
        DigitalChannel value = Objects.requireNonNull(channel, "channel");
        nativeRecord().addDigitalChannel(value.index, value.id, value.phase, value.ccbm,
                value.normalState ? 1 : 0);
    }

    public void addDigitalChannel(int index, String id, String phase, boolean normalState) {
        addDigitalChannel(new DigitalChannel(index, id, phase, normalState));
    }

    public DigitalChannel getDigitalChannel(int position) {
        ComtradeNativeRecord value = nativeRecord();
        return new DigitalChannel(value.digitalChannelIndex(position), value.digitalChannelId(position),
                value.digitalChannelPhase(position), value.digitalChannelCcbm(position),
                value.digitalChannelNormalState(position) != 0);
    }

    public void setDigitalChannel(int position, DigitalChannel channel) {
        DigitalChannel value = Objects.requireNonNull(channel, "channel");
        nativeRecord().setDigitalChannel(position, value.index, value.id, value.phase, value.ccbm,
                value.normalState ? 1 : 0);
    }
    public void removeDigitalChannel(int position) { nativeRecord().removeDigitalChannel(position); }
    public void clearDigitalChannels() { nativeRecord().clearDigitalChannels(); }
    public int getDigitalChannelCount() { return nativeRecord().digitalChannelCount(); }
    public int getTotalChannelCount() { return nativeRecord().totalChannelCount(); }

    public void addSampleRate(SampleRate sampleRate) {
        SampleRate value = Objects.requireNonNull(sampleRate, "sampleRate");
        nativeRecord().addSampleRate(value.samplesPerSecond, value.endSample);
    }
    public SampleRate getSampleRate(int position) {
        return new SampleRate(nativeRecord().sampleRateFrequency(position),
                nativeRecord().sampleRateEndSample(position));
    }
    public void setSampleRate(int position, SampleRate sampleRate) {
        SampleRate value = Objects.requireNonNull(sampleRate, "sampleRate");
        nativeRecord().setSampleRate(position, value.samplesPerSecond, value.endSample);
    }
    public void removeSampleRate(int position) { nativeRecord().removeSampleRate(position); }
    public void clearSampleRates() { nativeRecord().clearSampleRates(); }
    public int getSampleRateCount() { return nativeRecord().sampleRateCount(); }

    public void addSample(long timestampMicroseconds, double[] analogValues, boolean[] digitalValues) {
        DoubleVector analog = toNative(Objects.requireNonNull(analogValues, "analogValues"));
        IntVector digital = toNative(Objects.requireNonNull(digitalValues, "digitalValues"));
        try {
            nativeRecord().addSample(timestampMicroseconds, analog, digital);
        } finally {
            analog.delete();
            digital.delete();
        }
    }

    public long getSampleCount() { return nativeRecord().sampleCount(); }
    public long getTimestampMicroseconds(long sampleIndex) { return nativeRecord().timestampAt(sampleIndex); }
    public void setTimestampMicroseconds(long sampleIndex, long timestampMicroseconds) {
        nativeRecord().setTimestampAt(sampleIndex, timestampMicroseconds);
    }
    public double getAnalogValue(int channelIndex, long sampleIndex) {
        return nativeRecord().analogValueAt(channelIndex, sampleIndex);
    }
    public void setAnalogValue(int channelIndex, long sampleIndex, double value) {
        nativeRecord().setAnalogValueAt(channelIndex, sampleIndex, value);
    }
    public boolean getDigitalValue(int channelIndex, long sampleIndex) {
        return nativeRecord().digitalValueAt(channelIndex, sampleIndex);
    }
    public void setDigitalValue(int channelIndex, long sampleIndex, boolean value) {
        nativeRecord().setDigitalValueAt(channelIndex, sampleIndex, value);
    }
    public void removeSample(long sampleIndex) { nativeRecord().removeSample(sampleIndex); }
    public void clearSamples() { nativeRecord().clearSamples(); }

    /** Creates a constant-memory writer using a snapshot of this record's current CFG. */
    public ComtradeStreamWriter openStreamWriter(String datPath) {
        return new ComtradeStreamWriter(this, datPath);
    }

    @Override
    public void close() {
        if (nativeRecord != null) {
            nativeRecord.delete();
            nativeRecord = null;
        }
    }

    ComtradeNativeRecord nativeRecordForBinding() { return nativeRecord(); }

    static DoubleVector toNative(double[] values) {
        DoubleVector result = new DoubleVector();
        for (double value : values) result.add(value);
        return result;
    }

    static IntVector toNative(boolean[] values) {
        IntVector result = new IntVector();
        for (boolean value : values) result.add(value ? 1 : 0);
        return result;
    }

    private ComtradeNativeRecord nativeRecord() {
        if (nativeRecord == null) throw new IllegalStateException("ComtradeRecord is already closed");
        return nativeRecord;
    }
}
