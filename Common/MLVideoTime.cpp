#include "MLVideoTime.h"
#include <string.h>

MLVideoTime MLFrameNrToTime(const int framesPerSec, const int frameNr) {
    MLVideoTime r;
    memset(&r, 0, sizeof r);

    if (framesPerSec == 0) {
        return r;
    }

    r.frame = frameNr % framesPerSec;
    const int totalSec = frameNr / framesPerSec;

    r.hour = totalSec / 3600;
    int remain = totalSec - r.hour * 3600;
    r.min = remain / 60;
    remain -= r.min * 60;
    r.sec = remain;

    return r;
}

