/*
    OneLoneCoder.com - Simple Audio Noisy Thing (macOS version)
    "Allows you to simply listen to that waveform!" - @Javidx9

    ...

*/

#pragma once

#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <AudioToolbox/AudioToolbox.h>
using namespace std;

const double PI = 2.0 * acos(0.0);

template <class T>
class olcNoiseMaker
{
public:
    olcNoiseMaker(unsigned int nSampleRate = 44100, unsigned int nChannels = 1, unsigned int nBlocks = 8, unsigned int nBlockSamples = 512)
    {
        Create(nSampleRate, nChannels, nBlocks, nBlockSamples);
    }

    ~olcNoiseMaker()
    {
        Destroy();
    }

    bool Create(unsigned int nSampleRate = 44100, unsigned int nChannels = 1, unsigned int nBlocks = 8, unsigned int nBlockSamples = 512)
    {
        m_bReady = false;
        m_nSampleRate = nSampleRate;
        m_nChannels = nChannels;
        m_nBlockCount = nBlocks;
        m_nBlockSamples = nBlockSamples;
        m_nBlockFree = m_nBlockCount;
        m_nBlockCurrent = 0;
        m_pBlockMemory = nullptr;

        m_userFunction = nullptr;

        if (!InitializeAudio())
        {
            cerr << "Failed to initialize audio." << endl;
            return false;
        }

        // Allocate Wave|Block Memory
        m_pBlockMemory = new T[m_nBlockCount * m_nBlockSamples];
        if (m_pBlockMemory == nullptr)
        {
            cerr << "Failed to allocate memory." << endl;
            return false;
        }
        memset(m_pBlockMemory, 0, sizeof(T) * m_nBlockCount * m_nBlockSamples);

        m_bReady = true;

        m_thread = thread(&olcNoiseMaker::MainThread, this);

        // Start the ball rolling
        unique_lock<mutex> lm(m_muxBlockNotZero);
        m_cvBlockNotZero.notify_one();

        return true;
    }

    bool Destroy()
    {
        // Add cleanup code for macOS audio here
        return false;
    }

    void Stop()
    {
        m_bReady = false;
        m_thread.join();
    }

    // Override to process current sample
    virtual double UserProcess(double dTime)
    {
        return 0.0;
    }

    double GetTime()
    {
        return m_dGlobalTime;
    }

private:
    AudioUnit m_auOutput;

    unsigned int m_nSampleRate;
    unsigned int m_nChannels;
    unsigned int m_nBlockCount;
    unsigned int m_nBlockSamples;
    unsigned int m_nBlockCurrent;

    T *m_pBlockMemory;

    thread m_thread;
    atomic<bool> m_bReady;
    atomic<unsigned int> m_nBlockFree;
    condition_variable m_cvBlockNotZero;
    mutex m_muxBlockNotZero;

    atomic<double> m_dGlobalTime;

    static OSStatus AudioUnitRenderCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags,
                                            const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
                                            UInt32 inNumberFrames, AudioBufferList *ioData);

    double (*m_userFunction)(double); // Forward declaration
    bool InitializeAudio();

    void MainThread();
};

template <class T>
OSStatus olcNoiseMaker<T>::AudioUnitRenderCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags,
                                                    const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
                                                    UInt32 inNumberFrames, AudioBufferList *ioData)
{
    auto *self = reinterpret_cast<olcNoiseMaker *>(inRefCon);

    for (UInt32 frameIndex = 0; frameIndex < inNumberFrames; ++frameIndex)
    {
        for (UInt32 channel = 0; channel < ioData->mNumberBuffers; ++channel)
        {
            T *outputBuffer = (T *)ioData->mBuffers[channel].mData;
            outputBuffer[frameIndex] = self->m_pBlockMemory[self->m_nBlockCurrent * self->m_nBlockSamples + frameIndex];
        }
    }

    return noErr;
}

template <class T>
bool olcNoiseMaker<T>::InitializeAudio()
{
    // Find and open the default output audio unit
    AudioComponentDescription outputDesc = {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags = 0,
        .componentFlagsMask = 0};

    AudioComponent outputComponent = AudioComponentFindNext(NULL, &outputDesc);
    if (outputComponent == NULL)
    {
        cerr << "Failed to find default output audio unit." << endl;
        return false;
    }

    OSStatus status = AudioComponentInstanceNew(outputComponent, &m_auOutput);
    if (status != noErr)
    {
        cerr << "Failed to create new audio unit instance." << endl;
        return false;
    }

    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = AudioUnitRenderCallback;
    callbackStruct.inputProcRefCon = this;

    status = AudioUnitSetProperty(m_auOutput, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callbackStruct, sizeof(callbackStruct));
    if (status != noErr)
    {
        cerr << "Failed to set render callback property." << endl;
        return false;
    }

    AudioStreamBasicDescription audioFormat;
    audioFormat.mSampleRate = m_nSampleRate;
    audioFormat.mFormatID = kAudioFormatLinearPCM;
    audioFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsNonInterleaved;
    audioFormat.mBytesPerPacket = sizeof(T);
    audioFormat.mFramesPerPacket = 1;
    audioFormat.mBytesPerFrame = sizeof(T);
    audioFormat.mChannelsPerFrame = m_nChannels;
    audioFormat.mBitsPerChannel = sizeof(T) * 8;

    status = AudioUnitSetProperty(m_auOutput, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &audioFormat, sizeof(audioFormat));
    if (status != noErr)
    {
        cerr << "Failed to set stream format property." << endl;
        return false;
    }

    status = AudioUnitInitialize(m_auOutput);
    if (status != noErr)
    {
        cerr << "Failed to initialize audio unit." << endl;
        return false;
    }

    return true;
}

template <class T>
void olcNoiseMaker<T>::MainThread()
{
    m_dGlobalTime = 0.0;
    double dTimeStep = 1.0 / (double)m_nSampleRate;

    T nMaxSample = static_cast<T>(pow(2, (sizeof(T) * 8) - 1) - 1);
    double dMaxSample = static_cast<double>(nMaxSample);
    T nPreviousSample = 0;

    while (m_bReady)
    {
        // Wait for block to become available
        if (m_nBlockFree == 0)
        {
            unique_lock<mutex> lm(m_muxBlockNotZero);
            m_cvBlockNotZero.wait(lm);
        }

        // Block is here, so use it
        m_nBlockFree--;

        // Prepare block for processing
        T nNewSample = 0;
        int nCurrentBlock = m_nBlockCurrent * m_nBlockSamples;

        for (unsigned int n = 0; n < m_nBlockSamples; n++)
        {
            // User Process
            if (m_userFunction == nullptr)
                nNewSample = static_cast<T>(clip(UserProcess(m_dGlobalTime), 1.0) * dMaxSample);
            else
                nNewSample = static_cast<T>(clip(m_userFunction(m_dGlobalTime), 1.0) * dMaxSample);

            m_pBlockMemory[nCurrentBlock + n] = nNewSample;
            nPreviousSample = nNewSample;
            m_dGlobalTime = m_dGlobalTime + dTimeStep;
        }

        // Send block to sound device
        AudioBufferList audioBufferList;
        audioBufferList.mNumberBuffers = m_nChannels;
        for (UInt32 channel = 0; channel < m_nChannels; ++channel)
        {
            audioBufferList.mBuffers[channel].mNumberChannels = 1;
            audioBufferList.mBuffers[channel].mDataByteSize = m_nBlockSamples * sizeof(T);
            audioBufferList.mBuffers[channel].mData = m_pBlockMemory;
        }

        AudioTimeStamp timeStamp;
        memset(&timeStamp, 0, sizeof(AudioTimeStamp));

        UInt32 inBusNumber = 0; // assuming a single bus

        OSStatus renderStatus = AudioUnitRenderCallback(NULL, NULL, &timeStamp, inBusNumber, m_nBlockSamples, &audioBufferList);
        if (renderStatus != noErr)
        {
            cerr << "Failed to render audio unit callback." << endl;
        }

        m_nBlockCurrent++;
        m_nBlockCurrent %= m_nBlockCount;
    }
}

