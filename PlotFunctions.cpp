#include "PlotFunctions.h"

void FFTPlot(const std::vector<float>& y, const double center_frequency, const double fd, bool enable_low_frequency, double receiver_gain , bool marker_to_peak)
{
    if (y.empty())
        return;
    const double center_frequency_khz = center_frequency / 1e3;
    double min_frequency_khz = center_frequency_khz - 10.0;
    double max_frequency_khz = center_frequency_khz + 10.0;

    const double df = fd / static_cast<double>(y.size());
    /*const double nyquist_khz = fd / 2e3;
    const double clamped_display_width_khz = std::clamp(display_width_khz, 0.0, nyquist_khz);
    const double axis_half_width_khz = std::max(clamped_display_width_khz, df / 2e3);*/
   
    /*const double min_frequency_khz = center_frequency_khz - axis_half_width_khz;
    const double max_frequency_khz = center_frequency_khz + axis_half_width_khz;*/

    std::vector<float> x(y.size());
    std::vector<float> x_calib(y.size());
    for (size_t i = 0; i < x.size(); ++i)
    {
        x[i] = static_cast<float>((center_frequency - fd / 2.0 + static_cast<double>(i) * df) / 1e3);
        x_calib[i] = x[i]/1e3;
    }
    
    // Калибровка приёмника
    auto coeffs = log_lin_interp(x_calib);
    std::vector<float> y_calib(y.size());
    for (size_t i = 0; i < y.size(); ++i)
    {
        y_calib[i] = y[i] + coeffs[i] - receiver_gain;
    }
    // Вычисления для маркера на максимальное значение
    int index = -100;
    double peak_x = 0.0;
    double peak_y = 0.0;
    if (marker_to_peak)
    {
        index = std::distance(y_calib.begin(), std::max_element(y_calib.begin(), y_calib.end()));
        peak_x = x[index];
        peak_y = y_calib[index];
    }
    static ImPlotAxisFlags yflags = ImPlotAxisFlags_None;
    static ImPlotAxisFlags xflags = ImPlotAxisFlags_AutoFit;

    ImVec2 available_space = ImGui::GetContentRegionAvail();

    // Force the plot to fill the entire width and most of the available height.
    ImVec2 plot_size = ImVec2(available_space.x, available_space.y * 0.9f);

    ImGui::TextUnformatted("Y: "); ImGui::SameLine();
    ImGui::CheckboxFlags("Auto fit Y axis", (unsigned int*)&yflags, ImPlotAxisFlags_AutoFit);
    ImGui::TextUnformatted("X: "); ImGui::SameLine();
    ImGui::CheckboxFlags("Auto fit X axis", (unsigned int*)&xflags, ImPlotAxisFlags_AutoFit);

    if (ImPlot::BeginPlot("Spectrum", plot_size, ImPlotFlags_NoLegend)) {
        ImPlot::SetupAxes("Frequency, kHz", "Power Spectrum, dBm", xflags, yflags);
        if (enable_low_frequency)
            ImPlot::SetupAxisLimits(ImAxis_X1, min_frequency_khz, max_frequency_khz, ImGuiCond_Always);
        ImPlotSpec line_spec;
        line_spec.LineColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);

        ImPlot::PlotLine("Spectrum", x.data(), y_calib.data(), static_cast<int>(x.size()), line_spec);

        if (!enable_low_frequency)
            ImPlot::Annotation(0.0, 0.0, ImVec4(0.15f, 0.15f, 0.15f, 1.0f), ImVec2(10, 10), true, "Center frequency: %.2f MHz \n Frequency span: %.2f MHz", center_frequency/1e6, fd/1e6);
        else
            ImPlot::Annotation(0.0, 0.0, ImVec4(0.15f, 0.15f, 0.15f, 1.0f), ImVec2(10, 10), true, "Center frequency: %.2f MHz \n Frequency span: 20 kHz", center_frequency / 1e6);

        if (marker_to_peak)
        {
            ImPlotSpec marker_spec;
            marker_spec.Marker = ImPlotMarker_Circle;
            marker_spec.MarkerSize = 6.0f;
            marker_spec.MarkerFillColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            marker_spec.Flags = ImPlotItemFlags_NoLegend;
            ImPlot::PlotScatter("Peak", &peak_x, &peak_y, 1, marker_spec);

            // Подпись над маркером
            ImPlot::Annotation(peak_x, peak_y, ImVec4(0.15f, 0.15f, 0.15f, 1.0f), ImVec2(0, -10), true, "Max: %.2f", peak_y);
        }

        ImPlot::EndPlot();
    }
}
