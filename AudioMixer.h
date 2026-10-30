/*
 * ============================================================================
 * Candidate: Marc Edouard Ohlmann
 * Date: October 29th, 2026
 * Assignment: Plexsys - Senior Software Engineer Take Home
 *
 * Description: A real-time C++ audio mixing API that aggregates multiple
 *              continuous sinusoidal tones into a single output stream.
 * ============================================================================
 */

#pragma once

#include <algorithm> // For std::clamp, std::max, and std::min
#include <vector>    // For std::vector
#include <thread>    // For std::thread and std::this_thread::sleep_until
#include <mutex>     // For std::mutex and std::lock_guard
#include <atomic>    // For std::atomic<bool>
#include <chrono>    // For std::chrono::steady_clock and milliseconds
#include <cmath>     // For std::sin and std::fabs
#include <cstdint>   // For uint32_t


const double c_Pi = 3.14159265358979323846;
const double c_TwoPi = 2.0 * c_Pi;

const size_t c_MaxStreams = 32;
const uint32_t c_SampleRate = 48000;
const uint32_t kFrameDurationMs = 10;
const size_t c_SamplesPerFrame = (c_SampleRate * kFrameDurationMs) / 1000; // 480 samples per 10ms frame


// Abstract Output Stream Interface
class IOutputStream 
{
public:

    virtual ~IOutputStream() = default;
    virtual void WriteSamples(const float* buffer, size_t sampleCount) = 0;

};


// Abstract Input Stream Interface
class IInputStream 
{
public:

    virtual ~IInputStream() = default;
    virtual float ReadSample() = 0;

};


// Real-Time Audio Mixer
class AudioMixer 
{
public:

    AudioMixer() 
        : _bIsMixingThreadActive(false)
        , _mixingThread()
        , _mutex()
        , _inputStreams()
        , _outputStream(nullptr)
        , _nextStreamId(0)
        , _masterVolume(1.0f)
    {
        // pre-allocate once
        _inputStreams.reserve(c_MaxStreams);
    }

    ~AudioMixer() 
    {
        Stop();
    }

    // adds an input stream to be mixed and returns the id
    int AddInputStream(IInputStream* stream) 
    {
        if (stream == nullptr) 
        {
            return -1;
        }

        std::lock_guard<std::mutex> lock(_mutex);

        if (_inputStreams.size() >= c_MaxStreams)
        {
            return -1;
        }

        const int streamId = ++_nextStreamId;
        StreamEntry streamEntry;
        streamEntry.id = streamId;
        streamEntry.stream = stream;
        _inputStreams.push_back(streamEntry);


        return streamId;
    }

    // removes an input stream by given id
    bool RemoveInputStream(const int id) 
    {
        std::lock_guard<std::mutex> lock(_mutex);
        for (size_t i = 0; i < _inputStreams.size(); ++i)
        {
            if (_inputStreams[i].id == id)
            {
                // O(1) swap-and-pop removal
                _inputStreams[i] = _inputStreams.back();
                _inputStreams.pop_back();
                return true;
            }
        }


        return false;
    }

    // set the target output stream
    void SetOutputStream(IOutputStream* output) 
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _outputStream = output;
    }

    // start the mixing loop
    void Start() 
    {
        if (_bIsMixingThreadActive.exchange(true) == false)
        {
            _mixingThread = std::thread(&AudioMixer::MixingLoop, this);
        }
    }

    // stop the mixing loop
    void Stop() 
    {
        if (_bIsMixingThreadActive.exchange(false) && _mixingThread.joinable())
        {
            _mixingThread.join();
        }
    }

    void SetMasterVolume(float volume)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _masterVolume = volume;
    }

private:

    struct StreamEntry 
    {
        int id = 0;
        IInputStream* stream = nullptr;
    };

    std::atomic<bool> _bIsMixingThreadActive;
    std::thread _mixingThread;
    std::mutex _mutex;

    // the input streams to be mixed
    std::vector<StreamEntry> _inputStreams;

    // the target output stream
    IOutputStream* _outputStream = nullptr;

    // stream id assignment iterator
    int _nextStreamId;
    
    // master output gain multiplier
    float _masterVolume;


    // execution target for the mixing thread
    void MixingLoop() 
    {
        float mixBuffer[c_SamplesPerFrame];
        std::chrono::steady_clock::time_point nextWakeTime = std::chrono::steady_clock::now();

        while (_bIsMixingThreadActive.load()) 
        {
            nextWakeTime += std::chrono::milliseconds(kFrameDurationMs);

            IOutputStream* currentOutput = nullptr;
            float currentVolume = 0.f;

            // Lock only while reading streams and summing samples into mixBuffer
            {
                std::lock_guard<std::mutex> lock(_mutex);
                currentOutput = _outputStream;
                currentVolume = _masterVolume;

                for (size_t sampleIterator = 0; sampleIterator < c_SamplesPerFrame; ++sampleIterator)
                {
                    float sum = 0.0f;
                    for (size_t streamIterator = 0; streamIterator < _inputStreams.size(); ++streamIterator)
                    {
                        sum += _inputStreams[streamIterator].stream->ReadSample();
                    }
                    const float mixedOutput = sum * currentVolume;

                    // Clamp mixed output to normalized range
                    mixBuffer[sampleIterator] = std::clamp(mixedOutput, -1.0f, 1.0f);
                }
            } // Lock is released here before calling WriteSamples

            if (currentOutput != nullptr) 
            {
                currentOutput->WriteSamples(mixBuffer, c_SamplesPerFrame);
            }

            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
            if (now < nextWakeTime) 
            {
                std::this_thread::sleep_until(nextWakeTime);
            }
            else 
            {
                nextWakeTime = now;
            }
        }
    }

};