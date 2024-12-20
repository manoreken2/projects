#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <opencv2/ccalib/randpattern.hpp>

static void
PrintUsage(wchar_t* progName)
{
    printf("Usage: %S width height pngFileName\n", progName);
    printf("    width       : image width in pixel\n");
    printf("    height      : image height in pixel\n");
    printf("    pngFileName : png file name to write.\n");
}

static bool
Run(int width,
    int height,
    const wchar_t* pngFileName)
{
    cv::randpattern::RandomPatternGenerator::RandomPatternGenerator rpg(
        width, height);

    rpg.generatePattern();
    auto img = rpg.getPattern();


    return true;
}

int
wmain(int argc, wchar_t *argv[])
{
    if (argc != 4) {
        PrintUsage(argv[0]);
        return 1;
    }

    int width = _wtoi(argv[1]);
    if (width <= 0) {
        PrintUsage(argv[0]);
        return 1;
    }

    int height = _wtoi(argv[2]);
    if (height <= 0) {
        PrintUsage(argv[0]);
        return 1;
    }

    const wchar_t* pngFileName = argv[3];


    bool bRv = Run(width, height, pngFileName);

    return bRv ? 0 : 1;
}
