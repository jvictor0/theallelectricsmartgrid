#include "doctest.h"
#include "SmartGridOneLayout.hpp"

using Layout = SmartGridOneLayout;

DOCTEST_TEST_CASE("encoder layout: voice presence determines page and slot instances")
{
    DOCTEST_CHECK(Layout::GetPageInstance(Layout::Page::PanAndMelody, 0) != Layout::GetPageInstance(Layout::Page::PanAndMelody, 1));
    DOCTEST_CHECK(Layout::GetPageInstance(Layout::Page::Delay, 0) == Layout::GetPageInstance(Layout::Page::Delay, 2));
    DOCTEST_CHECK(Layout::GetSlotInstance(Layout::Slot::PanningAndSequencing, 0) != Layout::GetSlotInstance(Layout::Slot::PanningAndSequencing, 2));
    DOCTEST_CHECK(Layout::GetPage(Layout::Slot::Source, VoiceMachine::SourceMachine::Sample) == Layout::Page::SampleSource);
    DOCTEST_CHECK(Layout::GetPage(Layout::Slot::PanningAndSequencing, SingletonMachine::Unit) == Layout::Page::PanAndMelody);
    DOCTEST_CHECK(Layout::GetParamMode(Layout::Param::Harmonics1) == Layout::ParamMode::Voice);
    DOCTEST_CHECK(Layout::GetParamMode(Layout::Param::DelayTime) == Layout::ParamMode::Quad);
    DOCTEST_CHECK(Layout::GetParamMode(Layout::Param::Tempo) == Layout::ParamMode::Global);

    auto modes = Layout::x_paramModes;
    for (auto& mode : modes)
    {
        mode = Layout::ParamMode::Global;
    }

    modes[static_cast<size_t>(Layout::Param::Harmonics1)] = Layout::ParamMode::Voice;
    auto tables = Layout::BuildPageAndSlotTables(modes, Layout::x_pageMembers, Layout::x_slotRoutes);
    DOCTEST_CHECK(tables.m_valid);
    DOCTEST_CHECK(tables.m_slots[static_cast<size_t>(Layout::Slot::Source)].m_mode == Layout::ParamMode::Voice);
    DOCTEST_CHECK(tables.m_slots[static_cast<size_t>(Layout::Slot::Delay)].m_mode == Layout::ParamMode::Global);
    modes[static_cast<size_t>(Layout::Param::DelayTime)] = Layout::ParamMode::Quad;
    auto quadTables = Layout::BuildPageAndSlotTables(modes, Layout::x_pageMembers, Layout::x_slotRoutes);
    DOCTEST_CHECK(quadTables.m_slots[static_cast<size_t>(Layout::Slot::Delay)].m_mode == Layout::ParamMode::Quad);
    DOCTEST_CHECK(tables.m_pages[static_cast<size_t>(Layout::Page::DualVCOSource)].m_numInstances == 3);
    DOCTEST_CHECK(tables.m_pages[static_cast<size_t>(Layout::Page::SampleSource)].m_numInstances == 1);
    DOCTEST_CHECK(tables.m_slots[static_cast<size_t>(Layout::Slot::Source)].m_numInstances == 3);
    DOCTEST_CHECK(tables.m_slots[static_cast<size_t>(Layout::Slot::PanningAndSequencing)].m_numInstances == 1);
}

DOCTEST_TEST_CASE("encoder layout: malformed descriptors cannot produce valid routing")
{
    DOCTEST_SUBCASE("duplicate coordinate")
    {
        auto members = Layout::x_pageMembers;
        members[1] = members[0];
        DOCTEST_CHECK_FALSE(Layout::BuildPageAndSlotTables(Layout::x_paramModes, members, Layout::x_slotRoutes).m_valid);
    }

    DOCTEST_SUBCASE("coordinate outside the grid")
    {
        auto members = Layout::x_pageMembers;
        members[0].m_x = 4;
        DOCTEST_CHECK_FALSE(Layout::BuildPageAndSlotTables(Layout::x_paramModes, members, Layout::x_slotRoutes).m_valid);
    }

    DOCTEST_SUBCASE("unknown parameter")
    {
        auto members = Layout::x_pageMembers;
        members[0].m_param = static_cast<Layout::Param>(Layout::x_numParams);
        DOCTEST_CHECK_FALSE(Layout::BuildPageAndSlotTables(Layout::x_paramModes, members, Layout::x_slotRoutes).m_valid);
    }

    DOCTEST_SUBCASE("unknown page")
    {
        auto routes = Layout::x_slotRoutes;
        routes[0].m_page = Layout::Page::NumPages;
        DOCTEST_CHECK_FALSE(Layout::BuildPageAndSlotTables(Layout::x_paramModes, Layout::x_pageMembers, routes).m_valid);
    }

    DOCTEST_SUBCASE("mixed enum types")
    {
        auto routes = Layout::x_slotRoutes;
        routes[0] = Layout::SlotRoute(Layout::Slot::Source, SingletonMachine::Unit, Layout::Page::DualVCOSource);
        DOCTEST_CHECK_FALSE(Layout::BuildPageAndSlotTables(Layout::x_paramModes, Layout::x_pageMembers, routes).m_valid);
    }

    DOCTEST_SUBCASE("duplicate and missing machine value")
    {
        auto routes = Layout::x_slotRoutes;
        routes[1] = routes[0];
        DOCTEST_CHECK_FALSE(Layout::BuildPageAndSlotTables(Layout::x_paramModes, Layout::x_pageMembers, routes).m_valid);
    }

    DOCTEST_SUBCASE("machine value outside enum")
    {
        auto routes = Layout::x_slotRoutes;
        routes[0] = Layout::SlotRoute(Layout::Slot::Source, VoiceMachine::SourceMachine::NumSourceMachines, Layout::Page::DualVCOSource);
        DOCTEST_CHECK_FALSE(Layout::BuildPageAndSlotTables(Layout::x_paramModes, Layout::x_pageMembers, routes).m_valid);
    }

    DOCTEST_SUBCASE("unplaced parameter")
    {
        auto members = Layout::x_pageMembers;
        members.back().m_param = members[members.size() - 2].m_param;
        DOCTEST_CHECK_FALSE(Layout::BuildPageAndSlotTables(Layout::x_paramModes, members, Layout::x_slotRoutes).m_valid);
    }
}

DOCTEST_TEST_CASE("encoder layout: a parameter can appear on multiple routed pages")
{
    std::array<Layout::PageMember, Layout::x_pageMembers.size() + 1> members{};
    std::copy(Layout::x_pageMembers.begin(), Layout::x_pageMembers.end(), members.begin());
    members.back() = {Layout::Page::ThruSource, Layout::Param::DelayTime, 0, 0};
    DOCTEST_CHECK(Layout::BuildPageAndSlotTables(Layout::x_paramModes, members, Layout::x_slotRoutes).m_valid);
}
