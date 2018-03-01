#pragma once
#include <string>

#include "utils.hpp"
#include "wav.hpp"

class AFSK
{
    // sample rate of output, must be a multiple of lut_size
    static const size_t sample_rate = utils::lut_size * 20;

  public:
    // Encodes an AX.25 KISS message
    static void Encode(
        std::string message,
        const std::string &out,
        int begin_marker_size = 8,
        int end_marker_size = 3,
        int baud_rate = 1200)
    {
        // internal bit counter - needed for proper timing
        int total_bits = 0;

        WAV w(out, sample_rate);
        // write several 0x00's to allow the receiver to synchronize
        synth(std::string(begin_marker_size, 0x00), baud_rate, w, total_bits);
        synth(message, baud_rate, w, total_bits);
        // and more 0x00's for good measure
        synth(std::string(end_marker_size, 0x7E), baud_rate, w, total_bits);
    }

  private:
    static void synth(
        const std::string &message,
        const int baud_rate,
        WAV &wav,
        int &total_bits)
    {
        // frequency step - a minimum change in signal frequency that is for current sample rate
        // this is actually reduced to 1 Hz when calculating samples
        const int freq_step = sample_rate / utils::lut_size;
        // baud step - a number of samples comprising 1 baud
        // float because 1200 baud does not play nicely with most sample rates
        // TODO: we can probably reduce this to int by restricting sample rate
        const float baud_step = float(sample_rate) / baud_rate;

        int idx = 0;
        int ones = 0;
        uintmax_t freq = 1200;
        float mul = 1;

        for (size_t i = 0; i < message.size(); ++i)
        {
            uint8_t byte = message[i];
            bool separator = (byte == 0x7E) && (i == 0 || i == message.size() - 1);

            for (int bit = 1; bit <= 8; ++bit)
            {
                ++total_bits;

                if (!separator && ++ones == 6)
                {
                    ones = 0;
                    byte <<= 1; // holy fuck-knuckles
                    --bit;
                }

                // NRZI encoding
                if ((byte & 0x01) == 0)
                {
                    // wiggle frequency if bit is unset
                    wiggle(freq, mul);
                    ones = 0;
                }

                byte >>= 1;

                // write samples
                while (wav.size() < total_bits * baud_step)
                {
                    // do a LUT lookup
                    uint8_t x = 0.8 * utils::lut[(idx / freq_step) % utils::lut_size] + 128;
                    // write sample
                    wav.put(x);
                    // advance in LUT
                    idx += freq;
                }
            }
        }
    }

    static void demod()
    {

    }

    static void wiggle(uintmax_t &freq, float &mul)
    {
        constexpr uintmax_t freq_hi = 2200;
        constexpr uintmax_t freq_lo = 1200;

        if (freq == freq_lo)
        {
            freq = freq_hi;
            mul = 0.8;
        }
        else
        {
            freq = freq_lo;
            mul = 0.8f * 0.56f;
        }
    }
};