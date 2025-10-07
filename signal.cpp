#include "signal.h"


OpenDAQSignal::OpenDAQSignal(daq::SignalPtr signal, float seconds_shown, int max_points)
{
    signal_name_ = signal.getName().toStdString();
    if (signal.getDescriptor().assigned() && signal.getDescriptor().getUnit().assigned() && signal.getDescriptor().getUnit().getSymbol().assigned())
        signal_unit_ = signal.getDescriptor().getUnit().getSymbol().toStdString();
    else
        signal_unit_ = "";

    if (!signal.getDescriptor().assigned())
    {
        return;
    }
    
    has_domain_signal_ = signal.getDomainSignal().assigned();
    tick_resolution_ = has_domain_signal_ ? signal.getDomainSignal().getDescriptor().getTickResolution() : signal.getDescriptor().getTickResolution();
    float samples_per_second;
    try { samples_per_second = std::max<daq::Int>(1, daq::reader::getSampleRate(has_domain_signal_ ? signal.getDomainSignal().getDescriptor() : signal.getDescriptor())); } catch (...) { samples_per_second = 1; }
    samples_per_plot_sample_ = std::floor(std::max(1.0f, (float)samples_per_second * seconds_shown / (float)max_points));
    
    reader_ = daq::StreamReaderBuilder()
        .setSignal(signal)
        .setValueReadType(has_domain_signal_ ? daq::SampleType::Float64 : daq::SampleType::Int64)
        .setDomainReadType(daq::SampleType::Int64)
        .setSkipEvents(true)
        .build();

    plot_values_avg_ = std::vector<double>(max_points);
    plot_values_min_ = std::vector<double>(max_points);
    plot_values_max_ = std::vector<double>(max_points);
    plot_times_seconds_ = std::vector<double>(max_points);
    
    start_time_ = -1;
}

void OpenDAQSignal::Update()
{
    static size_t READ_BUFFER_SIZE = 1024 * 10;
    static std::vector<double> read_values(READ_BUFFER_SIZE);
    static std::vector<int64_t> read_times(READ_BUFFER_SIZE);

    if (reader_ == nullptr || !reader_.assigned())
        return;

    while (true)
    {
        daq::SizeT read_count = READ_BUFFER_SIZE;
        if (has_domain_signal_)
            reader_.readWithDomain(read_values.data(), read_times.data(), &read_count);
        else
            reader_.read(read_values.data(), &read_count);
        if (read_count == 0)
            break;
        
        if (start_time_ == -1)
            start_time_ = read_times[0];

        for (size_t i = 0, read_pos = 0; i + samples_per_plot_sample_ < read_count; i += samples_per_plot_sample_)
        {
            plot_times_seconds_[pos_in_plot_buffer_] = (read_times[read_pos] - start_time_) * tick_resolution_.getNumerator() / (double)tick_resolution_.getDenominator();
            plot_values_avg_[pos_in_plot_buffer_] = 0;
            plot_values_min_[pos_in_plot_buffer_] = 1e30;
            plot_values_max_[pos_in_plot_buffer_] = -1e30;
            for (size_t j = 0; j < samples_per_plot_sample_; ++j, ++read_pos)
            {
                plot_values_avg_[pos_in_plot_buffer_] += read_values[read_pos];
                plot_values_min_[pos_in_plot_buffer_] = std::min(read_values[read_pos], plot_values_min_[pos_in_plot_buffer_]);
                plot_values_max_[pos_in_plot_buffer_] = std::max(read_values[read_pos], plot_values_max_[pos_in_plot_buffer_]);
            }
            plot_values_avg_[pos_in_plot_buffer_] = plot_values_avg_[pos_in_plot_buffer_] / samples_per_plot_sample_;

            end_time_seconds_ = plot_times_seconds_[pos_in_plot_buffer_];
            pos_in_plot_buffer_ += 1; if (pos_in_plot_buffer_ >= plot_values_avg_.size()) pos_in_plot_buffer_ = 0;
            points_in_plot_buffer_ = std::min(points_in_plot_buffer_ + 1, plot_values_avg_.size());
        }
    }
}
