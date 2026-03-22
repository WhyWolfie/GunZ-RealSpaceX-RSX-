#pragma once
#include "stdafx.h"
#include "RBspObject.h"


class MapLoader
{
public:
    static IBspObject * load(const char * name, const char * ext);
};