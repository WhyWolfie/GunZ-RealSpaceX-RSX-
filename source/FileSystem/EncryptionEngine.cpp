#include "EncryptionEngine.h"

extern "C" {
#include "aes/aes.h"
}

#include "Timer.h"

TIMER_DECLARE(eet("Encryption time: "));
TIMER_DECLARE(edt("Decryption time: "));

byte def_iv[] = {0x7b, 0xe8, 0x93, 0xd5, 0x1b, 0xaa, 0x8f, 0x1f,
    0xf3, 0x6b, 0x38, 0x37, 0x85, 0x5f, 0x2b, 0x91,
    0xbd, 0xb1, 0xe7, 0x8c, 0xb5, 0x0b, 0x65, 0x63,
    0x93, 0xf2, 0x23, 0x6d, 0xc7, 0xd3, 0xe2, 0xa9};


EncryptionEngine::EncryptionEngine()
{
    aes_init();
    setIV(def_iv);
}
EncryptionEngine::EncryptionEngine(byte key[]) 
{
    aes_init();
    setKey(key);
    setIV(def_iv);
}

void EncryptionEngine::reset()
{
    aes_mode_reset(ecx);
    setIV(def_iv);
}

void EncryptionEngine::setKey(byte key[])
{
    aes_encrypt_key256(key, ecx);
}

void EncryptionEngine::setIV(byte v[])
{
    for (int i = 0; i < 32; ++i)
        iv[i] = v[i];
}

bool EncryptionEngine::encrypt(const byte in[], byte out[], uint size)
{
    TIMER_BEGIN(eet);
    bool b = aes_cfb_encrypt(in, out, size, iv, ecx) == 0;
    TIMER_END(eet);
    return b;
}

bool EncryptionEngine::decrypt(const byte in[], byte out[], uint size)
{
   TIMER_BEGIN(edt);
   bool b = aes_cfb_decrypt(in, out, size, iv, ecx) == 0;
   TIMER_END(edt);
   return b;
}