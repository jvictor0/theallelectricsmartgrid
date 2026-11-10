#pragma once

#include "EncoderBank.hpp"
#include <array>
#include <vector>

// Compact audio schedule derived from the editable encoder tree. Scalar leaf
// depths stay inline; nested depths and gestures use child-before-parent nodes.
// Active target ramps use the existing Compute path, then rejoin the schedule.
//
struct EncoderMatrix
{
    using Cell = SmartGrid::BankedEncoderCell;
    using Sources = Cell::ModulatorValues;
    using OutputCaches = std::array<std::vector<QuadFloat>, SampleTimer::x_oversample>;
    using OutputView = std::array<QuadFloat*, SampleTimer::x_oversample>;

    enum class Path
    {
        Simple,
        Graph,
        Editing
    };

    struct Route
    {
        int m_source;
        float m_left;
        float m_right;
        float m_depth;

        // Settled, unmodulated depths only change when the scene blend moves.
        // Source amplitude remains a live per-sample input.
        //
        void UpdateDepth(float blend)
        {
            float normalized = m_left * (1.0f - blend) + m_right * blend;
            m_depth = Cell::ModulationDepthFromValue(normalized * 2.0f - 1.0f);
        }
    };

    struct Root
    {
        Cell* m_cell;
        Sources* m_sources;
        Cell::GestureValues* m_gestureValues;
        size_t m_index;
        size_t m_start;
        size_t m_count;
        size_t m_revision;
        float m_left;
        float m_right;
        Path m_path;
    };

    static constexpr size_t x_leaf = static_cast<size_t>(-1);

    struct NestedRoute
    {
        Route m_amount;
        size_t m_node;
    };

    struct Gesture
    {
        size_t m_index;
        float m_left;
        float m_right;
        bool m_activeLeft;
        bool m_activeRight;
    };

    struct Node
    {
        size_t m_start;
        size_t m_count;
        float m_left;
        float m_right;
        BitSet16 m_sources;
        BitSet16 m_gestures;
        size_t m_gestureStart;
        size_t m_gestureCount;
        float m_base;
        float m_gestureWeight;
        QuadFloat m_output;
    };

    std::vector<Root> m_roots;
    std::vector<Route> m_routes;
    std::vector<Node> m_nodes;
    std::vector<NestedRoute> m_nestedRoutes;
    std::vector<Gesture> m_gestures;
    size_t m_generation = 0;
    bool m_rebuild = true;
    int m_leftScene = -1;
    int m_rightScene = -1;

    // Readers are bound outside the sample loop and expire on Rebuild. A root
    // can enter the editing path before the next rebuild, so reads follow it.
    //
    struct ValueReader
    {
        Cell* m_cell = nullptr;
        const Root* m_root = nullptr;
        const QuadFloat* m_output = nullptr;

        QuadFloat Read(const SmartGrid::SceneManager& scene) const
        {
            assert(m_cell);
            if (!m_root)
            {
                return *m_output;
            }

            if (m_cell->m_type == SmartGrid::EncoderType::ModulatorAmount && m_cell->m_modulatorsAffecting.IsZero())
            {
                return QuadFloat::Splat(m_cell->GetNeutralNormalizedValue());
            }

            if (m_root->m_path == Path::Editing)
            {
                return m_cell->m_output;
            }

            if (m_output)
            {
                return *m_output;
            }

            // Settled leaf depths and gesture values have no graph node.
            // Their normalized position is the scene blend before depth mapping.
            //
            return QuadFloat::Splat(m_cell->m_values[scene.m_scene1] * (1.0f - scene.m_blendFactor)
                + m_cell->m_values[scene.m_scene2] * scene.m_blendFactor);
        }
    };

    size_t FindNode(const Cell* cell, const Root& root) const
    {
        if (cell == root.m_cell)
        {
            return root.m_start + root.m_count - 1;
        }

        if (cell->m_type != SmartGrid::EncoderType::ModulatorAmount)
        {
            return x_leaf;
        }

        size_t parent = FindNode(cell->m_parent, root);
        if (parent != x_leaf)
        {
            const Node& node = m_nodes[parent];
            for (size_t i = node.m_start; i < node.m_start + node.m_count; ++i)
            {
                const NestedRoute& route = m_nestedRoutes[i];
                if (route.m_amount.m_source == cell->m_index)
                {
                    return route.m_node;
                }
            }
        }

        return x_leaf;
    }

    ValueReader BindValue(Cell* cell) const
    {
        assert(cell);
        if (!cell->m_parent)
        {
            return {cell, nullptr, &cell->m_output};
        }

        const Cell* rootCell = cell;
        while (rootCell->m_parent)
        {
            rootCell = rootCell->m_parent;
        }

        auto root = std::find_if(m_roots.begin(), m_roots.end(),
            [rootCell](const Root& candidate)
            {
                return candidate.m_cell == rootCell;
            });
        assert(root != m_roots.end());
        size_t node = root->m_path == Path::Graph ? FindNode(cell, *root) : x_leaf;
        return {cell, &*root, node == x_leaf ? nullptr : &m_nodes[node].m_output};
    }

    static bool HasInputs(Cell* cell)
    {
        return cell->m_modulators.m_numActiveModulators != 0
            && !cell->GetModulator(0)->m_modulatorsAffecting.IsZero();
    }

    static bool NeedsNode(Cell* cell)
    {
        return HasInputs(cell) || cell->m_modulators.m_numActiveGestures != 0;
    }

    static bool CanPrepare(Cell* cell)
    {
        if (!cell->m_connected || cell->m_samplesForceUpdate != 0)
        {
            return false;
        }

        for (size_t i = 0; i < cell->m_modulators.m_numActiveModulators; ++i)
        {
            Cell* amount = cell->GetModulator(i);
            if (amount->m_modulatorsAffecting.IsZero())
            {
                break;
            }

            if (!CanPrepare(amount))
            {
                return false;
            }
        }

        for (size_t i = 0; i < cell->m_modulators.m_numActiveGestures; ++i)
        {
            if (!CanPrepare(cell->m_modulators.GetGesture(i)))
            {
                return false;
            }
        }

        return true;
    }

    static Route PrepareRoute(Cell* cell, const SmartGrid::SceneManager& scene)
    {
        Route route{cell->m_index, cell->m_values[scene.m_scene1], cell->m_values[scene.m_scene2], 0};
        route.UpdateDepth(scene.m_blendFactor);
        return route;
    }

    size_t PrepareNode(Cell* cell, const SmartGrid::SceneManager& scene)
    {
        std::array<NestedRoute, Cell::x_numModulators> routes;
        size_t count = 0;
        for (size_t i = 0; i < cell->m_modulators.m_numActiveModulators; ++i)
        {
            Cell* amount = cell->GetModulator(i);
            if (amount->m_modulatorsAffecting.IsZero())
            {
                break;
            }

            size_t node = NeedsNode(amount) ? PrepareNode(amount, scene) : x_leaf;
            routes[count++] = {PrepareRoute(amount, scene), node};
        }

        size_t start = m_nestedRoutes.size();
        m_nestedRoutes.insert(m_nestedRoutes.end(), routes.begin(), routes.begin() + count);
        size_t gestureStart = m_gestures.size();
        for (size_t i = 0; i < cell->m_modulators.m_numActiveGestures; ++i)
        {
            Cell* gesture = cell->m_modulators.GetGesture(i);
            m_gestures.push_back({cell->m_modulators.GetGestureIndex(i),
                gesture->m_values[scene.m_scene1], gesture->m_values[scene.m_scene2],
                gesture->m_isActive[scene.m_scene1], gesture->m_isActive[scene.m_scene2]});
        }

        size_t index = m_nodes.size();
        m_nodes.push_back({start, count, cell->m_values[scene.m_scene1], cell->m_values[scene.m_scene2],
            cell->m_modulatorsAffecting, cell->m_gesturesAffecting,
            gestureStart, cell->m_modulators.m_numActiveGestures,
            cell->m_postGestureValue, cell->m_gestureWeightSum, cell->m_output});
        return index;
    }

    void Rebuild(SmartGrid::EncoderPtr* encoders, size_t count, const SmartGrid::SceneManager& scene)
    {
        m_roots.clear();
        m_routes.clear();
        m_nodes.clear();
        m_nestedRoutes.clear();
        m_gestures.clear();
        m_roots.reserve(count);
        m_routes.reserve(count * Cell::x_numModulators);
        m_nodes.reserve(count);
        m_nestedRoutes.reserve(count * Cell::x_numModulators);
        m_gestures.reserve(count);
        for (size_t index = 0; index < count; ++index)
        {
            Cell* cell = encoders[index].get();
            if (!cell || !cell->m_name)
            {
                continue;
            }

            cell->SynchronizePreparedState();
            cell->m_preparedScene1 = scene.m_scene1;
            cell->m_preparedScene2 = scene.m_scene2;
            Root root{cell, cell->m_modulatorValues, cell->m_gestureValues, index,
                m_routes.size(), 0, cell->m_evaluationRevision,
                cell->m_values[scene.m_scene1], cell->m_values[scene.m_scene2],
                CanPrepare(cell) ? Path::Simple : Path::Editing};
            if (root.m_path == Path::Simple && cell->m_modulators.m_numActiveGestures != 0)
            {
                root.m_path = Path::Graph;
            }

            if (root.m_path == Path::Simple)
            {
                for (size_t i = 0; i < cell->m_modulators.m_numActiveModulators; ++i)
                {
                    Cell* amount = cell->GetModulator(i);
                    if (amount->m_modulatorsAffecting.IsZero())
                    {
                        break;
                    }

                    if (NeedsNode(amount))
                    {
                        root.m_path = Path::Graph;
                        break;
                    }
                }
            }

            if (root.m_path == Path::Simple)
            {
                for (size_t i = 0; i < cell->m_modulators.m_numActiveModulators; ++i)
                {
                    Cell* amount = cell->GetModulator(i);
                    if (amount->m_modulatorsAffecting.IsZero())
                    {
                        break;
                    }

                    m_routes.push_back(PrepareRoute(amount, scene));
                    ++root.m_count;
                }
            }

            if (root.m_path == Path::Graph)
            {
                root.m_start = m_nodes.size();
                PrepareNode(cell, scene);
                root.m_count = m_nodes.size() - root.m_start;
            }

            m_roots.push_back(root);
        }

        m_leftScene = scene.m_scene1;
        m_rightScene = scene.m_scene2;
        m_rebuild = false;
        ++m_generation;
    }

    void ProcessGraph(const Root& root, float blend, size_t host)
    {
        const float leftWeight = 1.0f - blend;
        const QuadFloat one = QuadFloat::Splat(1.0f);
        const size_t end = root.m_start + root.m_count;
        for (size_t i = root.m_start; i < end; ++i)
        {
            Node& node = m_nodes[i];
            bool gestureChanged = !node.m_gestures.Intersect(root.m_gestureValues->m_changedGestures).IsZero();
            if (!root.m_sources->m_blendChanged && !gestureChanged
                && node.m_sources.Intersect(root.m_sources->m_changedModulators).IsZero())
            {
                continue;
            }

            if (root.m_sources->m_blendChanged || gestureChanged)
            {
                float base = node.m_left * leftWeight + node.m_right * blend;
                double weightSum = 0;
                double valueSum = 0;
                for (size_t g = node.m_gestureStart; g < node.m_gestureStart + node.m_gestureCount; ++g)
                {
                    const Gesture& gesture = m_gestures[g];
                    float input = root.m_gestureValues->m_gestureWeights[gesture.m_index];
                    float left = gesture.m_activeLeft ? input : 0;
                    float right = gesture.m_activeRight ? input : 0;
                    double w = left * leftWeight + right * blend;
                    float value = gesture.m_left * leftWeight + gesture.m_right * blend;
                    weightSum += w;
                    valueSum += w * (base * (1 - w) + static_cast<double>(value) * w);
                }

                node.m_base = weightSum > 0 ? static_cast<float>(valueSum / weightSum) : base;
                node.m_gestureWeight = static_cast<float>(weightSum);
            }

            QuadFloat sum;
            QuadFloat weight;
            QuadFloat offset;
            for (size_t r = node.m_start; r < node.m_start + node.m_count; ++r)
            {
                NestedRoute& route = m_nestedRoutes[r];
                QuadFloat depth;
                if (route.m_node == x_leaf)
                {
                    if (root.m_sources->m_blendChanged)
                    {
                        route.m_amount.UpdateDepth(blend);
                    }

                    depth = QuadFloat::Splat(route.m_amount.m_depth);
                }
                else
                {
                    depth = Cell::ModulationDepthFromValue(m_nodes[route.m_node].m_output * 2.0f - 1.0f);
                }

                QuadFloat w = depth * root.m_sources->m_amplitude[route.m_amount.m_source];
                sum += w * root.m_sources->m_hostValue[route.m_amount.m_source];
                weight += w.Abs();
                offset += QuadFloat::Max(QuadFloat(), -w);
            }

            QuadFloat baseValue = QuadFloat::Splat(node.m_base) * QuadFloat::Max(QuadFloat(), one - weight);
            node.m_output = baseValue + (sum + offset) / QuadFloat::Max(one, weight);
            if (i + 1 == end)
            {
                root.m_cell->SetOutput(node.m_output, host);
                root.m_cell->m_minValue = baseValue;
                root.m_cell->m_maxValue = baseValue + QuadFloat::Min(one, weight);
                root.m_cell->m_gestureWeightSum = node.m_gestureWeight;
                root.m_cell->m_brightness = std::clamp(1.0f - weight[0], 0.0f, 1.0f);
            }
        }
    }

    static void Publish(const Root& root, const OutputView& caches, size_t host)
    {
        Cell* cell = root.m_cell;
        // Clamp only published roots, before interpolation and bipolar mapping.
        //
        cell->m_output = QuadFloat::Min(QuadFloat::Splat(1), QuadFloat::Max(QuadFloat(), cell->m_output));
        size_t index = root.m_index;
        if (cell->m_outputHostSample != host)
        {
            QuadFloat value = cell->GetQuadValue();
            for (auto& cache : caches)
            {
                cache[index] = value;
            }

            return;
        }

        QuadFloat output = cell->m_output;
        QuadFloat delta = cell->m_outputPrev - output;
        bool bipolar = cell->m_bipolar;
        for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
        {
            QuadFloat value = output + delta * Cell::GetPreviousOutputWeight(sample, SampleTimer::x_oversample);
            caches[sample][index] = bipolar ? value * 2.0f - 1.0f : value;
        }
    }

    // The bank broadcasts blend changes to every mode. Curve four independent
    // signed positions together, preserving the scalar scene-blend expression
    // and the original order of each root's modulation sums.
    //
    void UpdateDepths(float blend)
    {
        size_t i = 0;
        for (; i + 4 <= m_routes.size(); i += 4)
        {
            QuadFloat position;
            for (size_t lane = 0; lane < 4; ++lane)
            {
                const Route& route = m_routes[i + lane];
                float normalized = route.m_left * (1.0f - blend) + route.m_right * blend;
                position[lane] = normalized * 2.0f - 1.0f;
            }

            QuadFloat depth = Cell::ModulationDepthFromValue(position);
            for (size_t lane = 0; lane < 4; ++lane)
            {
                m_routes[i + lane].m_depth = depth[lane];
            }
        }

        for (; i < m_routes.size(); ++i)
        {
            m_routes[i].UpdateDepth(blend);
        }
    }

    void Process(SmartGrid::EncoderPtr* encoders, size_t count,
        const SmartGrid::SceneManager& scene, OutputCaches& caches)
    {
        if (m_rebuild || m_leftScene != scene.m_scene1 || m_rightScene != scene.m_scene2)
        {
            Rebuild(encoders, count, scene);
        }

        const float blend = scene.m_blendFactor;
        const float leftWeight = 1.0f - blend;
        const size_t host = SampleTimer::GetHostSample();
        const QuadFloat one = QuadFloat::Splat(1.0f);
        bool depthsUpdated = false;

        // Buffer storage stays fixed during processing. Capture its pointers
        // once so publication writes do not reload vector metadata.
        //
        OutputView outputs;
        for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
        {
            outputs[sample] = caches[sample].data();
        }

        for (auto& root : m_roots)
        {
            Cell* cell = root.m_cell;
            bool invalid = cell->m_samplesForceUpdate != 0 || root.m_revision != cell->m_evaluationRevision;
            if (invalid || root.m_path == Path::Editing || !cell->m_connected)
            {
                cell->SynchronizePreparedState();
                root.m_path = Path::Editing;
                cell->Compute(true);
                if (cell->m_samplesForceUpdate == 0 && CanPrepare(cell))
                {
                    m_rebuild = true;
                }
            }
            else if (root.m_sources->m_blendChanged
                || !cell->m_modulatorsAffecting.Intersect(root.m_sources->m_changedModulators).IsZero()
                || !cell->m_gesturesAffecting.Intersect(root.m_gestureValues->m_changedGestures).IsZero())
            {
                cell->m_preparedValues = true;
                cell->m_preparedBlend = blend;
                if (root.m_path == Path::Graph)
                {
                    ProcessGraph(root, blend, host);
                    Publish(root, outputs, host);
                    continue;
                }

                if (root.m_sources->m_blendChanged && !depthsUpdated)
                {
                    UpdateDepths(blend);
                    depthsUpdated = true;
                }

                QuadFloat sum;
                QuadFloat weight;
                QuadFloat offset;
                for (size_t i = root.m_start; i < root.m_start + root.m_count; ++i)
                {
                    Route& route = m_routes[i];
                    QuadFloat depth = QuadFloat::Splat(route.m_depth);
                    QuadFloat w = depth * root.m_sources->m_amplitude[route.m_source];
                    sum += w * root.m_sources->m_hostValue[route.m_source];
                    weight += w.Abs();
                    offset += QuadFloat::Max(QuadFloat(), -w);
                }

                float base = root.m_left * leftWeight + root.m_right * blend;
                QuadFloat baseValue = QuadFloat::Splat(base) * QuadFloat::Max(QuadFloat(), one - weight);
                cell->SetOutput(baseValue + (sum + offset) / QuadFloat::Max(one, weight), host);
                cell->m_minValue = baseValue;
                cell->m_maxValue = baseValue + QuadFloat::Min(one, weight);
                cell->m_brightness = std::clamp(1.0f - weight[0], 0.0f, 1.0f);
            }

            Publish(root, outputs, host);
        }
    }
};
