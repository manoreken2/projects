#pragma once

struct MLVideoTime {
    int hour;
    int min;
    int sec;
    int frame;
};

MLVideoTime MLFrameNrToTime(const int framesPerSec, const int frameNr);
