#pragma once

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <stdio.h>
#include <cstddef>
#include <vector>
#include <cmath>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

constexpr int DEFAULT_FFT_EXPONENT = 15;
constexpr int MIN_FFT_EXPONENT = 11;
constexpr int MAX_FFT_EXPONENT = 16;
constexpr size_t DEFAULT_FFT_SIZE = size_t{1} << DEFAULT_FFT_EXPONENT;
const std::vector<double> FREQUENCIES = std::vector<double>{5, 10, 60, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000, 2100, 2200,
2300, 2400, 2500, 2600, 2700, 2800, 2900, 3000, 3100, 3200, 3300, 3400, 3500, 3600, 3700, 3800, 3900, 4000, 4100, 4200, 4300, 4400, 4500, 4600, 4700, 4800, 4900,
5000, 5100, 5200, 5300, 5400, 5500, 5600, 5700, 5800, 5900, 6000, 6100 };



const std::vector<double> CALIBRATION_COEFFICIENTS = std::vector<double>{ -89.25,-89.25, -85.34, -85.26,-83.87,-84.35,-84.12,-90.93,-91.42,-90.74,-88.92,-88.76,-88.78,-86.92,-87.25,-86.40,-84.09,-88.26,-87.70,-86.66,-87.57,-87.00,-84.34,-85.24,-84.23,-82.33,-83.05,-81.93,-80.66,-81.13,-80.41,-78.67,-79.06,-79.49,-79.38,
-79.98,-79.33,-78.71,-80.38,-79.19,-77.03,-77.82,-76.49,-75.89,-75.33,-73.47,-73.95,-73.14,-72.52,-72.86,-72.66,-72.43,-72.73,-73.22,-71.93,-72.42,-72.88,-70.58,-71.29,-70.49,-67.79,-66.98,-63.42 };

template<typename T>
std::vector<T> log_lin_interp(std::vector<T> frequencies)
{
	std::vector<T> result(frequencies.size());
	for (int i = 0; i < result.size(); i++)
	{
		T f = frequencies[i];
		for (int j = 0; j < FREQUENCIES.size()-1; j++)
		{
			if (f >= FREQUENCIES[j] && f < FREQUENCIES[j + 1])
			{
				T a = CALIBRATION_COEFFICIENTS[j];
				T b = CALIBRATION_COEFFICIENTS[j + 1];
				T f_a = FREQUENCIES[j];
				T f_b = FREQUENCIES[j + 1];
				result[i] = (b*std::log10(f/f_a) + a*std::log10(f_b/f))/std::log10(f_b / f_a);
				break;
			}
		}
	}
	return result;
}