#include <iostream>
#include "MLPngReader.h"
#include "MLBmpWriter.h"

static void
PrintUsage(void) {
    printf("Usage: PngToBmp inPngFile, outBmpFile\n");
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        PrintUsage();
        return 1;
    }

    const char* inPngFile = argv[1];
    const char* outBmpFile = argv[2];

    MLImage img;
    if (0 != MLPngRead(inPngFile, img)) {
        printf("Error reading PNG file. %s\n", inPngFile);
        PrintUsage();
        return 1;
    }

    if (0 != MLBmpWrite(outBmpFile, img)) {
        printf("Error writing BMP file. %s\n", outBmpFile);
        PrintUsage();
        return 1;
    }

    return 0;
}
