#pragma once

#include "FileWriter.hpp"
#include "QuadUtils.hpp"
#include "StereoUtils.hpp"
#include "AsyncLogger.hpp"
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
    bool m_error = false;
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
        m_error = false;
        
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
            m_error = true;
            INFO("WavWriter error: WriteSample called when not open or invalid channel (channel=%d, numChannels=%d, isOpen=%d)", channel, m_numChannels, m_isOpen);
            return;
        }
        
        // Convert float sample to 24-bit integer
        // Clamp to [-1.0, 1.0] range and convert to 24-bit signed integer
        //
        double clampedSample = std::max<double>(-1.0, std::min<double>(1.0, sample));
        int32_t intSample = static_cast<int32_t>(std::lround(clampedSample * 8388607));
        
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
            if (m_header.dataSize % (3 * m_numChannels) != 0)
            {
                m_error = true;
                INFO("WavWriter error: Data size mismatch (dataSize=%llu, expected multiple of %d)", static_cast<unsigned long long>(m_header.dataSize), 3 * m_numChannels);
            }
            
            m_samplesWritten++;
        }
    }

    void WriteSample(uint16_t channel, const QuadFloat& quadSample)
    {
        if (!m_isOpen || m_numChannels < 4)
        {
            m_error = true;
            INFO("WavWriter error: WriteSample QuadFloat called when not open or insufficient channels (channel=%d, numChannels=%d, isOpen=%d)", channel, m_numChannels, m_isOpen);
            return;
        }
        
        // Write each component of the QuadFloat to its corresponding channel
        //
        for (uint16_t i = 0; i < 4; ++i)
        {
            WriteSample(channel + i, quadSample[i]);
        }
    }

    void WriteSample(uint16_t channel, const StereoFloat& stereoSample)
    {
        if (!m_isOpen || m_numChannels < 2)
        {
            assert(false);
        }
        
        for (size_t i = 0; i < 2; ++i)
        {
            WriteSample(channel + i, stereoSample[i]);
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

    void WriteSampleIfOpen(uint16_t channel, const StereoFloat& stereoSample)
    {
        if (m_isOpen)
        {
            WriteSample(channel, stereoSample);
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
        
        // Check for errors from FileWriter after flush
        //
        if (m_fileWriter.m_error.load())
        {
            m_error = true;
            INFO("WavWriter error: FileWriter reported error during close");
        }
        
        // Update header with final sizes
        //
        m_header.riffSize = sizeof(Rf64Header) + m_header.dataSize - 8;
        m_header.sampleCount = m_samplesWritten;
        
        // Patch the header in the file
        //
        std::ofstream file(m_filename, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open())
        {
            m_error = true;
            INFO("WavWriter error: Failed to open file for header update: %s", m_filename.c_str());
            m_isOpen = false;
            m_fileWriter.m_error = false;
            return;
        }
        
        // Write updated header
        //
        file.write(reinterpret_cast<const char*>(&m_header), sizeof(Rf64Header));
        if (!file.good())
        {
            m_error = true;
            INFO("WavWriter error: Failed to write header update");
        }
        
        file.close();
        
        m_isOpen = false;
        m_fileWriter.m_error = false;
    }
}; 