#include "signal.h"
#include "utils.h"


OpenDAQSignal::OpenDAQSignal(daq::SignalPtr signal, float seconds_shown, int max_points)
{
    RebuildIfInvalid(signal, seconds_shown, max_points);
}

void OpenDAQSignal::RebuildIfInvalid(daq::SignalPtr signal, float seconds_shown, int max_points)
{
    signal_ = signal;
    reader_ = nullptr;
    pos_in_plot_buffer_ = 0;
    int64_t start_time_ = -1;
    points_in_plot_buffer_ = 0;
    end_time_seconds_ = 0;

    signal_name_ = signal.getName().toStdString();
    signal_id_ = signal.getGlobalId().toStdString();
    if (signal.getDescriptor().assigned() && signal.getDescriptor().getUnit().assigned() && signal.getDescriptor().getUnit().getSymbol().assigned())
        signal_unit_ = signal.getDescriptor().getUnit().getSymbol().toStdString();
    else
        signal_unit_ = "";

    if (!signal.getDescriptor().assigned())
    {
        return;
    }
    
    if (signal.getDomainSignal().assigned())
        signal_type_ = SignalType::DomainAndValue;
    else
        signal_type_ = SignalType::DomainOnly;

    tick_resolution_ = signal.getDomainSignal().assigned() ? signal.getDomainSignal().getDescriptor().getTickResolution() : signal.getDescriptor().getTickResolution();
    float samples_per_second;
    try { samples_per_second = std::max<daq::Int>(1, daq::reader::getSampleRate(signal.getDomainSignal().assigned() ? signal.getDomainSignal().getDescriptor() : signal.getDescriptor())); } catch (...) { samples_per_second = 1; }
    samples_per_plot_sample_ = std::max<int>(1, std::floor(samples_per_second * seconds_shown / (float)max_points));

    if (auto value_range = signal.getDescriptor().getValueRange(); value_range.assigned())
    {
        value_range_min_ = value_range.getLowValue();
        value_range_max_ = value_range.getHighValue();
    }
    if (signal_type_ == SignalType::DomainAndValue)
    {
        reader_ = daq::StreamReaderBuilder()
            .setSignal(signal)
            .setSkipEvents(true)
            .setValueReadType(daq::SampleType::Float64)
            .setDomainReadType(daq::SampleType::Int64)
            .build();
    }
    else
    {
        // for now we only read the last sample for domain-only signals
        reader_ = daq::TailReaderBuilder()
            .setHistorySize(1)
            .setSignal(signal)
            .setSkipEvents(true)
            .setValueReadType(daq::SampleType::Int64)
            .build();
    }

    read_values = std::vector<double>(READ_BUFFER_SIZE);
    read_times = std::vector<int64_t>(READ_BUFFER_SIZE);

    plot_values_avg_ = std::vector<double>(max_points);
    plot_values_min_ = std::vector<double>(max_points);
    plot_values_max_ = std::vector<double>(max_points);
    plot_times_seconds_ = std::vector<double>(max_points);

    leftover_samples_ = 0;
    
    start_time_ = -1;
}

void OpenDAQSignal::RebuildIfInvalid()
{
    RebuildIfInvalid(signal_);
}

void OpenDAQSignal::RebuildIfInvalid(daq::SignalPtr signal)
{
    if (reader_ != nullptr && reader_.assigned())
        return;

    float seconds_shown = 5.0;
    int max_points = 2000;
    RebuildIfInvalid(signal, seconds_shown, max_points);
}

void OpenDAQSignal::Update()
{
    if (reader_ == nullptr || !reader_.assigned())
        return;

    if (signal_type_ == SignalType::DomainAndValue)
    {
        ReadDomainAndValue();
    }
    else
    {
        ReadDomainOnly();
    }
}

void OpenDAQSignal::ReadDomainAndValue()
{
    while (true)
    {
        daq::SizeT read_count = READ_BUFFER_SIZE - leftover_samples_;
        castTo<daq::IStreamReader>(reader_)->readWithDomain(read_values.data() + leftover_samples_, read_times.data() + leftover_samples_, &read_count);

        if (read_count == 0)
            break;

        read_count += leftover_samples_;
        
        if (start_time_ == -1)
            start_time_ = read_times[0];

        size_t read_samples_evaluated, read_pos;
        for (read_samples_evaluated = 0, read_pos = 0; read_samples_evaluated + samples_per_plot_sample_ < read_count; read_samples_evaluated += samples_per_plot_sample_)
        {
            plot_times_seconds_[pos_in_plot_buffer_] = read_times[read_pos] * tick_resolution_.getNumerator() / (double)tick_resolution_.getDenominator();
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
        int new_leftover_samples = read_count - read_samples_evaluated;
        for (int j = 0; j < new_leftover_samples; ++j, ++read_pos)
        {
            if (signal_type_ == SignalType::DomainAndValue)
                read_values[j] = read_values[read_pos];
            read_times[j] = read_times[read_pos];
        }
        leftover_samples_ = new_leftover_samples;
    }
}

void OpenDAQSignal::ReadDomainOnly()
{
    daq::SizeT read_count = 1;
    castTo<daq::ITailReader>(reader_)->read(read_times.data() + leftover_samples_, &read_count);
    if (read_count == 0)
        return;
    
    if (start_time_ == -1)
        start_time_ = read_times[0];

    plot_times_seconds_[pos_in_plot_buffer_] = read_times[0] * tick_resolution_.getNumerator() / (double)tick_resolution_.getDenominator();
    plot_values_avg_[pos_in_plot_buffer_] = 0;
    plot_values_min_[pos_in_plot_buffer_] = 0;
    plot_values_max_[pos_in_plot_buffer_] = 0;

    end_time_seconds_ = plot_times_seconds_[pos_in_plot_buffer_];
    pos_in_plot_buffer_ += 1; if (pos_in_plot_buffer_ >= plot_values_avg_.size()) pos_in_plot_buffer_ = 0;
    points_in_plot_buffer_ = std::min(points_in_plot_buffer_ + 1, plot_values_avg_.size());
}
