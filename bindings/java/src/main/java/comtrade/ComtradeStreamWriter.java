package comtrade;

import java.util.Objects;

/** Constant-memory facade for {@code comtrade::StreamWriter}. */
public final class ComtradeStreamWriter implements AutoCloseable {
    static { System.loadLibrary("ComtradeCoreJava"); }

    private ComtradeNativeStreamWriter nativeWriter;

    /** Uses a snapshot of {@code configuration}'s current CFG. */
    public ComtradeStreamWriter(ComtradeRecord configuration) {
        Objects.requireNonNull(configuration, "configuration");
        nativeWriter = new ComtradeNativeStreamWriter(configuration.nativeRecordForBinding());
    }

    /** Uses a CFG snapshot and opens {@code datPath}, throwing when it cannot be opened. */
    public ComtradeStreamWriter(ComtradeRecord configuration, String datPath) {
        this(configuration);
        if (!open(datPath)) {
            close();
            throw new IllegalStateException("Failed to open COMTRADE DAT stream: " + datPath);
        }
    }

    public boolean open(String datPath) {
        return nativeWriter().open(Objects.requireNonNull(datPath, "datPath"));
    }

    public void pushRow(long timestampMicroseconds, double[] analogValues, boolean[] digitalValues) {
        DoubleVector analog = ComtradeRecord.toNative(
                Objects.requireNonNull(analogValues, "analogValues"));
        IntVector digital = ComtradeRecord.toNative(
                Objects.requireNonNull(digitalValues, "digitalValues"));
        try {
            nativeWriter().pushRow(timestampMicroseconds, analog, digital);
        } finally {
            analog.delete();
            digital.delete();
        }
    }

    @Override
    public void close() {
        if (nativeWriter != null) {
            nativeWriter.close();
            nativeWriter.delete();
            nativeWriter = null;
        }
    }

    private ComtradeNativeStreamWriter nativeWriter() {
        if (nativeWriter == null) throw new IllegalStateException("ComtradeStreamWriter is already closed");
        return nativeWriter;
    }
}
