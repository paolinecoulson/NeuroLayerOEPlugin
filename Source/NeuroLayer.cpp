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

#include "NeuroLayer.h"

#include "NeuroLayerEditor.h"

struct PluginSettingsObject
{
    // Store settings for the plugin here
};

NeuroLayer::NeuroLayer (SourceNode* sn) : DataThread (sn)
{
}

NeuroLayer::~NeuroLayer()
{
}

bool NeuroLayer::foundInputSource()
{
    return true;
}

void NeuroLayer::updateSettings (OwnedArray<ContinuousChannel>* continuousChannels,
                                       OwnedArray<EventChannel>* eventChannels,
                                       OwnedArray<SpikeChannel>* spikeChannels,
                                       OwnedArray<DataStream>* sourceStreams,
                                       OwnedArray<DeviceInfo>* devices,
                                       OwnedArray<ConfigurationObject>* configurationObjects)
{
}

bool NeuroLayer::startAcquisition()
{
    return true;
}

bool NeuroLayer::updateBuffer()
{
    return true;
}

bool NeuroLayer::stopAcquisition()
{
    return true;
}

void NeuroLayer::resizeBuffers()
{
}

std::unique_ptr<GenericEditor> NeuroLayer::createEditor (SourceNode* sn)
{
    std::unique_ptr<NeuroLayerEditor> editor = std::make_unique<NeuroLayerEditor> (sn, this);

    return editor;
}

void NeuroLayer::handleBroadcastMessage (const String& msg, const int64 messageTimestmpMilliseconds)
{
}

String NeuroLayer::handleConfigMessage (const String& msg)
{
    return "";
}

void NeuroLayer::registerParameters()
{
    // Register parameters for the plugin here (e.g. addParameter())
}

void NeuroLayer::parameterValueChanged (Parameter* parameter)
{
    // Handle parameter value changes here (e.g. update settings)
}