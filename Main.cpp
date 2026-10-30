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

#include "AudioMixer.h"
#include <iostream>
#include <iomanip>


// Concrete Output Stream for visual debugging and verification
class ConsoleMeterOutputStream : public IOutputStream 
{
public:

    void WriteSamples(const float* buffer, size_t sampleCount) override 
    {
        float peak = 0.0f;
        for (size_t i = 0; i < sampleCount; ++i) 
        {
            float absVal = std::fabs(buffer[i]);
            if (absVal > peak) 
            {
                peak = absVal;
            }
        }

        ++_frameCount;

        // Build a 20-character ASCII volume meter based on peak amplitude [0.0, 1.0]
        const int maxBars = 20;
        const int filledBars = static_cast<int>(peak * maxBars + 0.5f);

        std::cout << "Frame " 
            << std::setw(2) << _frameCount 
            << " | Peak: " 
            << std::fixed << std::setprecision(4) << peak 
            << " [";
        
        for (int b = 0; b < maxBars; ++b) 
        {
            std::cout << (b < filledBars ? '#' : ' ');
        }

        std::cout << "] | Samples[1..3]: ("
            << std::setw(7) << buffer[1] << ", "
            << std::setw(7) << buffer[2] << ", "
            << std::setw(7) << buffer[3] << ")\n";
    }

private:

    size_t _frameCount = 0;

};


int main() {

    ConsoleMeterOutputStream consoleOutput;
    AudioMixer mixer;

    mixer.SetOutputStream(&consoleOutput);

    // Create three sine wave tones in the audible spectrum (20 Hz - 20,000 Hz)
    SineWaveStream tone440(440.0f, 0.4f);   // A4 note, 0.4 amplitude
    SineWaveStream tone880(880.0f, 0.4f);   // A5 note, 0.4 amplitude
    SineWaveStream tone1000(1000.0f, 0.6f); // 1 kHz test tone, 0.6 amplitude

    mixer.Start();
    std::cout << "\n\nMixer started.\n\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(45));

    std::cout << "\n=== PHASE 1: Mixing two tones (440 Hz @ 0.4 + 880 Hz @ 0.4) ===\n";
    int id1 = mixer.AddInputStream(&tone440);
    int id2 = mixer.AddInputStream(&tone880);

    std::this_thread::sleep_for(std::chrono::milliseconds(45));

    int id3 = mixer.AddInputStream(&tone1000);
    std::cout << "\n=== PHASE 2: Adding 3rd tone (1000 Hz @ 0.6) -> Expect clamping at 1.0000 ===\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(45));

    mixer.RemoveInputStream(id1);
    mixer.RemoveInputStream(id3);
    std::cout << "\n=== PHASE 3: Removing 440 Hz and 1000 Hz -> Only 880 Hz @ 0.4 remains ===\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(45));

    mixer.RemoveInputStream(id2);
    std::cout << "\n=== PHASE 4: Removing last stream -> Expect silence (0.0000) ===\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(45));

    mixer.Stop();
    std::cout << "\nMixer stopped.\n";


    return 0;
}