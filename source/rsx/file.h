#pragma once
#include <stdio.h>
#include "defs.h"
#include "Matrix.h"



#define    FF_READ   1
#define    FF_WRITE  2
#define    FF_APPEND 4

class PathManager
{
	static std::vector<std::string> m_vPath;
public:
	static std::vector<std::string>& getPathList() {return m_vPath;} 
	static void clear() {m_vPath.clear();}
	static void add(std::string path) {m_vPath.push_back(path);}
	static void add(const char * path) {m_vPath.push_back(path);}
};

class File
{
    FILE *m_pFile;
	std::string path;
public:
	File(): m_pFile(nullptr) {};
	~File() {close();}

    bool open(const char * name, uint flag = FF_READ)
    {
        close();
        char *ff;
        switch (flag)
        {
            case FF_READ:
                ff = "rb";
                break;
            case FF_WRITE:
                ff = "wb";
                break;
            case FF_APPEND:
                ff = "ab";
                break;
        }

        m_pFile = fopen(name, ff);
		path = m_pFile ? name : "";
        return m_pFile != nullptr;
    }

	bool open2(const char * name, uint flag = FF_READ)
	{
		bool opened = open(name, flag);

		std::vector<std::string>::iterator itor = PathManager::getPathList().begin();
		while (!opened && itor != PathManager::getPathList().end())
		{
			std::string str = *itor;
			str.append(name);
			opened = open(str.c_str(), flag);
			++itor;
		}
		return opened;
	}

	bool isOpen() {return m_pFile != 0;}

	const char * getPath() {return path.c_str();}

    void close()
    {
        if (m_pFile) fclose(m_pFile);
        m_pFile = nullptr;
		path = "";
    }

    int read(uint size, void * buffer)
    {
        if (!m_pFile || !buffer || size <= 0) return -1;

        return fread(buffer, 1, size, m_pFile);
    }

    void * read(uint size)
    {
        if (!m_pFile || size <= 0) return nullptr;
        char * buffer = new char[size];

        if (fread(buffer, 1, size, m_pFile) == size) return buffer;
        else delete buffer;
        return nullptr;
    }

    int write(uint size, const char * buffer)
    {
        if (!m_pFile || !buffer) return -1;
        return fwrite(buffer, 1, size, m_pFile);
    }

    bool skip(uint size)
    {
        if (!m_pFile) return false;
        return fseek(m_pFile, size, SEEK_CUR) == 0;
    }

    bool setOffset(uint pos)
    {
        if (!m_pFile) return false;
        return fseek(m_pFile, pos, SEEK_SET) == 0;
    }

    uint getOffset()
    {
        return ftell(m_pFile);
    }

    uint getSize()
    {
        uint cur = getOffset();

        fseek(m_pFile, 0, SEEK_END);
        uint ret = getOffset();
        setOffset(cur);
        return ret;
    }
};
