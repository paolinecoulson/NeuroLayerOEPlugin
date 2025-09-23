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
#ifndef __NIDAQCOMPONENTS_H__
#define __NIDAQCOMPONENTS_H__

#include <DataThreadHeaders.h>
#include <stdexcept>
#include <string>
#include <vector>
#include "nidaq-api/NIDAQmx.h"
#include "NeuroConfig.h"

#define ERR_BUFF_SIZE 2048
#define STR2CHR(jString) ((jString).toUTF8())
#define CHANNEL_BUFFER_SIZE 32

// RAII wrapper for DAQmx calls
inline void DAQmxCheck(int32 error)
{
    if (DAQmxFailed(error))
    {
        char errBuff[ERR_BUFF_SIZE] = {0};
        NIDAQ::DAQmxGetExtendedErrorInfo(errBuff, ERR_BUFF_SIZE);
        throw std::runtime_error(std::string("DAQmx Error: ") + errBuff);
    }
}

inline static int32 GetTerminalNameWithDevPrefix (NIDAQ::TaskHandle taskHandle, const char terminalName[], char triggerName[])
{
    NIDAQ::int32 error = 0;
    char device[256];
    NIDAQ::int32 productCategory;
    NIDAQ::uInt32 numDevices, i = 1;

    DAQmxCheck (NIDAQ::DAQmxGetTaskNumDevices (taskHandle, &numDevices));
    while (i <= numDevices)
    {
        DAQmxCheck (NIDAQ::DAQmxGetNthTaskDevice (taskHandle, i++, device, 256));
        DAQmxCheck (NIDAQ::DAQmxGetDevProductCategory (device, &productCategory));
        if (productCategory != DAQmx_Val_CSeriesModule && productCategory != DAQmx_Val_SCXIModule)
        {
            *triggerName++ = '/';
            strcat (strcat (strcpy (triggerName, device), "/"), terminalName);
            break;
        }
    }
    return error;

}


/* ================================================================
   Base Channel class
   ================================================================ */
class Channel
{
public:
    Channel(String name, int dev_index) : name_(name), dev_index_(dev_index) {}
    virtual ~Channel() { stop(); }

    // Disable copy
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    String getName() const { return name_; }

    void configure();

    int getSampleRate() const { return sampleRate_; }
    void setSampleRate(int sampleRate) { sampleRate_ = sampleRate; }


    virtual void start()
    {
        if (taskHandle_ != 0)
            DAQmxCheck(NIDAQ::DAQmxStartTask(taskHandle_));
    }

    virtual void control()
    {
        if (taskHandle_ != 0)
            DAQmxCheck(NIDAQ::DAQmxTaskControl(taskHandle_, DAQmx_Val_Task_Commit));
    }

    virtual void stop()
    {
        if (taskHandle_ != 0)
        {
            NIDAQ::DAQmxStopTask(taskHandle_);
            NIDAQ::DAQmxClearTask(taskHandle_);
            taskHandle_ = 0;
        }
    }

    Array<float> voltageRanges;

protected:
    String name_;
    int sampleRate_{0};
    NIDAQ::TaskHandle taskHandle_{0};
    int dev_index_ = 0;

};

/* ================================================================
   Input Analog Channel
   ================================================================ */
class InputAIChannel : public Channel
{
public:
    InputAIChannel(String name, juce::StringArray analogLines, int dev_index)
        : Channel(name, dev_index), analogLines_(analogLines) {}

    void setup(int voltageRangeIndex)
    {
        DAQmxCheck(NIDAQ::DAQmxCreateTask(STR2CHR("AITask_" + name_), &taskHandle_));

        for (const auto& analogLine : analogLines_)
        {

            DAQmxCheck(NIDAQ::DAQmxCreateAIVoltageChan(
                taskHandle_,
                STR2CHR (name_ + "/"+ analogLine),
                "",
                DAQmx_Val_Diff,
                -voltageRanges[voltageRangeIndex],
                voltageRanges[voltageRangeIndex],
                DAQmx_Val_Volts,
                nullptr));
        }

    }
    std::string getClock (const char* trigger, int bufferSize) const
    {
        char trigName[256] = { '\0' };

        GetTerminalNameWithDevPrefix(taskHandle_, trigger, trigName);
        NIDAQ::DAQmxCfgSampClkTiming (taskHandle_,
                                      "",
                                      getSampleRate(),
                                      DAQmx_Val_Rising,
                                      DAQmx_Val_ContSamps,
                                      bufferSize);

        // Export master�s sample clock
        NIDAQ::DAQmxExportSignal (taskHandle_, DAQmx_Val_SampleClock, trigName);
        return std::string(trigName);
    }

    void setClock(const std::string& trigName, int bufferSize) {

        NIDAQ::DAQmxCfgSampClkTiming(
            taskHandle_,
            trigName.c_str(),
            getSampleRate(),
            DAQmx_Val_Rising,
            DAQmx_Val_ContSamps,
            bufferSize
        );
    }

    void acquire(std::vector<NIDAQ::float64>* ai_data, int buffer_size)
    {
        ai_data->resize (analogLines_.size() *buffer_size);

        NIDAQ::DAQmxReadAnalogF64 (
            taskHandle_,
            buffer_size,
            timeout_,
            DAQmx_Val_GroupByChannel,
            ai_data->data(),
            analogLines_.size()*buffer_size,
            nullptr,
            nullptr);
    }
    juce::StringArray analogLines_;

private:
    NIDAQ::float64 timeout_ = 5.0;
};

/* ================================================================
   Input Digital Channel
   ================================================================ */
class InputDIChannel : public Channel
{
public:
    InputDIChannel(String name, juce::String digitalPort, int dev_index, int numLines)
        : Channel (name, dev_index),
          digitalPort_ (digitalPort), numLines_(numLines) {}

    void setup(const std::string& trigName, int buffer, int numStation)
    {
            const int pulseLengthInSamples = 1;
            const int samplesPerStation = numLines_ * pulseLengthInSamples;

            DAQmxCheck (NIDAQ::DAQmxCreateTask (STR2CHR("DITask_" + name_), &taskHandle_));
            DAQmxCheck (NIDAQ::DAQmxCreateDOChan (taskHandle_,
                                                 STR2CHR(name_+ "/port0"),
                                                 "",
                                                 DAQmx_Val_ChanForAllLines));

            // DO uses counter clock directly at 125 kHz
            DAQmxCheck (NIDAQ::DAQmxCfgSampClkTiming (taskHandle_,
                                                     trigName.c_str(),
                                                     getSampleRate(),
                                                     DAQmx_Val_Rising,
                                                     DAQmx_Val_ContSamps,
                                                     buffer));

            DAQmxCheck (NIDAQ::DAQmxSetWriteRegenMode (taskHandle_, DAQmx_Val_AllowRegen));

            std::vector<NIDAQ::uInt32> waveform (samplesPerStation*numStation, 0);
            int startSample = dev_index_ * samplesPerStation;

            NIDAQ::uInt32 bitMask = 0;

            for (int line_i = 0; line_i < numLines_; line_i++)
            {
            
                int sampleOffset = startSample + line_i * pulseLengthInSamples;
                bitMask = static_cast<NIDAQ::uInt32>(1 << line_i);
                waveform[sampleOffset] = bitMask;
            }

            NIDAQ::int32 samplesWritten_dig = 0;
            DAQmxCheck (NIDAQ::DAQmxWriteDigitalU32 (
                taskHandle_,
                samplesPerStation * numStation,
                0,
                timeout_,
                DAQmx_Val_GroupByChannel,
                waveform.data(),
                NULL,
                NULL));

    }
    int numLines_ = 0;

private:
    String digitalPort_;
    NIDAQ::float64 timeout_ = 10.0;

};

/* ================================================================
   Event Channel
   ================================================================ */
class EventDIChannel : public Channel
{
public:
    EventDIChannel(String name, String digitalLine)
        : Channel(name, 0), digitalLine_(digitalLine) {}

    void setup(const std::string& trigName, int buffer)
    {
        DAQmxCheck(NIDAQ::DAQmxCreateTask("Event_DI_Task", &taskHandle_));

        DAQmxCheck(NIDAQ::DAQmxCreateDIChan(taskHandle_,
                                              STR2CHR (name_ + "/"+ digitalLine_),
                                            "",
                                            DAQmx_Val_ChanForAllLines));

        DAQmxCheck(NIDAQ::DAQmxCfgSampClkTiming(taskHandle_,
                                                trigName.c_str(),
                                                getSampleRate(),
                                                DAQmx_Val_Rising,
                                                DAQmx_Val_ContSamps,
                                                buffer));
    }

    void acquire(std::vector<NIDAQ::uInt32>* di_data, int buffer_size)
    {
        di_data->resize(buffer_size);

        DAQmxCheck(NIDAQ::DAQmxReadDigitalU32(
            taskHandle_,
            buffer_size,
            timeout_,
            DAQmx_Val_GroupByScanNumber,
            di_data->data(),
            buffer_size,
            nullptr,
            nullptr));
    }

private:
    String digitalLine_;
    NIDAQ::float64 timeout_ = 10.0;
};

/* ================================================================
   Start Channel
   ================================================================ */
class StartChannel : public Channel
{
public:
    StartChannel(String name, String digitalLine)
        : Channel(name, 0), digitalLine_(digitalLine) {}

    void setup(const std::string& trigName, float pulse_length)
    {
        NIDAQ::float64 timeout = 5.0;
        pulse_length = pulse_length * getSampleRate();
        LOGD (pulse_length);
        LOGD (getSampleRate());

        std::vector<NIDAQ::uInt32> waveform_start(pulse_length * 5, 0);

        DAQmxCheck(NIDAQ::DAQmxCreateTask("StartPulseTask", &taskHandle_));
        DAQmxCheck(NIDAQ::DAQmxCreateDOChan(taskHandle_,
                                            STR2CHR (name_ + "/"+ digitalLine_),
                                            "",
                                            DAQmx_Val_ChanPerLine));

        DAQmxCheck(NIDAQ::DAQmxCfgSampClkTiming(taskHandle_,
                                                trigName.c_str(),
                                                getSampleRate(),
                                                DAQmx_Val_Rising,
                                                DAQmx_Val_FiniteSamps,
                                                waveform_start.size()));

        DAQmxCheck(NIDAQ::DAQmxSetBufOutputBufSize(taskHandle_, waveform_start.size()));

        NIDAQ::uInt32 bitMask = static_cast<NIDAQ::uInt32>(1 << 8);

        std::fill(waveform_start.begin() + pulse_length,
                  waveform_start.begin() + 2 * pulse_length,
                  bitMask);

        std::fill(waveform_start.begin() + pulse_length * 3,
                  waveform_start.begin() + 4 * pulse_length,
                  bitMask);

        int chunkSize = 62500;
        for (int i = 0; i < waveform_start.size(); i += chunkSize)
        {
            int currentChunkSize = std::min(chunkSize, static_cast<int>(waveform_start.size() - i));
            DAQmxCheck(NIDAQ::DAQmxWriteDigitalU32(
                taskHandle_,
                currentChunkSize,
                0,
                timeout,
                DAQmx_Val_GroupByChannel,
                waveform_start.data() + i,
                nullptr,
                nullptr));
        }
    }

private:
    String digitalLine_;
};



class NeuroProcessor : public Thread
{
public:
    NeuroProcessor(NeuroConfig& cfg);
    ~NeuroProcessor() {};

    /* Pointer to the active device */
    Array<InputAIChannel*> AIdevices;
    Array<InputDIChannel*> DIdevices;
    Array<EventDIChannel*> eventDevices;
    StartChannel* startDevice = 0;

    /* Analog configuration */
    NIDAQ::float64 getSampleRate() { return sampleRate; };

    float getVoltageRange() { return AIdevices[0]->voltageRanges[voltageRangeIndex]; };
    Array<float> getAllVoltageRange() { return AIdevices[0]->voltageRanges; };
    void setVoltageRange (int index) { voltageRangeIndex = index; };

    int getNsample() { return 3200; };
    int getRowNumber() { return numProbeRow; };
    int getColumnNumber() { return numProbeColumn; };
    int getCellNumber() { return numProbeRow * numProbeColumn; };

    void run();

    NIDAQ::float64 sampleRate;
    DataBuffer* aiBuffer = nullptr;

private:
    HeapBlock<NIDAQ::uInt32> eventCodes;
    int voltageRangeIndex { 0 };
    int64 ai_timestamp = 0;
    uint64 eventCode =0;


    int numProbeColumn = 0; 
    int numProbeRow =  0;
};

#endif // __NIDAQCOMPONENTS_H__