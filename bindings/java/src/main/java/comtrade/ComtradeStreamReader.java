package comtrade;

import java.util.Objects;

/** Constant-memory callback facade for {@code comtrade::StreamReader}. */
public final class ComtradeStreamReader implements AutoCloseable {
    @FunctionalInterface
    public interface RowHandler {
        void onRow(SampleRow row);
    }

    /** Immutable Java representation of {@code comtrade::SampleRow}. */
    public static final class SampleRow {
        public final long index;
        public final long rawTimestamp;
        public final long timestampMicroseconds;
        public final long timeOffsetNanoseconds;
        public final String absoluteTime;
        public final double[] analogValues;
        public final boolean[] digitalValues;

        private SampleRow(long index, long rawTimestamp, long timestampMicroseconds,
                          long timeOffsetNanoseconds, String absoluteTime,
                          double[] analogValues, boolean[] digitalValues) {
            this.index = index;
            this.rawTimestamp = rawTimestamp;
            this.timestampMicroseconds = timestampMicroseconds;
            this.timeOffsetNanoseconds = timeOffsetNanoseconds;
            this.absoluteTime = absoluteTime;
            this.analogValues = analogValues;
            this.digitalValues = digitalValues;
        }
    }

    static { System.loadLibrary("ComtradeCoreJava"); }

    private ComtradeNativeStreamReader nativeReader;

    public ComtradeStreamReader(String cfgPath) {
        nativeReader = new ComtradeNativeStreamReader(Objects.requireNonNull(cfgPath, "cfgPath"));
    }

    /** Processes valid ASCII rows synchronously and returns their count. */
    public long processDatStream(String datPath, RowHandler rowHandler) {
        Objects.requireNonNull(datPath, "datPath");
        final RowHandler handler = Objects.requireNonNull(rowHandler, "rowHandler");
        ComtradeNativeRowCallback callback = new ComtradeNativeRowCallback() {
            @Override
            public void onRow(long index, long rawTimestamp, long timestampMicroseconds,
                              long timeOffsetNanoseconds, String absoluteTime,
                              DoubleVector analog, IntVector digital) {
                double[] analogValues = new double[(int) analog.size()];
                for (int i = 0; i < analogValues.length; ++i) analogValues[i] = analog.get(i);
                boolean[] digitalValues = new boolean[(int) digital.size()];
                for (int i = 0; i < digitalValues.length; ++i) digitalValues[i] = digital.get(i) != 0;
                handler.onRow(new SampleRow(index, rawTimestamp, timestampMicroseconds,
                        timeOffsetNanoseconds, absoluteTime, analogValues, digitalValues));
            }
        };
        try {
            return nativeReader().processDatStream(datPath, callback);
        } finally {
            callback.delete();
        }
    }

    public String getStationName() { return nativeReader().stationName(); }
    public String getDeviceId() { return nativeReader().deviceId(); }
    public ComtradeRecord.StandardVersion getStandardVersion() {
        return ComtradeRecord.StandardVersion.fromYear(nativeReader().versionYear());
    }
    public int getAnalogChannelCount() { return nativeReader().analogChannelCount(); }
    public int getDigitalChannelCount() { return nativeReader().digitalChannelCount(); }
    public int getTotalChannelCount() { return nativeReader().totalChannelCount(); }
    public ComtradeRecord.DataType getDataType() {
        return ComtradeRecord.DataType.valueOf(nativeReader().dataType());
    }
    public double getLineFrequency() { return nativeReader().lineFrequency(); }
    public boolean isVariableSampleRate() { return nativeReader().variableSampleRate(); }
    public double getTimeMultiplier() { return nativeReader().timeMultiplier(); }
    public int getTimestampFractionalDigits() { return nativeReader().timestampFractionalDigits(); }
    public String getTimeCode() { return nativeReader().timeCode(); }
    public String getLocalCode() { return nativeReader().localCode(); }
    public String getTimeQualityCode() { return nativeReader().timeQualityCode(); }
    public int getLeapSecond() { return nativeReader().leapSecond(); }
    public String getStartTime() { return nativeReader().startTime(); }
    public String getTriggerTime() { return nativeReader().triggerTime(); }

    public ComtradeRecord.AnalogChannel getAnalogChannel(int position) {
        ComtradeNativeStreamReader value = nativeReader();
        return new ComtradeRecord.AnalogChannel(
                value.analogChannelIndex(position), value.analogChannelId(position),
                value.analogChannelPhase(position), value.analogChannelCcbm(position),
                value.analogChannelUnit(position), value.analogChannelScale(position),
                value.analogChannelOffset(position), value.analogChannelSkew(position),
                value.analogChannelMinimum(position), value.analogChannelMaximum(position),
                value.analogChannelPrimary(position), value.analogChannelSecondary(position),
                value.analogChannelPrimarySecondary(position));
    }

    public ComtradeRecord.DigitalChannel getDigitalChannel(int position) {
        ComtradeNativeStreamReader value = nativeReader();
        return new ComtradeRecord.DigitalChannel(
                value.digitalChannelIndex(position), value.digitalChannelId(position),
                value.digitalChannelPhase(position), value.digitalChannelCcbm(position),
                value.digitalChannelNormalState(position) != 0);
    }

    public int getSampleRateCount() { return nativeReader().sampleRateCount(); }
    public ComtradeRecord.SampleRate getSampleRate(int position) {
        return new ComtradeRecord.SampleRate(nativeReader().sampleRateFrequency(position),
                nativeReader().sampleRateEndSample(position));
    }

    @Override
    public void close() {
        if (nativeReader != null) {
            nativeReader.delete();
            nativeReader = null;
        }
    }

    private ComtradeNativeStreamReader nativeReader() {
        if (nativeReader == null) throw new IllegalStateException("ComtradeStreamReader is already closed");
        return nativeReader;
    }
}
