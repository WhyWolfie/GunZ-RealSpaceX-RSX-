#pragma once
#include "stdafx.h"

#define    FF_READ   1
#define    FF_WRITE  2
#define    FF_APPEND 4

class IFile
{
public:
	virtual ~IFile() {}

	virtual bool isOpen() = 0;

	virtual const char * getPath()  = 0;

    virtual void close() = 0;

    virtual int read(uint size, void * buffer) = 0;

    virtual byte * read(uint size) = 0;

    virtual int write(uint size, const void * buffer) = 0;

    virtual bool skip(uint size) = 0;

    virtual bool setOffset(uint pos) = 0;

    virtual uint getOffset() = 0;

    virtual uint getSize() = 0;

    virtual uint getCrc32() = 0;
};



class IFileManager
{
public:
    virtual IFile * open(const char * name, uint flag = FF_READ) = 0;
    virtual bool mount(const char * name, const char * mount_point, bool override = false) = 0;

    virtual uint getFileCount() = 0;

    virtual const char * getFirstFileName() = 0;

    virtual const char * getNextFileName() = 0;
};