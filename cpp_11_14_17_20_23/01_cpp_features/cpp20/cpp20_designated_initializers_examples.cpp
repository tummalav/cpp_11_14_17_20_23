/*
 * C++20 Designated Initializers Use Cases and Examples
 *
 * Designated initializers allow you to initialize aggregate types by explicitly
 * naming the members being initialized, making code more readable and maintainable.
 *
 * Key Benefits:
 * 1. Improved code readability and self-documentation
 * 2. Order-independent initialization (within limits)
 * 3. Partial initialization with default values
 * 4. Better maintainability when struct members change
 * 5. Reduced errors from positional initialization
 * 6. Clear intent when initializing large structs
 */

#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <chrono>
#include <memory>
#include <algorithm>
#include <iomanip>

// ============================================================================
// 1. BASIC DESIGNATED INITIALIZERS
// ============================================================================

struct Point {
    int x = 0;
    int y = 0;
    int z = 0;
};

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

void demonstrate_basic_designated_initializers() {
    std::cout << "\n=== Basic Designated Initializers ===\n";

    // Traditional initialization (before C++20)
    Point p1{10, 20, 30};  // Must remember order, unclear what values mean

    // C++20 designated initializers - much clearer!
    Point p2{.x = 10, .y = 20, .z = 30};
    Point p3{.x = 5, .z = 15};  // y gets default value (0)
    Point p4{.y = 25};          // x and z get default values

    std::cout << "Traditional: p1(" << p1.x << ", " << p1.y << ", " << p1.z << ")\n";
    std::cout << "Designated: p2(" << p2.x << ", " << p2.y << ", " << p2.z << ")\n";
    std::cout << "Partial: p3(" << p3.x << ", " << p3.y << ", " << p3.z << ")\n";
    std::cout << "Partial: p4(" << p4.x << ", " << p4.y << ", " << p4.z << ")\n";

    // Colors are much clearer with designated initializers
    Color red{.r = 1.0f, .g = 0.0f, .b = 0.0f};
    Color semi_blue{.b = 1.0f, .a = 0.5f};  // r and g default to 0.0f
    Color transparent{.a = 0.0f};            // RGB defaults to black

    std::cout << "\nColors:\n";
    std::cout << "Red: RGBA(" << red.r << ", " << red.g << ", " << red.b << ", " << red.a << ")\n";
    std::cout << "Semi-blue: RGBA(" << semi_blue.r << ", " << semi_blue.g << ", " << semi_blue.b << ", " << semi_blue.a << ")\n";
    std::cout << "Transparent: RGBA(" << transparent.r << ", " << transparent.g << ", " << transparent.b << ", " << transparent.a << ")\n";
}

// ============================================================================
// 2. COMPLEX STRUCTURES WITH DESIGNATED INITIALIZERS
// ============================================================================

struct Address {
    std::string street;
    std::string city;
    std::string state;
    std::string zip_code;
    std::string country = "USA";
};

struct Person {
    std::string first_name;
    std::string last_name;
    int age = 0;
    Address address;
    std::string email;
    std::string phone;
};

struct Employee {
    Person personal_info;
    std::string employee_id;
    std::string department;
    double salary = 0.0;
    std::chrono::year_month_day hire_date;
    bool is_active = true;
};

void demonstrate_complex_structures() {
    std::cout << "\n=== Complex Structures with Designated Initializers ===\n";

    // Nested designated initializers
    Person john{
        .first_name = "John",
        .last_name = "Doe",
        .age = 30,
        .address = {
            .street = "123 Main St",
            .city = "New York",
            .state = "NY",
            .zip_code = "10001"
            // country uses default "USA"
        },
        .email = "john.doe@email.com",
        .phone = "+1-555-0123"
    };

    // Employee with nested person info
    Employee emp{
        .personal_info = {
            .first_name = "Alice",
            .last_name = "Smith",
            .age = 28,
            .address = {
                .street = "456 Oak Ave",
                .city = "San Francisco",
                .state = "CA",
                .zip_code = "94102"
            },
            .email = "alice.smith@company.com"
            // phone not specified, remains empty
        },
        .employee_id = "EMP001",
        .department = "Engineering",
        .salary = 120000.0,
        .hire_date = std::chrono::year_month_day{std::chrono::year{2023}, std::chrono::month{3}, std::chrono::day{15}}
        // is_active uses default true
    };

    std::cout << "Person: " << john.first_name << " " << john.last_name
              << ", Age: " << john.age << "\n";
    std::cout << "Address: " << john.address.street << ", " << john.address.city
              << ", " << john.address.state << " " << john.address.zip_code << "\n";

    std::cout << "\nEmployee: " << emp.personal_info.first_name << " "
              << emp.personal_info.last_name << "\n";
    std::cout << "Department: " << emp.department << ", Salary: $"
              << std::fixed << std::setprecision(2) << emp.salary << "\n";
    std::cout << "Active: " << std::boolalpha << emp.is_active << "\n";
}

// ============================================================================
// 3. FINANCIAL TRADING STRUCTURES
// ============================================================================

struct MarketData {
    std::string symbol;
    double bid_price = 0.0;
    double ask_price = 0.0;
    int bid_size = 0;
    int ask_size = 0;
    double last_price = 0.0;
    int volume = 0;
    std::chrono::system_clock::time_point timestamp;
};

struct Order {
    enum Side { BUY, SELL };
    enum Type { MARKET, LIMIT, STOP };

    std::string order_id;
    std::string symbol;
    Side side = BUY;
    Type type = MARKET;
    int quantity = 0;
    double price = 0.0;           // For limit orders
    double stop_price = 0.0;      // For stop orders
    std::string client_id;
    std::chrono::system_clock::time_point created_time;
    bool is_active = true;
};

struct Trade {
    std::string trade_id;
    std::string buy_order_id;
    std::string sell_order_id;
    std::string symbol;
    int quantity = 0;
    double price = 0.0;
    std::chrono::system_clock::time_point execution_time;
    double commission = 0.0;
};

struct Portfolio {
    std::string account_id;
    double cash_balance = 0.0;
    double total_value = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
    int positions_count = 0;
    std::chrono::system_clock::time_point last_updated;
};

void demonstrate_financial_structures() {
    std::cout << "\n=== Financial Trading Structures ===\n";

    auto now = std::chrono::system_clock::now();

    // Market data with designated initializers - very clear what each value represents
    MarketData aapl_quote{
        .symbol = "AAPL",
        .bid_price = 150.25,
        .ask_price = 150.30,
        .bid_size = 1000,
        .ask_size = 800,
        .last_price = 150.28,
        .volume = 1250000,
        .timestamp = now
    };

    // Limit buy order
    Order buy_order{
        .order_id = "ORD001",
        .symbol = "AAPL",
        .side = Order::BUY,
        .type = Order::LIMIT,
        .quantity = 100,
        .price = 150.20,
        .client_id = "CLIENT_123",
        .created_time = now
        // stop_price not needed for limit order, remains 0.0
        // is_active uses default true
    };

    // Stop-loss sell order
    Order stop_sell{
        .order_id = "ORD002",
        .symbol = "TSLA",
        .side = Order::SELL,
        .type = Order::STOP,
        .quantity = 50,
        .stop_price = 800.00,
        .client_id = "CLIENT_456",
        .created_time = now
        // price not needed for stop order, remains 0.0
    };

    // Trade execution
    Trade execution{
        .trade_id = "TRD001",
        .buy_order_id = "ORD001",
        .sell_order_id = "ORD003",
        .symbol = "AAPL",
        .quantity = 100,
        .price = 150.25,
        .execution_time = now,
        .commission = 1.50
    };

    // Portfolio summary
    Portfolio account{
        .account_id = "ACC_789",
        .cash_balance = 50000.00,
        .total_value = 75000.00,
        .unrealized_pnl = 2500.00,
        .realized_pnl = 1200.00,
        .positions_count = 5,
        .last_updated = now
    };

    std::cout << "Market Data: " << aapl_quote.symbol
              << " Bid: $" << aapl_quote.bid_price
              << " Ask: $" << aapl_quote.ask_price
              << " Last: $" << aapl_quote.last_price << "\n";

    std::cout << "Buy Order: " << buy_order.quantity << " shares of "
              << buy_order.symbol << " @ $" << buy_order.price << "\n";

    std::cout << "Stop Order: " << stop_sell.quantity << " shares of "
              << stop_sell.symbol << " stop @ $" << stop_sell.stop_price << "\n";

    std::cout << "Trade: " << execution.quantity << " shares of "
              << execution.symbol << " @ $" << execution.price
              << " (commission: $" << execution.commission << ")\n";

    std::cout << "Portfolio: " << account.account_id
              << " Cash: $" << account.cash_balance
              << " Total: $" << account.total_value
              << " P&L: $" << account.unrealized_pnl << "\n";
}

// ============================================================================
// 4. CONFIGURATION STRUCTURES
// ============================================================================

struct DatabaseConfig {
    std::string host = "localhost";
    int port = 5432;
    std::string database;
    std::string username;
    std::string password;
    int max_connections = 10;
    int timeout_seconds = 30;
    bool enable_ssl = false;
    bool enable_logging = true;
};

struct ServerConfig {
    std::string bind_address = "0.0.0.0";
    int port = 8080;
    int worker_threads = 4;
    int max_clients = 1000;
    bool enable_compression = true;
    bool enable_keepalive = true;
    int keepalive_timeout = 60;
    std::string log_level = "INFO";
    std::string log_file = "server.log";
};

struct TradingSystemConfig {
    DatabaseConfig database;
    ServerConfig server;
    std::string market_data_feed = "IEX";
    double max_position_size = 1000000.0;
    double risk_limit = 0.02;  // 2%
    bool enable_paper_trading = false;
    std::string timezone = "America/New_York";
    std::vector<std::string> allowed_symbols;
};

void demonstrate_configuration_structures() {
    std::cout << "\n=== Configuration Structures ===\n";

    // Database configuration with only necessary overrides
    DatabaseConfig db_config{
        .host = "prod-db.company.com",
        .database = "trading_system",
        .username = "trader",
        .password = "secure_password",
        .max_connections = 50,
        .enable_ssl = true
        // port, timeout_seconds, enable_logging use defaults
    };

    // Server configuration for production
    ServerConfig server_config{
        .port = 9090,
        .worker_threads = 8,
        .max_clients = 5000,
        .log_level = "WARN",
        .log_file = "/var/log/trading_server.log"
        // Other fields use defaults
    };

    // Complete trading system configuration
    TradingSystemConfig trading_config{
        .database = db_config,
        .server = server_config,
        .market_data_feed = "Bloomberg",
        .max_position_size = 5000000.0,
        .risk_limit = 0.015,  // 1.5%
        .timezone = "America/Chicago",
        .allowed_symbols = {"AAPL", "GOOGL", "MSFT", "TSLA", "AMZN"}
        // enable_paper_trading uses default false
    };

    std::cout << "Database: " << db_config.host << ":" << db_config.port
              << "/" << db_config.database << "\n";
    std::cout << "SSL: " << std::boolalpha << db_config.enable_ssl
              << ", Max connections: " << db_config.max_connections << "\n";

    std::cout << "Server: " << server_config.bind_address << ":"
              << server_config.port << "\n";
    std::cout << "Workers: " << server_config.worker_threads
              << ", Max clients: " << server_config.max_clients << "\n";

    std::cout << "Trading System:\n";
    std::cout << "  Market data: " << trading_config.market_data_feed << "\n";
    std::cout << "  Max position: $" << trading_config.max_position_size << "\n";
    std::cout << "  Risk limit: " << (trading_config.risk_limit * 100) << "%\n";
    std::cout << "  Timezone: " << trading_config.timezone << "\n";
    std::cout << "  Allowed symbols: ";
    for (const auto& symbol : trading_config.allowed_symbols) {
        std::cout << symbol << " ";
    }
    std::cout << "\n";
}

// ============================================================================
// 5. EVENT AND MESSAGE STRUCTURES
// ============================================================================

struct LogEntry {
    enum Level { DEBUG, INFO, WARN, ERROR, FATAL };

    Level level = INFO;
    std::string message;
    std::string component;
    std::chrono::system_clock::time_point timestamp;
    std::string thread_id;
    std::string file;
    int line = 0;
};

struct NetworkMessage {
    enum Type { HEARTBEAT, ORDER, CANCEL, TRADE, MARKET_DATA };

    Type type = HEARTBEAT;
    std::string source;
    std::string destination;
    std::vector<char> payload;
    std::chrono::system_clock::time_point sent_time;
    int sequence_number = 0;
    bool requires_ack = false;
};

struct AlertMessage {
    enum Severity { LOW, MEDIUM, HIGH, CRITICAL };

    Severity severity = MEDIUM;
    std::string title;
    std::string description;
    std::string component;
    std::chrono::system_clock::time_point created_time;
    std::string user_id;
    bool is_acknowledged = false;
    std::string acknowledgment_note;
};

void demonstrate_event_message_structures() {
    std::cout << "\n=== Event and Message Structures ===\n";

    auto now = std::chrono::system_clock::now();

    // Log entries with different levels
    LogEntry debug_log{
        .level = LogEntry::DEBUG,
        .message = "Processing order validation",
        .component = "OrderManager",
        .timestamp = now,
        .thread_id = "worker-1",
        .file = "order_manager.cpp",
        .line = 142
    };

    LogEntry error_log{
        .level = LogEntry::ERROR,
        .message = "Failed to connect to market data feed",
        .component = "MarketDataClient",
        .timestamp = now,
        .thread_id = "main"
        // file and line not specified for this log
    };

    // Network messages
    NetworkMessage heartbeat{
        .type = NetworkMessage::HEARTBEAT,
        .source = "TradingEngine",
        .destination = "RiskManager",
        .sent_time = now,
        .sequence_number = 12345
        // payload empty for heartbeat
        // requires_ack uses default false
    };

    std::string order_data = "BUY,AAPL,100,150.25";
    NetworkMessage order_msg{
        .type = NetworkMessage::ORDER,
        .source = "ClientGateway",
        .destination = "OrderManager",
        .payload = std::vector<char>(order_data.begin(), order_data.end()),
        .sent_time = now,
        .sequence_number = 12346,
        .requires_ack = true
    };

    // Alert messages
    AlertMessage risk_alert{
        .severity = AlertMessage::HIGH,
        .title = "Position Limit Exceeded",
        .description = "Account ACC_123 has exceeded 90% of position limit for AAPL",
        .component = "RiskManager",
        .created_time = now,
        .user_id = "risk_officer_1"
        // is_acknowledged uses default false
    };

    AlertMessage system_alert{
        .severity = AlertMessage::CRITICAL,
        .title = "Market Data Feed Disconnected",
        .description = "Primary market data feed has been disconnected for 30 seconds",
        .component = "MarketDataManager",
        .created_time = now,
        .user_id = "system",
        .is_acknowledged = true,
        .acknowledgment_note = "Failover to secondary feed activated"
    };

    std::cout << "Debug Log: [" << debug_log.component << "] "
              << debug_log.message << " at " << debug_log.file
              << ":" << debug_log.line << "\n";

    std::cout << "Error Log: [" << error_log.component << "] "
              << error_log.message << "\n";

    std::cout << "Heartbeat: " << heartbeat.source << " -> "
              << heartbeat.destination << " (seq: " << heartbeat.sequence_number << ")\n";

    std::cout << "Order Message: " << order_msg.source << " -> "
              << order_msg.destination << " (size: " << order_msg.payload.size()
              << " bytes, ack: " << std::boolalpha << order_msg.requires_ack << ")\n";

    std::cout << "Risk Alert: [" << risk_alert.title << "] "
              << risk_alert.description << "\n";

    std::cout << "System Alert: [" << system_alert.title << "] "
              << system_alert.description
              << " (Acknowledged: " << std::boolalpha << system_alert.is_acknowledged << ")\n";
}

// ============================================================================
// 6. ARRAY DESIGNATED INITIALIZERS
// ============================================================================

struct Matrix3x3 {
    std::array<std::array<double, 3>, 3> data = {};
};

struct ColorPalette {
    std::array<Color, 5> colors = {};
};

struct WeeklySchedule {
    enum Day { MONDAY = 0, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY, SATURDAY, SUNDAY };
    std::array<std::string, 7> activities = {};
};

void demonstrate_array_designated_initializers() {
    std::cout << "\n=== Array Designated Initializers ===\n";

    // Identity matrix using designated initializers
    Matrix3x3 identity{
        .data = {{
            {{1.0, 0.0, 0.0}},
            {{0.0, 1.0, 0.0}},
            {{0.0, 0.0, 1.0}}
        }}
    };

    // Color palette with designated initializers
    ColorPalette web_safe{
        .colors = {{
            {.r = 1.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f},  // Red
            {.r = 0.0f, .g = 1.0f, .b = 0.0f, .a = 1.0f},  // Green
            {.r = 0.0f, .g = 0.0f, .b = 1.0f, .a = 1.0f},  // Blue
            {.r = 1.0f, .g = 1.0f, .b = 0.0f, .a = 1.0f},  // Yellow
            {.r = 1.0f, .g = 0.0f, .b = 1.0f, .a = 1.0f}   // Magenta
        }}
    };

    // Weekly schedule
    WeeklySchedule schedule{
        .activities = {{
            "Team Meeting",      // Monday
            "Code Review",       // Tuesday
            "Client Call",       // Wednesday
            "Development",       // Thursday
            "Testing",          // Friday
            "Weekend Project",   // Saturday
            "Rest"              // Sunday
        }}
    };

    std::cout << "Identity Matrix:\n";
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            std::cout << identity.data[i][j] << " ";
        }
        std::cout << "\n";
    }

    std::cout << "\nColor Palette:\n";
    const std::array<std::string, 5> color_names = {"Red", "Green", "Blue", "Yellow", "Magenta"};
    for (size_t i = 0; i < web_safe.colors.size(); ++i) {
        const auto& color = web_safe.colors[i];
        std::cout << color_names[i] << ": RGBA("
                  << color.r << ", " << color.g << ", " << color.b << ", " << color.a << ")\n";
    }

    std::cout << "\nWeekly Schedule:\n";
    const std::array<std::string, 7> day_names = {
        "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
    };
    for (size_t i = 0; i < schedule.activities.size(); ++i) {
        std::cout << day_names[i] << ": " << schedule.activities[i] << "\n";
    }
}

// ============================================================================
// 7. OPTIONAL AND SMART POINTER INITIALIZATION
// ============================================================================

struct DatabaseConnection {
    std::string host;
    int port;
    std::optional<std::string> username;
    std::optional<std::string> password;
    std::optional<int> timeout;
    std::shared_ptr<void> ssl_context = nullptr;
};

struct CacheConfig {
    std::optional<size_t> max_size;
    std::optional<std::chrono::seconds> ttl;
    std::optional<std::string> eviction_policy;
    std::unique_ptr<void> custom_allocator = nullptr;
};

void demonstrate_optional_smart_pointer_init() {
    std::cout << "\n=== Optional and Smart Pointer Initialization ===\n";

    // Database connection with some optional fields
    DatabaseConnection db_conn{
        .host = "database.company.com",
        .port = 5432,
        .username = "admin",
        .password = "secret123",
        .timeout = 30
        // ssl_context remains nullptr
    };

    // Anonymous connection (no credentials)
    DatabaseConnection anon_conn{
        .host = "public-db.example.com",
        .port = 5432
        // username, password, timeout remain nullopt
        // ssl_context remains nullptr
    };

    // Cache configuration with partial settings
    CacheConfig cache_config{
        .max_size = 1024 * 1024,  // 1MB
        .ttl = std::chrono::seconds{300}  // 5 minutes
        // eviction_policy remains nullopt
        // custom_allocator remains nullptr
    };

    // Minimal cache config
    CacheConfig minimal_cache{
        .max_size = 512
        // All other fields use defaults
    };

    std::cout << "DB Connection: " << db_conn.host << ":" << db_conn.port;
    if (db_conn.username) {
        std::cout << " (user: " << *db_conn.username << ")";
    }
    if (db_conn.timeout) {
        std::cout << " (timeout: " << *db_conn.timeout << "s)";
    }
    std::cout << "\n";

    std::cout << "Anonymous DB: " << anon_conn.host << ":" << anon_conn.port;
    std::cout << " (no credentials)\n";

    std::cout << "Cache Config: ";
    if (cache_config.max_size) {
        std::cout << "max_size=" << *cache_config.max_size << " ";
    }
    if (cache_config.ttl) {
        std::cout << "ttl=" << cache_config.ttl->count() << "s ";
    }
    if (cache_config.eviction_policy) {
        std::cout << "policy=" << *cache_config.eviction_policy;
    }
    std::cout << "\n";

    std::cout << "Minimal Cache: ";
    if (minimal_cache.max_size) {
        std::cout << "max_size=" << *minimal_cache.max_size;
    }
    std::cout << "\n";
}

// ============================================================================
// 8. COMPARISON: BEFORE AND AFTER DESIGNATED INITIALIZERS
// ============================================================================

struct ComplexConfig {
    std::string name;
    std::string version;
    int major_version = 1;
    int minor_version = 0;
    int patch_version = 0;
    bool debug_mode = false;
    bool verbose_logging = false;
    std::string log_file = "app.log";
    int max_threads = 4;
    double timeout_seconds = 30.0;
    std::string data_directory = "./data";
    std::string temp_directory = "/tmp";
    bool enable_compression = true;
    bool enable_encryption = false;
    std::string encryption_key;
};

void demonstrate_before_after_comparison() {
    std::cout << "\n=== Before and After Designated Initializers ===\n";

    std::cout << "BEFORE C++20 (Positional initialization - error-prone):\n";
    std::cout << "ComplexConfig config1{\"MyApp\", \"2.1\", 2, 1, 0, true, false, \"debug.log\", 8, 60.0, \"./app_data\", \"/var/tmp\", false, true, \"secret_key\"};\n";
    std::cout << "// What does 'true, false' mean? What is 60.0? Very unclear!\n\n";

    // Before C++20: positional initialization (hard to read and maintain)
    ComplexConfig config_old{"MyApp", "2.1", 2, 1, 0, true, false, "debug.log", 8, 60.0, "./app_data", "/var/tmp", false, true, "secret_key"};

    std::cout << "AFTER C++20 (Designated initializers - self-documenting):\n";

    // After C++20: designated initializers (much clearer!)
    ComplexConfig config_new{
        .name = "MyApp",
        .version = "2.1",
        .major_version = 2,
        .minor_version = 1,
        .debug_mode = true,
        .log_file = "debug.log",
        .max_threads = 8,
        .timeout_seconds = 60.0,
        .data_directory = "./app_data",
        .temp_directory = "/var/tmp",
        .enable_compression = false,
        .enable_encryption = true,
        .encryption_key = "secret_key"
        // patch_version, verbose_logging use defaults
    };

    std::cout << "ComplexConfig config_new{\n";
    std::cout << "    .name = \"MyApp\",\n";
    std::cout << "    .version = \"2.1\",\n";
    std::cout << "    .major_version = 2,\n";
    std::cout << "    .minor_version = 1,\n";
    std::cout << "    .debug_mode = true,\n";
    std::cout << "    .log_file = \"debug.log\",\n";
    std::cout << "    .max_threads = 8,\n";
    std::cout << "    .timeout_seconds = 60.0,\n";
    std::cout << "    .data_directory = \"./app_data\",\n";
    std::cout << "    .temp_directory = \"/var/tmp\",\n";
    std::cout << "    .enable_compression = false,\n";
    std::cout << "    .enable_encryption = true,\n";
    std::cout << "    .encryption_key = \"secret_key\"\n";
    std::cout << "};\n";
    std::cout << "// Crystal clear what each value represents!\n\n";

    // Verify both configurations are equivalent
    bool configs_equal =
        config_old.name == config_new.name &&
        config_old.version == config_new.version &&
        config_old.major_version == config_new.major_version &&
        config_old.minor_version == config_new.minor_version &&
        config_old.debug_mode == config_new.debug_mode &&
        config_old.log_file == config_new.log_file &&
        config_old.max_threads == config_new.max_threads &&
        config_old.timeout_seconds == config_new.timeout_seconds &&
        config_old.enable_encryption == config_new.enable_encryption;

    std::cout << "Both configurations are equivalent: " << std::boolalpha << configs_equal << "\n";
}

// ============================================================================
// 9. LIMITATIONS AND RESTRICTIONS
// ============================================================================

struct OrderedFields {
    int first;
    int second;
    int third;
};

struct BaseClass {
    int base_value = 0;
};

struct DerivedClass : BaseClass {
    int derived_value = 0;
};

void demonstrate_limitations() {
    std::cout << "\n=== Limitations and Restrictions ===\n";

    std::cout << "1. Order matters in C++20 (unlike C99):\n";

    // VALID: Fields in declaration order
    OrderedFields valid1{.first = 1, .second = 2, .third = 3};
    OrderedFields valid2{.first = 1, .third = 3};  // Skip middle field

    std::cout << "Valid: {.first = 1, .second = 2, .third = 3}\n";
    std::cout << "Valid: {.first = 1, .third = 3} // skipping middle field\n";

    // INVALID: Out of order (compilation error)
    // OrderedFields invalid{.second = 2, .first = 1};  // ERROR!
    std::cout << "Invalid: {.second = 2, .first = 1} // ERROR: out of order\n";

    std::cout << "\n2. No mixing with positional initialization:\n";
    // INVALID: Cannot mix designated and positional
    // OrderedFields mixed{1, .second = 2, .third = 3};  // ERROR!
    std::cout << "Invalid: {1, .second = 2, .third = 3} // ERROR: mixing styles\n";

    std::cout << "\n3. Only works with aggregates (no constructors, private members, virtual functions):\n";

    // VALID: Simple aggregate
    OrderedFields aggregate{.first = 10};
    std::cout << "Valid: Simple aggregate initialization\n";

    // INVALID: Classes with constructors, private members, inheritance can't use designated initializers
    std::cout << "Invalid: Classes with constructors, private members, or inheritance\n";

    std::cout << "\n4. Cannot designate base class members directly:\n";
    // DerivedClass derived{.base_value = 5, .derived_value = 10};  // ERROR!
    std::cout << "Invalid: {.base_value = 5, .derived_value = 10} // ERROR: base class members\n";

    // Valid derived class initialization
    DerivedClass derived{.derived_value = 10};  // base_value gets default
    std::cout << "Valid: {.derived_value = 10} // only derived class members\n";

    std::cout << "\nValues:\n";
    std::cout << "valid1: (" << valid1.first << ", " << valid1.second << ", " << valid1.third << ")\n";
    std::cout << "valid2: (" << valid2.first << ", " << valid2.second << ", " << valid2.third << ")\n";
    std::cout << "derived: base=" << derived.base_value << ", derived=" << derived.derived_value << "\n";
}

// ============================================================================
// 10. BEST PRACTICES AND GUIDELINES
// ============================================================================

// GOOD: Clear, self-documenting initialization
struct TradingParameters {
    std::string strategy_name;
    double max_position_size = 1000000.0;
    double stop_loss_pct = 0.02;
    double take_profit_pct = 0.05;
    int max_trades_per_day = 10;
    bool enable_risk_checks = true;
    std::string log_level = "INFO";
};

// GOOD: Optional fields with meaningful defaults
struct APIConfig {
    std::string endpoint;
    int timeout_ms = 5000;
    int retry_count = 3;
    bool enable_tls = true;
    std::optional<std::string> api_key;
    std::optional<std::string> user_agent;
};

void demonstrate_best_practices() {
    std::cout << "\n=== Best Practices and Guidelines ===\n";

    std::cout << "1. Use designated initializers for configuration objects:\n";

    TradingParameters momentum_strategy{
        .strategy_name = "Momentum",
        .max_position_size = 500000.0,
        .stop_loss_pct = 0.015,  // 1.5%
        .take_profit_pct = 0.08, // 8%
        .max_trades_per_day = 5,
        .log_level = "DEBUG"
        // enable_risk_checks uses default true
    };

    std::cout << "Strategy: " << momentum_strategy.strategy_name
              << ", Max position: $" << momentum_strategy.max_position_size
              << ", Stop loss: " << (momentum_strategy.stop_loss_pct * 100) << "%\n";

    std::cout << "\n2. Combine with std::optional for truly optional fields:\n";

    APIConfig public_api{
        .endpoint = "https://api.public.com/v1",
        .timeout_ms = 10000,
        .enable_tls = true
        // api_key and user_agent remain nullopt for public API
    };

    APIConfig private_api{
        .endpoint = "https://api.private.com/v2",
        .api_key = "secret_key_123",
        .user_agent = "TradingBot/1.0"
        // timeout_ms, retry_count, enable_tls use defaults
    };

    std::cout << "Public API: " << public_api.endpoint
              << " (timeout: " << public_api.timeout_ms << "ms)\n";
    std::cout << "Private API: " << private_api.endpoint;
    if (private_api.api_key) {
        std::cout << " (authenticated)";
    }
    std::cout << "\n";

    std::cout << "\n3. Provide meaningful defaults:\n";
    std::cout << "- Always provide sensible default values for optional fields\n";
    std::cout << "- Document what each field does\n";
    std::cout << "- Use designated initializers for any struct with >3 fields\n";
    std::cout << "- Group related fields logically in struct definition\n";

    std::cout << "\n4. Migration strategy from old code:\n";
    std::cout << "- Convert structs one at a time\n";
    std::cout << "- Add default values to all fields first\n";
    std::cout << "- Update initialization sites to use designated initializers\n";
    std::cout << "- Consider breaking large structs into smaller, focused ones\n";
}

// ============================================================================
// MAIN DEMONSTRATION FUNCTION
// ============================================================================

int main() {
    std::cout << "C++20 Designated Initializers Use Cases and Examples\n";
    std::cout << "===================================================\n";

    demonstrate_basic_designated_initializers();
    demonstrate_complex_structures();
    demonstrate_financial_structures();
    demonstrate_configuration_structures();
    demonstrate_event_message_structures();
    demonstrate_array_designated_initializers();
    demonstrate_optional_smart_pointer_init();
    demonstrate_before_after_comparison();
    demonstrate_limitations();
    demonstrate_best_practices();

    std::cout << "\n=== Key Takeaways ===\n";
    std::cout << "1. Designated initializers make code self-documenting\n";
    std::cout << "2. Excellent for configuration and parameter structures\n";
    std::cout << "3. Reduce errors from positional initialization\n";
    std::cout << "4. Allow partial initialization with meaningful defaults\n";
    std::cout << "5. Fields must be initialized in declaration order (C++20 restriction)\n";
    std::cout << "6. Cannot mix with positional initialization\n";
    std::cout << "7. Only work with aggregate types (no constructors, inheritance)\n";
    std::cout << "8. Perfect for financial data structures (orders, trades, configs)\n";
    std::cout << "9. Combine with std::optional for truly optional fields\n";
    std::cout << "10. Significantly improve code readability and maintainability\n";
    std::cout << "11. Reduce cognitive load when reading complex initializations\n";
    std::cout << "12. Enable safe refactoring when struct fields change\n";

    return 0;
}

/*
 * Compilation Requirements:
 * - C++20 compatible compiler
 * - GCC 8+, Clang 10+, or MSVC 2019+
 * - Use -std=c++20 flag
 *
 * Example compilation:
 * g++ -std=c++20 -Wall -Wextra cpp20_designated_initializers_examples.cpp -o designated_init_demo
 *
 * Key Features of Designated Initializers:
 * 1. .member_name = value syntax
 * 2. Self-documenting initialization
 * 3. Order must match declaration order (C++20)
 * 4. Can skip fields (use default values)
 * 5. Cannot mix with positional initialization
 * 6. Only for aggregate types
 * 7. Nested initialization supported
 * 8. Works with arrays and std::array
 *
 * Benefits:
 * - Improved readability
 * - Reduced initialization errors
 * - Better maintainability
 * - Clear intent
 * - Partial initialization support
 * - Self-documenting code
 *
 * Common Use Cases:
 * - Configuration structures
 * - Financial data (orders, trades)
 * - Event/message structures
 * - API parameters
 * - Complex data initialization
 * - Database record initialization
 */
