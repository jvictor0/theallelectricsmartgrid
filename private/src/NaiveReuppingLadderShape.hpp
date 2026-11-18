#pragma once

#include "NaiveReuppingLadder.hpp"
#include "ReuppingLadderShape.hpp"

template<size_t Size>
using NaiveReuppingLadderShapeGeneric = ReuppingLadderShapeGeneric<Size, NaiveReuppingLadderGeneric<float, Size>>;

using TrioNaiveReuppingLadderShape = NaiveReuppingLadderShapeGeneric<3>;
