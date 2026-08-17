#include "one_euro_filter.h"
#include <algorithm>

namespace spacecal {
    double LowPassFilter::filter(double x, double alpha) {
        if (bFirstTime) {
            bFirstTime = false;
            hatxprev = x;
        }
        double hatx = alpha * x + (1.0 - alpha) * hatxprev;
        hatxprev = hatx;
        return hatx;
    }

    // time is total time in seconds, not deltatime
    double OneEuro1D::filter(double x, double time) {
        double dx = 0.0;
        if (bFirstTime) {
            bFirstTime = false;
        }
        else {
            dx = (x - xfilt.hatxprev) * rate;
        }
        double edx = dxfilt.filter(dx, alpha(rate, dcutoff));
        double cutoff = mincutoff + beta * std::abs(edx);
        return xfilt.filter(x, alpha(rate, cutoff));
    }

    double OneEuro1D::alpha(double rate, double cutoff) {
        double tau = 1.0 / (2.0 * M_PI * cutoff);
        double te = 1.0 / rate;
        return 1.0 / (1.0 + tau / te);
    }
}