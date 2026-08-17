#pragma once

#include "constants.h"
#include <inttypes.h>

namespace spacecal {
    // https://gery.casiez.net/1euro/
    class LowPassFilter {
    public:
        double filter(double x, double alpha);

        bool bFirstTime = true;
        double hatxprev = 0.0;
    };

    class OneEuro1D {
    public:
        // time is total time in seconds, not deltatime
        double filter(double x, double time);
        double alpha(double rate, double cutoff);

        bool bFirstTime = true;
        double rate = 30.0;
        double mincutoff = 1.0;
        double beta = 0.0;
        double dcutoff = 1.0;
        LowPassFilter xfilt;
        LowPassFilter dxfilt;
    };
}