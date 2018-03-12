#pragma once
#include <vector>
#include <string>

extern "C" {
#include "C-Wav-Lib/wav.h"
}

class WAVWriter
{
    const size_t sample_rate;

    FILE *f = nullptr;

    std::vector<uint8_t> samples;
    wav_sample_t sample = {1, 1, 1, nullptr};

  public:
    WAVWriter(const std::string &name, const size_t sample_rate)
        : sample_rate(sample_rate)
    {
        f = name != "-" ? fopen(name.c_str(), "wb") : stdout;
        samples.reserve(sample_rate);
    }

    ~WAVWriter()
    {
        write_wav_header(
            f,
            1,
            sample_rate,
            1,
            samples.size() + sample_rate / 10 * 2);

        std::vector<uint8_t> silence(sample_rate / 10, 128);

        // surround actual data with silence for easier playback
        sample.DataSize = silence.size();
        sample.sampleData = silence.data();
        //write_wav_sample(f, &sample);

        sample.DataSize = samples.size();
        sample.sampleData = samples.data();
        write_wav_sample(f, &sample);

        sample.DataSize = silence.size();
        sample.sampleData = silence.data();
        //write_wav_sample(f, &sample);

        fclose(f);
    }

    void put(uint8_t sample)
    {
        // don't care about RAM, care about speed
        if (samples.capacity() == samples.size())
            samples.reserve(samples.capacity() * 2);

        samples.push_back(sample);
    }

    size_t size() const
    {
        return samples.size();
    }
};

class WAVReader
{
    std::vector<uint8_t> samples;
    wav_sample_t sample = {1, 1, 1, nullptr};

  public:
    WAVReader(const std::string &name)
    {
        FILE *f = name != "-" ? fopen(name.c_str(), "rb") : stdin;
        wav_header_t hdr;
        read_wav_header(f, &hdr);
        samples.resize(hdr.Data.Subchunk2Size);
        fread(samples.data(), 1, hdr.Data.Subchunk2Size, f);
        fclose(f);
    }

    const std::vector<uint8_t> &Samples() const
    {
        return samples;
    }
};