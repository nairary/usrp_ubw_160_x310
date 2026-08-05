#include "PlotFunctions.h"

void FFTPlot(const std::vector<float>& y, const double fd, const double display_width_khz)
{
    if (y.empty())
        return;

    const double df = fd / static_cast<double>(y.size());
    const double nyquist_khz = fd / 2e3;
    const double clamped_display_width_khz = std::clamp(display_width_khz, 0.0, nyquist_khz);
    const double axis_half_width_khz = std::max(clamped_display_width_khz, df / 2e3);
    const double min_frequency_khz = -axis_half_width_khz;
    const double max_frequency_khz = axis_half_width_khz;

    std::vector<float> x(y.size());
    for (size_t i = 0; i < x.size(); ++i)
    {
        x[i] = static_cast<float>((-fd / 2.0 + static_cast<double>(i) * df) / 1e3);
    }

    static ImPlotAxisFlags yflags = ImPlotAxisFlags_None;

    ImVec2 available_space = ImGui::GetContentRegionAvail();

    // Force the plot to fill the entire width and most of the available height.
    ImVec2 plot_size = ImVec2(available_space.x, available_space.y * 0.9f);

    ImGui::TextUnformatted("Y: "); ImGui::SameLine();
    ImGui::CheckboxFlags("Auto fit Y axis", (unsigned int*)&yflags, ImPlotAxisFlags_AutoFit);

    if (ImPlot::BeginPlot("Spectrum", plot_size)) {
        ImPlot::SetupAxes("Frequency offset, kHz", "y", ImPlotAxisFlags_None, yflags);
        ImPlot::SetupAxisLimits(ImAxis_X1, min_frequency_khz, max_frequency_khz, ImGuiCond_Always);
        ImPlot::PlotLine("Spectrum", x.data(), y.data(), static_cast<int>(x.size()));
        ImPlot::EndPlot();
    }
}
