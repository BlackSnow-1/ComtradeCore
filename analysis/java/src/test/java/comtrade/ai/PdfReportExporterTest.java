package comtrade.ai;

import org.apache.pdfbox.Loader;
import org.apache.pdfbox.text.PDFTextStripper;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import java.nio.file.*;
import java.time.Instant;
import static org.junit.jupiter.api.Assertions.*;

class PdfReportExporterTest {
    @TempDir Path dir;
    @Test void exportsPaginatedExtractableUnicodeAndPreservesExistingFile() throws Exception {
        Path font=Path.of(System.getProperty("test.pdf.font","/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"));
        assertTrue(Files.isRegularFile(font),"Set -Dtest.pdf.font to a Unicode TTF font");
        var report=new AnalysisReport(Instant.parse("2024-01-01T00:00:00Z"),"mock-model",AiConfig.JSON.createObjectNode().put("samples",2),"Observation Ω café\n".repeat(150));
        Path pdf=dir.resolve("report.pdf"); PdfReportExporter.write(report,font,pdf);
        try(var document=Loader.loadPDF(pdf.toFile())) {
            assertTrue(document.getNumberOfPages()>2);
            String text=new PDFTextStripper().getText(document);
            assertTrue(text.contains("Observation Ω café")); assertTrue(text.contains("unverified hypotheses"));
            assertTrue(document.getPage(0).getResources().getFont(document.getPage(0).getResources().getFontNames().iterator().next()).isEmbedded());
        }
        byte[] original=Files.readAllBytes(pdf);
        assertThrows(java.io.IOException.class,()->PdfReportExporter.write(report,font,pdf));
        assertArrayEquals(original,Files.readAllBytes(pdf));
    }
    @Test void invalidFontLeavesNoPartialReport() {
        Path pdf=dir.resolve("failed.pdf");
        assertThrows(java.io.IOException.class,()->PdfReportExporter.write(new AnalysisReport(Instant.now(),"mock",AiConfig.JSON.createObjectNode(),"text"),dir.resolve("absent.ttf"),pdf));
        assertFalse(Files.exists(pdf));
    }
}
