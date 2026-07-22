#include "PlotFunctions.h"

void FFTPlot(const std::vector<float>& y, const double center_frequency, const double fd)
{
    double df = fd/y.size();
    double freq_start = center_frequency - fd / 2;
    std::vector<float> x(y.size());
    for (size_t i = 0; i < x.size();i++)
    {
        x[i] = static_cast<float>(freq_start + (double)i*df)/1e6;
    }

    static ImPlotAxisFlags xflags = ImPlotAxisFlags_AutoFit;
    static ImPlotAxisFlags yflags = ImPlotAxisFlags_None;

    ImGui::TextUnformatted("X: "); ImGui::SameLine();
    ImGui::CheckboxFlags("Auto fit X axis", (unsigned int*)&xflags, ImPlotAxisFlags_AutoFit);

    ImGui::TextUnformatted("Y: "); ImGui::SameLine();
    ImGui::CheckboxFlags("Auto fit Y axis", (unsigned int*)&yflags, ImPlotAxisFlags_AutoFit);

    if (ImPlot::BeginPlot("Spectrum")) {
        ImPlot::SetupAxes("Frequency, MHz", "y", xflags, yflags);
        ImPlot::PlotLine("Spectrum", x.data(), y.data(), x.size());
        ImPlot::EndPlot();
    }
}
