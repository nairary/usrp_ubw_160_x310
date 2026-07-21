#pragma once
#include "imgui/implot.h"
#include <algorithm>
#include <vector>
#include <ranges>

#include <iostream>

void FFTPlot(std::vector<float>& y, const double center_frequency, const double fd);