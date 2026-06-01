# ⚡ Fast Trading Terminal

A **production-ready algorithmic trading terminal** built in C++ with real-time Alpaca integration, multi-strategy support, and comprehensive risk management.

## 🚀 Features

- **Real-time Market Data** - WebSocket streaming from Alpaca
- **Multiple Strategies** - Momentum, Weighted (configurable)
- **Paper Trading** - Full Alpaca paper trading support
- **Risk Management** - Position limits, daily loss limits, trade counters
- **Performance Metrics** - Latency tracking, fill rates, P&L calculation
- **Configurable** - Everything via YAML config file

## 📋 Prerequisites

### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y cmake g++ libcurl4-openssl-dev nlohmann-json3-dev libssl-dev

## How to Run

1. Clone the repository:
```bash
git clone https://github.com/YOUR_USERNAME/algo-trading-terminal.git

Build the project (using CMake):

mkdir build
cd build
cmake ..
cmake --build .

Run the terminal application:

./algo_trader.exe --config ../config.yaml

📈 Strategies
Momentum Strategy
SMA Crossover (20/50 periods)

RSI (Relative Strength Index)

Trailing Stop Loss (2%)

Take Profit (5%)

Max Hold Time (5 minutes)

Weighted Strategy (Advanced)
Multi-indicator weighted scoring system:

Indicator	Weight	Purpose
Momentum	35%	Trend following
RSI	25%	Mean reversion
Volume	20%	Confirmation
VWAP	20%	Intraday reference
Entry threshold: 60%+ | Exit threshold: Below 30%

📈 Performance Metrics
Metric	Typical Value
Tick-to-signal latency	50-200 μs
Order placement	100-500 ms
Processing rate	500-1000 ticks/sec
Memory usage	10-50 MB

📝 License
Personal Use Software License - See LICENSE.txt