#include <cstdint>
#include <iostream>
#include <functional>
#include <cmath>
#include <vector>
#include <map>

#include "utils.hpp"
#include "aprs.hpp"
#include "afsk.hpp"
#include "wav.hpp"
#include "stack_guards.hpp"

using namespace std::literals;

void aprs_test(
    const std::string &callsign,
    uint8_t sender_ssid,
    const std::string &message,
    const std::string &out_wav)
{
    BEGIN();

    auto packet = APRSPacket(callsign, sender_ssid, message).Encode();
    auto decoded = APRSPacket::Decode(packet);

    std::cout << "Straightforward decoding test results: ";
    if (decoded.sender_callsign != callsign || decoded.sender_ssid != sender_ssid || decoded.custom_data != message)
    {
        std::cout << "FAILURE";
    }
    else
    {
        std::cout << "SUCCESS";
    }
    std::cout << std::endl;

    auto samples = AFSK::Encoder::Encode(packet);

    {
        WAVWriter ww(out_wav, AFSK::sample_rate);
        for (auto sample : samples)
        {
            ww.put(sample);
        }
    }

    WAVReader wr(out_wav);

    std::multimap<bool, int> phase_shift_result;
    std::multimap<bool, int> decode_result;

    for (size_t shift = 0; shift < AFSK::sample_rate / AFSK::baud_rate; ++shift)
    {
        auto result = AFSK::Decoder::demod(wr.Samples(), shift);

        if (packet.size() > result.size())
        {
            phase_shift_result.insert({false, shift});
        }
        else
        {
            for (size_t i = 0; i < packet.size(); ++i)
            {
                if (packet[i] != result[i])
                {
                    phase_shift_result.insert({false, shift});
                    goto end;
                }
            }

            phase_shift_result.insert({true, shift});

            auto decoded = APRSPacket::Decode(result);
            decode_result.insert(
                {decoded.sender_callsign == callsign && decoded.sender_ssid == sender_ssid && decoded.custom_data == message,
                 shift});
        }

    end:;
    }

    std::cout
        << "1. Phase shift test results: " << phase_shift_result.count(true) << " PASSED, " << phase_shift_result.count(false) << " FAILED ("
        << 100.0 * phase_shift_result.count(true) / (phase_shift_result.count(true) + phase_shift_result.count(false)) << "%)" << std::endl;

    std::cout << "2. AFSK decoding test results: " << decode_result.count(true) << " PASSED, " << decode_result.count(false) << " FAILED ("
              << 100.0 * decode_result.count(true) / (decode_result.count(true) + decode_result.count(false)) << "%)" << std::endl;

    std::cout << std::endl;
    END();
}

void decode_file()
{
    WAVReader wr("test.wav");
    auto result = AFSK::Decoder::demod(wr.Samples(), 0);
    auto decoded = APRSPacket::Decode(result);
}

int main(int argc, char *argv[])
{
    BEGIN();
    if (argc < 4)
    {
        std::cerr
            << "Usage: "s << argv[0] << "<arguments...>\n"
            << "Arguments:\n"
            << "<callsign> <cs_suffix> <message> <out>\n"
            << "callsign: sender callsign\n"
            << "cs_suffix: sender SSID, number, 1-15\n"
            << "message: the actual message to send, spaces are allowed, no quotes required\n"
            << "out: output .wav file name\n";
        return 1;
    }

    std::string message;
    for (int i = 3; i < argc - 1; ++i)
    {
        message.append(argv[i]);
        message.push_back(' ');
    }

    //aprs_test(argv[1], std::atoi(argv[2]), message, argv[argc - 1]);

    decode_file();

    END_AND_CATCH(ex)
    {
        std::cerr << ex.what() << std::endl;
    }
}
