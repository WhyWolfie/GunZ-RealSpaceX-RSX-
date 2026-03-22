// FSTest.cpp : Defines the entry point for the console application.
//

/*#include "stdafx.h"
#include "CompressionEngine.h"
#include "FileSystem.cpp"

#include "crc32.h"
#include "Timer.h"

extern Timer ct, dt;
extern Timer eet, edt;


int main(int argc, char* argv[])
{
    Timer t1;
    byte key[] = {0xff, 0x14, 0xa9, 0xd9, 0x51, 0xaf, 0x99, 0xea,
                  0x28, 0xea, 0x95, 0x81, 0x58, 0x12, 0xe0, 0x3d,
                  0x75, 0x8c, 0x7a, 0x75, 0x34, 0x17, 0x51, 0xd2,
                  0x07, 0xe6, 0x19, 0x4c, 0xca, 0xce, 0xec, 0x01};

    FileManager fm(key);

    bool b;

    fm.setCompression(COMPRESSOR_DOBOZ);
    fm.clear();
    dt.reset();
    t1.begin();
    b = fm.checkVFS("package2.gunz.hdr");
    t1.end();
    printf("Checked package2.gunz.hdr : %s\n", b ? "true" : "false");
    t1.print();
    t1.reset();
    dt.print();
    dt.reset();
    edt.print();
    edt.reset();


    fm.setCompression(COMPRESSOR_LZO);
    fm.clear();
    dt.reset();
    t1.begin();
    b = fm.checkVFS("package.gunz.hdr");
    t1.end();
    printf("Checked package.gunz.hdr : %s\n", b ? "true" : "false");
    t1.print();
    t1.reset();
    dt.print();
    dt.reset();
    edt.print();
    edt.reset();
 
    




    const char * output = nullptr;
    bool check = false;
    bool use_lzo = false;

    t1.begin();
    for (int i = 0; i < argc; ++i)
    {
        if (strcmp("-i", argv[i]) == 0)
        {
            while (++i < argc)
            {
                if (*argv[i] != '-')
                {
                    std::string path = "gunz/";
                    bool b = fm.mount2(argv[i], path.append(argv[i]).c_str(), true, true, true);
                    printf("Added path: %s \t %s\n", argv[i], b ? "true" : "false");
                }
                else
                    break;
            }
        }
        if (strcmp("-o", argv[i]) == 0 && i < argc-1)
        {
            output = argv[++i];
        }

        if (strcmp("-c", argv[i]) == 0)
        {
            check = true;
        }

        if (strcmp("-lzo", argv[i]) == 0) use_lzo = true;
    }
    t1.end();
    t1.print();
    t1.reset();

    printf("Press any key to save...\n");
    getchar();

    if (use_lzo) fm.setCompression(COMPRESSOR_LZO);

    if (output != nullptr)
    {
        t1.begin();
        bool b = fm.save(output, true, true);
        printf("Save package: %s \t %s\n", output, b ? "true" : "false");
        t1.end();
        t1.print();
        t1.reset();
    }
    else
    {
        printf("Null output!\n");
        return -1;
    }

    if (output != nullptr && check)
    {
        t1.begin();
        printf("Performing CRC32 check...\n");
        std::string name = output;
        fm.clear();
        if (fm.checkVFS(name.append(".hdr").c_str()))
            printf("CRC test succeeded!\n");
        else
            printf("CRC test failed;!\n");
        t1.end();
        t1.print();
    }

    getchar();

	return 0;
}
*/

