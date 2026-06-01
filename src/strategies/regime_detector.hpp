#pragma once
#include <deque>
#include <cmath>
#include <numeric>
#include <string>

class RegimeDetector {
private:
    std::deque<double> returns;
    std::deque<double> volumes;
    double high_vol_threshold{0.015};
    double low_vol_threshold{0.005};

    // FIX: add_data() used a `static double last_price` which is shared across
    // ALL instances and ALL symbols — any second RegimeDetector object (or a
    // second symbol) would corrupt the return calculation for every other user.
    // Moved to an instance member with a bool to handle the first-tick case.
    double last_price{0.0};
    bool   has_last_price{false};

    double calculate_volatility() const {
        if (returns.size() < 20) return 0.01;

        double mean = std::accumulate(returns.begin(), returns.end(), 0.0)
                      / static_cast<double>(returns.size());
        double sq_sum = 0;
        for (double r : returns) sq_sum += (r - mean) * (r - mean);

        return std::sqrt(sq_sum / static_cast<double>(returns.size()));
    }

    double calculate_volume_trend() const {
        if (volumes.size() < 20) return 1.0;

        int    half    = static_cast<int>(volumes.size()) / 2;
        double old_avg = 0, new_avg = 0;

        for (int i = 0; i < static_cast<int>(volumes.size()); i++) {
            if (i < half) old_avg += volumes[i];
            else          new_avg += volumes[i];
        }

        old_avg /= half;
        new_avg /= (static_cast<int>(volumes.size()) - half);

        return old_avg > 0 ? new_avg / old_avg : 1.0;
    }

public:
    enum class Regime {
        LOW_VOL_BULL,   // Increase size
        NORMAL_BULL,    // Normal trading
        HIGH_VOL_BEAR,  // Reduce size
        CRASH           // Stop trading
    };

    void add_data(double price, double volume) {
        if (has_last_price && last_price > 0.0) {
            double ret = (price - last_price) / last_price;
            returns.push_back(ret);
            volumes.push_back(volume);

            if (returns.size() > 100) returns.pop_front();
            if (volumes.size() > 100) volumes.pop_front();
        }
        last_price     = price;
        has_last_price = true;
    }

    Regime detect() const {
        double vol          = calculate_volatility();
        double volume_ratio = calculate_volume_trend();

        if (vol > high_vol_threshold * 2)                            return Regime::CRASH;
        if (vol > high_vol_threshold)                                return Regime::HIGH_VOL_BEAR;
        if (vol < low_vol_threshold && volume_ratio > 1.2)           return Regime::LOW_VOL_BULL;
        return Regime::NORMAL_BULL;
    }

    double get_position_multiplier() const {
        switch (detect()) {
            case Regime::LOW_VOL_BULL:  return 1.5;
            case Regime::NORMAL_BULL:   return 1.0;
            case Regime::HIGH_VOL_BEAR: return 0.5;
            case Regime::CRASH:         return 0.0;
            default:                    return 1.0;
        }
    }

    std::string get_regime_name() const {
        switch (detect()) {
            case Regime::LOW_VOL_BULL:  return "LOW_VOL_BULL";
            case Regime::NORMAL_BULL:   return "NORMAL_BULL";
            case Regime::HIGH_VOL_BEAR: return "HIGH_VOL_BEAR";
            case Regime::CRASH:         return "CRASH";
            default:                    return "UNKNOWN";
        }
    }
};
