# ASX OUCH Order Entry Handler

Ultra-low latency OUCH (Outbound UDP Connect Handler) order entry plugin for Australian Securities Exchange (ASX) with plugin architecture.

## Features

### Ultra-Low Latency Optimizations
- **Lock-free SPSC ring buffers** for message passing
- **CPU affinity** for dedicated thread isolation
- **Cache-aligned data structures** to prevent false sharing
- **Zero-copy message handling** where possible
- **RDTSC-based timestamping** for nanosecond precision
- **TCP_NODELAY and socket optimizations**
- **Memory prefaulting and huge pages support**
- **Branch prediction optimizations**

### Plugin Architecture
- **Dynamic loading** of OUCH handlers via shared libraries
- **Hot-swappable plugins** without system restart
- **Plugin manager** for lifecycle management
- **Event-driven callbacks** for order state changes
- **Configurable plugin parameters**

### OUCH Protocol Support
- Complete **OUCH 4.2 message set** for ASX
- **Message validation** and error handling
- **Order tracking** with state management
- **Automatic reconnection** and session management
- **Heartbeat handling** for connection monitoring

### Performance Monitoring
- **Real-time latency measurement** (min/avg/max/percentiles)
- **Throughput monitoring** (orders/executions per second)
- **Order statistics** tracking
- **Performance benchmarking tools**
- **Health monitoring** and alerting

## Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Application   │    │  Plugin Manager │    │  OUCH Plugin    │
│                 │    │                 │    │                 │
│  Market Maker   │◄──►│   Load/Unload   │◄──►│  ASX Handler    │
│  Strategy       │    │   Configuration │    │                 │
│                 │    │                 │    │                 │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                                        │
                                                        ▼
                       ┌─────────────────────────────────────────┐
                       │          Network Layer                  │
                       │                                         │
                       │  ┌─────────────┐  ┌─────────────────┐   │
                       │  │ Send Thread │  │ Receive Thread  │   │
                       │  │   CPU 3     │  │     CPU 2       │   │
                       │  └─────────────┘  └─────────────────┘   │
                       │           │              │              │
                       │           ▼              ▼              │
                       │     ┌─────────────────────────────┐     │
                       │     │     TCP Socket (ASX)       │     │
                       │     └─────────────────────────────┘     │
                       └─────────────────────────────────────────┘
```

## Message Flow

```
Order Entry:
Application → Plugin → Send Queue → TCP Socket → ASX

Order Response:
ASX → TCP Socket → Receive Buffer → Message Parser → Event Handler → Application
```

## Building

### Prerequisites
- **C++20 compatible compiler** (GCC 10+, Clang 12+)
- **CMake 3.16+**
- **Linux/macOS** (optimized for Linux)
- **POSIX threads**

### Quick Build
```bash
./build.sh release
```

### Build Options
```bash
./build.sh clean      # Clean build
./build.sh debug      # Debug build
./build.sh release    # Release build (default)
./build.sh performance # Performance optimized build
```

### Manual Build
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## Usage

### Basic Example
```cpp
#include "ouch_plugin_manager.hpp"

using namespace asx::ouch;

// Create and initialize plugin
auto plugin = std::make_unique<ASXOUCHOrderHandler>();
plugin->initialize(config_json);

// Create event handler
class MyEventHandler : public IOrderEventHandler {
    void onOrderAccepted(const OrderAcceptedMessage& msg) override {
        // Handle order acceptance
    }
    // ... implement other callbacks
};

auto handler = std::make_shared<MyEventHandler>();
plugin->registerEventHandler(handler);

// Send orders
auto order = OrderBuilder()
    .setOrderToken("ORDER001")
    .setSide(Side::BUY)
    .setQuantity(1000)
    .setInstrument("BHP.AX")
    .setPrice(4500)
    .setTimeInForce(TimeInForce::DAY)
    .build();

plugin->sendEnterOrder(order);
```

### Market Making Example
```cpp
// Simple market maker strategy
class SimpleMarketMaker {
public:
    void quoteBidAsk(const std::string& instrument, 
                     uint64_t mid_price, uint32_t spread) {
        auto bid = OrderBuilder()
            .setSide(Side::BUY)
            .setPrice(mid_price - spread/2)
            .setInstrument(instrument)
            .build();
            
        auto ask = OrderBuilder()
            .setSide(Side::SELL)
            .setPrice(mid_price + spread/2)
            .setInstrument(instrument)
            .build();
            
        plugin_->sendEnterOrder(bid);
        plugin_->sendEnterOrder(ask);
    }
};
```

## Configuration

### Network Configuration
```json
{
  "network": {
    "server_ip": "203.0.113.10",
    "server_port": 8080,
    "so_rcvbuf_size": 65536,
    "so_sndbuf_size": 65536,
    "tcp_nodelay": 1
  }
}
```

### Performance Tuning
```json
{
  "performance": {
    "cpu_affinity": {
      "receive_thread": 2,
      "send_thread": 3
    },
    "memory": {
      "huge_pages": true,
      "lock_memory": true
    }
  }
}
```

## Performance

### Latency Targets
- **Order submission latency**: < 10 microseconds (95th percentile)
- **Order response latency**: < 50 microseconds (95th percentile)
- **Round-trip latency**: < 100 microseconds (95th percentile)

### Throughput Targets
- **Single-threaded**: 10,000+ orders/second
- **Multi-threaded**: 50,000+ orders/second
- **Peak burst**: 100,000+ orders/second

### Memory Usage
- **Base memory**: ~10MB
- **Per order**: ~200 bytes (with tracking enabled)
- **Message pools**: ~4MB (configurable)

## Testing

### Run Example Application
```bash
cd build
./ouch_example
```

### Performance Testing
```bash
# Basic performance test
./ouch_performance_test

# Custom test parameters
./ouch_performance_test 10000 5000 4
#                      orders rate  threads
```

### Latency Benchmarking
```bash
# Measure latency for 1000 orders
./ouch_performance_test 1000 100 1
```

## Production Deployment

### System Requirements
- **CPU**: Dedicated cores for network threads
- **Memory**: 16GB+ RAM with huge pages
- **Network**: Low-latency network connection to ASX
- **OS**: Linux with RT kernel (recommended)

### Kernel Tuning
```bash
# Isolate CPUs for trading threads
echo "2,3" > /sys/devices/system/cpu/cpu2/cpuset.cpus
echo "2,3" > /sys/devices/system/cpu/cpu3/cpuset.cpus

# Enable huge pages
echo 1024 > /proc/sys/vm/nr_hugepages

# Network tuning
echo 1 > /proc/sys/net/ipv4/tcp_low_latency
echo 65536 > /proc/sys/net/core/rmem_max
echo 65536 > /proc/sys/net/core/wmem_max
```

### Process Priority
```bash
# Run with real-time priority
chrt -f 99 ./ouch_example

# Lock memory to prevent swapping
ulimit -l unlimited
```

## Monitoring

### Real-time Statistics
```cpp
// Get performance metrics
std::cout << "Average Latency: " << plugin->getAverageLatency() << " μs\n";
std::cout << "Orders/sec: " << plugin->getOrdersSent() << "\n";
std::cout << "Acceptance Rate: " 
          << (plugin->getOrdersAccepted() * 100.0 / plugin->getOrdersSent()) 
          << "%\n";
```

### Health Monitoring
```cpp
// Check plugin health
if (!plugin->isReady()) {
    // Handle connection issues
    plugin->reconnect();
}
```

## ASX OUCH Protocol Details

### Supported Message Types

#### Inbound Messages (Client → ASX)
- **Enter Order (O)**: Submit new order
- **Replace Order (U)**: Modify existing order  
- **Cancel Order (X)**: Cancel existing order

#### Outbound Messages (ASX → Client)
- **Order Accepted (A)**: Order accepted by exchange
- **Order Replaced (U)**: Order modification confirmed
- **Order Canceled (C)**: Order cancellation confirmed
- **Order Executed (E)**: Order execution notification
- **Order Rejected (J)**: Order rejection notification
- **Broken Trade (B)**: Trade cancellation
- **Price Tick (P)**: Market data update

### Message Format
All messages use packed binary format with big-endian encoding:

```cpp
struct MessageHeader {
    uint16_t length;      // Message length
    uint8_t message_type; // Message type identifier
    uint64_t timestamp;   // Nanosecond timestamp
};
```

### Order Token Format
14-character alphanumeric identifier:
- Characters 1-4: Firm identifier
- Characters 5-14: Sequence number

## Error Handling

### Connection Errors
- Automatic reconnection with exponential backoff
- Session state recovery
- Message replay capability

### Order Errors
- Validation before submission
- Reject reason codes
- Error statistics tracking

### Network Errors
- TCP connection monitoring
- Heartbeat timeout detection
- Graceful degradation

## Security

### Authentication
- Username/password authentication
- Session-based security
- Encrypted connections (TLS optional)

### Authorization
- Firm-level permissions
- Instrument-level restrictions
- Rate limiting enforcement

### Audit Trail
- Complete order audit log
- Message-level logging
- Performance metrics logging

## API Reference

### Core Classes

#### `IOUCHPlugin`
Main plugin interface for OUCH handlers.

**Methods:**
- `initialize(config)` - Initialize plugin with configuration
- `sendEnterOrder(order)` - Submit new order
- `sendReplaceOrder(replace)` - Modify existing order
- `sendCancelOrder(cancel)` - Cancel existing order
- `registerEventHandler(handler)` - Register event callbacks

#### `OrderBuilder`
Fluent API for order construction.

**Methods:**
- `setOrderToken(token)` - Set order identifier
- `setSide(side)` - Set buy/sell side
- `setQuantity(qty)` - Set order quantity
- `setPrice(price)` - Set order price
- `setInstrument(symbol)` - Set trading instrument

#### `IOrderEventHandler`
Interface for order event callbacks.

**Callbacks:**
- `onOrderAccepted(msg)` - Order accepted by exchange
- `onOrderExecuted(msg)` - Order execution notification
- `onOrderRejected(msg)` - Order rejection notification

### Performance Classes

#### `PerformanceMonitor`
Real-time performance tracking.

#### `SPSCRingBuffer`
Lock-free single-producer single-consumer queue.

#### `MessagePool`
High-performance memory pool for message allocation.

## Troubleshooting

### Common Issues

#### High Latency
- Check CPU affinity settings
- Verify network configuration
- Monitor system load
- Check for memory swapping

#### Connection Failures
- Verify network connectivity
- Check credentials
- Review firewall settings
- Monitor connection logs

#### Order Rejections
- Validate order parameters
- Check instrument availability
- Verify position limits
- Review market hours

### Debugging

#### Enable Debug Logging
```cpp
// Set log level in configuration
"log_level": "DEBUG"
```

#### Performance Profiling
```bash
# Profile with perf
perf record -g ./ouch_example
perf report

# Memory profiling with valgrind
valgrind --tool=massif ./ouch_example
```

## License

This software is provided for educational and evaluation purposes. 
Production use requires appropriate licensing agreements with ASX.

## Support

For technical support and questions:
- Review documentation and examples
- Check performance tuning guide
- Monitor system requirements
- Validate configuration settings

---

**Note**: This implementation is designed for ultra-low latency trading applications. 
Proper testing and validation in a controlled environment is essential before production deployment.
