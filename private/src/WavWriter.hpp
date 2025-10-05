#pragma once

#include "FileWriter.hpp"
#include "QuadUtils.hpp"
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <cassert>

#pragma pack(push, 1)
struct Rf64Header
{
    // RF64 header
    //
    char riffId[4] = {'R', 'F', '6', '4'};
    uint32_t fileSize = 0xFFFFFFFF; // Always 0xFFFFFFFF for RF64
    char waveId[4] = {'W', 'A', 'V', 'E'};
    
    // ds64 chunk (RF64 specific)
    //
    char ds64Id[4] = {'d', 's', '6', '4'};
    uint32_t ds64Size = 28;
    uint64_t riffSize;
    uint64_t dataSize;
    uint64_t sampleCount;
    uint32_t tableLength = 0;
    
    // fmt chunk
    //
    char fmtId[4] = {'f', 'm', 't', ' '};
    uint32_t fmtSize = 40;
    uint16_t audioFormat = 0xFFFE; 
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample = 24;

    uint16_t cbSize = 22;
    uint16_t validBitsPerSample = 24;
    uint32_t channelMask = 0;
    uint8_t  subFormat[16] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
    
    // data chunk
    //
    char dataId[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize32 = 0xFFFFFFFF; // Always 0xFFFFFFFF for RF64
};
#pragma pack(pop)

struct MultichannelWavWriter
{
    static constexpr size_t x_bytesPerSample = 3;
    
    FileWriter m_fileWriter;
    Rf64Header m_header;
    uint32_t m_sampleRate;
    uint16_t m_numChannels;
    bool m_isOpen = false;
    uint64_t m_samplesWritten = 0;
    std::string m_filename;

    MultichannelWavWriter()
      : m_sampleRate(48000)
      , m_numChannels(0)
      , m_isOpen(false)
      , m_samplesWritten(0)
    {
    }

    ~MultichannelWavWriter()
    {
        Close();
    }
    
    bool Open(uint16_t numChannels, const std::string& filename, uint32_t sampleRate)
    {
        if (m_isOpen)
        {
            assert(false);
        }
        
        m_numChannels = numChannels;
        m_sampleRate = sampleRate;
        m_samplesWritten = 0;
        m_filename = filename;
        
        // Initialize RF64 header
        //
        m_header.numChannels = numChannels;
        m_header.sampleRate = sampleRate;
        m_header.bitsPerSample = 24;
        m_header.blockAlign = numChannels * x_bytesPerSample;
        m_header.byteRate = sampleRate * m_header.blockAlign;
        m_header.riffSize = 0;
        m_header.dataSize = 0;
        m_header.sampleCount = 0;
        
        // Open file writer
        //
        if (!m_fileWriter.Open(filename))
        {
            assert(false);
        }
        
        // Write RF64 header
        //
        m_fileWriter.Write(reinterpret_cast<const uint8_t*>(&m_header), sizeof(Rf64Header));
        
        m_isOpen = true;
        return true;
    }

    bool OpenQuad(uint16_t numChannels, const std::string& filename, uint32_t sampleRate)
    {
        return Open(numChannels * 4, filename, sampleRate);
    }
    
    void WriteSample(uint16_t channel, double sample)
    {
        if (!m_isOpen || channel >= m_numChannels)
        {
            assert(false);
        }
        
        // Convert float sample to 24-bit integer
        // Clamp to [-1.0, 1.0] range and convert to 24-bit signed integer
        //
        double clampedSample = std::max<double>(-10.0, std::min<double>(10.0, sample));
        int32_t intSample = static_cast<int32_t>(std::lround(clampedSample * 838860.7));
        
        // Convert to bytes (little-endian, 24-bit)
        //
        uint8_t sampleBytes[3];
        sampleBytes[0] = static_cast<uint8_t>(intSample & 0xFF);
        sampleBytes[1] = static_cast<uint8_t>((intSample >> 8) & 0xFF);
        sampleBytes[2] = static_cast<uint8_t>((intSample >> 16) & 0xFF);
        
        // Write directly to file
        //
        m_fileWriter.Write(sampleBytes, 3);
        m_header.dataSize += 3;
        
        // Track samples written for this channel
        //
        if (channel == m_numChannels - 1)
        {
            // Verify data size is a multiple of 3 * numChannels
            //
            assert(m_header.dataSize % (3 * m_numChannels) == 0);
            m_samplesWritten++;
        }
    }

    void WriteSample(uint16_t channel, const QuadFloat& quadSample)
    {
        if (!m_isOpen || m_numChannels < 4)
        {
            assert(false);
        }
        
        // Write each component of the QuadFloat to its corresponding channel
        //
        for (uint16_t i = 0; i < 4; ++i)
        {
            WriteSample(4 * channel + i, quadSample[i]);
        }
    }

    void WriteSampleIfOpen(uint16_t channel, float sample)
    {
        if (m_isOpen)
        {
            WriteSample(channel, sample);
        }
    }

    void WriteSampleIfOpen(uint16_t channel, const QuadFloat& quadSample)
    {
        if (m_isOpen)
        {
            WriteSample(channel, quadSample);
        }
    }
    
    void Close()
    {
        if (!m_isOpen)
        {
            return;
        }
        
        // Close the file writer first
        //
        m_fileWriter.Close();
        
        // Update header with final sizes
        //
        m_header.riffSize = sizeof(Rf64Header) + m_header.dataSize - 8;
        m_header.sampleCount = m_samplesWritten;
        
        // Patch the header in the file
        //
        std::ofstream file(m_filename, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open())
        {
            assert(false);
        }
        
        // Write updated header
        //
        file.write(reinterpret_cast<const char*>(&m_header), sizeof(Rf64Header));
        file.close();
        
        m_isOpen = false;
    }
}; 