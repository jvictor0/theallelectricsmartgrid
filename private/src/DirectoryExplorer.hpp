#pragma once

#include "SnapshotUIState.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct DirectoryExplorer
{
    static constexpr size_t x_invalidChildIndex = static_cast<size_t>(-1);
    static constexpr size_t x_pathCapacity = 512;
    static constexpr size_t x_lineCapacity = 256;
    static constexpr size_t x_numListLines = 7;
    static constexpr ptrdiff_t x_listWindowCenterOffset = static_cast<ptrdiff_t>((x_numListLines - 1) / 2);

    enum class MessageType : uint8_t
    {
        Up,
        Down,
        Left,
        Right,
        Yes,
        No,
    };

    struct Node
    {
        bool m_isDirectory;
        bool m_isShell;
        std::string m_fileName;
        std::vector<Node*> m_contents;
        Node* m_parent;
        std::filesystem::path m_absolutePath;

        Node()
            : m_isDirectory(false)
            , m_isShell(false)
            , m_fileName()
            , m_contents()
            , m_parent(nullptr)
            , m_absolutePath()
        {
        }
    };

    struct UISnapshot
    {
        size_t m_selectedListRow;
        bool m_uiExplorerOpen;
        char m_relativePath[x_pathCapacity];
        char m_listLines[x_numListLines][x_lineCapacity];
    };

    struct UIState : SnapshotUIState<UISnapshot>
    {
        static constexpr size_t x_pathCapacity = DirectoryExplorer::x_pathCapacity;
        static constexpr size_t x_lineCapacity = DirectoryExplorer::x_lineCapacity;
        static constexpr size_t x_numListLines = DirectoryExplorer::x_numListLines;
        static constexpr ptrdiff_t x_listWindowCenterOffset = DirectoryExplorer::x_listWindowCenterOffset;

        const char* GetRelativePath() const
        {
            return GetCurrentSnapshot().m_relativePath;
        }

        const char* GetListLine(size_t row) const
        {
            return GetCurrentSnapshot().m_listLines[row];
        }

        size_t GetSelectedListRow() const
        {
            return GetCurrentSnapshot().m_selectedListRow;
        }

        bool GetUiExplorerOpen() const
        {
            return GetCurrentSnapshot().m_uiExplorerOpen;
        }
    };

    void PopulateUIStates()
    {
        PopulateUIState(m_UIState);
    }

    DirectoryExplorer()
        : m_root(nullptr)
        , m_selectedNode(nullptr)
        , m_selectedIndex(0)
        , m_rootPath()
        , m_UIState(nullptr)
    {
        Reset();
    }

    ~DirectoryExplorer()
    {
        Reset();
    }

    void Init(const std::filesystem::path& absoluteRootPath)
    {
        Reset();

        std::error_code ec;
        std::filesystem::path canonical = std::filesystem::weakly_canonical(absoluteRootPath, ec);

        if (ec)
        {
            canonical = absoluteRootPath;
        }

        m_rootPath = std::make_unique<std::filesystem::path>(canonical);
        m_root = MakeNodeShell(*m_rootPath, nullptr);

        if (m_root && m_root->m_isDirectory)
        {
            BuildSubtree(m_root);
        }

        m_selectedNode = m_root;
        m_selectedIndex = 0;
    }

    void Init(const std::filesystem::path& absoluteRootPath, const std::filesystem::path& pathRelativeToRoot)
    {
        Init(absoluteRootPath);

        if (!m_root || !m_selectedNode || !m_rootPath)
        {
            return;
        }

        std::filesystem::path relative = pathRelativeToRoot.lexically_normal();

        if (relative.empty() || relative == std::filesystem::path(".") || relative == std::filesystem::path("./"))
        {
            return;
        }

        for (const std::filesystem::path& piece : relative)
        {
            if (piece.empty() || piece == std::filesystem::path("."))
            {
                continue;
            }

            if (piece == std::filesystem::path(".."))
            {
                break;
            }

            if (!TraverseOneRelativeComponent(piece.generic_string()))
            {
                break;
            }
        }

        ClampSelectionIndex();
    }

    void Reset()
    {
        DestroySubtree(m_root);
        m_root = nullptr;
        m_selectedNode = nullptr;
        m_selectedIndex = 0;
        m_rootPath.reset();
    }

    DirectoryExplorer(const DirectoryExplorer&) = delete;
    DirectoryExplorer& operator=(const DirectoryExplorer&) = delete;
    DirectoryExplorer(DirectoryExplorer&&) = delete;
    DirectoryExplorer& operator=(DirectoryExplorer&&) = delete;

    void Process(MessageType message)
    {
        if (message == MessageType::No)
        {
            Reset();
            return;
        }

        if (!m_root || !m_selectedNode)
        {
            return;
        }

        switch (message)
        {
            case MessageType::Up:
            {
                MoveSelectionUpDown(-1);
                break;
            }

            case MessageType::Down:
            {
                MoveSelectionUpDown(1);
                break;
            }

            case MessageType::Left:
            {
                if (m_selectedNode->m_parent)
                {
                    Node* leaving = m_selectedNode;
                    m_selectedNode = m_selectedNode->m_parent;
                    m_selectedIndex = IndexOfChild(m_selectedNode, leaving);
                    DestroyChildrenOfNode(leaving);
                }

                break;
            }

            case MessageType::Right:
            {
                const size_t n = m_selectedNode->m_contents.size();

                if (n == 0)
                {
                    break;
                }

                if (m_selectedIndex >= n)
                {
                    m_selectedIndex = n - 1;
                }

                Node* child = m_selectedNode->m_contents[m_selectedIndex];

                if (child->m_isDirectory)
                {
                    BuildSubtree(child);
                    m_selectedNode = child;
                    m_selectedIndex = 0;
                }

                break;
            }

            case MessageType::Yes:
            {
                break;
            }

            case MessageType::No:
            {
                break;
            }
        }

        ClampSelectionIndex();
    }

    void PopulateUIState(UIState* uiState) const
    {
        if (!uiState)
        {
            return;
        }

        UISnapshot& snapshot = uiState->BeginSnapshot();

        if (!m_rootPath || !m_root || !m_selectedNode)
        {
            ClearBuffer(snapshot.m_relativePath, UIState::x_pathCapacity);

            for (size_t i = 0; i < UIState::x_numListLines; ++i)
            {
                ClearBuffer(snapshot.m_listLines[i], UIState::x_lineCapacity);
            }

            snapshot.m_selectedListRow = 0;
            snapshot.m_uiExplorerOpen = false;
            uiState->CommitSnapshot();
            return;
        }

        const std::string relativePathString = RelativePathString();
        CopyToBuffer(snapshot.m_relativePath, UIState::x_pathCapacity, relativePathString.c_str());

        const size_t n = m_selectedNode->m_contents.size();

        for (size_t i = 0; i < UIState::x_numListLines; ++i)
        {
            ClearBuffer(snapshot.m_listLines[i], UIState::x_lineCapacity);
        }

        snapshot.m_selectedListRow = 0;

        if (n == 0)
        {
            snapshot.m_uiExplorerOpen = false;
            uiState->CommitSnapshot();
            return;
        }

        const size_t start = ListWindowStart(n, m_selectedIndex);
        const size_t rows = std::min(n, UIState::x_numListLines);

        for (size_t row = 0; row < rows; ++row)
        {
            const Node* entry = m_selectedNode->m_contents[start + row];
            CopyToBuffer(snapshot.m_listLines[row], UIState::x_lineCapacity, entry->m_fileName.c_str());
        }

        if (m_selectedIndex >= start && m_selectedIndex < start + rows)
        {
            snapshot.m_selectedListRow = m_selectedIndex - start;
        }

        snapshot.m_uiExplorerOpen = true;
        uiState->CommitSnapshot();
    }

    std::filesystem::path SelectedDirectoryPath() const
    {
        if (!m_root || !m_selectedNode)
        {
            return {};
        }

        const size_t n = m_selectedNode->m_contents.size();

        if (n == 0 || m_selectedIndex >= n)
        {
            return m_selectedNode->m_absolutePath;
        }

        Node* entry = m_selectedNode->m_contents[m_selectedIndex];

        if (entry->m_isDirectory)
        {
            return entry->m_absolutePath;
        }

        return m_selectedNode->m_absolutePath;
    }

    std::string SelectedDirectoryRelativePathString() const
    {
        if (!m_rootPath)
        {
            return "";
        }

        std::filesystem::path absoluteSelected = SelectedDirectoryPath();

        if (absoluteSelected.empty())
        {
            return "";
        }

        std::error_code ec;
        std::filesystem::path relativePath = std::filesystem::relative(absoluteSelected, *m_rootPath, ec);

        if (ec)
        {
            return "";
        }

        std::string s = relativePath.generic_string();

        if (s.empty() || s == ".")
        {
            return "";
        }

        return s;
    }

    bool TraverseOneRelativeComponent(const std::string& fileName)
    {
        if (!m_selectedNode || !m_selectedNode->m_isDirectory)
        {
            return false;
        }

        BuildSubtree(m_selectedNode);

        const size_t childIndex = FindChildIndexByFileName(m_selectedNode, fileName);

        if (childIndex == x_invalidChildIndex)
        {
            return false;
        }

        Node* child = m_selectedNode->m_contents[childIndex];

        if (child->m_isDirectory)
        {
            BuildSubtree(child);
            m_selectedNode = child;
            m_selectedIndex = 0;
            return true;
        }

        m_selectedIndex = childIndex;
        return false;
    }

    static size_t FindChildIndexByFileName(Node* directory, const std::string& fileName)
    {
        if (!directory)
        {
            return x_invalidChildIndex;
        }

        for (size_t i = 0; i < directory->m_contents.size(); ++i)
        {
            if (directory->m_contents[i]->m_fileName == fileName)
            {
                return i;
            }
        }

        return x_invalidChildIndex;
    }

    Node* m_root;
    Node* m_selectedNode;
    size_t m_selectedIndex;
    std::unique_ptr<std::filesystem::path> m_rootPath;
    UIState* m_UIState;

    void ClampSelectionIndex()
    {
        if (!m_selectedNode)
        {
            return;
        }

        const size_t n = m_selectedNode->m_contents.size();

        if (n == 0)
        {
            m_selectedIndex = 0;
            return;
        }

        if (m_selectedIndex >= n)
        {
            m_selectedIndex = n - 1;
        }
    }

    static void ClearBuffer(char* buffer, size_t capacity)
    {
        if (capacity == 0)
        {
            return;
        }

        buffer[0] = '\0';
    }

    static void CopyToBuffer(char* buffer, size_t capacity, const char* source)
    {
        if (capacity == 0)
        {
            return;
        }

        if (!source)
        {
            buffer[0] = '\0';
            return;
        }

        std::strncpy(buffer, source, capacity - 1);
        buffer[capacity - 1] = '\0';
    }

    static void DestroyChildrenOfNode(Node* node)
    {
        if (!node)
        {
            return;
        }

        for (Node* child : node->m_contents)
        {
            DestroySubtree(child);
        }

        node->m_contents.clear();

        if (node->m_isDirectory)
        {
            node->m_isShell = true;
        }
    }

    static void DestroySubtree(Node* node)
    {
        if (!node)
        {
            return;
        }

        for (Node* child : node->m_contents)
        {
            DestroySubtree(child);
        }

        node->m_contents.clear();
        delete node;
    }

    static size_t IndexOfChild(const Node* parent, const Node* child)
    {
        if (!parent || !child)
        {
            return 0;
        }

        for (size_t i = 0; i < parent->m_contents.size(); ++i)
        {
            if (parent->m_contents[i] == child)
            {
                return i;
            }
        }

        return 0;
    }

    size_t ListWindowStart(size_t n, size_t selectedIndex) const
    {
        if (n <= UIState::x_numListLines)
        {
            return 0;
        }

        ptrdiff_t start = static_cast<ptrdiff_t>(selectedIndex) - UIState::x_listWindowCenterOffset;

        if (start < 0)
        {
            start = 0;
        }

        if (static_cast<size_t>(start) + UIState::x_numListLines > n)
        {
            start = static_cast<ptrdiff_t>(n - UIState::x_numListLines);
        }

        return static_cast<size_t>(start);
    }

    Node* MakeNodeShell(const std::filesystem::path& absolutePath, Node* parent)
    {
        Node* node = new Node();
        node->m_parent = parent;
        node->m_absolutePath = absolutePath;

        std::error_code ec;
        const bool exists = std::filesystem::exists(absolutePath, ec);

        if (!exists || ec)
        {
            node->m_fileName = absolutePath.filename().string();

            if (node->m_fileName.empty())
            {
                node->m_fileName = absolutePath.string();
            }

            node->m_isDirectory = false;
            node->m_isShell = false;
            return node;
        }

        node->m_fileName = absolutePath.filename().string();

        if (node->m_fileName.empty())
        {
            node->m_fileName = absolutePath.string();
        }

        std::error_code ecDir;
        const bool isDirectory = std::filesystem::is_directory(absolutePath, ecDir);

        node->m_isDirectory = isDirectory;
        node->m_isShell = isDirectory;

        return node;
    }

    void BuildSubtree(Node* directoryNode)
    {
        if (!directoryNode || !directoryNode->m_isDirectory)
        {
            return;
        }

        if (!directoryNode->m_contents.empty())
        {
            return;
        }

        std::vector<std::filesystem::directory_entry> entries;

        try
        {
            for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directoryNode->m_absolutePath))
            {
                entries.push_back(entry);
            }
        }
        catch (const std::filesystem::filesystem_error&)
        {
        }

        std::sort(
            entries.begin(),
            entries.end(),
            [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b)
            {
                return a.path().filename() < b.path().filename();
            });

        for (const std::filesystem::directory_entry& entry : entries)
        {
            Node* child = MakeNodeShell(entry.path(), directoryNode);
            directoryNode->m_contents.push_back(child);
        }

        directoryNode->m_isShell = false;
    }

    void MoveSelectionUpDown(int delta)
    {
        const size_t n = m_selectedNode->m_contents.size();

        if (n == 0)
        {
            return;
        }

        if (delta < 0)
        {
            m_selectedIndex = (m_selectedIndex + n - 1) % n;
        }
        else if (delta > 0)
        {
            m_selectedIndex = (m_selectedIndex + 1) % n;
        }
    }

    std::string RelativePathString() const
    {
        if (!m_rootPath || !m_root || !m_selectedNode)
        {
            return "";
        }

        std::error_code ec;
        std::filesystem::path relativePath = std::filesystem::relative(m_selectedNode->m_absolutePath, *m_rootPath, ec);

        if (ec)
        {
            return "";
        }

        std::string s = relativePath.string();

        if (s.empty() || s == ".")
        {
            return "";
        }

        return s;
    }
};
