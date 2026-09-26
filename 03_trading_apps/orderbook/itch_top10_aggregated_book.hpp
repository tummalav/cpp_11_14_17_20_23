#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace ull::itch {

enum class Side : uint8_t { Bid = 0, Ask = 1 };

template<std::size_t Depth>
struct alignas(64) PublishedSide {
    std::array<uint32_t, Depth> prices{};
    std::array<uint64_t, Depth> quantities{};
    std::array<uint32_t, Depth> order_counts{};
    uint8_t depth{0};
    uint8_t _pad[7]{};
};

template<std::size_t Depth>
struct alignas(64) PublishedBook {
    PublishedSide<Depth> bids{};
    PublishedSide<Depth> asks{};
    uint64_t last_trade_price{0};
    uint64_t total_traded_volume{0};
    uint64_t publish_sequence{0};
};

template<std::size_t PublishedDepth = 10,
         std::size_t MaxOrders = 1u << 15,
         std::size_t MaxPriceLevels = 1u << 12>
class alignas(64) ITCHTop10AggregatedBook {
    static_assert(std::has_single_bit(MaxOrders), "MaxOrders must be a power of two");
    static_assert(std::has_single_bit(MaxPriceLevels), "MaxPriceLevels must be a power of two");

    static constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();
    static constexpr uint64_t EMPTY_KEY = 0;
    static constexpr uint64_t TOMBSTONE_KEY = std::numeric_limits<uint64_t>::max();
    static constexpr uint64_t LEVEL_KEY_TAG = 1ull << 63;
    static constexpr uint64_t PRICE_MASK = 0x00000000FFFFFFFFull;
    static constexpr uint64_t ORDER_MASK = MaxOrders - 1;
    static constexpr uint64_t LEVEL_MASK = MaxPriceLevels - 1;

    struct alignas(32) OrderSlot {
        uint64_t order_ref{EMPTY_KEY};
        uint32_t price{0};
        uint32_t remaining_qty{0};
        uint32_t level_slot{INVALID_INDEX};
        uint8_t side{0};
        uint8_t _pad[11]{};
    };
    static_assert(sizeof(OrderSlot) == 32, "OrderSlot must stay compact");

    struct alignas(64) LevelNode {
        uint32_t price{0};
        uint32_t order_count{0};
        uint64_t aggregate_qty{0};
        uint32_t prev{INVALID_INDEX};
        uint32_t next{INVALID_INDEX};
        uint32_t slot{INVALID_INDEX};
        uint8_t side{0};
        uint8_t active{0};
        uint8_t _pad[34]{};
    };
    static_assert(sizeof(LevelNode) == 64, "LevelNode must fit one cache line");

    struct LevelMapSlot {
        uint64_t key{EMPTY_KEY};
        uint32_t level_slot{INVALID_INDEX};
    };

    struct alignas(64) SideState {
        uint32_t best{INVALID_INDEX};
        uint32_t worst{INVALID_INDEX};
        uint32_t visible_depth{0};
        uint32_t total_levels{0};
        uint8_t _pad[48]{};
    };
    static_assert(sizeof(SideState) == 64, "SideState must fit one cache line");

public:
    using Snapshot = PublishedBook<PublishedDepth>;

    ITCHTop10AggregatedBook() noexcept {
        for (uint32_t i = 0; i < MaxPriceLevels; ++i) {
            level_free_stack_[i] = MaxPriceLevels - 1u - i;
            levels_[i].slot = i;
        }
        publish();
    }

    [[nodiscard]] bool add_order(uint64_t order_ref,
                                 Side side,
                                 uint32_t price,
                                 uint32_t qty) noexcept {
        if (qty == 0 || order_ref == EMPTY_KEY || order_ref == TOMBSTONE_KEY) [[unlikely]] {
            return false;
        }

        OrderSlot* existing = find_order(order_ref);
        if (existing != nullptr) [[unlikely]] {
            return false;
        }

        const uint32_t level_slot = ensure_level(side, price);
        if (level_slot == INVALID_INDEX) [[unlikely]] {
            return false;
        }

        OrderSlot* slot = insert_order(order_ref);
        if (slot == nullptr) [[unlikely]] {
            return false;
        }

        slot->price = price;
        slot->remaining_qty = qty;
        slot->level_slot = level_slot;
        slot->side = static_cast<uint8_t>(side);

        LevelNode& level = levels_[level_slot];
        level.aggregate_qty += qty;
        ++level.order_count;
        publish();
        return true;
    }

    [[nodiscard]] bool cancel_order(uint64_t order_ref, uint32_t cancelled_qty) noexcept {
        return reduce_order(order_ref, cancelled_qty, 0, false);
    }

    [[nodiscard]] bool execute_order(uint64_t order_ref, uint32_t executed_qty) noexcept {
        return reduce_order(order_ref, executed_qty, 0, true);
    }

    [[nodiscard]] bool execute_order(uint64_t order_ref,
                                     uint32_t executed_qty,
                                     uint32_t printable_price) noexcept {
        return reduce_order(order_ref, executed_qty, printable_price, true);
    }

    [[nodiscard]] bool delete_order(uint64_t order_ref) noexcept {
        OrderSlot* slot = find_order(order_ref);
        if (slot == nullptr) [[unlikely]] {
            return false;
        }

        const uint32_t level_slot = slot->level_slot;
        LevelNode& level = levels_[level_slot];
        level.aggregate_qty -= slot->remaining_qty;
        --level.order_count;

        if (level.aggregate_qty == 0 || level.order_count == 0) {
            erase_level(level_slot);
        }

        erase_order(order_ref);
        publish();
        return true;
    }

    [[nodiscard]] bool replace_order(uint64_t old_order_ref,
                                     uint64_t new_order_ref,
                                     uint32_t new_price,
                                     uint32_t new_qty) noexcept {
        OrderSlot* existing = find_order(old_order_ref);
        if (existing == nullptr || new_qty == 0) [[unlikely]] {
            return false;
        }
        const Side side = static_cast<Side>(existing->side);
        if (!delete_order(old_order_ref)) [[unlikely]] {
            return false;
        }
        return add_order(new_order_ref, side, new_price, new_qty);
    }

    [[nodiscard]] bool read_snapshot(Snapshot& out) const noexcept {
        uint64_t seq0 = 0;
        uint64_t seq1 = 0;
        do {
            seq0 = publish_epoch_.load(std::memory_order_acquire);
            if ((seq0 & 1u) != 0u) [[unlikely]] {
                continue;
            }
            const uint32_t active = active_snapshot_.load(std::memory_order_acquire);
            out = published_[active];
            std::atomic_thread_fence(std::memory_order_acquire);
            seq1 = publish_epoch_.load(std::memory_order_relaxed);
        } while (seq0 != seq1);
        return true;
    }

    [[nodiscard]] uint32_t visible_bid_depth() const noexcept {
        return bid_state_.visible_depth;
    }

    [[nodiscard]] uint32_t visible_ask_depth() const noexcept {
        return ask_state_.visible_depth;
    }

    [[nodiscard]] uint32_t total_bid_levels() const noexcept {
        return bid_state_.total_levels;
    }

    [[nodiscard]] uint32_t total_ask_levels() const noexcept {
        return ask_state_.total_levels;
    }

private:
    [[nodiscard]] static constexpr uint64_t mix(uint64_t v) noexcept {
        v ^= v >> 33;
        v *= 0xff51afd7ed558ccdULL;
        v ^= v >> 33;
        v *= 0xc4ceb9fe1a85ec53ULL;
        v ^= v >> 33;
        return v;
    }

    [[nodiscard]] static constexpr uint64_t make_level_key(Side side, uint32_t price) noexcept {
        return LEVEL_KEY_TAG | (static_cast<uint64_t>(static_cast<uint8_t>(side)) << 32) | price;
    }

    [[nodiscard]] static constexpr bool better(Side side,
                                               uint32_t lhs_price,
                                               uint32_t rhs_price) noexcept {
        return side == Side::Bid ? lhs_price > rhs_price : lhs_price < rhs_price;
    }

    [[nodiscard]] static constexpr bool worse(Side side,
                                              uint32_t lhs_price,
                                              uint32_t rhs_price) noexcept {
        return side == Side::Bid ? lhs_price < rhs_price : lhs_price > rhs_price;
    }

    [[nodiscard]] SideState& side_state(Side side) noexcept {
        return side == Side::Bid ? bid_state_ : ask_state_;
    }

    [[nodiscard]] const SideState& side_state(Side side) const noexcept {
        return side == Side::Bid ? bid_state_ : ask_state_;
    }

    [[nodiscard]] OrderSlot* find_order(uint64_t order_ref) noexcept {
        uint64_t idx = mix(order_ref) & ORDER_MASK;
        for (std::size_t probe = 0; probe < MaxOrders; ++probe) {
            OrderSlot& slot = orders_[idx];
            if (slot.order_ref == EMPTY_KEY) {
                return nullptr;
            }
            if (slot.order_ref == order_ref) {
                return &slot;
            }
            idx = (idx + 1u) & ORDER_MASK;
        }
        return nullptr;
    }

    [[nodiscard]] OrderSlot* insert_order(uint64_t order_ref) noexcept {
        uint64_t idx = mix(order_ref) & ORDER_MASK;
        uint64_t first_tombstone = INVALID_INDEX;
        for (std::size_t probe = 0; probe < MaxOrders; ++probe) {
            OrderSlot& slot = orders_[idx];
            if (slot.order_ref == EMPTY_KEY) {
                OrderSlot& target = (first_tombstone != INVALID_INDEX) ? orders_[first_tombstone] : slot;
                target = {};
                target.order_ref = order_ref;
                return &target;
            }
            if (slot.order_ref == TOMBSTONE_KEY && first_tombstone == INVALID_INDEX) {
                first_tombstone = idx;
            }
            idx = (idx + 1u) & ORDER_MASK;
        }
        if (first_tombstone != INVALID_INDEX) {
            OrderSlot& target = orders_[first_tombstone];
            target = {};
            target.order_ref = order_ref;
            return &target;
        }
        return nullptr;
    }

    void erase_order(uint64_t order_ref) noexcept {
        OrderSlot* slot = find_order(order_ref);
        if (slot == nullptr) [[unlikely]] {
            return;
        }
        slot->order_ref = TOMBSTONE_KEY;
        slot->remaining_qty = 0;
        slot->level_slot = INVALID_INDEX;
    }

    [[nodiscard]] LevelMapSlot* find_level_map_slot(uint64_t key) noexcept {
        uint64_t idx = mix(key) & LEVEL_MASK;
        for (std::size_t probe = 0; probe < MaxPriceLevels; ++probe) {
            LevelMapSlot& slot = level_lookup_[idx];
            if (slot.key == EMPTY_KEY) {
                return nullptr;
            }
            if (slot.key == key) {
                return &slot;
            }
            idx = (idx + 1u) & LEVEL_MASK;
        }
        return nullptr;
    }

    [[nodiscard]] uint32_t ensure_level(Side side, uint32_t price) noexcept {
        const uint64_t key = make_level_key(side, price);
        if (LevelMapSlot* slot = find_level_map_slot(key); slot != nullptr) {
            return slot->level_slot;
        }

        if (level_free_top_ == 0) [[unlikely]] {
            return INVALID_INDEX;
        }

        const uint32_t level_slot = level_free_stack_[--level_free_top_];
        LevelNode& level = levels_[level_slot];
        level.price = price;
        level.order_count = 0;
        level.aggregate_qty = 0;
        level.prev = INVALID_INDEX;
        level.next = INVALID_INDEX;
        level.side = static_cast<uint8_t>(side);
        level.active = 1;

        insert_level_map(key, level_slot);
        link_level(level_slot, side);
        ++side_state(side).total_levels;
        return level_slot;
    }

    void insert_level_map(uint64_t key, uint32_t level_slot) noexcept {
        uint64_t idx = mix(key) & LEVEL_MASK;
        uint64_t first_tombstone = INVALID_INDEX;
        for (std::size_t probe = 0; probe < MaxPriceLevels; ++probe) {
            LevelMapSlot& slot = level_lookup_[idx];
            if (slot.key == EMPTY_KEY) {
                LevelMapSlot& target = (first_tombstone != INVALID_INDEX) ? level_lookup_[first_tombstone] : slot;
                target.key = key;
                target.level_slot = level_slot;
                return;
            }
            if (slot.key == TOMBSTONE_KEY && first_tombstone == INVALID_INDEX) {
                first_tombstone = idx;
            }
            idx = (idx + 1u) & LEVEL_MASK;
        }
        if (first_tombstone != INVALID_INDEX) {
            level_lookup_[first_tombstone].key = key;
            level_lookup_[first_tombstone].level_slot = level_slot;
        }
    }

    void erase_level_map(Side side, uint32_t price) noexcept {
        const uint64_t key = make_level_key(side, price);
        if (LevelMapSlot* slot = find_level_map_slot(key); slot != nullptr) {
            slot->key = TOMBSTONE_KEY;
            slot->level_slot = INVALID_INDEX;
        }
    }

    void link_level(uint32_t level_slot, Side side) noexcept {
        SideState& state = side_state(side);
        LevelNode& level = levels_[level_slot];

        if (state.best == INVALID_INDEX) [[unlikely]] {
            state.best = state.worst = level_slot;
            return;
        }

        LevelNode& best_level = levels_[state.best];
        LevelNode& worst_level = levels_[state.worst];

        if (better(side, level.price, best_level.price)) [[likely]] {
            level.next = state.best;
            best_level.prev = level_slot;
            state.best = level_slot;
            return;
        }

        if (worse(side, level.price, worst_level.price)) [[likely]] {
            level.prev = state.worst;
            worst_level.next = level_slot;
            state.worst = level_slot;
            return;
        }

        uint32_t cursor = state.best;
        while (cursor != INVALID_INDEX) {
            LevelNode& current = levels_[cursor];
            if (better(side, current.price, level.price)) {
                cursor = current.next;
                continue;
            }

            level.next = cursor;
            level.prev = current.prev;
            if (current.prev != INVALID_INDEX) {
                levels_[current.prev].next = level_slot;
            }
            current.prev = level_slot;
            return;
        }

        level.prev = state.worst;
        levels_[state.worst].next = level_slot;
        state.worst = level_slot;
    }

    void unlink_level(uint32_t level_slot, Side side) noexcept {
        SideState& state = side_state(side);
        LevelNode& level = levels_[level_slot];

        if (level.prev != INVALID_INDEX) {
            levels_[level.prev].next = level.next;
        } else {
            state.best = level.next;
        }

        if (level.next != INVALID_INDEX) {
            levels_[level.next].prev = level.prev;
        } else {
            state.worst = level.prev;
        }

        level.prev = INVALID_INDEX;
        level.next = INVALID_INDEX;
    }

    void erase_level(uint32_t level_slot) noexcept {
        LevelNode& level = levels_[level_slot];
        const Side side = static_cast<Side>(level.side);
        unlink_level(level_slot, side);
        erase_level_map(side, level.price);
        level.active = 0;
        level.aggregate_qty = 0;
        level.order_count = 0;
        level_free_stack_[level_free_top_++] = level_slot;
        --side_state(side).total_levels;
    }

    [[nodiscard]] bool reduce_order(uint64_t order_ref,
                                    uint32_t delta_qty,
                                    uint32_t printable_price,
                                    bool update_trade) noexcept {
        if (delta_qty == 0) [[unlikely]] {
            return false;
        }

        OrderSlot* slot = find_order(order_ref);
        if (slot == nullptr) [[unlikely]] {
            return false;
        }

        const uint32_t applied = slot->remaining_qty > delta_qty ? delta_qty : slot->remaining_qty;
        LevelNode& level = levels_[slot->level_slot];
        level.aggregate_qty -= applied;
        slot->remaining_qty -= applied;

        if (update_trade) {
            last_trade_price_ = printable_price == 0 ? slot->price : printable_price;
            total_traded_volume_ += applied;
        }

        if (slot->remaining_qty == 0) {
            --level.order_count;
            erase_order(order_ref);
        }

        if (level.aggregate_qty == 0 || level.order_count == 0) {
            erase_level(level.slot);
        }

        publish();
        return true;
    }

    template<Side BookSide>
    void publish_side(PublishedSide<PublishedDepth>& out) noexcept {
        out.depth = 0;
        uint32_t cursor = side_state(BookSide).best;
        while (cursor != INVALID_INDEX && out.depth < PublishedDepth) {
            const LevelNode& level = levels_[cursor];
            out.prices[out.depth] = level.price;
            out.quantities[out.depth] = level.aggregate_qty;
            out.order_counts[out.depth] = level.order_count;
            ++out.depth;
            cursor = level.next;
        }
        for (std::size_t i = out.depth; i < PublishedDepth; ++i) {
            out.prices[i] = 0;
            out.quantities[i] = 0;
            out.order_counts[i] = 0;
        }
    }

    void publish() noexcept {
        publish_epoch_.fetch_add(1, std::memory_order_relaxed);

        const uint32_t next = active_snapshot_.load(std::memory_order_relaxed) ^ 1u;
        Snapshot& snapshot = published_[next];

        publish_side<Side::Bid>(snapshot.bids);
        publish_side<Side::Ask>(snapshot.asks);
        snapshot.last_trade_price = last_trade_price_;
        snapshot.total_traded_volume = total_traded_volume_;
        snapshot.publish_sequence = publish_sequence_ + 1;

        bid_state_.visible_depth = snapshot.bids.depth;
        ask_state_.visible_depth = snapshot.asks.depth;
        publish_sequence_ = snapshot.publish_sequence;

        std::atomic_thread_fence(std::memory_order_release);
        active_snapshot_.store(next, std::memory_order_release);
        publish_epoch_.fetch_add(1, std::memory_order_release);
    }

    alignas(64) std::array<OrderSlot, MaxOrders> orders_{};
    alignas(64) std::array<LevelNode, MaxPriceLevels> levels_{};
    alignas(64) std::array<LevelMapSlot, MaxPriceLevels> level_lookup_{};
    alignas(64) std::array<uint32_t, MaxPriceLevels> level_free_stack_{};
    uint32_t level_free_top_{static_cast<uint32_t>(MaxPriceLevels)};

    alignas(64) SideState bid_state_{};
    alignas(64) SideState ask_state_{};

    alignas(64) std::array<Snapshot, 2> published_{};
    alignas(64) std::atomic<uint32_t> active_snapshot_{0};
    alignas(64) std::atomic<uint64_t> publish_epoch_{0};

    uint64_t last_trade_price_{0};
    uint64_t total_traded_volume_{0};
    uint64_t publish_sequence_{0};
};

} // namespace ull::itch
