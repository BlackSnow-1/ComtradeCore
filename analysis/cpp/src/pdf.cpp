#include <codecvt>
#include <comtrade/ai.hpp>
#include <cstdio>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <hpdf.h>
#include <hpdf_font.h>
#include <hpdf_fontdef.h>
#include <iomanip>
#include <locale>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace comtrade::ai {
namespace {
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
void checked(HPDF_STATUS status) { require(status == HPDF_OK, "PDF rendering failed"); }
std::string utf8(char32_t ch) {
    return std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t>{}.to_bytes(ch);
}
} // namespace
void writeNewFile(const std::filesystem::path &path, const std::string &bytes) {
#ifdef _WIN32
    int descriptor = _wopen(path.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
    require(descriptor >= 0, "Cannot create output (destination must not exist)");
    auto *file = _fdopen(descriptor, "wb");
#else
    int descriptor = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    require(descriptor >= 0, "Cannot create output (destination must not exist)");
    auto *file = fdopen(descriptor, "wb");
#endif
    if (!file) {
#ifdef _WIN32
        _close(descriptor);
#else
        close(descriptor);
#endif
        std::error_code error;
        std::filesystem::remove(path, error);
        throw std::runtime_error("Cannot open output stream");
    }
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    ok = std::fclose(file) == 0 && ok;
    if (!ok) {
        std::error_code error;
        std::filesystem::remove(path, error);
        throw std::runtime_error("Output write failed");
    }
}
void exportPdf(const std::filesystem::path &path, const Json &evidence, const std::string &interpretation,
               const Config &config) {
    require(!std::filesystem::exists(path), "PDF already exists");
    require(evidence.is_object(), "PDF evidence must be an object");
    std::unique_ptr<std::remove_pointer_t<HPDF_Doc>, decltype(&HPDF_Free)> pdf(HPDF_New(nullptr, nullptr),
                                                                               HPDF_Free);
    require(bool(pdf), "Cannot create PDF");
    checked(HPDF_UseUTFEncodings(pdf.get()));
    std::ifstream fontInput(std::filesystem::u8path(config.pdfFontPath), std::ios::binary);
    char signature[4]{};
    fontInput.read(signature, 4);
    require(bool(fontInput), "Cannot read PDF font");
    const char *name = std::string(signature, 4) == "ttcf"
                           ? HPDF_LoadTTFontFromFile2(pdf.get(), config.pdfFontPath.c_str(), 0, HPDF_TRUE)
                           : HPDF_LoadTTFontFromFile(pdf.get(), config.pdfFontPath.c_str(), HPDF_TRUE);
    require(name != nullptr, "Cannot load TrueType PDF font");
    auto font = HPDF_GetFont(pdf.get(), name, "UTF-8");
    require(font != nullptr, "Cannot initialize PDF font");
    Json appendix = evidence;
    if (appendix.contains("waveform")) {
        appendix["waveform"].erase("rows");
        appendix["waveform"]["note"] = "Full rows sent to model; omitted from PDF only.";
    }
    auto now = std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::ostringstream stamp;
    stamp << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    const std::string text = "COMTRADE 辅助分析报告\n\n本地测量事实 / Local evidence\nUTC: " + stamp.str() +
                             "\nModel: " + config.model + "\nCFG SHA256: " + evidence.value("cfgSha256", "") +
                             "\nDAT SHA256: " + evidence.value("datSha256", "") +
                             "\nSamples: " + evidence.value("samples", Json(0)).dump() +
                             "\n\n未经验证的模型解释（需人工复核）\n" + interpretation +
                             "\n\n本地证据附录\n" + appendix.dump(2);
    std::u32string characters;
    try {
        characters = std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t>{}.from_bytes(text);
    } catch (...) {
        throw std::runtime_error("PDF text is not valid UTF-8");
    }
    auto attr = static_cast<HPDF_FontAttr>(font->attr);
    // libharu 2.4.4 reuses a Type 1 CID CMap as ToUnicode for UTF-8 fonts.
    // Supply a valid Type 2 identity map so strict PDF readers can extract text.
    require(attr->cmap_stream != nullptr, "Missing PDF Unicode map");
    HPDF_MemStream_FreeData(attr->cmap_stream->stream);
    checked(HPDF_Stream_WriteStr(attr->cmap_stream->stream,
        "/CIDInit /ProcSet findresource begin\n12 dict begin\nbegincmap\n"
        "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n"
        "/CMapName /ComtradeUnicode def\n/CMapType 2 def\n"
        "1 begincodespacerange\n<0000> <FFFF>\nendcodespacerange\n"
        "1 beginbfrange\n<0000> <FFFF> <0000>\nendbfrange\n"
        "endcmap\nCMapName currentdict /CMap defineresource pop\nend\nend\n"));

    for (char32_t ch : characters) {
        if (ch == '\n' || ch == '\r' || ch == '\t')
            continue;
        require(ch >= 32 && ch <= 0xffff && !(ch >= 0xd800 && ch <= 0xdfff) &&
                    HPDF_TTFontDef_GetGlyphid(attr->fontdef, HPDF_UINT16(ch)) != 0,
                "PDF font is missing required glyphs");
    }
    HPDF_Page page = nullptr;
    float y = 0;
    int pageNumber = 0;
    auto newPage = [&] {
        page = HPDF_AddPage(pdf.get());
        require(page != nullptr, "Cannot add PDF page");
        checked(HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT));
        checked(HPDF_Page_SetFontAndSize(page, font, 9));
        checked(HPDF_Page_BeginText(page));
        auto label = "COMTRADE | " + std::to_string(++pageNumber);
        checked(HPDF_Page_TextOut(page, 44, 25, label.c_str()));
        checked(HPDF_Page_EndText(page));
        checked(HPDF_Page_SetFontAndSize(page, font, 10));
        y = HPDF_Page_GetHeight(page) - 44;
    };
    newPage();
    auto line = [&](const std::string &value) {
        if (y < 48)
            newPage();
        checked(HPDF_Page_BeginText(page));
        checked(HPDF_Page_TextOut(page, 44, y, value.c_str()));
        checked(HPDF_Page_EndText(page));
        y -= 15;
    };
    std::string current;
    float width = 0;
    for (char32_t ch : characters) {
        if (ch == '\r')
            continue;
        if (ch == '\n') {
            line(current);
            current.clear();
            width = 0;
            continue;
        }
        if (ch == '\t')
            ch = ' ';
        auto glyph = utf8(ch);
        float glyphWidth = HPDF_Font_GetUnicodeWidth(font, HPDF_UNICODE(ch)) * 0.01f;
        if (width + glyphWidth > HPDF_Page_GetWidth(page) - 88 && !current.empty()) {
            line(current);
            current.clear();
            width = 0;
        }
        current += glyph;
        width += glyphWidth;
    }
    if (!current.empty())
        line(current);
    checked(HPDF_SaveToStream(pdf.get()));
    checked(HPDF_ResetStream(pdf.get()));
    std::string bytes(HPDF_GetStreamSize(pdf.get()), '\0');
    auto length = HPDF_UINT32(bytes.size());
    checked(HPDF_ReadFromStream(pdf.get(), reinterpret_cast<HPDF_BYTE *>(bytes.data()), &length));
    bytes.resize(length);
    writeNewFile(path, bytes);
}
} // namespace comtrade::ai
