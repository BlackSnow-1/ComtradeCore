package comtrade.ai;

import org.apache.pdfbox.pdmodel.*;
import org.apache.pdfbox.pdmodel.common.PDRectangle;
import org.apache.pdfbox.pdmodel.font.PDType0Font;
import org.apache.fontbox.ttf.TrueTypeCollection;
import org.apache.fontbox.ttf.TrueTypeFont;
import java.io.IOException;
import java.nio.file.*;
import java.util.ArrayList;
import java.util.List;

/** Unicode, wrapping and pagination; text-only rendering never interprets model HTML/links. */
public final class PdfReportExporter {
    private PdfReportExporter() {}
    public static void write(AnalysisReport report, Path fontPath, Path destination) throws IOException {
        Path absolute=destination.toAbsolutePath();
        if (Files.exists(absolute)) throw new IOException("Report destination already exists");
        Path temp=Files.createTempFile(absolute.getParent(),"comtrade-report-",".pdf");
        try (PDDocument document=new PDDocument();
             TrueTypeCollection collection=fontPath.toString().toLowerCase(java.util.Locale.ROOT).endsWith(".ttc")
                     ? new TrueTypeCollection(fontPath.toFile()) : null) {
            PDType0Font font;
            if (collection == null) font=PDType0Font.load(document,fontPath.toFile());
            else {
                TrueTypeFont[] first={null};
                collection.processAllFonts(ttf -> { if (first[0]==null) first[0]=ttf; });
                if (first[0]==null) throw new IOException("Empty TrueType collection");
                font=PDType0Font.load(document,first[0],true);
            }
            List<String> paragraphs=new ArrayList<>();
            paragraphs.add("COMTRADE analysis report");
            paragraphs.add("AI-assisted interpretation; engineer review required before operational use.");
            paragraphs.add("Created: "+report.createdAt()+" | Model: "+report.model());
            paragraphs.add("1. Measured evidence (local calculations)");
            var evidence=report.evidence();
            paragraphs.add("CFG SHA-256: "+evidence.path("cfgSha256").asText());
            paragraphs.add("DAT SHA-256: "+evidence.path("datSha256").asText());
            paragraphs.add("Samples: "+evidence.path("samples").asText()+" | DAT: "+evidence.path("dataType").asText());
            paragraphs.add("Start: "+evidence.path("startTime").asText()+" | Trigger: "+evidence.path("triggerTime").asText());
            for (var warning:evidence.path("warnings")) paragraphs.add("Quality note: "+warning.asText());
            for (var channel:evidence.path("analog")) paragraphs.add(channel.toString());
            for (var channel:evidence.path("digital")) paragraphs.add(channel.toString());
            paragraphs.add("2. Model interpretation (unverified hypotheses)");
            paragraphs.add(report.interpretation());
            paragraphs.add("3. Full evidence appendix (bounded windows and retained events)");
            paragraphs.add(AiConfig.JSON.writerWithDefaultPrettyPrinter().writeValueAsString(evidence));
            List<String> lines=new ArrayList<>();
            for (String paragraph:paragraphs) {
                for (String raw:paragraph.split("\\R",-1)) wrap(raw,font,lines);
                lines.add("");
            }
            int perPage=48;
            for (int start=0;start<lines.size();start+=perPage) {
                PDPage page=new PDPage(PDRectangle.A4); document.addPage(page);
                try (PDPageContentStream stream=new PDPageContentStream(document,page)) {
                    stream.beginText(); stream.setFont(font,10); stream.setLeading(15); stream.newLineAtOffset(42,790);
                    for (int i=start;i<Math.min(start+perPage,lines.size());i++) { stream.showText(lines.get(i)); stream.newLine(); }
                    stream.endText(); stream.beginText(); stream.setFont(font,9); stream.newLineAtOffset(42,28);
                    stream.showText("ComtradeCore | Page "+(start/perPage+1)); stream.endText();
                }
            }
            document.save(temp.toFile());
            Files.move(temp,absolute); // Never overwrite an existing report.
        } catch (IllegalArgumentException e) {
            throw new IOException("PDF font lacks report glyphs; configure a compatible Unicode TTF font");
        } finally { Files.deleteIfExists(temp); }
    }
    private static void wrap(String text, PDType0Font font, List<String> lines) throws IOException {
        StringBuilder line=new StringBuilder(); float width=0;
        for (int cp:text.codePoints().toArray()) {
            if (Character.isISOControl(cp)) cp=' ';
            String glyph=new String(Character.toChars(cp)); float w=font.getStringWidth(glyph)*0.01f;
            if (width+w>510 && line.length()>0) { lines.add(line.toString()); line.setLength(0); width=0; }
            line.append(glyph); width+=w;
        }
        lines.add(line.toString());
    }
}
