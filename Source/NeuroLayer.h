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
#include "NeuroConfig.h"
#include "nidaq-api/NIDAQmx.h"

#define ERR_BUFF_SIZE 2048
#define STR2CHR(jString) ((jString).toUTF8())
#define CHANNEL_BUFFER_SIZE 32

// RAII wrapper for DAQmx calls
inline void DAQmxCheck (int32 error)
{
    if (DAQmxFailed (error))
    {
        char errBuff[ERR_BUFF_SIZE] = { 0 };
        NIDAQ::DAQmxGetExtendedErrorInfo (errBuff, ERR_BUFF_SIZE);
        throw std::runtime_error (std::string ("DAQmx Error: ") + errBuff);
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
    Channel (String name, int dev_index) : name_ (name), dev_index_ (dev_index) {}
    virtual ~Channel() { stop(); }

    // Disable copy
    Channel (const Channel&) = delete;
    Channel& operator= (const Channel&) = delete;

    String getName() const { return name_; }

    void configure();

    int getSampleRate() const { return sampleRate_; }
    void setSampleRate (int sampleRate) { sampleRate_ = sampleRate; }

    virtual void start()
    {

        if (counterTask != 0)
            DAQmxCheck (NIDAQ::DAQmxStartTask (counterTask));


        if (taskHandle_ != 0)
            DAQmxCheck (NIDAQ::DAQmxStartTask (taskHandle_));



    }

    virtual void control()
    {
        if (taskHandle_ != 0)
            DAQmxCheck (NIDAQ::DAQmxTaskControl (taskHandle_, DAQmx_Val_Task_Commit));
        if (counterTask != 0)
            DAQmxCheck (NIDAQ::DAQmxTaskControl (counterTask, DAQmx_Val_Task_Commit));
    }

    virtual void stop()
    {
        if (taskHandle_)
        {
            LOGD("Clear task handle ", name_);

            NIDAQ::DAQmxStopTask (taskHandle_);
            NIDAQ::DAQmxClearTask (taskHandle_);
            taskHandle_ = 0;
        }

        if (counterTask)
        {
             LOGD("Clear counter handle ", name_);
            NIDAQ::DAQmxStopTask (counterTask);
            NIDAQ::DAQmxClearTask (counterTask);
            counterTask = 0;
        }

    }

    Array<float> voltageRanges;

protected:
    String name_;
    int sampleRate_ { 0 };
    NIDAQ::TaskHandle taskHandle_ { 0 };
    NIDAQ::TaskHandle counterTask { 0 };
    int dev_index_ = 0;
};

/* ================================================================
   Input Analog Channel
   ================================================================ */
class InputAIChannel : public Channel
{
public:
    InputAIChannel (String name, juce::StringArray analogLines, int dev_index)
        : Channel (name, dev_index), analogLines_ (analogLines) {}

    void setup (int voltageRangeIndex)
    {
        DAQmxCheck (NIDAQ::DAQmxCreateTask (STR2CHR ("AITask_" + name_), &taskHandle_));

        for (const auto& analogLine : analogLines_)
        {
            DAQmxCheck (NIDAQ::DAQmxCreateAIVoltageChan (
                taskHandle_,
                STR2CHR (name_ + "/" + analogLine),
                "",
                DAQmx_Val_Diff,
                -voltageRanges[voltageRangeIndex],
                voltageRanges[voltageRangeIndex],
                DAQmx_Val_Volts,
                nullptr));
        }
    }
    void getClock (char* trig_name_di, char* trig_name_do, char* trig_name_start, int bufferSize) 
    {
        GetTerminalNameWithDevPrefix (taskHandle_, "PXI_Trig0", trig_name_di);

        NIDAQ::DAQmxCfgSampClkTiming (taskHandle_,
                                      "",
                                      getSampleRate(),
                                      DAQmx_Val_Rising,
                                      DAQmx_Val_ContSamps,
                                      bufferSize);

        NIDAQ::DAQmxExportSignal (taskHandle_, DAQmx_Val_SampleClock, trig_name_di);


        DAQmxCheck (NIDAQ::DAQmxCreateTask (STR2CHR ("CounterClockTask" + name_), &counterTask));
        
        // Create pulse train with frequency = 2*Fs and 50% duty cycle
        NIDAQ::DAQmxCreateCOPulseChanFreq (
            counterTask,
            STR2CHR (name_ + "/ctr0" ),
            "",
            DAQmx_Val_Hz, // Units
            DAQmx_Val_Low, // Idle state
            3.0 / (4*getSampleRate()), // Initial delay
            2 * getSampleRate(), // Frequency (2*Fs)
            0.5 // Duty cycle (50%)
        );


        // Configure for continuous pulse generation
        NIDAQ::DAQmxCfgImplicitTiming (counterTask, DAQmx_Val_ContSamps, 1000);

        // CRITICAL: Export counter output to PXI trigger line for chassis-wide sharing
        GetTerminalNameWithDevPrefix (taskHandle_, "PXI_Trig1", trig_name_do);
        NIDAQ::DAQmxExportSignal (counterTask, DAQmx_Val_CounterOutputEvent, trig_name_do);

        std::cout << "Counter clock (2*Fs) exported to: " << trig_name_do << std::endl;
        std::cout << "DO tasks will use: " << trig_name_di << std::endl;

         GetTerminalNameWithDevPrefix (taskHandle_, "PXI_Trig2", trig_name_start);
        NIDAQ::DAQmxExportSignal (taskHandle_, DAQmx_Val_StartTrigger, trig_name_start);

         NIDAQ::DAQmxCfgDigEdgeStartTrig (
             counterTask,
             trig_name_start,
             DAQmx_Val_Rising); // Set Start Clock;
    }

    void setClock (char* trigName, char* trigStart, int bufferSize)
    {
        NIDAQ::DAQmxCfgSampClkTiming (
            taskHandle_,
            trigName,
            getSampleRate(),
            DAQmx_Val_Rising,
            DAQmx_Val_ContSamps,
            bufferSize);

        
         NIDAQ::DAQmxCfgDigEdgeStartTrig (
            taskHandle_,
            trigStart,
            DAQmx_Val_Rising); // Set Start Clock;
    }

    void acquire (std::vector<NIDAQ::float64>* ai_data, int buffer_size)
    {
        ai_data->resize (analogLines_.size() * buffer_size);

        NIDAQ::DAQmxReadAnalogF64 (
            taskHandle_,
            buffer_size,
            timeout_,
            DAQmx_Val_GroupByChannel,
            ai_data->data(),
            analogLines_.size() * buffer_size,
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
    InputDIChannel (String name, juce::String digitalPort, int dev_index, int numLines)
        : Channel (name, dev_index),
          digitalPort_ (digitalPort),
          numLines_ (numLines) {}

    void setup (char* trigName, char* trigStart, int buffer, int numStation)
    {
        const int pulseLengthInSamples = 3;
        const int samplesPerStation = numLines_ * pulseLengthInSamples;

        DAQmxCheck (NIDAQ::DAQmxCreateTask (STR2CHR ("DITask_" + name_), &taskHandle_));
        DAQmxCheck (NIDAQ::DAQmxCreateDOChan (taskHandle_,
                                              STR2CHR (name_ + "/port0"),
                                              "",
                                              DAQmx_Val_ChanForAllLines));

        DAQmxCheck (NIDAQ::DAQmxCfgSampClkTiming (taskHandle_,
                                                  trigName,
                                                  getSampleRate() * 2,
                                                  DAQmx_Val_Rising,
                                                  DAQmx_Val_ContSamps,
                                                  buffer));
        NIDAQ::DAQmxCfgDigEdgeStartTrig (
            taskHandle_,
            trigStart,
            DAQmx_Val_Rising); // Set Start Clock;

        DAQmxCheck (NIDAQ::DAQmxSetWriteRegenMode (taskHandle_, DAQmx_Val_AllowRegen));

        std::vector<NIDAQ::uInt32> waveform (samplesPerStation * numStation, 0);
        int startSample = dev_index_ * samplesPerStation;

        NIDAQ::uInt32 bitMask = 0;

        for (int line_i = 0; line_i < numLines_; line_i++)
        {
            int sampleOffset = startSample + line_i * pulseLengthInSamples;
            bitMask = static_cast<NIDAQ::uInt32> (1 << line_i);
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
    EventDIChannel (String name, String digitalLine, int event_label)
        : Channel (name, 0), digitalLine_ (digitalLine), event_label_ (event_label) {}

    void setup (char* trigName, char* trigStart, int buffer)
    {
        DAQmxCheck (NIDAQ::DAQmxCreateTask ("Event_DI_Task", &taskHandle_));

        DAQmxCheck (NIDAQ::DAQmxCreateDIChan (taskHandle_,
                                              STR2CHR (name_ + "/" + digitalLine_),
                                              "",
                                              DAQmx_Val_ChanForAllLines));

        DAQmxCheck (NIDAQ::DAQmxCfgSampClkTiming (taskHandle_,
                                                  trigName,
                                                  getSampleRate(),
                                                  DAQmx_Val_Rising,
                                                  DAQmx_Val_ContSamps,
                                                  buffer));

        NIDAQ::DAQmxCfgDigEdgeStartTrig (
            taskHandle_,
            trigStart,
            DAQmx_Val_Rising); // Set Start Clock;


    }

    void acquire (std::vector<NIDAQ::uInt32>* di_data, int buffer_size)
    {
        di_data->resize (buffer_size);

        DAQmxCheck (NIDAQ::DAQmxReadDigitalU32 (
            taskHandle_,
            buffer_size,
            timeout_,
            DAQmx_Val_GroupByScanNumber,
            di_data->data(),
            buffer_size,
            nullptr,
            nullptr));
    }
    int event_label_ = 0;

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
    StartChannel (String name, String digitalLine, float start_time, int nbr_pulse, float pulse_duration)
        : Channel (name, 0), digitalLine_ (digitalLine), start_time_ (start_time), nbr_pulse_ (nbr_pulse), pulse_duration_ (pulse_duration) {}

    void setup (char* trigName, char* trigStart)
    {
        NIDAQ::float64 timeout = 5.0;
        float pulse_length = pulse_duration_ * getSampleRate();
        float pulse_start_length = start_time_ * getSampleRate();

        std::vector<NIDAQ::uInt32> waveform_start (pulse_length * (nbr_pulse_ * 2 + 1) + pulse_start_length, 0);
        LOGD (waveform_start.size());

        DAQmxCheck (NIDAQ::DAQmxCreateTask ("StartPulseTask", &taskHandle_));
        DAQmxCheck (NIDAQ::DAQmxCreateDOChan (taskHandle_,
                                              STR2CHR (name_ + "/" + digitalLine_),
                                              "",
                                              DAQmx_Val_ChanPerLine));

        LOGD (name_ + "/" + digitalLine_);
        DAQmxCheck (NIDAQ::DAQmxCfgSampClkTiming (taskHandle_,
                                                  trigName,
                                                  getSampleRate(),
                                                  DAQmx_Val_Rising,
                                                  DAQmx_Val_FiniteSamps,
                                                  waveform_start.size()));

                
         NIDAQ::DAQmxCfgDigEdgeStartTrig (
            taskHandle_,
            trigStart,
            DAQmx_Val_Rising); // Set Start Clock;

        DAQmxCheck (NIDAQ::DAQmxSetBufOutputBufSize (taskHandle_, waveform_start.size()));

        NIDAQ::uInt32 bitMask = static_cast<NIDAQ::uInt32> (1 << 8);

        for (int i = 0; i < nbr_pulse_; i++)
        {

            std::fill (waveform_start.begin() + pulse_start_length + pulse_length * (i * 2 + 1),
                       waveform_start.begin() + pulse_start_length + (i * 2 + 2) * pulse_length,
                       bitMask);
        }

        int chunkSize = 62500;
        for (int i = 0; i < waveform_start.size(); i += chunkSize)
        {
            int currentChunkSize = std::min (chunkSize, static_cast<int> (waveform_start.size() - i));

            DAQmxCheck (NIDAQ::DAQmxWriteDigitalU32 (
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
    float start_time_ = 0.0;
    int nbr_pulse_ = 0;
    float pulse_duration_ = 0.0;
};

class NeuroProcessor : public Thread
{
public:
    NeuroProcessor (NeuroConfig& cfg);
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
    int getCellNumber() { return getRowNumber() * getColumnNumber(); };

    void closeTask()
    {
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

        if (startDevice != 0 && startDevice != nullptr)
            startDevice->stop();
    };

    void run();

    NIDAQ::float64 sampleRate;
    DataBuffer* aiBuffer = nullptr;

private:
    HeapBlock<NIDAQ::uInt32> eventCodes;
    int voltageRangeIndex { 0 };
    int64 ai_timestamp = 0;
    uint64 eventCode = 0;

    int numProbeColumn = 0;
    int numProbeRow = 0;
};

#endif // __NIDAQCOMPONENTS_H__