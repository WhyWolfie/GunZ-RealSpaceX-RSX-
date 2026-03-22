#include "CompressionEngine.h"
#include "minilzo/minilzo.h"
#include "Timer.h"

TIMER_DECLARE(ct("Compression time: "));
TIMER_DECLARE(dt("Decompression time: "));

const char * lzo_error[] = {"LZO_E_OK", "LZO_E_ERROR", "LZO_E_OUT_OF_MEMORY",
    "LZO_E_NOT_COMPRESSIBLE", "LZO_E_INPUT_OVERRUN", 
    "LZO_E_OUTPUT_OVERRUN", "LZO_E_LOOKBEHIND_OVERRUN", 
    "LZO_E_EOF_NOT_FOUND", "LZO_E_INPUT_NOT_CONSUMED",
    "LZO_E_NOT_YET_IMPLEMENTED", "LZO_E_INVALID_ARGUMENT"};

//#ifdef USE_LZO

CompressionEngineLzo::CompressionEngineLzo(): err(0) 
{
    buffer = new byte[LZO1X_1_MEM_COMPRESS];
}
CompressionEngineLzo::~CompressionEngineLzo() {delete[] buffer;}

bool CompressionEngineLzo::compress(void * data, void * out_data, uint size, uint buffer_size, uint * out_size)
{
    TIMER_BEGIN(ct);
    err = lzo1x_1_compress((const lzo_bytep) data, (lzo_uint)size, (lzo_bytep)out_data,
        (lzo_uintp) out_size, (lzo_voidp) buffer);
    TIMER_END(dt);
    return err == 0;
}

void CompressionEngineLzo::print()
{
    int e = err < 0 ? -err : err;
    printf("ERROR: %s!\n", lzo_error[e]);
}

bool CompressionEngineLzo::decompress(void * data, void * out_data, uint size, uint buffer_size, uint * out_size)
{
    TIMER_BEGIN(dt);
    err = lzo1x_decompress((const lzo_bytep) data, (lzo_uint)size, (lzo_bytep)out_data,
        (lzo_uintp) out_size, (lzo_voidp) nullptr);
    TIMER_END(dt);
    if (*out_size > buffer_size)
    {
        printf("Buffer overflow while compressing data.\nThe program will crash soon!\n");
       return false;
    }
    return err == 0 ;
}


//#else

const char * doboz_error[] = {"RESULT_OK",
	"RESULT_ERROR_BUFFER_TOO_SMALL",
	"RESULT_ERROR_CORRUPTED_DATA",
	"RESULT_ERROR_UNSUPPORTED_VERSION"};

bool CompressionEngineDoboz::compress(void * data, void * out_data, uint size, uint buffer_size, uint * out_size)
{
    TIMER_BEGIN(ct);
    result = comp.compress(data, size, out_data, buffer_size, *out_size);
    TIMER_END(ct);
    return result  == doboz::RESULT_OK;
}

void CompressionEngineDoboz::print()
{
    printf("ERROR: %s!\n", doboz_error[result]);
}

bool CompressionEngineDoboz::decompress(void * data, void * out_data, uint size, uint buffer_size, uint * out_size)
{
    TIMER_BEGIN(dt);
    result = dec.decompress(data, size, out_data, buffer_size, *out_size);
    TIMER_END(dt);
    return result  == doboz::RESULT_OK;
}

//#endif