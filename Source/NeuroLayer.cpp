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
    closeTask();
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
    int dev_index = 0;

    for (const auto& col : cfg.neuroLayerSystem.columns)
    {
        
        const String moduleName = std::get<0>(col); // PXI module name
        juce::StringArray analogLines;

        analogLines =  std::get<1>(col);

        maxColumnsPerStation = std::max(maxColumnsPerStation, int(analogLines.size()));

        auto* aiDevice = new InputAIChannel (moduleName, analogLines, dev_index);
        aiDevice->configure();
        AIdevices.add(aiDevice);
        numProbeColumn += int (analogLines.size());

        dev_index+=1;
    }

     // --- Compute Sample Rate ---
    // Take the minimum sample rate among columns if columns have different numbers of lines
    sampleRate = 500000.0 / maxColumnsPerStation;

    for (const auto& dev : AIdevices)
    {
        dev->setSampleRate (sampleRate);
    }

    dev_index = 0;

    // --- Setup DI Devices (rows) ---
    for (const auto& row : cfg.neuroLayerSystem.rows)
    {
        const String moduleName = std::get<0>(row);
        juce::String portName =  std::get<1>(row);
        auto* diDevice = new InputDIChannel(moduleName, portName, dev_index, cfg.neuroLayerSystem.numRows);
        diDevice->configure();
        diDevice->setSampleRate (sampleRate);
        DIdevices.add(diDevice);
        dev_index+=1;
        numProbeRow += cfg.neuroLayerSystem.numRows;
    }

    // --- Setup Event Devices ---
    for (const auto& evt : cfg.eventInputs)
    {
        const String moduleName = evt.name;
        const String digitalLine = evt.digital_line;
        auto* evDevice = new EventDIChannel (moduleName, digitalLine, evt.oe_event_label);
        evDevice->configure();
        evDevice->setSampleRate (sampleRate);
        eventDevices.add(evDevice);
    }

    // --- Setup Start Device ---
    startDevice = new StartChannel(cfg.startEventOutput.name,
                                   cfg.startEventOutput.digital_line, 
                                   cfg.startEventOutput.start_time, cfg.startEventOutput.nbr_pulse, cfg.startEventOutput.pulse_duration);
    
    startDevice->configure();
    startDevice->setSampleRate (sampleRate);


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
    voltageRanges = { 0.1, 0.2, 0.5, 1, 2, 5, 10 };
    
}

void NeuroProcessor::run()
{
    /**************************************/
    /********CONFIG ANALOG CHANNELS********/
    /**************************************/
    /* Create an analog input task */
    try
    {
        for (int dev_i = 0; dev_i < AIdevices.size(); dev_i++)
        {
            AIdevices[dev_i]->setup (voltageRangeIndex);
        }
        char trig_clock_fs[256] = { "\0" };
        char trig_clock_2fs[256] = { "\0" };

        // Master: internal clock
        AIdevices[0]->getClock (trig_clock_fs, trig_clock_2fs, getNsample() * CHANNEL_BUFFER_SIZE * 10);

        // Slaves: use master’s clock
        for (int dev_i = 1; dev_i < AIdevices.size(); dev_i++)
        {
            AIdevices[dev_i]->setClock (trig_clock_fs, getNsample() * CHANNEL_BUFFER_SIZE * 10);
        }

        /************************************/
        /********CONFIG DIGITAL LINES********/
        /************************************/

        for (int dev_i = 0; dev_i < DIdevices.size(); dev_i++)
        {
            DIdevices[dev_i]->setup (trig_clock_2fs, CHANNEL_BUFFER_SIZE * getNsample(), DIdevices.size());
        }

        for (int dev_i = 0; dev_i < eventDevices.size(); dev_i++)
        {
            eventDevices[dev_i]->setup (trig_clock_fs, getNsample() * CHANNEL_BUFFER_SIZE * 10);
        }
        startDevice->setup (trig_clock_fs);
    }
    catch (const std::exception& e)
    {
        closeTask();
        LOGD ("Failed to setup the device: ");
        LOGD (e.what());
        return;
    }

    try
    {
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

    }
    catch (const std::exception& e)
    {
        LOGD ("Failed to start the device: ");
        LOGD (e.what());
        closeTask();
        return;
    }

    double ts;

    ai_timestamp = 0;

    aiBuffer->clear();

    LOGD ("Start acquisition");

    int numDevices = AIdevices.size();
    int nbr_channel = getCellNumber();
    
    try
    {
        while (! threadShouldExit())
        {

            HeapBlock<float> output;
            output.allocate (nbr_channel, true);
            std::vector<std::vector<NIDAQ::float64>> dev_ai_data (numDevices);
            std::vector<std::vector<NIDAQ::uInt32>> dev_di_event (eventDevices.size());
            
             for (size_t i = 0; i < numDevices; ++i)
            {
                AIdevices[i]->acquire (&dev_ai_data[i], CHANNEL_BUFFER_SIZE * getNsample());
            }

             for (size_t i = 0; i < eventDevices.size(); ++i)
            {
                eventDevices[i]->acquire (&dev_di_event[i], getNsample() * CHANNEL_BUFFER_SIZE);
            }
             for (int nsample = 0; nsample < getNsample(); ++nsample)
            {
                int writeIdx = 0;
                for (int station = 0; station < numDevices; ++station)
                {
                    for (int analogch = 0; analogch < AIdevices[station]->analogLines_.size(); ++analogch)
                    {
                        for (int ch = 0; ch < getRowNumber(); ch++)
                            output[writeIdx++] = dev_ai_data[station][ch + analogch * getRowNumber() * getNsample() + nsample * getRowNumber()]; // step per sample
                    }
                }
                juce::uint64 eventCode = 0;

                for (size_t i = 0; i < eventDevices.size(); ++i)
                {
                    bool isActive = std::accumulate (
                                        dev_di_event[i].begin() + nsample * CHANNEL_BUFFER_SIZE,
                                        dev_di_event[i].begin() + (nsample + 1) * CHANNEL_BUFFER_SIZE,
                                        0.0)
                                    > 0;

                    if (isActive)
                    {
                        if (eventDevices[i]->event_label_ < 64)
                            eventCode |= (juce::uint64 (1) << eventDevices[i]->event_label_); // cast avant le shift
                        else
                            LOGD ("Warning: cannot set event " + std::to_string(i) + " (exceeds the 64 possible events)");
                    }
                }

                ai_timestamp++;
                aiBuffer->addToBuffer (output, &ai_timestamp, &ts, &eventCode, 1);
            }
        }
    }
        catch (const std::exception& e)
    {
        LOGD ("Error during acquisition: ");
        LOGD (e.what());
        
    }
    // fflush(stdout);
    closeTask();



    return;
}