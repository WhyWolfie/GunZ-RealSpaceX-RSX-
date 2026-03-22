#pragma once
#include "stdafx.h"

std::string& fixSlash(std::string& str);

bool isAnySlash(char ch)
{
    return (ch == '/' || ch == '\\');
}

bool isSlash(char ch)
{
    return (ch == '/');
}

std::string fixDirectory(const char * dir)
{
    std::string ret = dir;
    fixSlash(ret);
    uint size = ret.size();
    if (!size) ret = "";
    else if (size == 1 && ret[0] == '/') ret = "";
    else if (ret[size-1] != '/') ret.append("/");
    return ret;
}

std::string& fixSlash(std::string& str)
{
    char buf[256];
    char * ptr = buf;
    uint size = str.size();
    for (int i = 0; i < size; ++i)
    {
        if (isAnySlash(str[i]))
        {
            *ptr++ = '/';
            while ((i+1 < size) && isAnySlash(str[i+1]))
                ++i;
        } else
            *ptr++ = str[i];
    }
    *ptr++ = 0;
    str = buf;
    return str;
}

char * toLower(char * str)
{
    char * ptr = str;
    if (str == nullptr) return nullptr;
    char dif = 'A' - 'a';
    while (* ptr != 0)
    {
        if (* ptr>= 'A' && * ptr <= 'Z')
            * ptr -= dif;
        ++ ptr;
    }
    return str;
}

std::string& toLower(std::string& ret)
{
    char dif = 'A' - 'a';
    std::string::iterator itor = ret.begin(), end = ret.end();;
    while (itor != end)
    {
        if (*itor >= 'A' && *itor <= 'Z')
            *itor -= dif;
        ++itor;
    }
    return ret;
}