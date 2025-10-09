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

#ifndef NEUROLAYEREDITOR_H_DEFINED
#define NEUROLAYEREDITOR_H_DEFINED

#include <EditorHeaders.h>

#include "NeuroLayerThread.h"

class NeuroLayerEditor : public GenericEditor, 
                         public ComboBox::Listener, 
                         public Button::Listener
{
public:
    /** The class constructor, used to initialize any members. */
    NeuroLayerEditor (GenericProcessor* parentNode, NeuroLayerThread* thread);

    /** The class destructor, used to deallocate memory */
    ~NeuroLayerEditor() {}

    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatChanged) override;

    void saveCustomParametersToXml(XmlElement *xml) override;
    void loadCustomParametersFromXml(XmlElement *xml) override;

private:
    NeuroLayerThread* thread = nullptr;

    ScopedPointer<ComboBox> voltageRangeSelector;
    ScopedPointer<juce::Label> voltageLabel;
    ScopedPointer<juce::TextButton> configFileButton;
    ScopedPointer <juce::Label> configFileLabel;

    juce::File configFile;

    void setupUI();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuroLayerEditor);
};

#endif
