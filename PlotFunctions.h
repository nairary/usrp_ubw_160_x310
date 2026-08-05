#pragma once
#include "imgui/implot.h"
#include <algorithm>
#include <vector>
#include <ranges>

#include <iostream>

void FFTPlot(const std::vector<float>& y, const double fd, const double display_width_khz);
