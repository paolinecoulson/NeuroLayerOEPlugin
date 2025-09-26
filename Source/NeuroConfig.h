#pragma once

#include <juce_core/juce_core.h>
#include <map>

// ---------------------------------------------------
// Structs
// ---------------------------------------------------

struct NeuroLayerSystemConfig
{
    // Map each module -> list of lines
    std::map<juce::String, juce::StringArray> columns; // e.g. "PXI2" -> {"line0", "line1"}
    std::map<juce::String, juce::String> rows;    // e.g. "PXI2" -> {"Port0"}
    int numRows; // number of lines used in the digital Port

};

struct StartEventOutputConfig
{
    float start_time = 0;
    int nbr_pulse = 0;
    float pulse_duration = 0;
    juce::String name;
    juce::String digital_line;
};

struct EventInputConfig
{
    juce::String name;
    juce::String digital_line;
    int oe_event_label = 0;
};

struct NeuroConfig
{
    NeuroLayerSystemConfig neuroLayerSystem;
    StartEventOutputConfig startEventOutput;
    juce::Array<EventInputConfig> eventInputs;
};

// ---------------------------------------------------
// Parsing function
// ---------------------------------------------------

inline void  parseNeuroConfig (NeuroConfig& cfg , const File& configFile)
{
    

    if (!configFile.existsAsFile())
    {
        LOGE("Config file not found: " + configFile.getFullPathName());
    }

    auto jsonText = configFile.loadFileAsString();
    var parsed = JSON::parse(jsonText);

    if (parsed.isVoid() || !parsed.isObject())
    {
        LOGE("Failed to parse JSON config.");

    }

    auto* root = parsed.getDynamicObject();

    // ----------------------
    // neuroLayerSystem
    // ----------------------
    if (root->hasProperty("neuroLayerSystem"))
    {
        var sys = root->getProperty("neuroLayerSystem");

        if (auto* sysObj = sys.getDynamicObject())
        {
            // Columns: [["PXI2","line0"], ...]
            if (sysObj->hasProperty("columns"))
            {
                var cols = sysObj->getProperty("columns");
                if (cols.isArray())
                {
                    for (auto& col : *cols.getArray())
                    {
                        if (col.isArray() && col.getArray()->size() == 2)
                        {
                            auto module = col[0].toString();
                            auto line   = col[1].toString();
                            cfg.neuroLayerSystem.columns[module].add (line);
                        }
                    }
                }
            }

            // Rows: [["PXI2","Port0"], ...]
            if (sysObj->hasProperty("rows"))
            {
                var rows = sysObj->getProperty("rows");
                if (rows.isArray())
                {
                    for (auto& row : *rows.getArray())
                    {
                        if (row.isArray() && row.getArray()->size() == 2)
                        {
                            String module = row[0].toString();
                            String port   = row[1].toString();
                            cfg.neuroLayerSystem.rows[module]= port;
                        }
                    }
                }
            }
            
            if (sysObj->hasProperty("numRows"))
            {
                cfg.neuroLayerSystem.numRows= sysObj->getProperty("numRows");
            }

        }
    }

    // ----------------------
    // start_event_output
    // ----------------------
    if (root->hasProperty("start_event_output"))
    {
        var start = root->getProperty("start_event_output");
        if (auto* startObj = start.getDynamicObject())
        {
            cfg.startEventOutput.start_time    = float(startObj->getProperty("start_time"));
            cfg.startEventOutput.nbr_pulse     = int(startObj->getProperty("nbr_pulse"));
            cfg.startEventOutput.pulse_duration= float(startObj->getProperty("pulse_duration"));
            cfg.startEventOutput.name   = startObj->getProperty("module_name").toString();
            cfg.startEventOutput.digital_line  = startObj->getProperty("digital_line").toString();
        }
    }

    // ----------------------
    // event_input
    // ----------------------
    if (root->hasProperty("event_input"))
    {
        var events = root->getProperty("event_input");
        if (events.isArray())
        {
            for (auto& ev : *events.getArray())
            {
                if (auto* evObj = ev.getDynamicObject())
                {
                    EventInputConfig input;
                    input.name   = evObj->getProperty("module_name").toString();
                    input.digital_line  = evObj->getProperty("digital_line").toString();
                    input.oe_event_label= int(evObj->getProperty("oe_event_label"));

                    cfg.eventInputs.add(input);
                }
            }
        }
    }
   
}