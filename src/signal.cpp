#include "signal.h"
#include "utils.h"


OpenDAQSignal::OpenDAQSignal(daq::SignalPtr signal, float seconds_shown, int max_points)
    : seconds_shown_(seconds_shown)
    , max_points_(max_points)
{
    RebuildIfInvalid(signal, seconds_shown, max_points);
}

void OpenDAQSignal::UpdateConfiguration(float seconds_shown, int max_points)
{
    if (std::abs(seconds_shown - seconds_shown_) < 1e-5 && max_points == max_points_)
        return;

    RebuildIfInvalid(signal_, seconds_shown, max_points);
}

void OpenDAQSignal::RebuildIfInvalid(daq::SignalPtr signal, float seconds_shown, int max_points)
{
    seconds_shown_ = seconds_shown;
    max_points_ = max_points;

    signal_ = signal;
    reader_ = nullptr;
    pos_in_plot_buffer_ = 0;
    start_time_ = -1;
    points_in_plot_buffer_ = 0;
    end_time_seconds_ = 0;

    signal_name_ = signal.getName().toStdString();
    signal_id_ = signal.getGlobalId().toStdString();
    if (signal.getDescriptor().assigned() && signal.getDescriptor().getUnit().assigned() && signal.getDescriptor().getUnit().getSymbol().assigned())
        signal_unit_ = signal.getDescriptor().getUnit().getSymbol().toStdString();
    else
        signal_unit_ = "";

    if (!signal.getDescriptor().assigned())
        return;
    
    if (signal.getDomainSignal().assigned())
        signal_type_ = SignalType::DomainAndValue;
    else
        signal_type_ = SignalType::DomainOnly;

    data_size_ = 1;
    axes_.clear();
    for (const daq::DimensionPtr& dimension : signal.getDescriptor().getDimensions())
    {
        Axis axis;
        axis.name_ = dimension.getName().toStdString();
        if (dimension.getUnit().assigned() && dimension.getUnit().getSymbol().assigned())
            axis.unit_ = dimension.getUnit().getSymbol().toStdString();
        else
            axis.unit_ = "";
        daq::ListPtr<daq::IBaseObject> labels = dimension.getLabels();
        size_t dim_size = labels.getCount();

        if (dim_size > 0)
        {
            data_size_ *= dim_size;
            auto first_label = labels[0];
            if (first_label.getCoreType() == daq::CoreType::ctString)
            {
                std::vector<std::string> label_strs;
                for (const auto& label : labels)
                    label_strs.push_back(daq::StringPtr(label).toStdString());
                axis.values_ = label_strs;
            }
            else
            {
                std::vector<double> label_floats;
                for (const auto& label : labels)
                {
                     if (label.getCoreType() == daq::CoreType::ctFloat)
                        label_floats.push_back((double)daq::FloatPtr(label));
                     else if (label.getCoreType() == daq::CoreType::ctInt)
                        label_floats.push_back((double)daq::IntegerPtr(label));
                     else
                        label_floats.push_back(0.0);
                }
                axis.values_ = label_floats;
            }
        }
        axes_.push_back(axis);
    }

    try
    {
        tick_resolution_ = signal.getDomainSignal().assigned()
                         ? signal.getDomainSignal().getDescriptor().getTickResolution()
                         : signal.getDescriptor().getTickResolution();
    } catch (...)
    {
        return;
    }
    float samples_per_second = 1;
    try
    {
        daq::Int rate = (daq::Int)daq::reader::getSampleRate(signal.getDomainSignal().assigned() ? signal.getDomainSignal().getDescriptor() : signal.getDescriptor());
        samples_per_second = (float)std::max<daq::Int>(1, rate);
    } catch (...)
    {
    }
    samples_per_plot_sample_ = std::max<int>(1, (int)std::ceil((double)samples_per_second * seconds_shown / (float)max_points));

    if (auto value_range = signal.getDescriptor().getValueRange(); value_range.assigned())
    {
        value_range_min_ = value_range.getLowValue();
        value_range_max_ = value_range.getHighValue();
    }

    if (!axes_.empty())
    {
        // we are only reading the last sample for multi-dimensional signals, but we use the stream reader
        // because TailReader keeps thinking it has 1 sample available so it is just mindlessly rewriting
        // the read array over and over again
        plot_values_avg_ = std::vector<double>(data_size_);
        reader_ = daq::StreamReaderBuilder()
            .setSignal(signal)
            .setSkipEvents(false)
            .setValueReadType(daq::SampleType::Float64)
            .build();
    }
    else
    {
        read_values = std::vector<double>(READ_BUFFER_SIZE);
        read_times = std::vector<int64_t>(READ_BUFFER_SIZE);

        plot_values_avg_ = std::vector<double>(max_points);
        plot_values_min_ = std::vector<double>(max_points);
        plot_values_max_ = std::vector<double>(max_points);
        plot_times_seconds_ = std::vector<double>(max_points);

        leftover_samples_ = 0;

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
            // for now we also only read the last sample for domain-only signals
            reader_ = daq::TailReaderBuilder()
                .setHistorySize(1)
                .setSignal(signal)
                .setSkipEvents(true)
                .setValueReadType(daq::SampleType::Int64)
                .build();
        }
    }
}

void OpenDAQSignal::RebuildIfInvalid()
{
    RebuildIfInvalid(signal_);
}

void OpenDAQSignal::RebuildIfInvalid(daq::SignalPtr signal)
{
    if (reader_ != nullptr && reader_.assigned())
        return;

    RebuildIfInvalid(signal, seconds_shown_, max_points_);
}

void OpenDAQSignal::Update()
{
    if (reader_ == nullptr || !reader_.assigned())
        return;

    if (!axes_.empty())
    {
        ReadMultiDimensional();
    }
    else if (signal_type_ == SignalType::DomainAndValue)
    {
        ReadDomainAndValue();
    }
    else
    {
        ReadDomainOnly();
    }
}

void OpenDAQSignal::ReadMultiDimensional()
{
    while (true)
    {
        daq::SizeT read_count = 1;
        daq::ReaderStatusPtr status = daq::StreamReaderPtr(reader_).read(plot_values_avg_.data(), &read_count);
        // The first event is gonna be descriptor changed so we ignore it and just naively assume we already have the correct descriptor,
        // but we have to rebuild the reader on subsequent events
        if (status.getReadStatus() == daq::ReadStatus::Event && start_time_ != -1)
        {
            reader_.release();
            reader_ = nullptr;
            RebuildIfInvalid();
            return;
        }
        start_time_ = 0;
        if (read_count == 0)
            break;
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
            for (size_t j = 0; j < (size_t)samples_per_plot_sample_; ++j, ++read_pos)
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
        int new_leftover_samples = (int)read_count - (int)read_samples_evaluated;
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
