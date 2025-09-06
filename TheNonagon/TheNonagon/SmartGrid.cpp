// Suppress integer precision warnings from private source files
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshorten-64-to-32"

#include "SmartGrid.hpp"

#pragma GCC diagnostic pop

namespace SmartGrid
{

GridIdAllocator g_gridIds;
SmartBusHolder g_smartBus;

}
