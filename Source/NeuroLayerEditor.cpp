/*
 ------------------------------------------------------------------

 This file is part of the Open Ephys GUI
 Copyright (C) 2025 VIB Haesler lab
 Developed by Marine Guyot - CodingResearcher

 ------------------------------------------------------------------

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

#include "NeuroLayerEditor.h"
#include "NeuroLayerThread.h"

NeuroLayerEditor::NeuroLayerEditor(GenericProcessor* parentNode, NeuroLayer* thread)
    : GenericEditor(parentNode), thread(thread)
{
    desiredWidth = 200;
    setupUI();

}

void NeuroLayerEditor::setupUI()
{
    voltageLabel = new Label ();
    addAndMakeVisible (voltageLabel.get());
    voltageLabel->setText ("Voltage range:", dontSendNotification);
    voltageLabel->setBounds (10, 50, 100, 20);

    voltageRangeSelector = new ComboBox("Voltage Range");
    voltageRangeSelector->addListener(this);
    addAndMakeVisible(voltageRangeSelector.get());
    voltageRangeSelector->setBounds(105, 50, 90, 20);

    // Config File Button
    configFileButton = new TextButton("Select Config File");
    configFileButton->addListener(this);
    addAndMakeVisible(configFileButton.get());
    configFileButton->setBounds(15, 80, 150, 20);

    configFileLabel = new Label();
    addAndMakeVisible(configFileLabel.get());
    configFileLabel->setBounds(15, 105, 200, 20);
}

void NeuroLayerEditor::comboBoxChanged(ComboBox* comboBoxThatChanged)
{
    if (thread != nullptr && comboBoxThatChanged == voltageRangeSelector.get())
    {
        int selectedId = voltageRangeSelector->getSelectedId();
        thread->setVoltageRange(selectedId); 
        CoreServices::updateSignalChain (this);
    }
}

void NeuroLayerEditor::buttonClicked(Button* button)
{
    if (button == configFileButton.get())
    {
        FileChooser chooser("Select Config File",
                            File::getSpecialLocation(File::userHomeDirectory),
                            "*.json");

        if (chooser.browseForFileToOpen())
        {
            File configFile = chooser.getResult();
            configFileLabel->setText(configFile.getFileName(), dontSendNotification);
            if (thread != nullptr)
                thread->setConfigFile(configFile); // processor handles parsing

                auto voltage_range = thread->getVoltageRange();
                voltageRangeSelector->clear(); 

                for(int i=0; i<voltage_range.size(); i++){
                    voltageRangeSelector->addItem ("-" + String(voltage_range[i]) + " to " +  String(voltage_range[i]) + " V", i);
                }
                voltageRangeSelector->setSelectedId(1); 
                CoreServices::updateSignalChain (this);
        }

    }
}
void NeuroLayerEditor::saveCustomParametersToXml (XmlElement* xml)
{
    if (xml == nullptr)
        return;

    // -----------------------------
    // neuroLayerSystem
    // -----------------------------
    XmlElement* sysXml = xml->createNewChildElement ("neuroLayerSystem");
    sysXml->setAttribute ("numRows", thread->neuroConfig.neuroLayerSystem.numRows);

    // Columns (array of pairs)
    XmlElement* colsXml = sysXml->createNewChildElement ("columns");
    for (const auto& entry : thread->neuroConfig.neuroLayerSystem.columns)
    {
        const auto& module = entry.first;
        for (const auto& line : entry.second)
        {
                XmlElement* colItem = colsXml->createNewChildElement ("item");
                colItem->setAttribute ("module", module);
                colItem->setAttribute ("line", line);
        }
    }

    // Rows (array of pairs)
    XmlElement* rowsXml = sysXml->createNewChildElement ("rows");
    for (const auto& entry : thread->neuroConfig.neuroLayerSystem.rows)
    {
        XmlElement* rowItem = rowsXml->createNewChildElement ("item");
        rowItem->setAttribute ("module", entry.first);
        rowItem->setAttribute ("port", entry.second);
    }

    // -----------------------------
    // start_event_output
    // -----------------------------
    const auto& start = thread->neuroConfig.startEventOutput;
    XmlElement* startXml = xml->createNewChildElement ("start_event_output");
    startXml->setAttribute ("start_time", start.start_time);
    startXml->setAttribute ("nbr_pulse", start.nbr_pulse);
    startXml->setAttribute ("pulse_duration", start.pulse_duration);
    startXml->setAttribute ("module_name", start.name);
    startXml->setAttribute ("digital_line", start.digital_line);

    // -----------------------------
    // event_input
    // -----------------------------
    XmlElement* eventsXml = xml->createNewChildElement ("event_input");
    for (const auto& ev : thread->neuroConfig.eventInputs)
    {
        XmlElement* evXml = eventsXml->createNewChildElement ("item");
        evXml->setAttribute ("module_name", ev.name);
        evXml->setAttribute ("digital_line", ev.digital_line);
        evXml->setAttribute ("oe_event_label", ev.oe_event_label);
    }

    // -----------------------------
    // voltage_range
    // -----------------------------
    XmlElement* voltXml = xml->createNewChildElement ("voltage_range");

    const auto& voltageRanges = thread->getVoltageRange();
    for (auto v : voltageRanges)
    {
        XmlElement* item = voltXml->createNewChildElement ("item");
        item->setAttribute ("voltage", v);
    }

    // Store currently selected voltage range ID (if available)
    voltXml->setAttribute ("voltage_id", voltageRangeSelector->getSelectedId());

    // -----------------------------
    // config file name
    // -----------------------------
    xml->setAttribute ("config_file_name", configFileLabel->getText());
}

void NeuroLayerEditor::loadCustomParametersFromXml (XmlElement* xml)
{
    if (xml == nullptr)
        return;

    // -----------------------------
    // neuroLayerSystem
    // -----------------------------
    if (auto* sysXml = xml->getChildByName("neuroLayerSystem"))
    {
        thread->neuroConfig.neuroLayerSystem.columns.clear();
        thread->neuroConfig.neuroLayerSystem.rows.clear();
        thread->neuroConfig.neuroLayerSystem.numRows = 
            sysXml->getIntAttribute("numRows", 0);

        // Columns
        if (auto* colsXml = sysXml->getChildByName("columns"))
        {
            forEachXmlChildElementWithTagName(*colsXml, item, "item")
            {
                const auto module = item->getStringAttribute("module");
                const auto line   = item->getStringAttribute("line");
                if (module.isNotEmpty() && line.isNotEmpty())
                    thread->neuroConfig.neuroLayerSystem.columns[module].add(line);
            }
        }

        // Rows
        if (auto* rowsXml = sysXml->getChildByName("rows"))
        {
            forEachXmlChildElementWithTagName(*rowsXml, item, "item")
            {
                const auto module = item->getStringAttribute("module");
                const auto port   = item->getStringAttribute("port");
                if (module.isNotEmpty() && port.isNotEmpty())
                    thread->neuroConfig.neuroLayerSystem.rows[module] = port;
            }
        }
    }

    // -----------------------------
    // start_event_output
    // -----------------------------
    if (auto* startXml = xml->getChildByName("start_event_output"))
    {
        auto& start = thread->neuroConfig.startEventOutput;
        start.start_time   = (float) startXml->getDoubleAttribute("start_time", 0.0);
        start.nbr_pulse    = startXml->getIntAttribute("nbr_pulse", 0);
        start.pulse_duration = (float) startXml->getDoubleAttribute("pulse_duration", 0.0);
        start.name         = startXml->getStringAttribute("module_name", "");
        start.digital_line = startXml->getStringAttribute("digital_line", "");
    }

    // -----------------------------
    // event_input
    // -----------------------------
    thread->neuroConfig.eventInputs.clear();
    if (auto* eventsXml = xml->getChildByName("event_input"))
    {
        forEachXmlChildElementWithTagName(*eventsXml, evXml, "item")
        {
            EventInputConfig ev;
            ev.name          = evXml->getStringAttribute("module_name", "");
            ev.digital_line  = evXml->getStringAttribute("digital_line", "");
            ev.oe_event_label= evXml->getIntAttribute("oe_event_label", 0);

            // Only add valid entries
            if (ev.name.isNotEmpty() || ev.digital_line.isNotEmpty())
                thread->neuroConfig.eventInputs.add(ev);
        }
    }

    thread->reloadConfig();

    // -----------------------------
    // voltage_range
    // -----------------------------
    if (auto* voltXml = xml->getChildByName("voltage_range"))
    {
        voltageRangeSelector->clear();
        int i = 1; // JUCE combo box IDs start at 1

        forEachXmlChildElementWithTagName(*voltXml, item, "item")
        {
            auto voltageStr = (float)item->getDoubleAttribute("voltage", 0);
            if (voltageStr != 0)
            {
                auto label = "-" + String(voltageStr) + " to " + String(voltageStr) + " V";
                voltageRangeSelector->addItem(label, i++);
            }
        }

        voltageRangeSelector->setSelectedId(
            voltXml->getIntAttribute("voltage_id", 0),
            dontSendNotification
        );
    }
    else
    {
        voltageRangeSelector->clear();
    }

    // -----------------------------
    // config file name
    // -----------------------------
    configFileLabel->setText(
        xml->getStringAttribute("config_file_name", ""),
        dontSendNotification
    );
}
