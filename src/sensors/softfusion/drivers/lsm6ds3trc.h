#pragma once

#include <cstdint>
#include <array>
#include <algorithm>

namespace SlimeVR::Sensors::SoftFusion::Drivers
{

// Driver uses acceleration range at 8g
// and gyroscope range at 1000dps

template <template<uint8_t> typename I2CImpl>
struct LSM6DS3TRC
{
    static constexpr uint8_t DevAddr = 0x6a;
    static constexpr auto Name = "LSM6DS3TR-C";
    static constexpr auto Type = 11;

    static constexpr float Freq = 425;

    static constexpr float GyrTs=1.0/Freq;
    static constexpr float AccTs=1.0/Freq;
    static constexpr float MagTs=1.0/Freq;

    static constexpr float GyroSensitivity = 28.571428571f;
    static constexpr float AccelSensitivity = 4098.360655738f;

    using i2c = I2CImpl<DevAddr>;
 
    //uint32_t m_freqSamples = 1;
    //float m_freq = 425.0f;
    //unsigned long m_lastTimestamp = millis();


    struct Regs {
        struct WhoAmI {
            static constexpr uint8_t reg = 0x0f;
            static constexpr uint8_t value = 0x6a;
        };
        static constexpr uint8_t OutTemp = 0x20;
        struct Ctrl1XL {
            static constexpr uint8_t reg = 0x10;
            static constexpr uint8_t value = (0b11 << 2) | (0b0110 << 4); //8g, 416Hz
        };
        struct Ctrl2G {
            static constexpr uint8_t reg = 0x11;
            static constexpr uint8_t value = (0b10 << 2) | (0b0110 << 4); //1000dps, 416Hz
        };
        struct Ctrl3C {
            static constexpr uint8_t reg = 0x12;
            static constexpr uint8_t valueSwReset = 1;
            static constexpr uint8_t value = (1 << 6) | (1 << 2); //BDU = 1, IF_INC = 1
        };
        struct FifoCtrl3 {
            static constexpr uint8_t reg = 0x08;
            static constexpr uint8_t value = 0b001 | (0b001 << 3); //accel no decimation, gyro no decimation
        };
        struct FifoCtrl5 {
            static constexpr uint8_t reg = 0x0a;
            static constexpr uint8_t value = 0b110 | (0b0111 << 3); //continuous mode, odr = 833Hz
        };

        static constexpr uint8_t FifoStatus = 0x3a;
        static constexpr uint8_t FifoData = 0x3e;
    };

    bool initialize()
    {
        // perform initialization step
        i2c::writeReg(Regs::Ctrl3C::reg, Regs::Ctrl3C::valueSwReset);
        delay(20);
        i2c::writeReg(Regs::Ctrl1XL::reg, Regs::Ctrl1XL::value);
        i2c::writeReg(Regs::Ctrl2G::reg, Regs::Ctrl2G::value);
        i2c::writeReg(Regs::Ctrl3C::reg, Regs::Ctrl3C::value);
        i2c::writeReg(Regs::FifoCtrl3::reg, Regs::FifoCtrl3::value);
        i2c::writeReg(Regs::FifoCtrl5::reg, Regs::FifoCtrl5::value);
        return true;
    }

    float getDirectTemp() const
    {
        const auto value = static_cast<int16_t>(i2c::readReg16(Regs::OutTemp));
        float result = ((float)value / 256.0f) + 25.0f;

        return result;
    }

    template <typename AccelCall, typename GyroCall>
    void bulkRead(AccelCall &&processAccelSample, GyroCall &&processGyroSample) {
        const auto read_result = i2c::readReg16(Regs::FifoStatus);
        if (read_result & 0x4000) { // overrun!
            // disable and re-enable fifo to clear it
            printf("Fifo overrun, resetting\n");
            i2c::writeReg(Regs::FifoCtrl5::reg, 0);
            i2c::writeReg(Regs::FifoCtrl5::reg, Regs::FifoCtrl5::value);
            return;
        }
        const auto unread_entries = read_result & 0x7ff;
        constexpr auto single_measurement_words = 6;
        constexpr auto single_measurement_bytes = sizeof(uint16_t) * single_measurement_words;
        
        std::array<int16_t, 60> read_buffer; // max 10 packages of 6 16bit values of data form fifo
        const auto bytes_to_read = std::min(static_cast<size_t>(read_buffer.size()), static_cast<size_t>(unread_entries)) \
                     * sizeof(uint16_t) / single_measurement_bytes * single_measurement_bytes;

        i2c::readBytes(Regs::FifoData, bytes_to_read, reinterpret_cast<uint8_t *>(read_buffer.data()));
        //static auto samples = 0;
        for (uint16_t i=0; i<bytes_to_read/sizeof(uint16_t); i+=single_measurement_words) {
            //printf("\r%d/%d     ", i, bytes_to_read/*, read_buffer[3], read_buffer[4], read_buffer[5]*/);
            processGyroSample(reinterpret_cast<const int16_t *>(&read_buffer[i]), GyrTs);
            processAccelSample(reinterpret_cast<const int16_t *>(&read_buffer[i+3]), AccTs);
            //samples++;
        }
        /*
        auto stop = millis();
        if (stop - m_lastTimestamp >= 1000) {
            float lastSamples =  (samples*1000.0) / (stop - m_lastTimestamp);
            printf("Samples %f mean %f diff %d\n", lastSamples, m_freq, stop - m_lastTimestamp);
            m_freq += (lastSamples - m_freq) / m_freqSamples;
            samples = 0;
            m_lastTimestamp += 1000;

            m_freqSamples++;
        }*/
        
    }


};

} // namespace