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

#include "NeuroConfig.h"
#include "NeuroLayerEditor.h"

struct PluginSettingsObject
{
    // Store settings for the plugin here
};

DataThread* NeuroLayerThread::createDataThread (SourceNode* sn)
{
    return new NeuroLayerThread (sn);
}

NeuroLayerThread::NeuroLayerThread (SourceNode* sn) : DataThread (sn)
{
}

NeuroLayerThread::~NeuroLayerThread()
{
}

bool NeuroLayerThread::foundInputSource()
{
    return true;
}

void NeuroLayerThread::updateSettings (OwnedArray<ContinuousChannel>* continuousChannels,
                                 OwnedArray<EventChannel>* eventChannels,
                                 OwnedArray<SpikeChannel>* spikeChannels,
                                 OwnedArray<DataStream>* dataStreams,
                                 OwnedArray<DeviceInfo>* devices,
                                 OwnedArray<ConfigurationObject>* configurationObjects)
{
    LOGD("Reload setting");
    sourceStreams.clear();
    if (!processor) // initialize data streams
    {
        
        DataStream::Settings settings {
            "PXI",
            "Analog input channels from a NIDAQ device",
            "identifier",
            500000

        };

        sourceStreams.add (new DataStream (settings));
    }
    else
    {
        DataStream::Settings settings {
            "PXI",
            "Analog input channels from a NIDAQ device",
            "identifier",
            processor->getSampleRate() / processor->getRowNumber()

        };

        sourceStreams.add (new DataStream (settings));
    }

    dataStreams->clear();
    eventChannels->clear();
    continuousChannels->clear();
    spikeChannels->clear();
    devices->clear();
    configurationObjects->clear();

     DataStream* currentStream = sourceStreams[0];

        currentStream->setName ("NeuroLayer");

        currentStream->clearChannels();

        if (processor)
        {
            for (int ch = 0; ch < processor->getCellNumber(); ch++)
            {
                float bitVolts = processor->getVoltageRange() / float (0x7fff);

                ContinuousChannel::Settings settings {
                    ContinuousChannel::Type::ADC,
                    "C" + String (ch % processor->getColumnNumber()) + ",R" + String (ch / processor->getRowNumber()),
                    "Electrode",
                    "identifier",
                    bitVolts,
                    currentStream
                };

                continuousChannels->add (new ContinuousChannel (settings));
            }
        }

        EventChannel::Settings settings {
            EventChannel::Type::TTL,
            "Synchro ttl",
            "Stimuli synchronization ttl",
            "identifier",
            currentStream,
            64
        };

        eventChannels->add (new EventChannel (settings));
        dataStreams->add (new DataStream (*currentStream)); // copy existing stream
    
}

bool NeuroLayerThread::startAcquisition()
{
    if (! processor)
        return false;

    processor->startThread();
    return true;
}

bool NeuroLayerThread::updateBuffer()
{


    return true;
}

bool NeuroLayerThread::stopAcquisition()
{
    if (! processor)
        return false;

    if (processor->isThreadRunning())
    {
        processor->signalThreadShouldExit();
    }
    return true;
}

void NeuroLayerThread::resizeBuffers()
{
}

std::unique_ptr<GenericEditor> NeuroLayerThread::createEditor (SourceNode* sn)
{
    std::unique_ptr<NeuroLayerEditor> editor = std::make_unique<NeuroLayerEditor> (sn, this);

    return editor;
}

void NeuroLayerThread::handleBroadcastMessage (const String& msg, const int64 messageTimestmpMilliseconds)
{
}

String NeuroLayerThread::handleConfigMessage (const String& msg)
{
    return "";
}

void NeuroLayerThread::registerParameters()
{
}

void NeuroLayerThread::parameterValueChanged (Parameter* parameter)
{
}

void NeuroLayerThread::setVoltageRange (int id)
{
    if (! processor)
        return; // nothing to configure yet

    processor->setVoltageRange (id);
    sourceStreams.clear();
}



Array<float> NeuroLayerThread::getVoltageRange()
{
    if (! processor)
        return Array<float>();

    return processor->getAllVoltageRange();

}

void NeuroLayerThread::setConfigFile (File config)
{
    LOGD ("Config file updated: " + config.getFullPathName());
    
    parseNeuroConfig (neuroConfig, config);
    reloadConfig();

}

void NeuroLayerThread::reloadConfig()
{
    processor = std::make_unique<NeuroProcessor> (neuroConfig);
    sourceBuffers.clear();
    sourceBuffers.add (new DataBuffer (processor->getCellNumber(), 10000));
    LOGD(processor->getCellNumber())
    processor->aiBuffer = sourceBuffers.getLast();
    sourceStreams.clear();
}