#pragma once
#include "stdafx.h"
#include "aes/aes.h"



class EncryptionEngine
{
    aes_encrypt_ctx ecx[1];

    byte iv[32];
public:
    EncryptionEngine();
    EncryptionEngine(byte key[]);

    void reset();

    void setKey(byte key[]);

    void setIV(byte v[]);

    bool encrypt(const byte in[], byte out[], uint size);

    bool decrypt(const byte in[], byte out[], uint size);
};