#pragma once

#include <algorithm>

#include "EncoderBank.hpp"

namespace SmartGrid
{

// User edits applied as many small increments, one per sample, so a stream of
// encoder ticks moves the value continuously. Each tick's delta is spread over the
// time since that cell's previous tick, clamped to 2..50 ms.
//
// A listed cell's m_editSlot points at its entry's m_cell. A cell that is destroyed
// or cancels its edit nulls that pointer and the entry is dropped on the next pass;
// the list clears m_editSlot when it releases an entry or is destroyed.
//
struct EncoderEditList
{
    static constexpr size_t x_maxEdits = 64;
    static constexpr size_t x_minRampSamples = 96;
    static constexpr size_t x_maxRampSamples = 2400;

    enum class Kind
    {
        Relative,
        Absolute
    };

    struct Edit
    {
        BankedEncoderCell* m_cell;
        Kind m_kind;
        float m_amount;
        size_t m_samplesLeft;
    };

    Edit m_edits[x_maxEdits];
    size_t m_numEdits;
    size_t m_sample;

    // Scene pair and blend seen by the previous Process, which pending edits target.
    //
    size_t m_scene1;
    size_t m_scene2;
    float m_blendFactor;

    EncoderEditList()
        : m_edits{}
        , m_numEdits(0)
        , m_sample(0)
        , m_scene1(0)
        , m_scene2(1)
        , m_blendFactor(0)
    {
    }

    ~EncoderEditList()
    {
        Clear();
    }

    void Clear()
    {
        for (size_t i = 0; i < m_numEdits; ++i)
        {
            if (m_edits[i].m_cell)
            {
                m_edits[i].m_cell->m_editSlot = nullptr;
            }

            m_edits[i].m_cell = nullptr;
        }

        m_numEdits = 0;
    }

    Edit* Find(BankedEncoderCell* cell)
    {
        for (size_t i = 0; i < m_numEdits; ++i)
        {
            if (m_edits[i].m_cell == cell)
            {
                return &m_edits[i];
            }
        }

        return nullptr;
    }

    // Returns the cell's edit, reset to kind if it was another kind, or nullptr when
    // the list is full.
    //
    Edit* Acquire(BankedEncoderCell* cell, Kind kind)
    {
        Edit* edit = Find(cell);
        if (!edit)
        {
            if (m_numEdits == x_maxEdits)
            {
                return nullptr;
            }

            edit = &m_edits[m_numEdits];
            ++m_numEdits;
            edit->m_cell = cell;
            edit->m_kind = kind;
            edit->m_amount = 0;
            cell->m_editSlot = &edit->m_cell;
        }
        else if (edit->m_kind != kind)
        {
            edit->m_kind = kind;
            edit->m_amount = 0;
        }

        size_t elapsed = m_sample - cell->m_lastEditSample;
        cell->m_lastEditSample = m_sample;
        edit->m_samplesLeft = std::max(x_minRampSamples, std::min(x_maxRampSamples, elapsed));
        return edit;
    }

    void AddRelative(BankedEncoderCell* cell, float delta)
    {
        Edit* edit = Acquire(cell, Kind::Relative);
        if (!edit)
        {
            cell->Increment(delta);
            return;
        }

        edit->m_amount += delta;
    }

    void AddAbsolute(BankedEncoderCell* cell, float target)
    {
        Edit* edit = Acquire(cell, Kind::Absolute);
        if (!edit)
        {
            SetTo(cell, target);
            return;
        }

        edit->m_amount = target;
    }

    void Remove(BankedEncoderCell* cell)
    {
        for (size_t i = 0; i < m_numEdits; ++i)
        {
            if (m_edits[i].m_cell == cell)
            {
                RemoveAt(i);
                return;
            }
        }
    }

    void RemoveAt(size_t i)
    {
        if (m_edits[i].m_cell)
        {
            m_edits[i].m_cell->m_editSlot = nullptr;
        }

        --m_numEdits;
        if (i != m_numEdits)
        {
            m_edits[i] = m_edits[m_numEdits];
            if (m_edits[i].m_cell)
            {
                m_edits[i].m_cell->m_editSlot = &m_edits[i].m_cell;
            }
        }

        m_edits[m_numEdits].m_cell = nullptr;
    }

    static void SetTo(BankedEncoderCell* cell, float value)
    {
        cell->SetToValue(value);
        cell->SetForceUpdateRecursive();
        cell->SetModulatorsAffectingRecursive();
    }

    // Pending edits were aimed at the previous scene pair. If the pair changed, land
    // them there in full before ramping anything into the new scenes.
    //
    void FlushIntoPreviousScenes(SceneManager& sceneManager)
    {
        size_t scene1 = sceneManager.m_scene1;
        size_t scene2 = sceneManager.m_scene2;
        float blendFactor = sceneManager.m_blendFactor;
        sceneManager.m_scene1 = m_scene1;
        sceneManager.m_scene2 = m_scene2;
        sceneManager.m_blendFactor = m_blendFactor;
        Flush();
        sceneManager.m_scene1 = scene1;
        sceneManager.m_scene2 = scene2;
        sceneManager.m_blendFactor = blendFactor;
    }

    void Process(SceneManager& sceneManager)
    {
        if (m_numEdits > 0 && (sceneManager.m_scene1 != m_scene1 || sceneManager.m_scene2 != m_scene2))
        {
            FlushIntoPreviousScenes(sceneManager);
        }

        m_scene1 = sceneManager.m_scene1;
        m_scene2 = sceneManager.m_scene2;
        m_blendFactor = sceneManager.m_blendFactor;

        ++m_sample;
        for (size_t i = m_numEdits; i > 0; --i)
        {
            Edit& edit = m_edits[i - 1];
            if (!edit.m_cell)
            {
                RemoveAt(i - 1);
                continue;
            }

            float fraction = 1.0f / static_cast<float>(edit.m_samplesLeft);
            if (edit.m_kind == Kind::Relative)
            {
                float step = edit.m_amount * fraction;
                edit.m_amount -= step;
                edit.m_cell->Increment(step);
            }
            else
            {
                float current = edit.m_cell->GetSceneNormalizedValue();
                SetTo(edit.m_cell, current + (edit.m_amount - current) * fraction);
            }

            --edit.m_samplesLeft;
            if (edit.m_samplesLeft == 0)
            {
                RemoveAt(i - 1);
            }
        }
    }

    // Applies every pending remainder at once.
    //
    void Flush()
    {
        for (size_t i = 0; i < m_numEdits; ++i)
        {
            Edit& edit = m_edits[i];
            if (!edit.m_cell)
            {
                continue;
            }

            if (edit.m_kind == Kind::Relative)
            {
                if (edit.m_amount != 0)
                {
                    edit.m_cell->Increment(edit.m_amount);
                }
            }
            else
            {
                SetTo(edit.m_cell, edit.m_amount);
            }
        }

        Clear();
    }
};

}
