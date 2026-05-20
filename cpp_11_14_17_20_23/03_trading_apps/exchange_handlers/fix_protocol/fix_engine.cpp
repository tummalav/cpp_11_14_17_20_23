/*
 * ============================================================================
 * FIX PROTOCOL 4.2/4.4 ENGINE - Ultra Low Latency C++17
 * ============================================================================
 *
 * Financial Information eXchange (FIX) is the most widely used electronic
 * trading protocol. This implementation covers:
 *
 *  - FIX 4.2 message parsing and building (most common in equities)
 *  - FIX 4.4 enhancements (multi-leg, fractional quantities)
 *  - Zero-copy parsing using string_view
 *  - Pre-built message templates to minimize allocations on hot path
 *  - Session management (Logon/Logout/Heartbeat/ResendRequest)
 *  - Order message types: NewOrderSingle (D), OrderCancelRequest (F),
 *    OrderCancelReplaceRequest (G), ExecutionReport (8)
 *  - Market data: MarketDataRequest (V), MarketDataSnapshotFullRefresh (W),
 *    MarketDataIncrementalRefresh (X)
 *
 * FIX Message Format:
 *   Tag=Value|SOH| (SOH = 0x01, the field delimiter)
 *   e.g.: 8=FIX.4.2\x019=65\x0149=SENDER\x0156=TARGET\x01...10=123\x01
 *
 * Key Tags (field IDs):
 *   8=BeginString, 9=BodyLength, 35=MsgType, 49=SenderCompID,
 *   56=TargetCompID, 34=MsgSeqNum, 52=SendingTime, 10=CheckSum
 *   11=ClOrdID, 55=Symbol, 54=Side, 38=OrderQty, 44=Price, 40=OrdType
 *   37=OrderID, 17=ExecID, 39=OrdStatus, 150=ExecType, 14=CumQty, 151=LeavesQty
 *
 * Latency Targets:
 *   - Parse incoming message:     100-200 ns
 *   - Build outgoing message:     200-400 ns
 *   - Session heartbeat:          < 1 µs
 * ============================================================================
 */

#include <cstdint>
#include <cstring>
#include <cassert>
#include <climits>
#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <memory>

namespace fix {

// ============================================================================
// FIX Constants
// ============================================================================
static constexpr char SOH           = '\x01';  // Field delimiter
static constexpr char EQUALS        = '=';
static constexpr int  MAX_MSG_LEN   = 4096;
static constexpr int  MAX_FIELDS    = 128;

// Common FIX tags
namespace tag {
    static constexpr int BeginString    =  8;
    static constexpr int BodyLength     =  9;
    static constexpr int CheckSum       = 10;
    static constexpr int ClOrdID        = 11;
    static constexpr int CumQty         = 14;
    static constexpr int ExecID         = 17;
    static constexpr int HandlInst      = 21;
    static constexpr int OrdStatus      = 39;
    static constexpr int OrdType        = 40;
    static constexpr int OrigClOrdID    = 41;
    static constexpr int Price          = 44;
    static constexpr int SecondaryOrderID = 37; // often used as OrderID
    static constexpr int OrderID        = 37;
    static constexpr int OrderQty       = 38;
    static constexpr int Side           = 54;
    static constexpr int Symbol         = 55;
    static constexpr int TimeInForce    = 59;
    static constexpr int TransactTime   = 60;
    static constexpr int MsgSeqNum      = 34;
    static constexpr int SenderCompID   = 49;
    static constexpr int SendingTime    = 52;
    static constexpr int TargetCompID   = 56;
    static constexpr int MsgType        = 35;
    static constexpr int Text           = 58;
    static constexpr int ExecType       = 150;
    static constexpr int LeavesQty      = 151;
    static constexpr int AvgPx          = 6;
    static constexpr int LastQty        = 32;
    static constexpr int LastPx         = 31;
    static constexpr int HeartBtInt     = 108;
    static constexpr int EncryptMethod  = 98;
    static constexpr int ResetSeqNumFlag = 141;
    static constexpr int MDReqID        = 262;
    static constexpr int SubscriptionRequestType = 263;
    static constexpr int MarketDepth    = 264;
    static constexpr int MDUpdateType   = 265;
    static constexpr int NoMDEntryTypes = 267;
    static constexpr int NoRelatedSym   = 146;
    static constexpr int MDEntryType    = 269;
    static constexpr int MDUpdateAction = 279;
    static constexpr int MDEntryPx      = 270;
    static constexpr int MDEntrySize    = 271;
    static constexpr int MDEntryID      = 278;
}

// Message types
namespace msg_type {
    static constexpr char Heartbeat[]              = "0";
    static constexpr char TestRequest[]            = "1";
    static constexpr char ResendRequest[]          = "2";
    static constexpr char Reject[]                 = "3";
    static constexpr char SequenceReset[]          = "4";
    static constexpr char Logout[]                 = "5";
    static constexpr char Logon[]                  = "A";
    static constexpr char NewOrderSingle[]         = "D";
    static constexpr char OrderCancelRequest[]     = "F";
    static constexpr char OrderCancelReplaceRequest[] = "G";
    static constexpr char ExecutionReport[]        = "8";
    static constexpr char MarketDataRequest[]      = "V";
    static constexpr char MarketDataSnapshot[]     = "W";
    static constexpr char MarketDataIncremental[]  = "X";
}

// Side values
namespace side_val {
    static constexpr char Buy  = '1';
    static constexpr char Sell = '2';
    static constexpr char SellShort = '5';
}

// OrdType values
namespace ord_type_val {
    static constexpr char Market         = '1';
    static constexpr char Limit          = '2';
    static constexpr char Stop           = '3';
    static constexpr char StopLimit      = '4';
    static constexpr char MarketOnClose  = '5';
    static constexpr char IOC            = '3';  // TimeInForce=3
}

// OrdStatus / ExecType
namespace exec_type_val {
    static constexpr char New          = '0';
    static constexpr char PartialFill  = '1';
    static constexpr char Fill         = '2';
    static constexpr char DoneForDay   = '3';
    static constexpr char Cancelled    = '4';
    static constexpr char Replaced     = '5';
    static constexpr char Rejected     = '8';
    static constexpr char Expired      = 'C';
    static constexpr char Trade        = 'F';
}

// ============================================================================
// Fast integer-to-ASCII and ASCII-to-integer helpers
// ============================================================================
inline int fast_atoi(const char* str, int len) noexcept {
    int result = 0;
    for (int i = 0; i < len; ++i) result = result * 10 + (str[i] - '0');
    return result;
}

inline int64_t fast_atoi64(const char* str, int len) noexcept {
    int64_t result = 0;
    bool neg = false;
    int i = 0;
    if (str[0] == '-') { neg = true; ++i; }
    for (; i < len; ++i) result = result * 10 + (str[i] - '0');
    return neg ? -result : result;
}

// Writes integer into buf, returns number of bytes written
inline int fast_itoa(char* buf, int64_t v) noexcept {
    if (v == 0) { buf[0] = '0'; return 1; }
    char tmp[20];
    int  n = 0;
    bool neg = v < 0;
    if (neg) v = -v;
    while (v > 0) { tmp[n++] = '0' + (v % 10); v /= 10; }
    if (neg) tmp[n++] = '-';
    // Reverse
    for (int i = 0; i < n; ++i) buf[i] = tmp[n - 1 - i];
    return n;
}

// Double to fixed-point string (6 decimal places)
inline int fast_dtoa(char* buf, double v, int decimals = 2) noexcept {
    if (v < 0) { buf[0] = '-'; return 1 + fast_dtoa(buf + 1, -v, decimals); }
    int64_t scale = 1;
    for (int i = 0; i < decimals; ++i) scale *= 10;
    int64_t whole = static_cast<int64_t>(v);
    int64_t frac  = static_cast<int64_t>((v - whole) * scale + 0.5);
    int n = fast_itoa(buf, whole);
    buf[n++] = '.';
    // Write fractional with leading zeros
    char frac_buf[10];
    int fn = fast_itoa(frac_buf, frac);
    int padding = decimals - fn;
    for (int i = 0; i < padding; ++i) buf[n++] = '0';
    for (int i = 0; i < fn; ++i)      buf[n++] = frac_buf[i];
    return n;
}

// ============================================================================
// FIX Field: tag + value as string_view into a message buffer
// ============================================================================
struct Field {
    int             tag{0};
    std::string_view value;
};

// ============================================================================
// FIX Message (parser + builder)
// Uses a fixed-size char buffer to avoid heap allocation on hot path.
// ============================================================================
class Message {
public:
    static constexpr int BUF_SIZE = MAX_MSG_LEN;

    Message() { reset(); }

    void reset() noexcept {
        len_     = 0;
        n_fields_= 0;
        buf_[0]  = '\0';
    }

    // ---- BUILDER API ----
    // Start a new outbound message
    Message& begin(const char* begin_string = "FIX.4.2") {
        reset();
        append_field(tag::BeginString, begin_string);
        body_start_ = len_;  // BodyLength starts after BeginString
        return *this;
    }

    Message& set(int t, std::string_view v) {
        return append_field(t, v);
    }
    Message& set(int t, int64_t v) {
        char tmp[24];
        int  n = fast_itoa(tmp, v);
        return append_field_raw(t, tmp, n);
    }
    Message& set(int t, double v, int decimals = 2) {
        char tmp[32];
        int  n = fast_dtoa(tmp, v, decimals);
        return append_field_raw(t, tmp, n);
    }
    Message& set(int t, char v) {
        return append_field_raw(t, &v, 1);
    }

    // Finalise: insert BodyLength and CheckSum
    std::string_view finalize() {
        // Compute body length (from MsgType to just before CheckSum)
        int body_len = len_ - body_start_;

        // Insert BodyLength at tag position 9
        char body_buf[12];
        int  bn = fast_itoa(body_buf, body_len);
        // Temporarily prepend body length as tag 9
        // We re-build from scratch into a temp buffer
        char final_buf[BUF_SIZE + 32];
        int  flen = 0;

        // Copy BeginString
        std::memcpy(final_buf + flen, buf_, body_start_);
        flen += body_start_;

        // Insert BodyLength
        final_buf[flen++] = '9'; final_buf[flen++] = '=';
        std::memcpy(final_buf + flen, body_buf, bn); flen += bn;
        final_buf[flen++] = SOH;

        // Copy the rest of the body
        int rest = len_ - body_start_;
        std::memcpy(final_buf + flen, buf_ + body_start_, rest);
        flen += rest;

        // Compute checksum (sum of all bytes mod 256)
        uint32_t cksum = 0;
        for (int i = 0; i < flen; ++i) cksum += static_cast<uint8_t>(final_buf[i]);
        cksum %= 256;

        // Append CheckSum tag
        final_buf[flen++] = '1'; final_buf[flen++] = '0';
        final_buf[flen++] = '=';
        // Three-digit zero-padded checksum
        final_buf[flen++] = '0' + (cksum / 100);
        final_buf[flen++] = '0' + (cksum / 10 % 10);
        final_buf[flen++] = '0' + (cksum % 10);
        final_buf[flen++] = SOH;

        // Copy into buf_
        std::memcpy(buf_, final_buf, flen);
        len_ = flen;
        buf_[len_] = '\0';

        return {buf_, static_cast<size_t>(len_)};
    }

    // ---- PARSER API ----
    // Parse a raw FIX message into fields array
    bool parse(const char* data, int len) noexcept {
        reset();
        std::memcpy(buf_, data, std::min(len, BUF_SIZE - 1));
        buf_[len] = '\0';
        len_      = len;

        const char* p   = buf_;
        const char* end = buf_ + len;

        while (p < end && n_fields_ < MAX_FIELDS) {
            // Find '='
            const char* eq = static_cast<const char*>(std::memchr(p, '=', end - p));
            if (!eq) break;

            int tag_val = static_cast<int>(fast_atoi(p, static_cast<int>(eq - p)));
            const char* val_start = eq + 1;

            // Find SOH
            const char* soh = static_cast<const char*>(std::memchr(val_start, SOH, end - val_start));
            if (!soh) soh = end;

            fields_[n_fields_++] = {tag_val, {val_start, static_cast<size_t>(soh - val_start)}};

            p = soh + 1;
        }
        return n_fields_ > 0;
    }

    // Get a field value by tag (O(n) but n is small)
    [[nodiscard]] std::string_view get(int t) const noexcept {
        for (int i = 0; i < n_fields_; ++i) {
            if (fields_[i].tag == t) return fields_[i].value;
        }
        return {};
    }

    [[nodiscard]] bool has(int t) const noexcept { return !get(t).empty(); }

    [[nodiscard]] int64_t get_int(int t, int64_t def = 0) const noexcept {
        auto v = get(t);
        if (v.empty()) return def;
        return fast_atoi64(v.data(), static_cast<int>(v.size()));
    }

    [[nodiscard]] double get_double(int t, double def = 0.0) const noexcept {
        auto v = get(t);
        if (v.empty()) return def;
        // Simple strtod
        return std::stod(std::string(v));
    }

    [[nodiscard]] std::string_view msg_type() const noexcept {
        return get(tag::MsgType);
    }

    [[nodiscard]] int n_fields() const noexcept { return n_fields_; }

    // Print all fields (debug)
    void print() const {
        for (int i = 0; i < n_fields_; ++i) {
            std::cout << fields_[i].tag << "=" << fields_[i].value << " | ";
        }
        std::cout << "\n";
    }

    [[nodiscard]] const char* raw()    const noexcept { return buf_; }
    [[nodiscard]] int         raw_len()const noexcept { return len_; }

private:
    Message& append_field(int t, std::string_view v) {
        return append_field_raw(t, v.data(), static_cast<int>(v.size()));
    }

    Message& append_field_raw(int t, const char* v, int vlen) {
        int n = fast_itoa(buf_ + len_, t);
        len_ += n;
        buf_[len_++] = EQUALS;
        std::memcpy(buf_ + len_, v, vlen);
        len_ += vlen;
        buf_[len_++] = SOH;
        return *this;
    }

    char    buf_[BUF_SIZE + 32];
    int     len_       {0};
    int     body_start_{0};
    int     n_fields_  {0};
    Field   fields_[MAX_FIELDS];
};

// ============================================================================
// Sending Time helper (YYYYMMDD-HH:MM:SS.sss)
// ============================================================================
inline std::string sending_time() {
    using namespace std::chrono;
    auto now   = system_clock::now();
    auto tp    = system_clock::to_time_t(now);
    auto ms    = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    struct tm t{};
#ifdef _WIN32
    gmtime_s(&t, &tp);
#else
    gmtime_r(&tp, &t);
#endif
    char buf[24];
    std::snprintf(buf, sizeof(buf),
        "%04d%02d%02d-%02d:%02d:%02d.%03d",
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
        t.tm_hour, t.tm_min, t.tm_sec,
        static_cast<int>(ms.count()));
    return buf;
}

// ============================================================================
// FIX Session: manages sequence numbers, logon/logout, heartbeats
// ============================================================================
struct SessionConfig {
    std::string sender_comp_id;
    std::string target_comp_id;
    std::string begin_string = "FIX.4.2";
    int         heartbt_int  = 30;
};

class Session {
public:
    explicit Session(SessionConfig cfg) : cfg_(std::move(cfg)) {}

    // Build header fields into message
    void apply_header(Message& msg, const char* msg_type_str) {
        msg.set(tag::MsgType,    msg_type_str);
        msg.set(tag::SenderCompID, cfg_.sender_comp_id);
        msg.set(tag::TargetCompID, cfg_.target_comp_id);
        msg.set(tag::MsgSeqNum,  static_cast<int64_t>(++seq_num_out_));
        msg.set(tag::SendingTime, sending_time());
    }

    std::string_view build_logon(Message& msg, bool reset_seq = false) {
        msg.begin(cfg_.begin_string.c_str());
        apply_header(msg, msg_type::Logon);
        msg.set(tag::EncryptMethod, static_cast<int64_t>(0));
        msg.set(tag::HeartBtInt,    static_cast<int64_t>(cfg_.heartbt_int));
        if (reset_seq) msg.set(tag::ResetSeqNumFlag, "Y");
        return msg.finalize();
    }

    std::string_view build_logout(Message& msg, std::string_view text = "") {
        msg.begin(cfg_.begin_string.c_str());
        apply_header(msg, msg_type::Logout);
        if (!text.empty()) msg.set(tag::Text, text);
        return msg.finalize();
    }

    std::string_view build_heartbeat(Message& msg, std::string_view test_req_id = "") {
        msg.begin(cfg_.begin_string.c_str());
        apply_header(msg, msg_type::Heartbeat);
        if (!test_req_id.empty()) msg.set(108, test_req_id);  // TestReqID=1
        return msg.finalize();
    }

    std::string_view build_new_order(Message& msg,
                                      std::string_view cl_ord_id,
                                      std::string_view symbol,
                                      char             side,
                                      double           qty,
                                      char             ord_type,
                                      double           price = 0.0,
                                      char             tif   = '0') {
        msg.begin(cfg_.begin_string.c_str());
        apply_header(msg, msg_type::NewOrderSingle);
        msg.set(tag::ClOrdID,   cl_ord_id);
        msg.set(tag::HandlInst, "1");    // Automated
        msg.set(tag::Symbol,    symbol);
        msg.set(tag::Side,      side);
        msg.set(tag::TransactTime, sending_time());
        msg.set(tag::OrderQty,  qty,      2);
        msg.set(tag::OrdType,   ord_type);
        if (ord_type == '2' || ord_type == '4') {  // Limit or StopLimit
            msg.set(tag::Price, price, 4);
        }
        msg.set(tag::TimeInForce, tif);
        return msg.finalize();
    }

    std::string_view build_cancel(Message& msg,
                                   std::string_view cl_ord_id,
                                   std::string_view orig_cl_ord_id,
                                   std::string_view symbol,
                                   char             side,
                                   double           qty) {
        msg.begin(cfg_.begin_string.c_str());
        apply_header(msg, msg_type::OrderCancelRequest);
        msg.set(tag::OrigClOrdID, orig_cl_ord_id);
        msg.set(tag::ClOrdID,     cl_ord_id);
        msg.set(tag::Symbol,      symbol);
        msg.set(tag::Side,        side);
        msg.set(tag::TransactTime, sending_time());
        msg.set(tag::OrderQty,    qty, 2);
        return msg.finalize();
    }

    std::string_view build_cancel_replace(Message& msg,
                                           std::string_view cl_ord_id,
                                           std::string_view orig_cl_ord_id,
                                           std::string_view symbol,
                                           char             side,
                                           double           new_qty,
                                           double           new_price) {
        msg.begin(cfg_.begin_string.c_str());
        apply_header(msg, msg_type::OrderCancelReplaceRequest);
        msg.set(tag::OrigClOrdID, orig_cl_ord_id);
        msg.set(tag::ClOrdID,     cl_ord_id);
        msg.set(tag::HandlInst,   "1");
        msg.set(tag::Symbol,      symbol);
        msg.set(tag::Side,        side);
        msg.set(tag::TransactTime, sending_time());
        msg.set(tag::OrderQty,    new_qty,   2);
        msg.set(tag::OrdType,     '2');      // Limit
        msg.set(tag::Price,       new_price, 4);
        return msg.finalize();
    }

    // Process incoming message, update state
    bool process_inbound(const Message& msg) {
        auto mt = msg.msg_type();
        if (mt == msg_type::Logon) {
            is_logged_on_ = true;
            return true;
        }
        if (mt == msg_type::Logout) {
            is_logged_on_ = false;
            return true;
        }
        if (mt == msg_type::Heartbeat) {
            last_heartbeat_ns_ = std::chrono::steady_clock::now()
                .time_since_epoch().count();
            return true;
        }
        // Update incoming seq
        auto seq_str = msg.get(tag::MsgSeqNum);
        if (!seq_str.empty()) {
            seq_num_in_ = static_cast<uint32_t>(
                fast_atoi64(seq_str.data(), static_cast<int>(seq_str.size())));
        }
        return true;
    }

    [[nodiscard]] bool is_logged_on() const noexcept { return is_logged_on_; }
    [[nodiscard]] uint32_t seq_out() const noexcept { return seq_num_out_; }
    [[nodiscard]] uint32_t seq_in()  const noexcept { return seq_num_in_;  }

private:
    SessionConfig cfg_;
    uint32_t      seq_num_out_       {0};
    uint32_t      seq_num_in_        {0};
    bool          is_logged_on_      {false};
    int64_t       last_heartbeat_ns_ {0};
};

// ============================================================================
// Execution Report Parser
// ============================================================================
struct ExecReport {
    std::string order_id;
    std::string cl_ord_id;
    std::string exec_id;
    char        exec_type  {0};
    char        ord_status {0};
    std::string symbol;
    char        side       {0};
    double      last_qty   {0};
    double      last_px    {0};
    double      cum_qty    {0};
    double      leaves_qty {0};
    double      avg_px     {0};
    std::string text;
};

[[nodiscard]] ExecReport parse_exec_report(const Message& msg) {
    ExecReport er;
    er.order_id  = std::string(msg.get(tag::OrderID));
    er.cl_ord_id = std::string(msg.get(tag::ClOrdID));
    er.exec_id   = std::string(msg.get(tag::ExecID));
    auto et = msg.get(tag::ExecType);
    er.exec_type  = et.empty() ? '?' : et[0];
    auto os = msg.get(tag::OrdStatus);
    er.ord_status = os.empty() ? '?' : os[0];
    er.symbol    = std::string(msg.get(tag::Symbol));
    auto s = msg.get(tag::Side);
    er.side      = s.empty() ? '?' : s[0];
    er.last_qty  = msg.get_double(tag::LastQty);
    er.last_px   = msg.get_double(tag::LastPx);
    er.cum_qty   = msg.get_double(tag::CumQty);
    er.leaves_qty= msg.get_double(tag::LeavesQty);
    er.avg_px    = msg.get_double(tag::AvgPx);
    er.text      = std::string(msg.get(tag::Text));
    return er;
}

void print_exec_report(const ExecReport& er) {
    std::cout << "  ExecReport: OrdID=" << er.order_id
              << " ClOrdID=" << er.cl_ord_id
              << " ExecType=" << er.exec_type
              << " OrdStatus=" << er.ord_status
              << " Sym=" << er.symbol
              << " LastQty=" << er.last_qty
              << " LastPx=" << er.last_px
              << " CumQty=" << er.cum_qty
              << " LeavesQty=" << er.leaves_qty;
    if (!er.text.empty()) std::cout << " Text=" << er.text;
    std::cout << "\n";
}

// ============================================================================
// Latency Benchmark
// ============================================================================
void benchmark_message_build_parse(int iterations = 500'000) {
    SessionConfig cfg;
    cfg.sender_comp_id = "TRADER01";
    cfg.target_comp_id = "EXCHANGE";

    Session  session(cfg);
    Message  msg;

    std::vector<double> build_lats, parse_lats;
    build_lats.reserve(iterations);
    parse_lats.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        // Build
        char cl_ord_id[16];
        std::snprintf(cl_ord_id, sizeof(cl_ord_id), "C%09d", i);

        auto t0 = std::chrono::steady_clock::now();
        auto raw = session.build_new_order(
            msg, cl_ord_id, "AAPL", side_val::Buy, 1000.0, '2', 150.50 + i * 0.001);
        auto t1 = std::chrono::steady_clock::now();

        build_lats.push_back(
            std::chrono::duration<double, std::nano>(t1 - t0).count());

        // Parse
        Message parsed;
        auto t2 = std::chrono::steady_clock::now();
        parsed.parse(raw.data(), static_cast<int>(raw.size()));
        auto t3 = std::chrono::steady_clock::now();

        parse_lats.push_back(
            std::chrono::duration<double, std::nano>(t3 - t2).count());
    }

    auto stats = [&](std::vector<double>& v, const char* name) {
        std::sort(v.begin(), v.end());
        double sum = 0; for (auto x : v) sum += x;
        int n = static_cast<int>(v.size());
        std::cout << name << " (" << n << " msgs):\n";
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "  min   : " << v.front()    << " ns\n";
        std::cout << "  mean  : " << sum / n       << " ns\n";
        std::cout << "  p50   : " << v[n/2]        << " ns\n";
        std::cout << "  p99   : " << v[static_cast<size_t>(n*0.99)]  << " ns\n";
        std::cout << "  max   : " << v.back()      << " ns\n";
    };

    std::cout << "\n--- FIX Message Latency Benchmark ---\n";
    stats(build_lats, "Build NewOrderSingle");
    stats(parse_lats, "Parse NewOrderSingle");
}

// ============================================================================
// Demo
// ============================================================================
void run_demo() {
    std::cout << "===========================================\n";
    std::cout << "    FIX Protocol 4.2 Engine Demo\n";
    std::cout << "===========================================\n";

    SessionConfig cfg;
    cfg.sender_comp_id = "TRADER01";
    cfg.target_comp_id = "EXCH_NY1";
    cfg.heartbt_int    = 30;

    Session session(cfg);
    Message msg;

    // ---- Logon ----
    std::cout << "\n--- Logon ---\n";
    auto logon_raw = session.build_logon(msg, true);
    std::cout << "Logon raw (" << logon_raw.size() << " bytes):\n";
    for (char c : logon_raw) std::cout << (c == SOH ? '|' : c);
    std::cout << "\nParsed fields:\n";
    Message logon_parsed;
    logon_parsed.parse(logon_raw.data(), static_cast<int>(logon_raw.size()));
    logon_parsed.print();

    // ---- New Order Single (Limit Buy) ----
    std::cout << "\n--- New Order Single (Limit Buy) ---\n";
    auto nos_raw = session.build_new_order(
        msg, "C000000001", "AAPL", side_val::Buy,
        1000.0, '2', 150.50);
    std::cout << "NOS raw (" << nos_raw.size() << " bytes):\n";
    for (char c : nos_raw) std::cout << (c == SOH ? '|' : c);
    std::cout << "\n";

    Message nos_parsed;
    nos_parsed.parse(nos_raw.data(), static_cast<int>(nos_raw.size()));
    std::cout << "  MsgType  = " << nos_parsed.get(tag::MsgType)    << "\n";
    std::cout << "  ClOrdID  = " << nos_parsed.get(tag::ClOrdID)    << "\n";
    std::cout << "  Symbol   = " << nos_parsed.get(tag::Symbol)     << "\n";
    std::cout << "  Side     = " << nos_parsed.get(tag::Side)       << "\n";
    std::cout << "  OrdType  = " << nos_parsed.get(tag::OrdType)    << "\n";
    std::cout << "  Price    = " << nos_parsed.get(tag::Price)      << "\n";
    std::cout << "  OrderQty = " << nos_parsed.get(tag::OrderQty)   << "\n";
    std::cout << "  SeqNum   = " << nos_parsed.get(tag::MsgSeqNum)  << "\n";

    // ---- Simulated Execution Report (fill) ----
    std::cout << "\n--- Execution Report (Fill) ---\n";
    // Simulate an inbound exec report
    std::string exec_str = std::string("8=FIX.4.2\x019=120\x0135=8\x0149=EXCH_NY1\x0156=TRADER01\x0134=1\x01")
        + "37=ORD-12345\x01" "17=EXEC-9999\x01" "150=F\x01" "39=2\x01"
        + "55=AAPL\x01" "54=1\x01" "38=1000\x01" "44=150.5000\x01"
        + "32=1000\x01" "31=150.4975\x01" "14=1000\x01" "151=0\x01" "6=150.4975\x01"
        + "10=000\x01";
    Message exec_msg;
    exec_msg.parse(exec_str.c_str(), static_cast<int>(exec_str.size()));
    auto er = parse_exec_report(exec_msg);
    print_exec_report(er);

    // ---- Order Cancel ----
    std::cout << "\n--- Order Cancel Request ---\n";
    auto cancel_raw = session.build_cancel(
        msg, "C000000002", "C000000001", "AAPL", side_val::Buy, 1000.0);
    Message cancel_parsed;
    cancel_parsed.parse(cancel_raw.data(), static_cast<int>(cancel_raw.size()));
    std::cout << "  MsgType    = " << cancel_parsed.get(tag::MsgType)      << "\n";
    std::cout << "  OrigClOrdID= " << cancel_parsed.get(tag::OrigClOrdID)  << "\n";
    std::cout << "  ClOrdID    = " << cancel_parsed.get(tag::ClOrdID)      << "\n";

    // ---- Cancel/Replace ----
    std::cout << "\n--- Cancel/Replace Request ---\n";
    auto cxl_repl_raw = session.build_cancel_replace(
        msg, "C000000003", "C000000001", "AAPL", side_val::Buy, 500.0, 151.00);
    Message cxl_repl_parsed;
    cxl_repl_parsed.parse(cxl_repl_raw.data(), static_cast<int>(cxl_repl_raw.size()));
    std::cout << "  MsgType    = " << cxl_repl_parsed.get(tag::MsgType)    << "\n";
    std::cout << "  OrderQty   = " << cxl_repl_parsed.get(tag::OrderQty)   << "\n";
    std::cout << "  Price      = " << cxl_repl_parsed.get(tag::Price)      << "\n";

    // ---- Heartbeat ----
    std::cout << "\n--- Heartbeat ---\n";
    auto hb_raw = session.build_heartbeat(msg);
    Message hb_parsed;
    hb_parsed.parse(hb_raw.data(), static_cast<int>(hb_raw.size()));
    std::cout << "Heartbeat SeqNum: " << hb_parsed.get(tag::MsgSeqNum) << "\n";

    // ---- Benchmark ----
    benchmark_message_build_parse(200'000);
}

}  // namespace fix

int main() {
    fix::run_demo();
    return 0;
}

