#pragma once
#include <deque>
#include <algorithm>
#include <cstddef>

// KellyPositionSizer is structurally sound. No bug fixes required here.
// Included as-is with minor style cleanup (explicit std:: qualifiers).

class KellyPositionSizer {
private:
    std::deque<double> returns;
    size_t max_history{100};

public:
    void add_return(double pnl_percent) {
        returns.push_back(pnl_percent);
        if (returns.size() > max_history) returns.pop_front();
    }

    double calculate_kelly() {
        if (returns.size() < 20) return 0.02;  // Default 2% risk until sample is large enough

        double wins = 0, losses = 0;
        double total_win = 0, total_loss = 0;

        for (double r : returns) {
            if (r > 0) { wins++;   total_win  += r;  }
            else        { losses++; total_loss += -r; }
        }

        double win_rate = wins / static_cast<double>(returns.size());
        double avg_win  = wins   > 0 ? total_win  / wins   : 0.0;
        double avg_loss = losses > 0 ? total_loss / losses : 0.0;

        if (avg_loss == 0.0) return 0.25;

        double b     = avg_win / avg_loss;
        double kelly = (win_rate * b - (1.0 - win_rate)) / b;

        // Conservative: half-Kelly, capped at 15% per trade
        double half_kelly = kelly / 2.0;
        return std::max(0.01, std::min(0.15, half_kelly));
    }

    double calculate_position_size(double capital, double entry_price,
                                   double stop_loss_pct = 0.02) {
        double risk_per_trade = calculate_kelly();
        double risk_amount    = capital * risk_per_trade;
        double stop_distance  = entry_price * stop_loss_pct;

        if (stop_distance <= 0) return 0;

        double shares     = risk_amount / stop_distance;
        double max_shares = capital * 0.25 / entry_price;  // Max 25% of capital per trade

        return std::min(shares, max_shares);
    }

    void reset() { returns.clear(); }
};