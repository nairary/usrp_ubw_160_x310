#pragma once
#include "imgui/implot.h"
#include "Core.h"
#include <algorithm>
#include <vector>
#include <ranges>

#include <iostream>

void FFTPlot(const std::vector<float>& y, const double center_frequency, const double fd, bool enable_low_frequency = false, double receiver_gain = 0.0, bool marker_to_peak = false);
