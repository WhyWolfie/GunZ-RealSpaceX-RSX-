#include "stdafx.h"
#include <windows.h>

#ifdef _DEBUG
#   define TIMER_DECLARE(x) Timer x;
#   define TIMER_BEGIN(x) {(x).begin();}
#   define TIMER_END(x) {(x).end();}
#   define TIMER_RESET(x) {(x).reset();}
#else
#   define TIMER_DECLARE(x)
#   define TIMER_BEGIN(x)
#   define TIMER_END(x)
#   define TIMER_RESET(x)
#endif


class Timer
{
private:
    uint64 total;
    uint64 current;
    uint64 freq;
    std::string name;
public:
    Timer(const char * str = nullptr) : total(0), current(0) { if (str != nullptr) name = str; else name = "Time : ";}

    void begin()
    {
        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        QueryPerformanceCounter((LARGE_INTEGER*)&current);
    }

    double end()
    {
        uint64 t;
        QueryPerformanceCounter((LARGE_INTEGER*)&t);
        t -= current;
        total += t;

        return toMs(t);
    }

    void  reset()
    {
        total = current = 0;
    }

    double get()
    {
        return toMs(total);
    }

    double toMs(uint64& t)
    {
        return double(t) / (double(freq) * 0.001);
    }

    void print()
    {
        double ms = get();
        uint s = ms / 1000;
        uint m = s / 60;
        uint h = m / 60;
        printf("%s %02d:%02d:%02d (%f ms)\n", name.c_str(), h % 24, m % 60, s % 60, ms);
    }
};