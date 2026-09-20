// clang-format off
#include <httplib.h>
// clang-format on
#include <comtrade/ai.hpp>
#include <comtrade/ai/statistics.hpp>
#include <comtrade/record.hpp>
#include <comtrade/stream_writer.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <thread>
using namespace comtrade::ai;
namespace fs = std::filesystem;
namespace {
std::string read(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
std::string answer(const std::string &text = "分析完成，需人工复核。") {
    return Json{{"choices", Json::array({{{"finish_reason", "stop"}, {"message", {{"content", text}}}}})}}
        .dump();
}
struct Fixture : testing::Test {
    fs::path dir, cfg, dat;
    void SetUp() override {
        dir = fs::current_path() / "ai-test-artifacts" /
              testing::UnitTest::GetInstance()->current_test_info()->name();
        fs::create_directories(dir);
        cfg = dir / "sample.cfg";
        dat = dir / "sample.dat";
    }
    void make(comtrade::DataType type = comtrade::DataType::ASCII, int samples = 4) {
        comtrade::CfgData c;
        c.version = comtrade::StandardVersion::V2013;
        c.station_name = "private-station";
        c.rec_dev_id = "private-device";
        c.data_type = type;
        c.analog_count = 1;
        c.digital_count = 1;
        c.total_channels = 2;
        comtrade::AnalogChannel a{};
        a.index = 1;
        a.id = "private-channel";
        a.uu = "A";
        a.phase = "A";
        a.a = 2;
        a.b = 1;
        a.primary = 1000;
        a.secondary = 1;
        c.analog_channels.push_back(a);
        comtrade::DigitalChannel d{};
        d.index = 1;
        d.id = "trip";
        c.digital_channels.push_back(d);
        c.sample_rates.push_back({1000, uint32_t(samples)});
        c.trigger_time = c.start_time + std::chrono::milliseconds(2);
        comtrade::Record record;
        record.getMutableCfg() = c;
        ASSERT_TRUE(record.saveCfg(cfg.string()));
        comtrade::StreamWriter writer(c);
        ASSERT_TRUE(writer.open(dat.string()));
        for (int i = 0; i < samples; ++i)
            writer.pushRow(uint32_t(i * 1000), {double(2 * i + 1)}, {bool(i % 2)});
    }
    Config fontConfig() {
        Config c;
        c.model = "test-model";
        const char *font = std::getenv("COMTRADE_TEST_FONT");
        if (font)
            c.pdfFontPath = font;
#ifdef _WIN32
        else
            c.pdfFontPath = "C:/Windows/Fonts/simhei.ttf";
#else
        else
            c.pdfFontPath = "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc";
#endif
        return c;
    }
};
class Mock {
  public:
    httplib::Server server;
    std::thread worker;
    Config config;
    Mock(httplib::Server::Handler handler) {
        server.new_task_queue = [] { return new httplib::ThreadPool(2); };
        server.Post("/v1/chat/completions", std::move(handler));
        int port = server.bind_to_any_port("127.0.0.1");
        if (port < 0)
            throw std::runtime_error("Cannot bind mock server");
        config.endpoint = "http://127.0.0.1:" + std::to_string(port) + "/v1/chat/completions";
        config.model = "test-model";
        config.allowInsecureLocalhost = true;
        config.tokenEnv.clear();
        config.timeoutMs = 2000;
        worker = std::thread([&] { server.listen_after_bind(); });
        server.wait_until_ready();
    }
    ~Mock() {
        server.stop();
        if (worker.joinable())
            worker.join();
    }
};
} // namespace
TEST(Statistics, RmsMeanExtremaAndMerge) {
    Statistics a, b;
    a.add(-3, 10);
    b.add(4, 20);
    a.merge(b);
    EXPECT_DOUBLE_EQ(a.rms(), std::sqrt(12.5));
    EXPECT_DOUBLE_EQ(a.mean, .5);
    EXPECT_EQ(a.minTime, 10);
    EXPECT_EQ(a.maxTime, 20);
    EXPECT_EQ(a.count, 2u);
}
TEST(Statistics, ExtremeFiniteAndNonFinite) {
    Statistics a;
    a.add(1e308, 0);
    a.add(-1e308, 1);
    EXPECT_NEAR(a.rms() / 1e308, 1, 1e-15);
    EXPECT_DOUBLE_EQ(a.mean, 0);
    EXPECT_THROW(a.add(std::numeric_limits<double>::infinity(), 2), std::invalid_argument);
}
TEST(Config, Validation) {
    EXPECT_THROW(Config::fromJson({{"maxWindows", 1}}), std::runtime_error);
    EXPECT_THROW(Config::fromJson({{"maxWindows", 2.5}}), std::runtime_error);
    EXPECT_THROW(Config::fromJson({{"unknown", 1}}), std::runtime_error);
    EXPECT_THROW(Config::fromJson({{"headers", {{"Authorization", "x"}}}}), std::runtime_error);
    EXPECT_THROW(Config::fromJson({{"headers", {{"X-Test", "x\r\ny"}}}}), std::runtime_error);
    EXPECT_THROW(Config::fromJson({{"requestParameters", {{"messages", Json::array()}}}}),
                 std::runtime_error);
    Config c;
    c.model = "m";
    for (auto url : {"http://example.com/v1", "http://127.0.0.1/v1", "https://user:pass@example.com/v1",
                     "https://example.com/v1#fragment", "https://example.com:99999/v1"}) {
        c.endpoint = url;
        EXPECT_THROW(c.validate(true), std::runtime_error);
    }
    c.endpoint = "https://example.com/v1";
    EXPECT_NO_THROW(c.validate(true));
}
TEST_F(Fixture, FourFormatsAndScaling) {
    for (auto type : {comtrade::DataType::ASCII, comtrade::DataType::BINARY, comtrade::DataType::BINARY32,
                      comtrade::DataType::FLOAT32}) {
        make(type);
        Config c;
        c.fullWaveform = true;
        auto e = summarize(cfg, dat, c);
        EXPECT_EQ(e["samples"], 4);
        EXPECT_DOUBLE_EQ(e["analog"][0]["mean"].get<double>(), 4);
        EXPECT_DOUBLE_EQ(e["analog"][0]["rms"].get<double>(), std::sqrt(21.0));
        EXPECT_EQ(e["beforeTrigger"][0]["count"], 2);
        EXPECT_EQ(e["fromTrigger"][0]["count"], 2);
        EXPECT_EQ(e["waveform"]["rows"][3][3], 7);
        EXPECT_EQ(e["waveform"]["rows"][3][2], "3000000");
        EXPECT_EQ(e["digital"][0]["transitions"], 3);
        EXPECT_EQ(e["cfgSha256"].get<std::string>().size(), 64u);
        EXPECT_EQ(e.dump().find("private-station"), std::string::npos);
        EXPECT_EQ(e.dump().find("private-channel"), std::string::npos);
        c.includeIdentifiers = true;
        EXPECT_EQ(summarize(cfg, dat, c)["station"], "private-station");
    }
}
TEST_F(Fixture, WindowCompactionAndEventBounds) {
    make(comtrade::DataType::ASCII, 101);
    Config c;
    c.maxWindows = 3;
    c.windowMs = 1;
    c.maxDigitalEvents = 2;
    auto e = summarize(cfg, dat, c);
    EXPECT_LE(e["windows"].size(), 3u);
    EXPECT_EQ(e["digitalEvents"].size(), 2u);
    EXPECT_EQ(e["digitalEventsOmitted"], 98);
    uint64_t count = 0;
    double max = 0;
    for (auto &w : e["windows"]) {
        count += w["analog"][0]["count"].get<uint64_t>();
        max = std::max(max, w["analog"][0]["max"].get<double>());
    }
    EXPECT_EQ(count, 101u);
    EXPECT_EQ(max, 201);
}
TEST_F(Fixture, FullLimitsAndMalformedRows) {
    make();
    Config c;
    c.fullWaveform = true;
    c.maxWaveformSamples = 3;
    EXPECT_THROW(summarize(cfg, dat, c), std::runtime_error);
    c.maxWaveformSamples = 100;
    c.maxRequestBytes = 10;
    EXPECT_THROW(summarize(cfg, dat, c), std::runtime_error);
    c.maxRequestBytes = 10000;
    {
        std::ofstream f(dat, std::ios::app);
        f << "bad-row\n";
    }
    EXPECT_THROW(summarize(cfg, dat, c), std::runtime_error);
    c.fullWaveform = false;
    EXPECT_EQ(summarize(cfg, dat, c)["physicalRows"], 5);
    c.maxChannels = 1;
    EXPECT_THROW(summarize(cfg, dat, c), std::runtime_error);
    EXPECT_THROW(summarize(cfg, dir / "missing.dat", Config{}), std::runtime_error);
}
TEST_F(Fixture, PartialBinaryRejected) {
    make(comtrade::DataType::BINARY);
    {
        std::ofstream f(dat, std::ios::app | std::ios::binary);
        f << 'x';
    }
    EXPECT_THROW(summarize(cfg, dat, Config{}), std::runtime_error);
}
TEST_F(Fixture, QualityWarningsAndNonFinite) {
    make();
    {
        std::ofstream f(dat);
        f << "1,0,1,0\n3,0,nan,1\n4,0,2,0\n5,1,3,0\n";
    }
    auto e = summarize(cfg, dat, Config{});
    EXPECT_EQ(e["nonFiniteValues"], 1);
    EXPECT_EQ(e["nonIncreasingTimes"], 2);
    EXPECT_EQ(e["indexDiscontinuities"], 1);
    Config c;
    c.fullWaveform = true;
    EXPECT_THROW(summarize(cfg, dat, c), std::runtime_error);
}
TEST(Response, InvalidTruncatedRefusedAndToolCalls) {
    EXPECT_EQ(parseResponse(answer("ok")), "ok");
    EXPECT_THROW(parseResponse("not-json-secret"), std::runtime_error);
    EXPECT_THROW(parseResponse("{}"), std::runtime_error);
    for (auto reason : {"length", "tool_calls", "content_filter"}) {
        auto j = Json::parse(answer());
        j["choices"][0]["finish_reason"] = reason;
        EXPECT_THROW(parseResponse(j.dump()), std::runtime_error);
    }
    for (auto key : {"refusal", "tool_calls", "function_call"}) {
        auto j = Json::parse(answer());
        j["choices"][0]["message"][key] = "no";
        EXPECT_THROW(parseResponse(j.dump()), std::runtime_error);
    }
    EXPECT_THROW(parseResponse(answer("  \n")), std::runtime_error);
}
TEST(Http, RealPostBodyHeadersAndByteBudget) {
    std::atomic<int> requests{0};
    Json received;
    std::string auth;
    Mock mock([&](const httplib::Request &req, httplib::Response &res) {
        ++requests;
        received = Json::parse(req.body);
        auth = req.get_header_value("Authorization");
        res.set_content(answer(), "application/json");
    });
    mock.config.token = "test-secret";
    mock.config.requestParameters["temperature"] = 0.2;
    EXPECT_EQ(analyze({{"evidence", "中文"}}, mock.config), "分析完成，需人工复核。");
    EXPECT_EQ(received["model"], "test-model");
    EXPECT_EQ(received["temperature"], 0.2);
    EXPECT_EQ(auth, "Bearer test-secret");
    auto evidence = Json::parse(received["messages"][1]["content"].get<std::string>());
    EXPECT_EQ(evidence["evidence"], "中文");
    mock.config.maxRequestBytes = 5;
    EXPECT_THROW(analyze({}, mock.config), std::runtime_error);
    EXPECT_EQ(requests, 1);
}
TEST(Http, ResponseLimitTimeoutErrorAndRedirect) {
    std::atomic<int> requests{0};
    Mock mock([&](const httplib::Request &, httplib::Response &res) {
        ++requests;
        res.set_content(std::string(5000, 'x'), "application/json");
    });
    mock.config.maxResponseBytes = 100;
    EXPECT_THROW(analyze({}, mock.config), std::runtime_error);
    Mock slow([](const httplib::Request &, httplib::Response &res) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        res.set_content(answer(), "application/json");
    });
    slow.config.timeoutMs = 40;
    EXPECT_THROW(analyze({}, slow.config), std::runtime_error);
    Mock error([](const httplib::Request &, httplib::Response &res) {
        res.status = 401;
        res.set_content("secret-body", "text/plain");
    });
    try {
        analyze({}, error.config);
        FAIL();
    } catch (const std::exception &e) {
        EXPECT_EQ(std::string(e.what()).find("secret-body"), std::string::npos);
    }
    Mock redirect([&](const httplib::Request &, httplib::Response &res) {
        res.set_redirect(mock.config.endpoint, 307);
    });
    EXPECT_THROW(analyze({}, redirect.config), std::runtime_error);
    EXPECT_EQ(requests, 1);
}
TEST_F(Fixture, EndToEndChinesePdf) {
    make();
    Config c = fontConfig();
    ASSERT_TRUE(fs::exists(c.pdfFontPath)) << "Set COMTRADE_TEST_FONT to a Chinese TTF/TTC font";
    c.fullWaveform = true;
    auto evidence = summarize(cfg, dat, c);
    Mock mock([](const httplib::Request &, httplib::Response &res) {
        std::string text;
        for (int i = 0; i < 160; ++i)
            text += "中文录波分析：A1 电流变化，必须结合原始波形人工复核。\n";
        res.set_content(answer(text), "application/json");
    });
    auto text = analyze(evidence, mock.config);
    auto pdf = dir / "report.pdf";
    if (fs::exists(pdf))
        fs::remove(pdf);
    exportPdf(pdf, evidence, text, c);
    EXPECT_EQ(read(pdf).substr(0, 5), "%PDF-");
    EXPECT_GT(fs::file_size(pdf), 1000u);
    EXPECT_THROW(exportPdf(pdf, evidence, text, c), std::runtime_error);
    EXPECT_EQ(read(pdf).find("Bearer"), std::string::npos);
}
TEST_F(Fixture, PdfErrorsAndExclusiveOutput) {
    auto c = fontConfig();
    auto path = dir / "bad.pdf";
    EXPECT_THROW(exportPdf(path, {}, "\xf0\x9f\x98\x80", c), std::runtime_error);
    EXPECT_FALSE(fs::exists(path));
    c.pdfFontPath = "missing.ttf";
    EXPECT_THROW(exportPdf(path, {}, "x", c), std::runtime_error);
    EXPECT_FALSE(fs::exists(path));
    auto output = dir / "existing.txt";
    {
        std::ofstream f(output);
        f << "keep";
    }
    EXPECT_THROW(writeNewFile(output, "replace"), std::runtime_error);
    EXPECT_EQ(read(output), "keep");
}

TEST_F(Fixture, TlsTrustAndHostnameVerification) {
    // Temporary diagnostics for a CI-only failure of this exact test: prints the TLS trust
    // decision (subject/issuer/verify error code) to stderr, visible via
    // `ctest --output-on-failure`. Harmless if left in: off unless this variable is set.
#ifdef _WIN32
    _putenv_s("COMTRADE_AI_TLS_DEBUG", "1");
#else
    setenv("COMTRADE_AI_TLS_DEBUG", "1", 1);
#endif
    struct ResetDebugEnv {
        ~ResetDebugEnv() {
#ifdef _WIN32
            _putenv_s("COMTRADE_AI_TLS_DEBUG", "");
#else
            unsetenv("COMTRADE_AI_TLS_DEBUG");
#endif
        }
    } resetDebugEnv;
    // Generate a short-lived localhost certificate; no external server or credentials.
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> context(
        EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), EVP_PKEY_CTX_free);
    ASSERT_TRUE(context);
    ASSERT_EQ(EVP_PKEY_keygen_init(context.get()), 1);
    ASSERT_EQ(EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(), 2048), 1);
    EVP_PKEY *rawKey = nullptr;
    ASSERT_EQ(EVP_PKEY_keygen(context.get(), &rawKey), 1);
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(rawKey, EVP_PKEY_free);
    std::unique_ptr<X509, decltype(&X509_free)> cert(X509_new(), X509_free);
    ASSERT_TRUE(cert);
    ASSERT_EQ(X509_set_version(cert.get(), 2), 1);
    ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1);
    X509_gmtime_adj(X509_get_notBefore(cert.get()), -3600);
    X509_gmtime_adj(X509_get_notAfter(cert.get()), 86400);
    ASSERT_EQ(X509_set_pubkey(cert.get(), key.get()), 1);
    auto *name = X509_get_subject_name(cert.get());
    ASSERT_EQ(X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                         reinterpret_cast<const unsigned char *>("localhost"), -1, -1, 0),
              1);
    ASSERT_EQ(X509_set_issuer_name(cert.get(), name), 1);
    X509V3_CTX extensionContext{};
    X509V3_set_ctx(&extensionContext, cert.get(), cert.get(), nullptr, nullptr, 0);
    auto addExtension = [&](int nid, const char *value) {
        auto *extension = X509V3_EXT_conf_nid(nullptr, &extensionContext, nid, const_cast<char *>(value));
        if (!extension)
            return false;
        bool ok = X509_add_ext(cert.get(), extension, -1) == 1;
        X509_EXTENSION_free(extension);
        return ok;
    };
    ASSERT_TRUE(addExtension(NID_basic_constraints, "critical,CA:TRUE"));
    ASSERT_TRUE(addExtension(NID_subject_alt_name, "IP:127.0.0.1"));
    ASSERT_EQ(X509_sign(cert.get(), key.get(), EVP_sha256()) > 0, true);
    auto certPath = dir / "localhost.pem", keyPath = dir / "localhost-key.pem";
    {
        std::unique_ptr<BIO, decltype(&BIO_free)> memory(BIO_new(BIO_s_mem()), BIO_free);
        ASSERT_TRUE(memory);
        ASSERT_EQ(PEM_write_bio_X509(memory.get(), cert.get()), 1);
        char* bytes = nullptr; auto length = BIO_get_mem_data(memory.get(), &bytes);
        std::ofstream(certPath, std::ios::binary).write(bytes, length);
        ASSERT_EQ(BIO_reset(memory.get()), 1);
        ASSERT_EQ(PEM_write_bio_PrivateKey(memory.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr), 1);
        length = BIO_get_mem_data(memory.get(), &bytes);
        std::ofstream(keyPath, std::ios::binary).write(bytes, length);
    }
    httplib::SSLServer server(certPath.string().c_str(), keyPath.string().c_str());
    ASSERT_TRUE(server.is_valid());
    server.new_task_queue = [] { return new httplib::ThreadPool(2); };
    server.Post("/v1", [](const httplib::Request &, httplib::Response &response) {
        response.set_content(answer("tls-ok"), "application/json");
    });
    int port = server.bind_to_any_port("127.0.0.1");
    ASSERT_GT(port, 0);
    std::thread worker([&] { server.listen_after_bind(); });
    server.wait_until_ready();
    struct Join {
        httplib::SSLServer &server;
        std::thread &worker;
        ~Join() {
            server.stop();
            worker.join();
        }
    } join{server, worker};
    Config c;
    c.model = "test";
    c.tokenEnv.clear();
    c.timeoutMs = 2000;
    c.endpoint = "https://127.0.0.1:" + std::to_string(port) + "/v1";
    EXPECT_THROW(analyze(Json::object(), c), std::runtime_error);
    c.caBundle = certPath.string();
    EXPECT_EQ(analyze(Json::object(), c), "tls-ok");
    c.endpoint = "https://localhost:" + std::to_string(port) + "/v1";
    EXPECT_THROW(analyze(Json::object(), c), std::runtime_error);
}

TEST(Http, EnvironmentTokenOverridesFallback) {
    std::string received;
    Mock mock([&](const httplib::Request &request, httplib::Response &response) {
        received = request.get_header_value("Authorization");
        response.set_content(answer("ok"), "application/json");
    });
    const char *key = "COMTRADE_CPP_TEST_TOKEN";
    const char *previous = std::getenv(key);
    std::string saved = previous ? previous : "";
    bool existed = previous != nullptr;
    auto set = [&](const char *value) {
#ifdef _WIN32
        _putenv_s(key, value ? value : "");
#else
        if (value)
            setenv(key, value, 1);
        else
            unsetenv(key);
#endif
    };
    mock.config.tokenEnv = key;
    mock.config.token = "fallback";
    set("environment");
    EXPECT_EQ(analyze(Json::object(), mock.config), "ok");
    EXPECT_EQ(received, "Bearer environment");
    set("");
    EXPECT_EQ(analyze(Json::object(), mock.config), "ok");
    EXPECT_EQ(received, "Bearer fallback");
    set(existed ? saved.c_str() : nullptr);
}
