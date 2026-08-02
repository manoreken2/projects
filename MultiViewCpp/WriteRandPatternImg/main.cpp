#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <opencv2/ccalib/randpattern.hpp>

static void
PrintUsage(char* progName)
{
    printf("Usage: %s numImg width height pngFileNameWithoutExt\n", progName);
    printf("    numImg      : total image count to write\n");
    printf("    width       : image width in pixel\n");
    printf("    height      : image height in pixel\n");
    printf("    pngFileNameWithoutExt : png file name (without extension) to write.\n");
}

static bool
Run(int n,
    int width,
    int height,
    const char* pngFileNameWithoutExt)
{
    cv::randpattern::RandomPatternGenerator rpg(
        width, height);

    char pngFileName[_MAX_PATH];

    for (int i = 0; i < n; ++i) {
        sprintf_s(pngFileName, "%s%04d.png", pngFileNameWithoutExt, i);

        rpg.generatePattern();
        auto img = rpg.getPattern();

        bool bRv = cv::imwrite(pngFileName, img);

        if (!bRv) {
            printf("Error: imwrite failed %s\n", pngFileName);
            return false;
        } else {
            printf("Wrote %s\n", pngFileName);
        }
    }

    return true;
}

int
main(int argc, char *argv[])
{
    if (argc != 5) {
        PrintUsage(argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    if (n <= 0) {
        PrintUsage(argv[0]);
        return 1;
    }

    int width = atoi(argv[2]);
    if (width <= 0) {
        PrintUsage(argv[0]);
        return 1;
    }

    int height = atoi(argv[3]);
    if (height <= 0) {
        PrintUsage(argv[0]);
        return 1;
    }

    const char* pngFileName = argv[4];

    bool bRv = Run(n, width, height, pngFileName);

    return bRv ? 0 : 1;
}
