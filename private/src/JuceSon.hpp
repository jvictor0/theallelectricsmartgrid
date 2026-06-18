#pragma once

// JuceSon.hpp — compatibility shim.
//
// Historically this header forked between two heap-allocating JSON adapters.
// Allocating during ToJSON, which runs on the audio thread, is a real-time
// hazard. They are now replaced by a single self-contained, arena-backed library
// with no framework dependency. This header just forwards to it so existing
// `#include "JuceSon.hpp"` sites keep working.
//
#include "Json.hpp"
