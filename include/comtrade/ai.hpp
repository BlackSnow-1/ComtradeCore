/**
 * @file ai.hpp
 * @brief Single include for the optional, header-only COMTRADE AI analysis module, implemented
 *        under comtrade/ai/ as its own self-contained subtree, separate from the rest of the
 *        header-only COMTRADE reader/writer implementation in comtrade/*.hpp.
 *
 * Given a COMTRADE CFG/DAT pair, this module builds a local measurement summary
 * (comtrade::ai::summarize, ai/evidence.hpp), optionally sends it to a configured
 * Chat-Completions-compatible model over HTTPS (comtrade::ai::analyze, ai/client.hpp) and renders
 * the result into a Chinese-capable PDF report (comtrade::ai::exportPdf, ai/pdf.hpp). Everything is
 * driven by comtrade::ai::Config (ai/config.hpp), normally loaded from a JSON file such as
 * config/ai-config.example.json via Config::fromFile().
 *
 * This module is entirely optional: including this header pulls in nlohmann/json, cpp-httplib,
 * OpenSSL and libharu, none of which the rest of ComtradeCore needs. Build it with the CMake option
 * COMTRADE_BUILD_AI=ON, which defines the comtrade::ComtradeAI target with the right usage
 * requirements; installing it is a separate COMTRADE_INSTALL_AI switch. See docs/ai-analysis.md for
 * build instructions, the exact evidence schema, wire protocol and safety guarantees.
 */
#pragma once

#include "ai/client.hpp"
#include "ai/config.hpp"
#include "ai/evidence.hpp"
#include "ai/pdf.hpp"
#include "ai/statistics.hpp"
