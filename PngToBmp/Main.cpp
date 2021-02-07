#include <iostream>
#include "MLPngReader.h"
#include "MLBmpWriter.h"

static void
PrintUsage(void) {
    printf("Usage: PngToBmp inPngFile, outBmpFile\n");
}

int
wmain(int argc, wchar_t *argv[])
{
    if (argc != 3) {
        PrintUsage();
        return 1;
    }

    const wchar_t* inPngFile = argv[1];
    const wchar_t* outBmpFile = argv[2];

    MLImage img;
    if (0 != MLPngRead(inPngFile, img)) {
        printf("Error reading PNG file. %S\n", inPngFile);
        PrintUsage();
        return 1;
    }

    if (0 != MLBmpWrite(outBmpFile, img)) {
        printf("Error writing BMP file. %S\n", outBmpFile);
        PrintUsage();
        return 1;
    }

    return 0;
}
