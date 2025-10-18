#pragma once

#include <opendaq/opendaq.h>
#include <vector>
#include <string>


enum class SignalType
{
    DomainOnly,
    DomainAndValue
};

class OpenDAQSignal
{
public:
    OpenDAQSignal() {};
    OpenDAQSignal(daq::SignalPtr signal, float seconds_shown, int max_points);
    void Update();
    void RebuildIfInvalid(daq::SignalPtr signal, float seconds_shown, int max_points);
    void RebuildIfInvalid(daq::SignalPtr signal);
    void RebuildIfInvalid();

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
    SignalType signal_type_;

private:
    void ReadDomainAndValue();
    void ReadDomainOnly();

    daq::ReaderPtr reader_;
    daq::SignalPtr signal_;
    daq::RatioPtr tick_resolution_;
    int64_t start_time_{-1};
    int leftover_samples_{0};
    int samples_per_plot_sample_ = 1;

    static constexpr size_t READ_BUFFER_SIZE = 1024 * 10;
    std::vector<double> read_values;
    std::vector<int64_t> read_times;
};
