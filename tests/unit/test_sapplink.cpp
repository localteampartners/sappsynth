#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "plugin/SappLinkCCMap.h"

// The vendored manifest at tests/data/sapplink-manifest.json mirrors the
// SOURCE OF TRUTH at ~/apps/sapptune/sapplink/manifests/sappsynth.json.
// If sapptune's manifest changes, update the vendored copy AND the table in
// SappLinkCCMap.cpp together — this test exists to make silent drift fail CI.

using namespace sappsynth::sapplink;

namespace {

struct ManifestRow
{
    int cc { -1 };
    std::string id, curve;
    float lo { 0 }, hi { 0 };
};

// Minimal extractor for the known manifest shape (no JSON dependency in the
// core test target): parses each object in the "parameters" array.
std::vector<ManifestRow> loadManifest(const std::string& path)
{
    std::ifstream file(path);
    REQUIRE(file.good());
    std::stringstream ss;
    ss << file.rdbuf();
    const std::string text = ss.str();

    auto grabString = [](const std::string& obj, const std::string& key)
    {
        const auto k = obj.find("\"" + key + "\"");
        if (k == std::string::npos) return std::string();
        const auto q1 = obj.find('"', obj.find(':', k));
        const auto q2 = obj.find('"', q1 + 1);
        return obj.substr(q1 + 1, q2 - q1 - 1);
    };

    std::vector<ManifestRow> rows;
    std::size_t pos = text.find("\"parameters\"");
    while ((pos = text.find("{ \"id\"", pos)) != std::string::npos)
    {
        const auto end = text.find('}', pos);
        const std::string obj = text.substr(pos, end - pos);
        ManifestRow row;
        row.id = grabString(obj, "id");
        row.curve = grabString(obj, "curve");
        row.cc = std::stoi(obj.substr(obj.find(':', obj.find("\"cc\"")) + 1));
        const auto rangeStart = obj.find('[', obj.find("\"range\""));
        const auto comma = obj.find(',', rangeStart);
        row.lo = std::stof(obj.substr(rangeStart + 1, comma - rangeStart - 1));
        row.hi = std::stof(obj.substr(comma + 1, obj.find(']', comma) - comma - 1));
        rows.push_back(row);
        pos = end;
    }
    return rows;
}

} // namespace

TEST_CASE("SappLink table matches the vendored manifest exactly")
{
    const auto rows = loadManifest(std::string(SAPPSYNTH_TEST_DATA_DIR) + "/sapplink-manifest.json");
    REQUIRE(rows.size() == static_cast<std::size_t>(kNumMappings));

    for (const auto& row : rows)
    {
        const auto* mapping = findMapping(row.cc);
        INFO("cc " << row.cc << " id " << row.id);
        REQUIRE(mapping != nullptr);
        REQUIRE(std::string(mapping->paramId) == row.id);
        REQUIRE(mapping->lo == row.lo);
        REQUIRE(mapping->hi == row.hi);
        REQUIRE((mapping->curve == Curve::Log ? "log" : "linear") == row.curve);
    }
}

TEST_CASE("CC curves interpolate correctly at endpoints and midpoint")
{
    const auto* cutoff = findMapping(74);
    REQUIRE(cutoff != nullptr);
    REQUIRE(std::abs(ccToEngineering(*cutoff, 0) - 20.0f) < 1e-3f);
    REQUIRE(std::abs(ccToEngineering(*cutoff, 127) - 20000.0f) < 1.0f);
    // Log midpoint = geometric mean-ish: sqrt(20 * 20000) ~ 632 Hz.
    const float mid = ccToEngineering(*cutoff, 64);
    REQUIRE(mid > 550.0f);
    REQUIRE(mid < 730.0f);

    const auto* master = findMapping(7);
    REQUIRE(master != nullptr);
    REQUIRE(std::abs(ccToEngineering(*master, 0) - (-40.0f)) < 1e-3f);
    REQUIRE(std::abs(ccToEngineering(*master, 127) - 6.0f) < 1e-3f);

    // Reserved controllers are NOT mapped: mod wheel, sustain.
    REQUIRE(findMapping(1) == nullptr);
    REQUIRE(findMapping(64) == nullptr);

    // Every mapping is monotonic and finite across the CC range.
    for (const auto& mapping : mappings())
    {
        float previous = ccToEngineering(mapping, 0);
        for (int v = 1; v <= 127; ++v)
        {
            const float value = ccToEngineering(mapping, v);
            REQUIRE(std::isfinite(value));
            REQUIRE(value >= previous - 1e-6f);
            previous = value;
        }
    }
}
