#include "itch_top10_aggregated_book.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>

namespace {

using Book = ull::itch::ITCHTop10AggregatedBook<10, 1u << 12, 1u << 10>;

void print_side(const char* label, const ull::itch::PublishedSide<10>& side) {
    std::cout << label << " depth=" << static_cast<int>(side.depth) << '\n';
    for (uint8_t i = 0; i < side.depth; ++i) {
        std::cout << "  L" << static_cast<int>(i + 1)
                  << " px=" << std::setw(6) << side.prices[i]
                  << " qty=" << std::setw(6) << side.quantities[i]
                  << " ord=" << side.order_counts[i] << '\n';
    }
}

} // namespace

int main() {
    Book book;
    Book::Snapshot snapshot;

    for (uint32_t i = 0; i < 12; ++i) {
        const uint32_t bid_px = 100000 - i;
        const uint32_t ask_px = 100010 + i;
        assert(book.add_order(1'000 + i, ull::itch::Side::Bid, bid_px, 100 + i));
        assert(book.add_order(2'000 + i, ull::itch::Side::Ask, ask_px, 200 + i));
    }

    assert(book.read_snapshot(snapshot));
    assert(snapshot.bids.depth == 10);
    assert(snapshot.asks.depth == 10);
    assert(snapshot.bids.prices[0] == 100000);
    assert(snapshot.bids.prices[9] == 99991);
    assert(snapshot.asks.prices[0] == 100010);
    assert(snapshot.asks.prices[9] == 100019);
    assert(book.total_bid_levels() == 12);
    assert(book.total_ask_levels() == 12);

    // Best bid removed -> hidden 11th level automatically becomes visible.
    assert(book.delete_order(1'000));
    assert(book.read_snapshot(snapshot));
    assert(snapshot.bids.prices[0] == 99999);
    assert(snapshot.bids.prices[9] == 99990);

    // New best bid inserted -> current 10th visible level falls back to hidden depth.
    assert(book.add_order(3'000, ull::itch::Side::Bid, 100005, 777));
    assert(book.read_snapshot(snapshot));
    assert(snapshot.bids.prices[0] == 100005);
    assert(snapshot.bids.prices[9] == 99991);

    // Partial cancel/execute preserve the level while reducing aggregate quantity.
    assert(book.cancel_order(3'000, 100));
    assert(book.execute_order(2'000, 50, 100010));
    assert(book.read_snapshot(snapshot));
    assert(snapshot.bids.quantities[0] == 677);
    assert(snapshot.asks.quantities[0] == 150);
    assert(snapshot.last_trade_price == 100010);

    std::cout << "ITCH top-10 aggregated book demo\n";
    std::cout << "================================\n";
    std::cout << "Internal levels tracked: bids=" << book.total_bid_levels()
              << " asks=" << book.total_ask_levels() << '\n';
    std::cout << "Visible publication depth: bids=" << book.visible_bid_depth()
              << " asks=" << book.visible_ask_depth() << '\n';
    std::cout << "Publish sequence=" << snapshot.publish_sequence
              << " traded_volume=" << snapshot.total_traded_volume
              << " last_trade_px=" << snapshot.last_trade_price << "\n\n";

    print_side("BIDS", snapshot.bids);
    print_side("ASKS", snapshot.asks);
    return 0;
}
