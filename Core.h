#pragma once

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <stdio.h>
#include <cstddef>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

constexpr int DEFAULT_FFT_EXPONENT = 15;
constexpr int MIN_FFT_EXPONENT = 11;
constexpr int MAX_FFT_EXPONENT = 16;
constexpr size_t DEFAULT_FFT_SIZE = size_t{1} << DEFAULT_FFT_EXPONENT;

