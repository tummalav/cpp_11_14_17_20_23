#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <type_traits>
#include <cassert>

// ================================
// SCOPED ENUMS (ENUM CLASS)
// ================================

// Basic scoped enum
enum class Color {
    Red,
    Green,
    Blue,
    Yellow,
    Purple
};

// Scoped enum with explicit underlying type
enum class Status : uint8_t {
    Inactive = 0,
    Active = 1,
    Pending = 2,
    Error = 255
};

// Scoped enum with custom values
enum class HttpStatus : int {
    OK = 200,
    NotFound = 404,
    InternalError = 500,
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403
};

// Scoped enum for file operations
enum class FileMode : unsigned int {
    Read = 1,
    Write = 2,
    Append = 4,
    Binary = 8,
    ReadWrite = Read | Write,
    ReadWriteBinary = Read | Write | Binary
};

// Scoped enum for logging levels
enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

// Scoped enum for game states
enum class GameState {
    Menu,
    Playing,
    Paused,
    GameOver,
    Loading
};

// ================================
// COMPARISON WITH UNSCOPED ENUMS
// ================================

// Traditional unscoped enum (C++98 style)
enum OldColor {
    OLD_RED,
    OLD_GREEN,
    OLD_BLUE
};

// This would cause naming conflicts if we had:
// enum AnotherEnum { RED, GREEN, BLUE }; // Error: redefinition

// ================================
// UTILITY FUNCTIONS AND OPERATORS
// ================================

// Custom string conversion for Color
std::string toString(Color color) {
    switch (color) {
        case Color::Red:    return "Red";
        case Color::Green:  return "Green";
        case Color::Blue:   return "Blue";
        case Color::Yellow: return "Yellow";
        case Color::Purple: return "Purple";
        default:           return "Unknown";
    }
}

// Custom string conversion for Status
std::string toString(Status status) {
    switch (status) {
        case Status::Inactive: return "Inactive";
        case Status::Active:   return "Active";
        case Status::Pending:  return "Pending";
        case Status::Error:    return "Error";
        default:              return "Unknown";
    }
}

// Custom string conversion for LogLevel
std::string toString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warning:  return "WARNING";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        default:                return "UNKNOWN";
    }
}

// Bitwise operations for FileMode (treating it as flags)
FileMode operator|(FileMode lhs, FileMode rhs) {
    return static_cast<FileMode>(
        static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs)
    );
}

FileMode operator&(FileMode lhs, FileMode rhs) {
    return static_cast<FileMode>(
        static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs)
    );
}

bool hasFlag(FileMode mode, FileMode flag) {
    return (mode & flag) == flag;
}

// Increment operator for enums (useful for iteration)
Color& operator++(Color& color) {
    color = static_cast<Color>(static_cast<int>(color) + 1);
    return color;
}

Color operator++(Color& color, int) {
    Color temp = color;
    ++color;
    return temp;
}

// ================================
// TEMPLATE UTILITIES
// ================================

// Template to get underlying type value
template<typename E>
constexpr auto toUnderlying(E e) noexcept {
    return static_cast<std::underlying_type_t<E>>(e);
}

// Template to check if enum is valid (within range)
template<typename E>
constexpr bool isValidEnum(int value) {
    // This is a simplified check - in real code you'd want to be more specific
    return value >= 0 && value < 10; // Adjust based on your enum
}

// Template for enum iteration (requires contiguous values starting from 0)
template<typename E>
class EnumIterator {
private:
    int value;

public:
    explicit EnumIterator(int val) : value(val) {}

    E operator*() const { return static_cast<E>(value); }
    EnumIterator& operator++() { ++value; return *this; }
    bool operator!=(const EnumIterator& other) const { return value != other.value; }
};

template<typename E>
struct EnumRange {
    int begin_val, end_val;

    EnumRange(int begin, int end) : begin_val(begin), end_val(end) {}

    EnumIterator<E> begin() const { return EnumIterator<E>(begin_val); }
    EnumIterator<E> end() const { return EnumIterator<E>(end_val); }
};

// Helper to create enum range
template<typename E>
EnumRange<E> enumRange(E begin, E end) {
    return EnumRange<E>(toUnderlying(begin), toUnderlying(end) + 1);
}

// ================================
// CLASS EXAMPLES USING SCOPED ENUMS
// ================================

class Logger {
private:
    LogLevel minLevel;

public:
    Logger(LogLevel level = LogLevel::Info) : minLevel(level) {}

    void setLevel(LogLevel level) { minLevel = level; }

    void log(LogLevel level, const std::string& message) {
        if (level >= minLevel) {
            std::cout << "[" << toString(level) << "] " << message << "\n";
        }
    }

    // Convenience methods
    void debug(const std::string& msg) { log(LogLevel::Debug, msg); }
    void info(const std::string& msg) { log(LogLevel::Info, msg); }
    void warning(const std::string& msg) { log(LogLevel::Warning, msg); }
    void error(const std::string& msg) { log(LogLevel::Error, msg); }
    void critical(const std::string& msg) { log(LogLevel::Critical, msg); }
};

class HttpResponse {
private:
    HttpStatus status;
    std::string body;

public:
    HttpResponse(HttpStatus s, const std::string& b) : status(s), body(b) {}

    HttpStatus getStatus() const { return status; }
    std::string getBody() const { return body; }

    bool isSuccess() const {
        return toUnderlying(status) >= 200 && toUnderlying(status) < 300;
    }

    bool isClientError() const {
        return toUnderlying(status) >= 400 && toUnderlying(status) < 500;
    }

    bool isServerError() const {
        return toUnderlying(status) >= 500 && toUnderlying(status) < 600;
    }
};

class FileManager {
private:
    FileMode currentMode;

public:
    FileManager(FileMode mode) : currentMode(mode) {}

    void setMode(FileMode mode) { currentMode = mode; }

    bool canRead() const { return hasFlag(currentMode, FileMode::Read); }
    bool canWrite() const { return hasFlag(currentMode, FileMode::Write); }
    bool canAppend() const { return hasFlag(currentMode, FileMode::Append); }
    bool isBinary() const { return hasFlag(currentMode, FileMode::Binary); }

    void printCapabilities() const {
        std::cout << "File capabilities: ";
        if (canRead()) std::cout << "Read ";
        if (canWrite()) std::cout << "Write ";
        if (canAppend()) std::cout << "Append ";
        if (isBinary()) std::cout << "Binary ";
        std::cout << "\n";
    }
};

class Game {
private:
    GameState currentState;

public:
    Game() : currentState(GameState::Menu) {}

    void setState(GameState state) { currentState = state; }
    GameState getState() const { return currentState; }

    void update() {
        switch (currentState) {
            case GameState::Menu:
                std::cout << "Showing menu...\n";
                break;
            case GameState::Playing:
                std::cout << "Game is running...\n";
                break;
            case GameState::Paused:
                std::cout << "Game is paused\n";
                break;
            case GameState::GameOver:
                std::cout << "Game over screen\n";
                break;
            case GameState::Loading:
                std::cout << "Loading...\n";
                break;
        }
    }

    bool canPause() const {
        return currentState == GameState::Playing;
    }

    bool canResume() const {
        return currentState == GameState::Paused;
    }
};

// ================================
// DEMONSTRATION FUNCTIONS
// ================================

void demonstrateBasicScopedEnums() {
    std::cout << "\n=== BASIC SCOPED ENUMS ===\n";

    // Scoped enums require scope resolution
    Color color1 = Color::Red;
    Color color2 = Color::Blue;

    std::cout << "Color 1: " << toString(color1) << "\n";
    std::cout << "Color 2: " << toString(color2) << "\n";

    // Cannot implicitly convert to int (compile error if uncommented)
    // int colorValue = color1; // Error!

    // Must explicitly cast
    int colorValue = static_cast<int>(color1);
    std::cout << "Color 1 as int: " << colorValue << "\n";

    // No naming conflicts with scoped enums
    Status status = Status::Active;
    std::cout << "Status: " << toString(status) << "\n";
    std::cout << "Status underlying value: " << toUnderlying(status) << "\n";
}

void demonstrateUnscopedVsScoped() {
    std::cout << "\n=== UNSCOPED VS SCOPED COMPARISON ===\n";

    // Unscoped enum - names pollute global namespace
    OldColor oldColor = OLD_RED;
    std::cout << "Old color: " << oldColor << " (automatically converts to int)\n";

    // Scoped enum - names are properly scoped
    Color newColor = Color::Red;
    std::cout << "New color: " << toString(newColor) << " (requires explicit conversion)\n";

    // This shows the difference:
    if (oldColor == 0) {  // Works because OLD_RED == 0
        std::cout << "Unscoped enum implicitly converts to int\n";
    }

    if (newColor == Color::Red) {  // Must compare with same enum type
        std::cout << "Scoped enum requires explicit type\n";
    }
}

void demonstrateExplicitTypes() {
    std::cout << "\n=== EXPLICIT UNDERLYING TYPES ===\n";

    Status status = Status::Error;
    std::cout << "Status size: " << sizeof(status) << " bytes\n";
    std::cout << "Status value: " << static_cast<int>(status) << "\n";

    HttpStatus httpStatus = HttpStatus::NotFound;
    std::cout << "HTTP Status: " << static_cast<int>(httpStatus) << "\n";

    // Demonstrate type safety
    // status = httpStatus; // Error: cannot convert between different enum types
}

void demonstrateBitwiseOperations() {
    std::cout << "\n=== BITWISE OPERATIONS (FLAGS) ===\n";

    FileMode mode1 = FileMode::Read;
    FileMode mode2 = FileMode::Write;
    FileMode combined = mode1 | mode2;

    FileManager fm(combined);
    fm.printCapabilities();

    // More complex example
    FileMode complexMode = FileMode::Read | FileMode::Write | FileMode::Binary;
    FileManager fm2(complexMode);
    fm2.printCapabilities();

    // Check specific flags
    std::cout << "Can read: " << std::boolalpha << hasFlag(complexMode, FileMode::Read) << "\n";
    std::cout << "Can append: " << hasFlag(complexMode, FileMode::Append) << "\n";
}

void demonstrateEnumIteration() {
    std::cout << "\n=== ENUM ITERATION ===\n";

    std::cout << "Iterating through colors:\n";
    for (Color color : enumRange(Color::Red, Color::Purple)) {
        std::cout << "  " << toString(color) << " = " << toUnderlying(color) << "\n";
    }

    // Manual iteration with increment operator
    std::cout << "\nUsing increment operator:\n";
    Color color = Color::Red;
    for (int i = 0; i < 3; ++i) {
        std::cout << "  " << toString(color) << "\n";
        ++color;
    }
}

void demonstrateClassUsage() {
    std::cout << "\n=== CLASS USAGE EXAMPLES ===\n";

    // Logger example
    std::cout << "--- Logger Example ---\n";
    Logger logger(LogLevel::Warning);
    logger.debug("This won't be shown");
    logger.info("This won't be shown either");
    logger.warning("This will be shown");
    logger.error("This is an error");
    logger.critical("Critical issue!");

    // HTTP Response example
    std::cout << "\n--- HTTP Response Example ---\n";
    HttpResponse response1(HttpStatus::OK, "Success");
    HttpResponse response2(HttpStatus::NotFound, "Page not found");
    HttpResponse response3(HttpStatus::InternalError, "Server error");

    std::vector<HttpResponse> responses = {response1, response2, response3};

    for (const auto& resp : responses) {
        std::cout << "Status " << toUnderlying(resp.getStatus()) << ": ";
        if (resp.isSuccess()) {
            std::cout << "Success - " << resp.getBody() << "\n";
        } else if (resp.isClientError()) {
            std::cout << "Client Error - " << resp.getBody() << "\n";
        } else if (resp.isServerError()) {
            std::cout << "Server Error - " << resp.getBody() << "\n";
        }
    }

    // Game state example
    std::cout << "\n--- Game State Example ---\n";
    Game game;

    std::vector<GameState> stateSequence = {
        GameState::Loading,
        GameState::Menu,
        GameState::Playing,
        GameState::Paused,
        GameState::Playing,
        GameState::GameOver
    };

    for (GameState state : stateSequence) {
        game.setState(state);
        game.update();

        if (game.canPause()) {
            std::cout << "  (Can pause)\n";
        }
        if (game.canResume()) {
            std::cout << "  (Can resume)\n";
        }
    }
}

void demonstrateAdvancedFeatures() {
    std::cout << "\n=== ADVANCED FEATURES ===\n";

    // Using enums as map keys
    std::unordered_map<Color, std::string> colorNames = {
        {Color::Red, "Rouge"},
        {Color::Green, "Vert"},
        {Color::Blue, "Bleu"}
    };

    std::cout << "Color translations:\n";
    for (const auto& [color, translation] : colorNames) {
        std::cout << "  " << toString(color) << " -> " << translation << "\n";
    }

    // Template usage
    std::cout << "\nUnderlying values:\n";
    std::cout << "  Color::Red = " << toUnderlying(Color::Red) << "\n";
    std::cout << "  Status::Active = " << toUnderlying(Status::Active) << "\n";
    std::cout << "  HttpStatus::OK = " << toUnderlying(HttpStatus::OK) << "\n";

    // Type traits
    std::cout << "\nType information:\n";
    std::cout << "  Color is enum: " << std::is_enum_v<Color> << "\n";
    // std::cout << "  Color is scoped enum: " << std::is_scoped_enum_v<Color> << "\n";  // C++23 feature
    std::cout << "  Color underlying type size: " << sizeof(std::underlying_type_t<Color>) << "\n";
}

void demonstrateErrorHandling() {
    std::cout << "\n=== ERROR HANDLING WITH ENUMS ===\n";

    // Function that returns status
    auto processData = [](bool shouldFail) -> Status {
        if (shouldFail) {
            return Status::Error;
        }
        return Status::Active;
    };

    // Handle different statuses
    std::vector<bool> testCases = {false, true, false};

    for (size_t i = 0; i < testCases.size(); ++i) {
        Status result = processData(testCases[i]);
        std::cout << "Test " << (i + 1) << " result: " << toString(result);

        switch (result) {
            case Status::Active:
                std::cout << " - Processing completed successfully\n";
                break;
            case Status::Error:
                std::cout << " - An error occurred during processing\n";
                break;
            case Status::Pending:
                std::cout << " - Still processing...\n";
                break;
            case Status::Inactive:
                std::cout << " - System is inactive\n";
                break;
        }
    }
}

// ================================
// MAIN FUNCTION
// ================================

int main() {
    std::cout << "C++ SCOPED ENUMS (ENUM CLASS) DEMONSTRATION\n";
    std::cout << "===========================================\n";

    demonstrateBasicScopedEnums();
    demonstrateUnscopedVsScoped();
    demonstrateExplicitTypes();
    demonstrateBitwiseOperations();
    demonstrateEnumIteration();
    demonstrateClassUsage();
    demonstrateAdvancedFeatures();
    demonstrateErrorHandling();

    std::cout << "\n=== END OF DEMONSTRATION ===\n";
    return 0;
}

/*
KEY CONCEPTS SUMMARY:

1. SCOPED ENUMS (enum class):
   - Names don't pollute the global namespace
   - No implicit conversion to integers
   - Type-safe - cannot mix different enum types
   - Can specify underlying type explicitly

2. ADVANTAGES OVER UNSCOPED ENUMS:
   - Better type safety
   - No naming conflicts
   - More explicit code
   - Forward declarable

3. FEATURES:
   - Explicit underlying types (uint8_t, int, etc.)
   - Custom values for enumerators
   - Bitwise operations (with custom operators)
   - Can be used as map keys and in templates

4. BEST PRACTICES:
   - Use enum class instead of enum
   - Provide string conversion functions
   - Use meaningful names
   - Consider underlying type for memory efficiency
   - Implement operators when treating as flags

5. COMMON USE CASES:
   - State machines
   - Error codes and status values
   - Configuration options
   - Bit flags and permissions
   - Type-safe constants

6. C++20 ADDITIONS:
   - std::is_scoped_enum type trait
   - Better support in constexpr contexts
   - Enhanced template usage
*/
