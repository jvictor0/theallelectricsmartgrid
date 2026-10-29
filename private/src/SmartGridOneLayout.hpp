#pragma once

#include "VoiceMachineEnums.hpp"
#include <algorithm>
#include <array>
#include <cstddef>

enum class SingletonMachine
{
    Unit = 0,
    NumMachines = 1
};

struct SmartGridOneLayout
{
    static constexpr size_t x_numTrios = 3;
    static constexpr size_t x_gridSize = 16;
    static constexpr size_t x_noRoute = static_cast<size_t>(-1);

    enum class ParamMode
    {
        Voice,
        Quad,
        Global
    };

    enum class Param
    {
#define F(name, shortName, mode, defaultValue, description, color, switchValues, bipolar) name,
#include "ForEachSmartGridOneParam.hpp"
#undef F
        NumParams
    };

    enum class Page
    {
        DualVCOSource,
        ThruSource,
        PhysicalModelingSource,
        SampleSource,
        Shape,
        PanAndMelody,
        VoiceLFOs,
        Delay,
        Reverb,
        PartialMachine,
        QuadLFOs,
        TheoryOfTime,
        Mastering,
        Inputs,
        DeepVocoder,
        FXMatrix,
        Sub,
        NumPages
    };

    enum class Slot
    {
        Source,
        Shape,
        PanningAndSequencing,
        VoiceLFOs,
        Delay,
        Reverb,
        PartialMachine,
        QuadLFOs,
        TheoryOfTime,
        Mastering,
        Inputs,
        DeepVocoder,
        FXMatrix,
        Sub,
        NumSlots
    };

    enum class MachineType
    {
        Singleton,
        Source,
        Filter,
        NumTypes
    };

    static constexpr size_t x_numParams = static_cast<size_t>(Param::NumParams);
    static constexpr size_t x_numPages = static_cast<size_t>(Page::NumPages);
    static constexpr size_t x_numSlots = static_cast<size_t>(Slot::NumSlots);
    static constexpr size_t x_numMachineTypes = static_cast<size_t>(MachineType::NumTypes);
    static constexpr size_t x_maxMachines = std::max(
        static_cast<size_t>(VoiceMachine::SourceMachine::NumSourceMachines),
        static_cast<size_t>(VoiceMachine::FilterMachine::NumFilterMachines));

    static constexpr MachineType GetMachineType(SingletonMachine)
    {
        return MachineType::Singleton;
    }

    static constexpr MachineType GetMachineType(VoiceMachine::SourceMachine)
    {
        return MachineType::Source;
    }

    static constexpr MachineType GetMachineType(VoiceMachine::FilterMachine)
    {
        return MachineType::Filter;
    }

    static constexpr size_t GetMachineCount(MachineType type)
    {
        switch (type)
        {
            case MachineType::Singleton: return static_cast<size_t>(SingletonMachine::NumMachines);
            case MachineType::Source: return static_cast<size_t>(VoiceMachine::SourceMachine::NumSourceMachines);
            case MachineType::Filter: return static_cast<size_t>(VoiceMachine::FilterMachine::NumFilterMachines);
            default: return 0;
        }
    }

    struct PageMember
    {
        Page m_page;
        Param m_param;
        int m_x;
        int m_y;
    };

    struct SlotRoute
    {
        Slot m_slot;
        MachineType m_machineType;
        size_t m_machineValue;
        Page m_page;

        template <typename Machine>
        constexpr SlotRoute(Slot slot, Machine machine, Page page)
            : m_slot(slot)
            , m_machineType(GetMachineType(machine))
            , m_machineValue(static_cast<size_t>(machine))
            , m_page(page)
        {
        }
    };

    inline static constexpr std::array x_paramModes =
    {
#define F(name, shortName, mode, defaultValue, description, color, switchValues, bipolar) ParamMode::mode,
#include "ForEachSmartGridOneParam.hpp"
#undef F
    };

    inline static constexpr std::array x_pageMembers =
    {
#define F(page, param, x, y) PageMember{Page::page, Param::param, x, y},
#include "ForEachSmartGridOnePage.hpp"
#undef F
    };

    static constexpr size_t x_numSlotRoutes =
        0
#define F(slot, enumtype, enumval, page, color) + 1
#include "ForEachSmartGridOneSlot.hpp"
#undef F
        ;

    static const std::array<SlotRoute, x_numSlotRoutes> x_slotRoutes;

    struct PageTable
    {
        std::array<Param, x_gridSize> m_grid;
        size_t m_firstInstance;
        size_t m_numInstances;
    };

    struct SlotTable
    {
        ParamMode m_mode;
        MachineType m_machineType;
        std::array<size_t, x_maxMachines> m_routes;
        size_t m_firstInstance;
        size_t m_numInstances;
    };

    struct SlotInstance
    {
        Slot m_slot;
        size_t m_trio;
    };

    struct AffectedSlots
    {
        std::array<Slot, x_numSlots> m_slots;
        size_t m_count;
    };

    struct Tables
    {
        bool m_valid;
        std::array<PageTable, x_numPages> m_pages;
        std::array<SlotTable, x_numSlots> m_slots;
        std::array<SlotInstance, x_numSlots * x_numTrios> m_slotInstances;
        std::array<AffectedSlots, x_numMachineTypes> m_affectedSlots;
        size_t m_numPageInstances;
        size_t m_numSlotInstances;
    };

    template <size_t NumMembers, size_t NumRoutes>
    static constexpr Tables BuildPageAndSlotTables(
        const std::array<ParamMode, x_numParams>& modes,
        const std::array<PageMember, NumMembers>& members,
        const std::array<SlotRoute, NumRoutes>& routes)
    {
        Tables tables{};
        tables.m_valid = true;
        std::array<bool, x_numParams> placedParams{};
        std::array<ParamMode, x_numPages> pageModes{};
        for (auto& mode : pageModes)
        {
            mode = ParamMode::Global;
        }

        for (auto& page : tables.m_pages)
        {
            for (auto& param : page.m_grid)
            {
                param = Param::NumParams;
            }

            page.m_numInstances = 1;
        }

        for (auto& slot : tables.m_slots)
        {
            slot.m_mode = ParamMode::Global;
            slot.m_machineType = MachineType::NumTypes;
            slot.m_numInstances = 1;
            for (auto& route : slot.m_routes)
            {
                route = x_noRoute;
            }
        }

        for (const auto& member : members)
        {
            size_t pageIx = static_cast<size_t>(member.m_page);
            size_t paramIx = static_cast<size_t>(member.m_param);
            if (pageIx >= x_numPages || paramIx >= x_numParams ||
                member.m_x < 0 || member.m_x >= 4 || member.m_y < 0 || member.m_y >= 4)
            {
                tables.m_valid = false;
                continue;
            }

            auto& page = tables.m_pages[pageIx];
            auto& position = page.m_grid[member.m_y * 4 + member.m_x];
            if (position != Param::NumParams)
            {
                tables.m_valid = false;
            }

            position = member.m_param;
            pageModes[pageIx] = std::min(pageModes[pageIx], modes[paramIx]);
            if (modes[paramIx] == ParamMode::Voice)
            {
                page.m_numInstances = x_numTrios;
            }
        }

        for (auto& page : tables.m_pages)
        {
            page.m_firstInstance = tables.m_numPageInstances;
            tables.m_numPageInstances += page.m_numInstances;
        }

        for (size_t row = 0; row < routes.size(); ++row)
        {
            const auto& route = routes[row];
            size_t slotIx = static_cast<size_t>(route.m_slot);
            size_t pageIx = static_cast<size_t>(route.m_page);
            size_t machineCount = GetMachineCount(route.m_machineType);
            if (slotIx >= x_numSlots || pageIx >= x_numPages ||
                route.m_machineValue >= machineCount || machineCount > x_maxMachines)
            {
                tables.m_valid = false;
                continue;
            }

            auto& slot = tables.m_slots[slotIx];
            if ((slot.m_machineType != MachineType::NumTypes && slot.m_machineType != route.m_machineType) ||
                slot.m_routes[route.m_machineValue] != x_noRoute)
            {
                tables.m_valid = false;
            }

            slot.m_machineType = route.m_machineType;
            slot.m_routes[route.m_machineValue] = row;
            const auto& page = tables.m_pages[pageIx];
            slot.m_mode = std::min(slot.m_mode, pageModes[pageIx]);
            if (page.m_numInstances == x_numTrios)
            {
                slot.m_numInstances = x_numTrios;
            }

            for (Param param : page.m_grid)
            {
                if (param == Param::NumParams)
                {
                    continue;
                }

                placedParams[static_cast<size_t>(param)] = true;
            }
        }

        for (size_t slotIx = 0; slotIx < x_numSlots; ++slotIx)
        {
            auto& slot = tables.m_slots[slotIx];
            size_t machineCount = GetMachineCount(slot.m_machineType);
            if (machineCount == 0)
            {
                tables.m_valid = false;
                continue;
            }

            for (size_t machine = 0; machine < machineCount; ++machine)
            {
                if (slot.m_routes[machine] == x_noRoute)
                {
                    tables.m_valid = false;
                }
            }

            slot.m_firstInstance = tables.m_numSlotInstances;
            for (size_t trio = 0; trio < slot.m_numInstances; ++trio)
            {
                tables.m_slotInstances[tables.m_numSlotInstances++] = {static_cast<Slot>(slotIx), trio};
            }

            auto& affected = tables.m_affectedSlots[static_cast<size_t>(slot.m_machineType)];
            affected.m_slots[affected.m_count++] = static_cast<Slot>(slotIx);
        }

        for (bool placed : placedParams)
        {
            if (!placed)
            {
                tables.m_valid = false;
            }
        }

        return tables;
    }

    static const Tables x_tables;

    static constexpr ParamMode GetParamMode(Param param)
    {
        return x_paramModes[static_cast<size_t>(param)];
    }

    static constexpr size_t GetPageInstance(Page page, size_t trio);
    static constexpr size_t GetSlotInstance(Slot slot, size_t trio);

    template <typename Machine>
    static constexpr size_t GetRoute(Slot slot, Machine machine);

    template <typename Machine>
    static constexpr Page GetPage(Slot slot, Machine machine);
};

inline constexpr std::array<SmartGridOneLayout::SlotRoute, SmartGridOneLayout::x_numSlotRoutes>
    SmartGridOneLayout::x_slotRoutes =
{{
#define F(slot, enumtype, enumval, page, color) SlotRoute{Slot::slot, enumtype::enumval, Page::page},
#include "ForEachSmartGridOneSlot.hpp"
#undef F
}};

inline constexpr SmartGridOneLayout::Tables SmartGridOneLayout::x_tables =
    SmartGridOneLayout::BuildPageAndSlotTables(x_paramModes, x_pageMembers, x_slotRoutes);

static_assert(SmartGridOneLayout::x_tables.m_valid, "Invalid SmartGridOne page or slot catalogue");

constexpr size_t SmartGridOneLayout::GetPageInstance(Page page, size_t trio)
{
    const auto& table = x_tables.m_pages[static_cast<size_t>(page)];
    return table.m_firstInstance + (table.m_numInstances == x_numTrios ? trio : 0);
}

constexpr size_t SmartGridOneLayout::GetSlotInstance(Slot slot, size_t trio)
{
    const auto& table = x_tables.m_slots[static_cast<size_t>(slot)];
    return table.m_firstInstance + (table.m_numInstances == x_numTrios ? trio : 0);
}

template <typename Machine>
constexpr size_t SmartGridOneLayout::GetRoute(Slot slot, Machine machine)
{
    const auto& table = x_tables.m_slots[static_cast<size_t>(slot)];
    size_t value = static_cast<size_t>(machine);
    return table.m_machineType == GetMachineType(machine) && value < GetMachineCount(table.m_machineType)
        ? table.m_routes[value] : x_noRoute;
}

template <typename Machine>
constexpr SmartGridOneLayout::Page SmartGridOneLayout::GetPage(Slot slot, Machine machine)
{
    size_t route = GetRoute(slot, machine);
    return route != x_noRoute ? x_slotRoutes[route].m_page : Page::NumPages;
}
