#pragma once

// TempDir: a tiny RAII temporary-directory helper for the JUCE-free test binary.
//
// Creates a unique directory under the system temp root (mkdtemp) on
// construction and recursively deletes it on destruction. Used by WP-9 recording
// round-trip tests to give the IoTaskThread a real, isolated sample-directory
// root to persist WAVs into without leaving artifacts behind.
//
// Move-only (owns a filesystem resource). The path is stable for the lifetime of
// the object.

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace synthrig
{

class TempDir
{
public:
    TempDir()
    {
        // Honor TMPDIR if set (macOS CI uses per-user temp dirs), else /tmp.
        //
        const char* tmp = std::getenv("TMPDIR");
        std::filesystem::path base = (tmp && tmp[0] != '\0')
            ? std::filesystem::path(tmp)
            : std::filesystem::path("/tmp");

        std::string templ = (base / "synthrig_wp9_XXXXXX").string();
        std::vector<char> buf(templ.begin(), templ.end());
        buf.push_back('\0');

        char* made = ::mkdtemp(buf.data());
        if (made)
        {
            m_path = std::filesystem::path(made);
            m_valid = true;
        }
    }

    ~TempDir()
    {
        Cleanup();
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    TempDir(TempDir&& other) noexcept
        : m_path(std::move(other.m_path))
        , m_valid(other.m_valid)
    {
        other.m_valid = false;
    }

    TempDir& operator=(TempDir&& other) noexcept
    {
        if (this != &other)
        {
            Cleanup();
            m_path = std::move(other.m_path);
            m_valid = other.m_valid;
            other.m_valid = false;
        }
        return *this;
    }

    bool Valid() const { return m_valid; }
    const std::filesystem::path& Path() const { return m_path; }
    std::string String() const { return m_path.string(); }

private:
    void Cleanup()
    {
        if (m_valid && !m_path.empty())
        {
            std::error_code ec;
            std::filesystem::remove_all(m_path, ec);
        }
        m_valid = false;
    }

    std::filesystem::path m_path;
    bool m_valid = false;
};

} // namespace synthrig
