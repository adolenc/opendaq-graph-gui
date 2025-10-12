#pragma once

#include <opendaq/opendaq.h>
#include <vector>
#include <string>


class OpenDAQSignal
{
public:
    OpenDAQSignal() {};
    OpenDAQSignal(daq::SignalPtr signal, float seconds_shown, int max_points);
    void Update();

    std::vector<double> plot_values_avg_;
    std::vector<double> plot_values_min_;
    std::vector<double> plot_values_max_;
    std::vector<double> plot_times_seconds_;
    double end_time_seconds_ = 0;
    size_t pos_in_plot_buffer_ = 0;
    size_t points_in_plot_buffer_ = 0;

    std::string signal_name_{""};
    std::string signal_id_{""};
    std::string signal_unit_{""};
    float value_range_min_ = -5.0f;
    float value_range_max_ = 5.0f;
    bool has_domain_signal_;

private:
    daq::StreamReaderPtr reader_;
    daq::RatioPtr tick_resolution_;
    int64_t start_time_{-1};
    int leftover_samples_{0};
    float samples_per_plot_sample_ = 1.0f;

    static constexpr size_t READ_BUFFER_SIZE = 1024 * 10;
    std::vector<double> read_values;
    std::vector<int64_t> read_times;
};
