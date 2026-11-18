#pragma once

#include "ConservativeReuppingLadder.hpp"
#include "ReuppingLadderShape.hpp"

template<size_t Size>
using ConservativeReuppingLadderShapeGeneric = ReuppingLadderShapeGeneric<Size, ConservativeReuppingLadderGeneric<float, Size>>;

using TrioConservativeReuppingLadderShape = ConservativeReuppingLadderShapeGeneric<3>;
