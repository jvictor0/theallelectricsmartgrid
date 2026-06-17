#pragma once

// JuceSon.hpp — compatibility shim.
//
// Historically this header forked between a jansson-backed JSON adapter
// (EMBEDDED_BUILD) and a juce::var-backed one (plugin build). Both heap-allocated
// during ToJSON, which runs on the audio thread — a real-time hazard. They are
// now replaced by a single self-contained, arena-backed library that depends on
// neither jansson nor JUCE. This header just forwards to it so existing
// `#include "JuceSon.hpp"` sites keep working.
//
#include "Json.hpp"
