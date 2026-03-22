#include "IFileManager.h"

#include "FileSystem.h"
#include "CompressionEngine.h"
#include "EncryptionEngine.h"
#include "crc32.h"

#include "string_utils.h"

#include <Windows.h>

#define PKG_USE_COMPRESSION     1
#define PKG_USE_ENCRYPTION      1 << 1
#define PKG_IS_COMPRESSED       1 << 2
#define PKG_IS_ENCRYPTED        1 << 3
#define PKG_VIRTUAL_FILE        1 << 4
#define PKG_OS_FILE             1 << 5
#define PKG_LINK_FILE           1 << 6
#define PKG_MAGIC               0xFEDCBA98

struct PackageHeader
{
    uint magic;
    uint crc32;
    uint entry_count;
    uint flags;
    uint size;
    uint compressed_size;
    uint data_name_size;

    PackageHeader() : magic(0xFEDCBA98), crc32(0), entry_count(0), flags(0), size(0), compressed_size(0), data_name_size(0) {}
};

struct PackageEntry
{
    uint flags;
    uint crc32;
    uint compressed_size;
    uint size;
    uint offset;
    uint name_size;

    PackageEntry() : flags(0), crc32(0), compressed_size(0), size(0), name_size(0) {}
};

class OSFile : public IFile
{
    FILE *m_pFile;
	std::string path;
    //std::vector<std::unique_ptr<byte>> m_vAllocations;
    std::vector<byte*> m_vAllocations;
public:
	OSFile(): m_pFile(nullptr) {};
	~OSFile() {close();}

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

	bool isOpen() {return m_pFile != 0;}

	const char * getPath() {return path.c_str();}

    void close()
    {
        if (m_pFile) fclose(m_pFile);
        m_pFile = nullptr;
		path = "";
        for (int i = 0; i < m_vAllocations.size(); ++i)
            delete m_vAllocations[i];
        m_vAllocations.clear();
    }

    uint getCrc32()
    {
        uint i = getSize();
        std::unique_ptr<byte> buf = std::unique_ptr<byte>(new byte[i]);
        if (i != read(i, buf.get())) return 0;
        CRC32 crc32;
        return crc32.get(buf.get(), buf.get() + i);
    }

    int read(uint size, void * buffer)
    {
        if (!m_pFile || !buffer || size < 0) return -1;

        return !size ? 0 : fread(buffer, 1, size, m_pFile);
    }

    byte * read(uint size)
    {
        if (!m_pFile || size <= 0) return nullptr;
        std::unique_ptr<byte> buffer = std::unique_ptr<byte>(new byte[size]);

        if (fread(buffer.get(), 1, size, m_pFile) == size)
        {
            byte * ptr = buffer.release();
            m_vAllocations.push_back(ptr);
            return ptr;
        }
        return nullptr;
    }

    int write(uint size, const void * buffer)
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

class Logger
{
public:
    static void print(const char *str, ...)
    {
//#ifdef RELEASE_DEBUG
        char temp[16*1024];	// 16k

        va_list args;

        va_start(args, str);
        vsprintf(temp, str, args);
        va_end(args);
        OSFile f;
        f.open("FSLog.txt", FF_APPEND);
        int i =strlen(temp);
        f.write(i, temp);
//#endif
    }
};


struct FileEntry
{
    uint flags;
    uint crc32;
    uint compressed_size;
    uint size;
    uint offset;
    std::string name;
    std::string parent_name;

    FileEntry() : flags(0), crc32(0), compressed_size(0), size(0), offset(0) {}
};




class VFile : public IFile
{
	std::string path;
    std::unique_ptr<byte> data;
    uint offset;
    uint fsize;
public:
    VFile(): offset(0), fsize(0) {}
	~VFile() {close();}

    bool open(FileEntry& fe, EncryptionEngine& ee, ICompressionEngine* ce)
    {
        offset = 0;
        fsize = 0;
        OSFile file;
        if (!file.open(fe.parent_name.c_str())) return false;
        uint size = file.getSize();
        file.setOffset(fe.offset);

        std::unique_ptr<byte> buffer(new byte[fe.compressed_size]);
        if (fe.flags & PKG_IS_ENCRYPTED)
        {
            void * ptr = file.read(fe.compressed_size);
            if (ptr == nullptr) return false;
            ee.reset();
            if (!ee.decrypt((byte*)ptr, buffer.get(), fe.compressed_size)) return false;
        }
        else
        {
            if (file.read(fe.compressed_size, buffer.get()) != fe.compressed_size) return false;
        }


        if (fe.flags & PKG_IS_COMPRESSED)
        {
            data  = std::unique_ptr<byte>(new byte[fe.size]);
            uint out_size;
            if (!ce->decompress(buffer.get(), data.get(), fe.compressed_size, fe.size, &out_size))
            {
                Logger::print("Failed to decompress file!\n");
                return false;
            }
            if (out_size != fe.size)
            {
                Logger::print("Failed to decompress file!\n");
                return false;
            }
        }
        else
            data = std::move(buffer);

        path = fe.parent_name;
        path.append(fe.name);
        fsize = fe.size;

        return true;
    }

    uint getCrc32()
    {
        CRC32 crc32;
        return crc32.get(data.get(), data.get() + fsize);
    }

    bool isOpen() {return data.get() != nullptr;}

	const char * getPath() {return path.c_str();}

    void close()
    {
        data.reset();
		path = "";
    }

    int read(uint size, void * buffer)
    {
        size = offset + size < fsize ? size : fsize - offset;
        if (size > 0)
        {
            memcpy(buffer, data.get() + offset, size);
            offset += size;
        }
        return size;
    }

    byte * read(uint size)
    {
        
        size = offset + size < fsize ? size : fsize - offset;
        if (size > 0)
        {
            uint p = offset;
            offset += size;
            return data.get() + p;
        }
        return nullptr;
    }

    int write(uint size, const void * buffer)
    {
        return -1;
    }

    bool skip(uint size)
    {
        if (offset+size < fsize)
        {
            offset += size;
            return true;
        }
        return false;
    }

    bool setOffset(uint pos)
    {
        if (pos >=0 && pos < fsize)
        {
            offset = pos;
            return true;
        }
        return false;
    }

    uint getOffset()
    {
        return offset;
    }

    uint getSize()
    {
        return fsize;
    }
};



#define RET_FALSE_IF_NOT(x) {if (!(x)) return false;}

class FileManager : public IFileManager
{
    std::map<std::string, FileEntry> m_FileMap;
    bool use_os_file;
    bool skip_crc;
    bool no_case;

    ICompressionEngine * ce;
    EncryptionEngine ee;
    CRC32 crc32;

    std::map<std::string, FileEntry>::iterator curr;

public:

    FileManager(byte * key) : ee(key), ce(nullptr) { setCompression(COMPRESSOR_DOBOZ);}

    void setCompression(CompressorType t)
    {
        if (ce) delete ce;
        switch (t)
        {
        case COMPRESSOR_LZO:
            ce = new CompressionEngineLzo();
            break;
        case COMPRESSOR_DOBOZ:
            ce = new CompressionEngineDoboz();
            break;
        default:
            ;
        }
    }

    void setCaseSensitive(bool b) {no_case = !b;}
    void setCrcEnable(bool b) {skip_crc = !b;}
    void setOSFile(bool b) {use_os_file = b;}

    virtual uint getFileCount() 
    {
        return m_FileMap.size();
    }

    virtual const char * getFirstFileName()
    {
        curr = m_FileMap.begin();
        if (curr != m_FileMap.end())
            return curr->first.c_str();
        return nullptr;
    }

    virtual const char * getNextFileName()
    {
        if (++curr != m_FileMap.end())
            return curr->first.c_str();
        return nullptr;
    }

    virtual IFile * open(const char * name, uint flag = FF_READ)
    {
        std::string path = name;
        fixSlash(path);
        if (no_case)
            toLower(path);
            
        std::map<std::string, FileEntry>::iterator itor = m_FileMap.find(path);
        if (itor != m_FileMap.end())
        {
            if ((itor->second.flags & PKG_VIRTUAL_FILE) && (flag && FF_READ))
            {
                std::unique_ptr<VFile> f = std::unique_ptr<VFile>(new VFile());
                if (f->open(itor->second, ee, ce)) return f.release();
            }
            else if (itor->second.flags & PKG_OS_FILE)
            {
                std::unique_ptr<OSFile> f = std::unique_ptr<OSFile>(new OSFile());
                std::string path = itor->second.parent_name;
                path.append(itor->second.name);
                if (f->open(path.c_str(), flag)) return f.release();
            }
        }

        if (use_os_file)
        {
            std::unique_ptr<OSFile> f = std::unique_ptr<OSFile>(new OSFile());
            if (f->open(name, flag)) return f.release();
        }

        Logger::print("Failed to open file %s.\n", path.c_str());

        return nullptr;
    }

    uint getCrc32(byte * buffer, uint size) 
    {
        return crc32.get(buffer, (byte*)(buffer + size));
    }

    bool isHeaderValid(PackageHeader& header)
    {
        return header.magic == PKG_MAGIC;
    }

    bool mount(const char * name, const char * mount_point, bool override = false)
    {
        OSFile file;
        if (!file.open(name)) return false;

        PackageHeader header;
        file.read(sizeof(PackageHeader), &header);
        if (!isHeaderValid(header))
        {
            Logger::print("Invalid header: %s!\n", name);
            return false;
        }

        char * hdrn = (char*)file.read(header.data_name_size+1);
        if (hdrn == nullptr != hdrn[header.data_name_size] != 0) return false;

        std::string package_file = hdrn;

        std::unique_ptr<byte> data(new byte[header.compressed_size]);

        if (header.flags & PKG_IS_ENCRYPTED)
        {
            byte * enc_data = (byte*)file.read(header.compressed_size);
            if (data == nullptr) return false;

            data = std::unique_ptr<byte>(new byte[header.compressed_size]);
            ee.reset();
            if (!ee.decrypt(enc_data, data.get(), header.compressed_size))
            {
                Logger::print("failed to decrypt header data(%s)!\n", name);
                return false;
            }
        }

        if (header.flags & PKG_IS_COMPRESSED)
        {
            std::unique_ptr<byte> uncompressed(new byte[header.size]);
            uint size;
            if (!ce->decompress(data.get(), uncompressed.get(), header.compressed_size, header.size, &size)) return false;

            if (header.size != size) return false;

            data = std::move(uncompressed);
        }

        if (!skip_crc)
        {
            uint crc32 = getCrc32(data.get(), header.size);
            if (crc32 != header.crc32)
            {
                Logger::print("Invalid header crc32(%s)!", name);
                return false;
            }
        }

        //if (data.get()[header.data_name_size] != 0) return false;

        byte * ptr = data.get();
        byte * end = ptr + header.size;
        //char * data_file = (char*) ptr;

        //ptr += header.data_name_size+1;

        for (uint i = 0; i < header.entry_count; ++i)
        {
            PackageEntry * pe = (PackageEntry *)ptr;
            ptr += sizeof(PackageEntry);

            char * file_name = (char*)ptr;
            ptr += pe->name_size+1;

            if (ptr > end || file_name[pe->name_size] != 0)
                return false;

            FileEntry fe;
            fe.flags = pe->flags;
            fe.crc32 = pe->crc32;
            fe.compressed_size = pe->compressed_size;
            fe.size = pe->size;
            fe.name = file_name;
            fe.parent_name = package_file;
            fe.offset = pe->offset;
            std::string path = fixDirectory(mount_point);
            path.append(file_name);

            if (no_case)
                toLower(path);

            std::map<std::string, FileEntry>::iterator itor = m_FileMap.find(path);
            if (itor == m_FileMap.end() || override)
                m_FileMap[path] = fe;
        }
        return true;
    }

    bool addEntry(const char * path, const char * directory, const char * name, bool c, bool e)
    {
        FileEntry fe;
        fe.parent_name = directory;
        fe.name = name;
        if (c) fe.flags |= PKG_USE_COMPRESSION;
        if (e) fe.flags |= PKG_USE_ENCRYPTION;
        fe.flags |= PKG_OS_FILE;

        std::map<std::string, FileEntry>::iterator itor = m_FileMap.find(path);
        if (itor == m_FileMap.end())
        {
            m_FileMap[path] = fe;
            return true;
        }
        return false;
    }

    bool mount2(const char * directory, const char * mount_point, bool recursive, bool compress, bool encrypt)
    {
        WIN32_FIND_DATAA fdFile;
        HANDLE hFind = NULL;
        char sPath[2048];

        std::vector<std::string> directories;

        std::string dir1 = fixDirectory(directory);
        uint dir1_size = dir1.size();
        directories.push_back(dir1);
        std::vector<std::string>::iterator itor = directories.begin();

        uint i = 0;
        while (i < directories.size())
        {
            std::string dir = directories[i];
            dir.append("*.*");

            if((hFind = FindFirstFileA(dir.c_str(), &fdFile)) == INVALID_HANDLE_VALUE)
            {
                Logger::print("Path not found: [%s]\n", directories[i].c_str());
                continue;
            }

            do
            {
                //Find first file will always return "."
                //    and ".." as the first two directories.
                if(strcmp(fdFile.cFileName, ".") != 0 && strcmp(fdFile.cFileName, "..") != 0)
                {
                    //Build up our file path using the passed in
                    //  [sDir] and the file/foldername we just found:
                    sprintf(sPath, "%s%s", directories[i].c_str(), fdFile.cFileName);

                    //Is the entity a File or Folder?
                    if(fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && recursive)
                    {
                        std::string str = fixDirectory(sPath);
                        fixSlash(str);
                        directories.push_back(str);
                        //printf("Adicionando diretorio: %s\n", sPath);
                    }
                    else
                    {
                        //printf("Adicionando arquivo: %s\n", sPath);
                        char * ptr = sPath + dir1_size;
                        std::string path = fixDirectory(mount_point);
                        path.append(ptr);
                        fixSlash(path);
                        if (no_case)
                            toLower(path);
                        addEntry(path.c_str(), dir1.c_str(), ptr, compress, encrypt);
                    }
                }
            }
            while(FindNextFileA(hFind, &fdFile)); //Find the next file.

            FindClose(hFind);
            ++i;
        }

        return true;
    }

    bool save(const char * name, bool compress, bool encrypt)
    {
        OSFile file;
        OSFile fh;
        uint totalEntries = 0;
        if (!file.open(name, FF_WRITE)) return false;

        std::string hdr = name; hdr.append(".hd");
        if (!fh.open(hdr.c_str(), FF_WRITE)) return false;

        std::map<std::string, FileEntry>::iterator itor = m_FileMap.begin();
        for (; itor != m_FileMap.end(); ++itor)
        {
            uint offset = file.getOffset();
            uint hoffset = fh.getOffset();
            if (itor->second.flags & PKG_OS_FILE)
            {
                FileEntry & e = itor->second;
                std::string path = e.parent_name;
                path.append(e.name);

                OSFile f;
                if (!f.open(path.c_str()))
                {
                    Logger::print("can't open file : %s\n", path.c_str());
                    continue;
                }

                PackageEntry entry;
                entry.name_size = itor->first.size();
                entry.flags |= PKG_VIRTUAL_FILE;
                entry.size = f.getSize();
                entry.offset = offset;

                std::unique_ptr<byte> buffer;

                byte * ptr = (byte*)f.read(entry.size);
                if (!ptr)
                {
                    Logger::print("Could not read file : %s!\n", path.c_str());
                    continue;
                }

                entry.crc32 = getCrc32(ptr, entry.size);

                if (e.flags & PKG_USE_COMPRESSION)
                {
                    uint bs = (entry.size << 2) < 40 * 1024 * 1024 ? (entry.size << 1) : (entry.size << 2);
                    bs = bs < 1024 ? 1024 : bs;

                    buffer = std::unique_ptr<byte>(new byte[bs]);
                    uint comp_size;
                    if (!ce->compress(ptr, buffer.get(), entry.size, bs, &comp_size))
                    {
                        Logger::print("Failed to compress file: %s!\n", path.c_str());
                        continue;
                    }

                    entry.compressed_size = comp_size;
                    entry.flags |= PKG_IS_COMPRESSED;
                } 
                else 
                {
                    entry.compressed_size = entry.size;
                    buffer = std::unique_ptr<byte>(new byte[entry.size]);
                    memcpy(buffer.get(), ptr, entry.size);
                }

                if (e.flags & PKG_USE_ENCRYPTION)
                {
                    std::unique_ptr<byte> buffer2 = std::unique_ptr<byte>(new byte[entry.compressed_size]);
                    ee.reset();
                    if (!ee.encrypt(buffer.get(), buffer2.get(), entry.compressed_size))
                    {
                        Logger::print("Failed to encrypt data for file: %s\n", path.c_str());
                        continue;
                    }
                    buffer = std::move(buffer2);
                    entry.flags |= PKG_IS_ENCRYPTED;
                }

                char ch = 0;
                if (!fh.write(sizeof(PackageEntry), (byte*)&entry) ||
                    !fh.write(entry.name_size, itor->first.c_str()) ||
                    !fh.write(1, &ch))
                {
                    Logger::print("Failed to write entry for file: %s\n", path.c_str());
                    Logger::print("Rolling back...\n");
                    fh.setOffset(hoffset);
                    continue;
                }

                if (!file.write(entry.compressed_size, buffer.get()))
                {
                    Logger::print("Failed to write file data: %s\n", path.c_str());
                    Logger::print("Rolling back...\n");
                    file.setOffset(offset);
                    continue;
                }
                ++totalEntries;
            }
        }

        fh.close();

        if (!fh.open(hdr.c_str()))
        {
            Logger::print("Failed to open header!\n");
            return false;
        }

        PackageHeader header;
        header.size = fh.getSize();
        header.entry_count = totalEntries;
        header.data_name_size = strlen(name);

        byte * buf = fh.read(header.size);
        if (!buf)
        {
            Logger::print("Failed to read header!\n");
            return false;
        }

        
        header.crc32 = getCrc32(buf, header.size);
        std::unique_ptr<byte> buffer = std::unique_ptr<byte>(new byte[header.size << 2]);
        if (compress)
        {
            if (!ce->compress(buf, buffer.get(), header.size, header.size << 2, &header.compressed_size))
            {
                Logger::print("failed to compress header!\n");
                return false;
            }
            header.flags |= PKG_IS_COMPRESSED;
        }
        else
            header.compressed_size = header.size;

        if (encrypt)
        {
            std::unique_ptr<byte> buffer2 = std::unique_ptr<byte>(new byte[header.compressed_size]);
            ee.reset();
            if (!ee.encrypt(buffer.get(), buffer2.get(), header.compressed_size))
            {
                Logger::print("failed to compress header!\n");
                return false;
            }
            header.flags |= PKG_IS_ENCRYPTED;
            buffer = std::move(buffer2);
        }

        OSFile final_hdr;
        final_hdr.open(hdr.append("r").c_str(), FF_WRITE);

        if (!final_hdr.write(sizeof(header), &header)|| 
            !final_hdr.write(header.data_name_size+1, name) ||
            !final_hdr.write(header.compressed_size, buffer.get()))
        {
            Logger::print("Failed to save header!");
            return false;
        }

        return true;
    }

    bool checkVFS(const char * header = nullptr)
    {
        if (header != nullptr)
        {
            if (!mount(header, "/", true)) return false;
        }

        bool ret = true;
        std::map<std::string, FileEntry>::iterator itor = m_FileMap.begin();
        for (; itor != m_FileMap.end(); ++itor)
        {
            std::unique_ptr<IFile> pFile = std::unique_ptr<IFile>(open(itor->first.c_str()));
            if (!pFile.get())
            {
                Logger::print("Failed to open file: %s\n", itor->first.c_str());
                ret = false;
                continue;
            }

            uint crc = pFile->getCrc32();
            if (itor->second.crc32 != crc)
            {
                Logger::print("Invalid crc32 for file: %s (%08x->%08x)\n", itor->first.c_str(), itor->second.crc32, crc);
                ret = false;
                continue;
            }
        }
        return ret;
    }

    void clear()
    {
        m_FileMap.clear();
    }
};

IFileManager * getFileManager()
{
    byte key[] = {0xff, 0x14, 0xa9, 0xd9, 0x51, 0xaf, 0x99, 0xea,
                  0x28, 0xea, 0x95, 0x81, 0x58, 0x12, 0xe0, 0x3d,
                  0x75, 0x8c, 0x7a, 0x75, 0x34, 0x17, 0x51, 0xd2,
                  0x07, 0xe6, 0x19, 0x4c, 0xca, 0xce, 0xec, 0x01};
    static FileManager * fm = nullptr;
    if (fm == nullptr)
    {
        std::unique_ptr<FileManager> f = std::unique_ptr<FileManager>(new FileManager(key));
        f->setCaseSensitive(false);
        f->setOSFile(true);
        f->setCrcEnable(false);
        
		if (!f->mount("package.gunz.hdr", ""))
            Logger::print("Failed to mount package.gunz.hdr!\n");
        if (!f->mount("rsx.gunz.hdr", "", true))
            Logger::print("Failed to mount rsx.gunz.hdr!\n");
        if (!f->mount("rsx2.gunz.hdr", "", true))
            Logger::print("Failed to mount rsx2.gunz.hdr!\n");
		
		f->mount2("interface", "", true, false, false);

        fm = f.release();
    }
    return fm;
}


void dumpFile(const char * name, IFile * file)
{
    OSFile f;
    f.open(name, FF_WRITE);
    uint off = file->getOffset();
    file->setOffset(0);
    byte *  buf = file->read(file->getSize());

    f.write(file->getSize(), buf);
}
