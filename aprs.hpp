#pragma once
#include <string>
#include <stdexcept>

class APRSPacket
{
  public:
    uint8_t sender_ssid;
    std::string sender_callsign;
    uint8_t target_ssid;
    std::string send_path;
    std::string custom_data;

    APRSPacket(
        uint8_t sender_ssid,
        const std::string &sender_callsign,
        uint8_t target_ssid,
        const std::string &send_path,
        const std::string &custom_data = "")
        : sender_ssid(sender_ssid),
          sender_callsign(sender_callsign),
          target_ssid(target_ssid),
          send_path(send_path),
          custom_data(custom_data)
    {
    }

    static std::string prepareCallsign(std::string cs)
    {
        if (cs.size() > 6)
        {
            throw std::runtime_error("Callsign " + cs + " is longer than 6 symbols!");
        }

        cs.append(6 - cs.size(), ' ');
        return cs;
    }

    std::string ToString() const
    {
        std::string packet;

        // Field name   | FLAG | DEST   | SOURCE | DIGIS | CONTROL | PROTO | INFO   | FCS   | FLAG
        // Size (bytes) | 1    | 7      | 7      | 0-56  | 1       | 1     | 1-256  | 2     | 1
        // Example      | 0x7E | APZQ00 | PIRATE | WIDE1 | 0x03    | 0xF0  | >HELLO | <...> | 0x7E
        // The above example will send the status HELLO from PIRATE with APZQ00 (experimental software v0.0)

        // this will speed shit up a notch
        packet.reserve(512);

        // start flag
        // shifted right because at the end we shift everything to the left
        packet.push_back(0x7E >> 1);

        // APRS address structure is as follows:
        // XXXXXXb, where XXXXXX is an uppercase ASCII 6-symbol callsign
        // and b is the SSID byte:
        //     0b0C11SSSS
        //         C - command/response bit
        //         S - 4 SSID bits (APRS symbol id, 0-15)

        // we identify ourselves as APZQ01 - experimental software, version 0.1
        packet.append("APZQ01");
        packet.push_back(0b01110000 | target_ssid);

        // Source Address
        packet.append(prepareCallsign(sender_callsign));
        packet.push_back(0b00110000 | (sender_ssid & 0x0F));

        packet.append(prepareCallsign("WIDE1"));
        packet.push_back(0b00110001); // 0b0HRRSSID (H - 'has been repeated' bit, RR - reserved '11', SSID - 0-15)

        packet.append(prepareCallsign("WIDE2"));
        packet.push_back(0b00110001); // 0b0HRRSSID (H - 'has been repeated' bit, RR - reserved '11', SSID - 0-15)

        // left shift the address bytes
        for (auto &byte : packet)
        {
            byte <<= 1;
        }

        // the last byte's LSB set to '1' to indicate the end of the address fields
        packet.back() |= 0x01;

        // Control Field
        packet.push_back(0x03);

        // Protocol ID
        packet.push_back(0xF0);

        // Information Field
        packet.append(custom_data);

        // Frame Check Sequence - CRC-16-CCITT (0xFFFF)
        uint16_t crc = 0xFFFF;
        for (uint16_t i = 1; i < packet.size(); i++)
        {
            crc = crc_ccitt_update(crc, packet[i]);
        }

        crc = ~crc;
        packet.push_back(crc & 0xFF);        // FCS is sent low-byte first
        packet.push_back((crc >> 8) & 0xFF); // and with the bits flipped

        // end flag
        packet.push_back(0x7E);
        return packet;
    }

  private:
    static uint16_t crc_ccitt_update(uint16_t crc, uint8_t data)
    {
        data ^= crc & 0xff;
        data ^= data << 4;

        return ((((uint16_t)data << 8) | (crc >> 8)) ^ (uint8_t)(data >> 4) ^ ((uint16_t)data << 3));
    }

    static std::string timestr()
    {
        time_t rawtime;
        struct tm *timeinfo;
        char buffer[10];

        time(&rawtime);
        timeinfo = localtime(&rawtime);

        strftime(buffer, sizeof(buffer), "%H%M%Sh", timeinfo);
        return buffer;
    }
};
