#pragma once

#define IOS_BUILD
#include "TheNonagon.hpp"

struct NonagonHolder
{
    TheNonagonSmartGrid m_nonagon;
    NonagonHolder() 
    {
    }
    
    ~NonagonHolder() 
    { 
    }
};  