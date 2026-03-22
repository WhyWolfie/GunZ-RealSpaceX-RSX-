#include "stdafx.h"
#include "MMatchUtil.h"
#include "MZFileSystem.h"
#include <random>

#include "../FileSystem/FileSystem.h"

unsigned long int MGetTimeDistance(unsigned long int nTimeA, unsigned long int nTimeB)
{
	if(nTimeB >= nTimeA)
	{
		return nTimeB - nTimeA;
	}
	else
	{
		return nTimeB + (UINT_MAX - nTimeA + 1);
	}

	return 0;
}

unsigned long MGetMemoryChecksum(char *pBuffer, int nLen)
{
	unsigned long nChecksum = 0;
	for (int i=0; i<nLen; i++)
		nChecksum += pBuffer[i];

	return nChecksum;
}

unsigned long MGetMZFileChecksum(const char* pszFileName) 
{
	std::unique_ptr<IFile> pFile = std::unique_ptr<IFile>(getFileManager()->open(pszFileName));
    if (!pFile.get())
    {
        return 0;
    }

	std::unique_ptr<char> buffer = std::unique_ptr<char>(new char[pFile->getSize()+1]);
    pFile->read(pFile->getSize(), buffer.get());
	buffer.get()[pFile->getSize()] = 0;

	unsigned long nChecksum = MGetMemoryChecksum(buffer.get(), pFile->getSize());
	return nChecksum;
}
int RangeRand(int nMin, int nMax)
{
	std::random_device r;
	std::mt19937 gen(r());
	std::uniform_int_distribution<> distributor(nMin, nMax);
	return distributor(gen);
}
void MMakeSeedKey(MPacketCrypterKey* pKey, const MUID& uidServer, const MUID& uidClient, unsigned int nTimeStamp)
{
	unsigned char* p = (unsigned char*)pKey->szKey;
	// key
	memset(p, 0, sizeof(MPacketCrypterKey));
	int nUIDSize = sizeof(MUID);

	memcpy(p, &nTimeStamp, sizeof(unsigned int));
	memcpy(p+sizeof(unsigned int), &uidServer.Low, sizeof(unsigned int));
	memcpy(p+nUIDSize, &uidClient, nUIDSize);

	const BYTE XOR[32] = { 12, 22, 25, 1, 45, 7, 3, 4, 77, 76, 44, 76, 85, 46, 5, 124, 32, 62, 15, 87, 95, 34, 254, 2, 37, 26, 24, 216, 235, 126, 132, 42 };

	for (int i = 0; i < 32; i++)
	{
		p[i] ^= XOR[i % 32];
	}
	p = p + 16;
	// iv
	p[0] = 57;
	p[7] = 85;
	p[1] = 6;
	p[2] = 33;
	p[3] = 26;
	p[4] = 17;
	p[5] = MCOMMAND_VERSION;
	p[11] = 202;
	p[6] = 13;
	p[8] = 40;
	p[9] = 6;
	p[10] = 9;
	p[12] = 2;
	p[13] = 64;
	p[14] = 7;
	p[15] = 35;
	p[16] = 17;
	p[17] = 45;
	p[18] = 66;
	p[19] = 21;
	p[20] = 56;
	p[21] = 67;
	p[22] = 99;
	p[23] = 122;
	p[24] = 43;
	p[25] = 254;
	p[26] = 128;
	p[27] = 29;
	p[28] = 52;
	p[29] = 129;
	p[30] = 44;
	p[31] = 85;
}

/*
void MMakeSeedKey(BYTE Key[SEED_USER_KEY_LEN], BYTE IV[SEED_BLOCK_LEN], const MUID& uidServer, const MUID& uidClient, unsigned int nTimeStamp)
{
	// key
	memset(Key, 0, sizeof(BYTE)*SEED_USER_KEY_LEN);
	int nUIDSize = sizeof(MUID);

	memcpy(Key, &nTimeStamp, sizeof(unsigned int));
	memcpy(Key+sizeof(unsigned int), &uidServer.Low, sizeof(unsigned int));
	memcpy(Key+nUIDSize, &uidClient, nUIDSize);

	const BYTE XOR[16] = {87, 2, 91, 4, 52, 6, 1, 8, 55, 10, 18, 105, 65, 56, 15, 120};

	for (int i = 0; i < SEED_USER_KEY_LEN; i++)
	{
		Key[i] ^= XOR[i];
	}

	// iv
	IV[0] = 55;
	IV[7] = 83;
	IV[1] = 4;
	IV[2] = 93;
	IV[3] = 46;
	IV[4] = 67;
	IV[5] = 182;
	IV[11] = 201;
	IV[6] = 73;
	IV[8] = 80;
	IV[9] = 5;
	IV[10] = 19;
	IV[12] = 40;
	IV[13] = 164;
	IV[14] = 77;
	IV[15] = 5;
}
*/