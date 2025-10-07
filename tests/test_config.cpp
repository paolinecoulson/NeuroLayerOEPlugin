#include <juce_core/juce_core.h>
#include <gtest/gtest.h>
#include "../Source/NeuroConfig.h"   


// Helper to create a temporary file with given JSON contents
static juce::File writeTempJson(const juce::String& jsonText, const juce::String& fileName = "test_config.json")
{
    auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory);
    auto tempFile = tempDir.getChildFile(fileName);
    tempFile.replaceWithText(jsonText);
    return tempFile;
}

// -------------------------------------------------------------
// TESTS
// -------------------------------------------------------------

TEST(ConfigTests, FailsOnMissingFile)
{
    NeuroConfig cfg;
    juce::File nonExistent ("Z:/some/fake/path/config.json");
    parseNeuroConfig(cfg, nonExistent);
    // This should log an error but not crash — there's no direct return value
    // We can check that structures remain empty
    EXPECT_TRUE(cfg.neuroLayerSystem.columns.empty());
    EXPECT_TRUE(cfg.neuroLayerSystem.rows.empty());
    EXPECT_EQ(cfg.neuroLayerSystem.numRows, 0);
    EXPECT_EQ(cfg.eventInputs.size(), 0);
}

TEST(ConfigTests, ParsesValidNeuroLayerSystem)
{
    const char* json = R"json(
    {
        "neuroLayerSystem": {
            "columns": [
                ["PXI2", "line0"],
                ["PXI2", "line1"]
            ],
            "rows": [
                ["PXI2", "Port0"]
            ],
            "numRows": 8
        }
    }
    )json";

    NeuroConfig cfg;
    juce::File configFile = writeTempJson (json);
    parseNeuroConfig(cfg, configFile);

    ASSERT_TRUE(cfg.neuroLayerSystem.columns.find("PXI2") != cfg.neuroLayerSystem.columns.end());
    auto& lines = cfg.neuroLayerSystem.columns["PXI2"];
    EXPECT_EQ(lines.size(), 2);
    EXPECT_TRUE(lines.contains("line0"));
    EXPECT_TRUE(lines.contains("line1"));

    ASSERT_TRUE(cfg.neuroLayerSystem.rows.find("PXI2") != cfg.neuroLayerSystem.rows.end());
    EXPECT_EQ(cfg.neuroLayerSystem.rows["PXI2"], "Port0");

    EXPECT_EQ(cfg.neuroLayerSystem.numRows, 8);
}

TEST(ConfigTests, ParsesStartEventOutput)
{
    const char* json = R"json(
    {
        "start_event_output": {
            "start_time": 1.5,
            "nbr_pulse": 3,
            "pulse_duration": 0.25,
            "module_name": "PXI3",
            "digital_line": "line7"
        }
    }
    )json";

    NeuroConfig cfg;
    juce::File configFile = writeTempJson (json);
    parseNeuroConfig(cfg, configFile);

    EXPECT_FLOAT_EQ(cfg.startEventOutput.start_time, 1.5f);
    EXPECT_EQ(cfg.startEventOutput.nbr_pulse, 3);
    EXPECT_FLOAT_EQ(cfg.startEventOutput.pulse_duration, 0.25f);
    EXPECT_EQ(cfg.startEventOutput.name, "PXI3");
    EXPECT_EQ(cfg.startEventOutput.digital_line, "line7");
}

TEST(ConfigTests, ParsesEventInputs)
{
    const char* json = R"json(
    {
        "event_input": [
            {
                "module_name": "PXI4",
                "digital_line": "line2",
                "oe_event_label": 42
            },
            {
                "module_name": "PXI5",
                "digital_line": "line3",
                "oe_event_label": 99
            }
        ]
    }
    )json";

    NeuroConfig cfg;
    juce::File configFile = writeTempJson (json);
    parseNeuroConfig(cfg, configFile);

    ASSERT_EQ(cfg.eventInputs.size(), 2);
    EXPECT_EQ(cfg.eventInputs[0].name, "PXI4");
    EXPECT_EQ(cfg.eventInputs[0].digital_line, "line2");
    EXPECT_EQ(cfg.eventInputs[0].oe_event_label, 42);

    EXPECT_EQ(cfg.eventInputs[1].name, "PXI5");
    EXPECT_EQ(cfg.eventInputs[1].digital_line, "line3");
    EXPECT_EQ(cfg.eventInputs[1].oe_event_label, 99);
}

TEST(ConfigTests, HandlesInvalidJson)
{
    const char* invalidJson = "{ this is not valid json";
    juce::File configFile = writeTempJson (invalidJson, "invalid.json");
    NeuroConfig cfg;
    parseNeuroConfig(cfg, configFile);

    EXPECT_TRUE(cfg.neuroLayerSystem.columns.empty());
    EXPECT_TRUE(cfg.neuroLayerSystem.rows.empty());
    EXPECT_EQ(cfg.eventInputs.size(), 0);
}
