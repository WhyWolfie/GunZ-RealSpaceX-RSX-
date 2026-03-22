#pragma once

#include "stdafx.h"
#include "Doboz/Compressor.h"
#include "Doboz/Decompressor.h"

enum CompressorType
{
    COMPRESSOR_LZO,
    COMPRESSOR_DOBOZ
};

class ICompressionEngine
{
public:
    virtual bool compress(void * data, void * out_data, uint size, uint buffer_size, uint * out_size) = 0;

    virtual void print() = 0;

    virtual bool decompress(void * data, void * out_data, uint size, uint buffer_size, uint * out_size) = 0;
};

//#ifdef USE_LZO

class CompressionEngineLzo : public ICompressionEngine
{
    byte * buffer;
    int err;
public:
    CompressionEngineLzo();
    ~CompressionEngineLzo();

    bool compress(void * data, void * out_data, uint size, uint buffer_size, uint * out_size);

    void print();

    bool decompress(void * data, void * out_data, uint size, uint buffer_size, uint * out_size);
};

//#else

class CompressionEngineDoboz : public ICompressionEngine
{
    doboz::Compressor comp;
    doboz::Decompressor dec;
    doboz::Result result;
public:
    CompressionEngineDoboz():result(doboz::RESULT_OK) {}
    ~CompressionEngineDoboz() {}

    bool compress(void * data, void * out_data, uint size, uint buffer_size, uint * out_size);

    void print();

    bool decompress(void * data, void * out_data, uint size, uint buffer_size, uint * out_size);
};

//#endif