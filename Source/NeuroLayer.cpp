/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2024 Open Ephys

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
#include <algorithm>
#include <chrono>
#include <math.h>

NeuroProcessor::NeuroProcessor(NeuroConfig& cfg)
    : Thread("HaeslerProbe")
{
    // Clear existing devices
    AIdevices.clear();
    DIdevices.clear();
    eventDevices.clear();
    startDevice = nullptr;
    numProbeColumn = 0;
    numProbeRow = 0;

    // --- Setup AI Devices (columns) ---
    // Each column corresponds to one PXI module with all its lines
    int maxColumnsPerStation = 0;

    for (const auto& col : cfg.neuroLayerSystem.columns)
    {
        const String moduleName = std::get<0>(col); // PXI module name
        LOGD("want to add new AI device " + moduleName)
        juce::StringArray analogLines;

        analogLines =  std::get<1>(col);
        maxColumnsPerStation = std::max(maxColumnsPerStation, int(analogLines.size()));

        auto* aiDevice = new InputAIChannel (moduleName, analogLines);
        aiDevice->configure();
        AIdevices.add(aiDevice);
        numProbeColumn += int (analogLines.size());
    }

    

    // --- Setup DI Devices (rows) ---
    for (const auto& row : cfg.neuroLayerSystem.rows)
    {
        const String moduleName = std::get<0>(row);
        juce::StringArray portName =  std::get<1>(row);
        auto* diDevice = new InputDIChannel(moduleName, portName);
        diDevice->configure();
        DIdevices.add(diDevice);
        numProbeRow += 8;
    }

    // --- Setup Event Devices ---
    for (const auto& evt : cfg.eventInputs)
    {
        const String moduleName = evt.name;
        const String digitalLine = evt.digital_line;
        auto* evDevice = new EventDIChannel(moduleName, digitalLine);
        evDevice->configure();
        eventDevices.add(evDevice);
    }

    // --- Setup Start Device ---
    startDevice = new StartChannel(cfg.startEventOutput.name,
                                    cfg.startEventOutput.digital_line);
    
    startDevice->configure();

    // --- Compute Sample Rate ---
    // Take the minimum sample rate among columns if columns have different numbers of lines

    sampleRate = 500000 / maxColumnsPerStation;

    // --- Default Voltage Range ---
    if (!AIdevices.isEmpty())
        voltageRangeIndex = AIdevices[0]->voltageRanges.size() - 1;
    else
        voltageRangeIndex = 0;
}

void Channel::configure()
{
    /* Get category type */
    String deviceName = name_;
    LOGD("Device :" + deviceName)
    NIDAQ::int32 Category;
    NIDAQ::DAQmxGetDevProductCategory (STR2CHR (deviceName), &Category);
    LOGD ("Product Category: ", Category);

    int digitalReadSize = 32;

    NIDAQ::uInt32 productNum;
    NIDAQ::DAQmxGetDevProductNum (STR2CHR (deviceName), &productNum);
    LOGD ("Product Num: ", productNum);

    NIDAQ::uInt32 serialNum;
    NIDAQ::DAQmxGetDevSerialNum (STR2CHR (deviceName), &serialNum);
    LOGD ("Serial Num: ", serialNum);

    /* Get simultaneous sampling supported */
    NIDAQ::bool32 supported = false;
    NIDAQ::DAQmxGetDevAISimultaneousSamplingSupported (STR2CHR (deviceName), &supported);
    bool simAISamplingSupported = supported;
    LOGD ("Simultaneous sampling supported: ", supported ? "YES" : "NO");

    /* Get device sample rates */
    NIDAQ::float64 smin;
    NIDAQ::DAQmxGetDevAIMinRate (STR2CHR (deviceName), &smin);
    LOGD ("Min sample rate: ", smin);

    NIDAQ::float64 smaxs;
    NIDAQ::DAQmxGetDevAIMaxSingleChanRate (STR2CHR (deviceName), &smaxs);
    LOGD ("Max single channel sample rate: ", smaxs);

    NIDAQ::float64 smaxm;
    NIDAQ::DAQmxGetDevAIMaxMultiChanRate (STR2CHR (deviceName), &smaxm);
    LOGD ("Max multi channel sample rate: ", smaxm);

    NIDAQ::float64 data[512];
    NIDAQ::DAQmxGetDevAIVoltageRngs (STR2CHR (deviceName), &data[0], sizeof (data));

    // Get available voltage ranges
    voltageRanges.clear();
    LOGD ("Detected voltage ranges: \n");
    for (int i = 0; i < 512; i += 2)
    {
        NIDAQ::float64 vmin = data[i];
        NIDAQ::float64 vmax = data[i + 1];

        if (vmin == vmax || abs (vmin) < 1e-10 || vmax < 1e-2)
            break;

        voltageRanges.add (vmax);
    }

    /*char ai_channel_data[2048];
    NIDAQ::DAQmxGetDevAIPhysicalChans (STR2CHR (name_), &ai_channel_data[0], sizeof (ai_channel_data));

    StringArray channel_list;
    channel_list.addTokens (&ai_channel_data[0], ", ", "\"");

    LOGD ("Detected ", channel_list.size(), " analog input channels");

    for (int i = 0; i < channel_list.size(); i++)
    {
        if (channel_list[i].length() > 0)
        {
            String name = channel_list[i].toRawUTF8();
            LOGD ("Adding analog input channel: ", name);
        }
    }

    // Get Digital Input Channels

    char di_channel_data[2048];
    NIDAQ::DAQmxGetDevDILines (STR2CHR (deviceName), &di_channel_data[0], sizeof (di_channel_data)); // gets ports on line
    LOGD ("Found digital inputs: ");

    channel_list.clear();
    channel_list.addTokens (&di_channel_data[0], ", ", "\"");

    for (int i = 0; i < channel_list.size(); i++)
    {
        StringArray channel_type;
        channel_type.addTokens (channel_list[i], "/", "\"");
        if (channel_list[i].length() > 0)
        {
            String fullName = channel_list[i].toRawUTF8();

            String lineName = fullName.fromFirstOccurrenceOf ("/", false, false);
            String portName = fullName.upToLastOccurrenceOf ("/", false, false);
        }
    }*/
}

void NeuroProcessor::run()
{
    /**************************************/
    /********CONFIG ANALOG CHANNELS********/
    /**************************************/
    /* Create an analog input task */

    for (int dev_i = 0; dev_i < AIdevices.size(); dev_i++)
    {
        AIdevices[dev_i]->setup(voltageRangeIndex);
    }

    // Master: internal clock
    auto trigName = AIdevices[0]->getClock ("PXI_Trig0");

    // Slaves: use master’s clock
    for (int dev_i = 0; dev_i < AIdevices.size(); dev_i++)
    {
        AIdevices[dev_i]->setClock (trigName, getNsample() * CHANNEL_BUFFER_SIZE * 10);
    }

    /************************************/
    /********CONFIG DIGITAL LINES********/
    /************************************/
    for (int dev_i = 0; dev_i < DIdevices.size(); dev_i++)
    {
        DIdevices[dev_i]->setup (trigName, CHANNEL_BUFFER_SIZE * getNsample());
    }

    for (int dev_i = 0; dev_i < eventDevices.size(); dev_i++)
    {
        eventDevices[dev_i]->setup (trigName, getNsample() * CHANNEL_BUFFER_SIZE * 10);
    }

    startDevice->setup (trigName, 0.1);

    // This order is necessary to get the timing right
    for (auto& device : AIdevices)
    {
        device->control();
    }

    for (auto& device : DIdevices)
    {
        device->control();
    }

    for (auto& device : eventDevices)
    {
        device->control();
    }

    startDevice->control();

    for (auto& device : DIdevices)
    {
        device->start();
    }

    for (auto& device : eventDevices)
    {
        device->start();
    }

    startDevice->start();

    for (int i = 1; i < AIdevices.size(); i++)
    {
        AIdevices[i]->start();
    }

    AIdevices[0]->start();

    double ts;

    ai_timestamp = 0;

    aiBuffer->clear();

    LOGD ("Start acquisition");

    int numDevices = AIdevices.size();
    int nbr_channel = numProbeColumn * numDevices * numProbeRow;
    juce::uint64 eventCode = 0;
    int buffer_size = numProbeColumn * CHANNEL_BUFFER_SIZE * getNsample();

    while (! threadShouldExit())
    {
        HeapBlock<float> output;
        output.allocate (nbr_channel, true);
        std::vector<std::vector<NIDAQ::float64>> dev_ai_data (AIdevices.size());
        std::vector<std::vector<NIDAQ::uInt32>> dev_di_event (eventDevices.size());

        for (size_t i = 0; i < numDevices; ++i)
        {
            AIdevices[i]->acquire (dev_ai_data[i], buffer_size);
        }

        for (size_t i = 0; i < eventDevices.size(); ++i)
        {
            eventDevices[i]->acquire (dev_di_event[i], getNsample() * CHANNEL_BUFFER_SIZE);
        }

        for (int nsample = 0; nsample < getNsample(); ++nsample)
        {
            int writeIdx = 0;
            for (int station = 0; station < numDevices; ++station)
            {
                for (int analogch = 0; analogch < (numProbeColumn); ++analogch)
                {
                    for (int ch = 0; ch < numProbeRow; ch++)
                        output[writeIdx++] = dev_ai_data[station][ch + analogch * numProbeRow * getNsample() + nsample * numProbeRow]; // step per sample
                }
            }

            for (size_t i = 0; i < eventDevices.size(); ++i)
            {
                bool isActive = std::accumulate (
                                    dev_di_event[i].begin() + nsample * CHANNEL_BUFFER_SIZE,
                                    dev_di_event[i].begin() + (nsample + 1) * CHANNEL_BUFFER_SIZE,
                                    0.0)
                                > 0;

                if (isActive)
                {
                    eventCode |= (1 << i); // set the i-th bit
                }
            }

            ai_timestamp++;
            aiBuffer->addToBuffer (output, &ai_timestamp, &ts, &eventCode, 1);
        }
    }
    // fflush(stdout);

    /*********************************************/
    // DAQmx Stop Code
    /*********************************************/
    for (int dev_i = 0; dev_i < AIdevices.size(); dev_i++)
    {
        AIdevices[dev_i]->stop();
    }

    for (int dev_i = 0; dev_i < DIdevices.size(); dev_i++)
    {
        DIdevices[dev_i]->stop();
    }

    for (int dev_i = 0; dev_i < eventDevices.size(); dev_i++)
    {
        eventDevices[dev_i]->stop();
    }

    startDevice->stop();

    return;
}