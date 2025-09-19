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
    voltageRangeSelector = new ComboBox("Voltage Range");
    voltageRangeSelector->addListener(this);
    addAndMakeVisible(voltageRangeSelector.get());
    voltageRangeSelector->setBounds(15, 50, 100, 20);

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
        thread->setVoltageRange(selectedId); // Map 1->1V, 2->5V, 3->10V in your processor
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
                LOGD ("voltage rang: " + std::to_string (voltage_range[i]));
                voltageRangeSelector->addItem ("-" + std::to_string (voltage_range[i]) + " to " + std::to_string (voltage_range[i]) + " V", i);
                }
                voltageRangeSelector->setSelectedId(1); 
                CoreServices::updateSignalChain (this);
        }

    }
}
