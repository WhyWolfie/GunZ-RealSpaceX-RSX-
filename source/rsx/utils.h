#pragma once
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "defs.h"

namespace rsx {
namespace utils {

class Logger
{
public:
    static void print(const char *str, ...)
    {
        char temp[16*1024];

        va_list args;

        va_start(args, str);
        vsprintf(temp, str, args);
        va_end(args);

        FILE *f = fopen("rsx_log.txt", "rb+");
        if (f)
        {
            fwrite(temp, 1, strlen(temp), f);
            fclose(f);
        }
    }

    static void print2(const char * file, const char *str, ...)
    {
        char temp[16*1024];

        va_list args;

        va_start(args, str);
        vsprintf(temp, str, args);
        va_end(args);

        FILE *f = fopen(file, "rb+");
        if (f)
        {
            fwrite(temp, 1, strlen(temp), f);
            fclose(f);
        }
    }
};

#if 0
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

    const char * getName()
    {
        return name.c_str();
    }
};

class TimerCounter : public Timer
{
    uint64 counter;
    double _min;
    double _max;
public:
    TimerCounter(const char * str = nullptr): Timer(str), counter(0), _max(0.0), _min(DBL_MAX) {}

    void begin()
    {
        ++counter;
        Timer::begin();
    }

    double end()
    {
        double t = Timer::end();
        _min = t < _min ? t : _min;
        _max = t > _max ? t : _max;
        return t;
    }

    double getMin()
    {
        return _min;
    }

    double getMax()
    {
        return _max;
    }

    double getMean()
    {
        return Timer::get() / (double)counter;
    }

    uint64 getCount()
    {
        return counter;
    }

    void  reset()
    {
        counter = 0;
        _max = 0.0;
        _min = DBL_MAX;
        Timer::reset();
    }
};

}
}