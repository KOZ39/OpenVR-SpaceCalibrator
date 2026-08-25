#pragma once

#include <vector>

namespace spacecal {
template <typename T>
class TimeSeries {
public:
    struct TimeSeriesSample {
        double time;
        T value;
    };
    const std::vector<TimeSeriesSample>& data() const { return m_data; }

    void push(double currentTime, double timeSpan, const T& data)
    {
        m_data.push_back(TimeSeriesSample { .time = currentTime, .value = data });

        double cutoff = currentTime - timeSpan;
        size_t eraseCount = 0;
        while (eraseCount < m_data.size() && m_data[eraseCount].time < cutoff) {
            eraseCount++;
        }

        if (eraseCount > 0) {
            m_data.erase(m_data.begin(), m_data.begin() + eraseCount);
        }
    }

    int size() const { return static_cast<int>(m_data.size()); }
    const TimeSeriesSample& operator[](int index) const { return m_data[index]; }

    T last() const
    {
        return !m_data.empty() ? m_data.back().value : T {};
    }

    double lastTs() const
    {
        return !m_data.empty() ? m_data.back().time : -1.0;
    }

    void clear()
    {
        m_data.clear();
    }

private:
    std::vector<TimeSeriesSample> m_data;
};
} // spacecal