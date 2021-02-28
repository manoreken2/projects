#include "MLPngReader.h"
#include "MLExrWriter.h"
#include <Windows.h>

static void
PrintUsage(void) {
    printf("Usage: PngToExr inPngFile outExrFile\n");
}


int
wmain(int argc, wchar_t* argv[])
{
    if (argc != 3) {
        PrintUsage();
        return 1;
    }

    const wchar_t* inPngFile = argv[1];
    const wchar_t* outExrFile = argv[2];

    MLImage2 img;
    if (0 != MLPngRead(inPngFile, img)) {
        printf("Error reading PNG file. %S\n", inPngFile);
        PrintUsage();
        return 1;
    }

    MLExrWriter exrW;

    char s[512*3];
    memset(s, 0, sizeof s);
    WideCharToMultiByte(CP_ACP, 0, outExrFile, -1, s, sizeof s - 1, nullptr, FALSE);

    if (0 != exrW.Write(s, img)) {
        printf("Error writing EXR file. %S\n", outExrFile);
        PrintUsage();
        return 1;
    }

    return 0;
}
