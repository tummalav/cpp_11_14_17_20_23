# Execution Algorithms — Ultra-Low Latency C++ Implementation

Five production-quality execution algorithms for capital markets, implemented with
full ULL discipline.  Each file is self-contained, compiles standalone, and includes
a simulation harness with a `main()`.

---

## Table of Contents

1. [Algo Overview & When to Use Each](#1-algo-overview--when-to-use-each)
2. [VWAP — Volume Weighted Average Price](#2-vwap--volume-weighted-average-price)
3. [TWAP — Time Weighted Average Price](#3-twap--time-weighted-average-price)
4. [Participate / POV — Percentage of Volume](#4-participate--pov--percentage-of-volume)
5. [MOO — Market On Open](#5-moo--market-on-open)
6. [MOC — Market On Close](#6-moc--market-on-close)
7. [Side-by-Side Comparison](#7-side-by-side-comparison)
8. [ULL Architecture (shared across all algos)](#8-ull-architecture-shared-across-all-algos)
9. [Build & Run](#9-build--run)

---

## 1. Algo Overview & When to Use Each

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │  INPUT = large parent order to fill without moving the market   │
                    └────────────────────────────┬────────────────────────────────────┘
                                                 │
               ┌────────────────────────────────────────────────────────────────┐
               │      Do you need to fill at a specific TIME-OF-DAY PRICE?     │
               └───────────┬────────────────────────────────────────┬───────────┘
                           │ YES                                     │ NO
                    ┌──────┴──────┐                         ┌────────┴────────┐
                    │  Which one? │                         │  Fill over time │
                    └──────┬──────┘                         │  tracking mkt   │
                           │                                └────────┬────────┘
              ┌────────────┼────────────┐                            │
              ▼            ▼            ▼                     ┌──────┴──────┐
           Opening      Closing    Intraday VWAP/TWAP         │   POV/     │
           Auction       Auction    benchmark?                 │ Participate │
              │              │           │                     └─────────────┘
           MOO algo      MOC algo        │
                                ┌────────┴────────┐
                                │ Volume data      │
                                │ available?       │
                                └────┬─────────────┘
                                     │
                       ┌─────────────┼─────────────┐
                       │ YES         │             │ NO / unreliable
                       ▼             ▼             ▼
                     VWAP          VWAP          TWAP
                  (U-shaped     (custom        (flat time
                   profile)      profile)       schedule)
```

| Algo | Benchmark | Volume data needed | Clock-driven | Use when… |
|---|---|---|---|---|
| VWAP | Intraday VWAP price | Yes — historical profile | Partially | Must beat the day's VWAP; liquid instruments with stable volume patterns |
| TWAP | Time-uniform average | No | Fully | Volume data unreliable; compliance mandates uniform participation; illiquid/basket |
| POV / Participate | No benchmark — minimize impact | Yes — real-time tape | No | Reduce position gradually; match natural liquidity pace; newly-listed stocks |
| MOO | Opening auction print | No | Yes — specific time | Index rebalance at open; earnings pre-positioning; avoid pre-market spread |
| MOC | Closing auction print | No | Yes — specific time | Index/ETF NAV benchmark; close-of-day repositioning; highest-volume print |

---

## 2. VWAP — Volume Weighted Average Price

### Concept

VWAP = Σ(price × volume) / Σ(volume) over the trading window.

The algo pre-splits the parent order across time buckets **proportional to a historical
volume profile**.  If 12% of daily volume historically trades in the 9:30–10:00 slot,
the algo sends 12% of the parent in that bucket.

The goal is to achieve an average execution price ≤ (for buys) or ≥ (for sells)
the day's VWAP benchmark — the most widely used execution quality metric globally.

### Execution flow

```
At start():
  1. Normalise volume_profile[] → compute bucket_qty[i] = total_qty × profile[i]
  2. Store bucket fire times, end times

On each timer tick (every ~100ms):
  1. Determine current bucket index
  2. If bucket changed: cancel live orders, redistribute residual (catch-up)
  3. If no live order and bucket not yet sent:
       a. Compute child qty = min(bucket_target - filled, max_pct × mkt_vol)
       b. Apply ±randomise_bp jitter (prevents fingerprinting)
       c. Set limit price via urgency (passive/mid/aggressive)
       d. Submit child order (IOC)

On fill:
  Update bucket filled qty, cumulative filled qty, total value (for avg px)
  If cum_filled >= total_qty: finish
```

### All Parameters

| Parameter | Type | Default | Description |
|---|---|---|---|
| `symbol` | `char[8]` | `"AAPL"` | Instrument identifier (8-char, null-padded) |
| `side` | `Side` | `Buy` | Buy or Sell |
| `total_qty` | `Qty` | 100,000 | Parent order quantity (shares/contracts) |
| `start_time_ns` | `NsTime` | 09:30 | Window start (nanoseconds since midnight) |
| `end_time_ns` | `NsTime` | 16:00 | Window end |
| `num_buckets` | `uint32_t` | 13 | Number of equal time slices (13×30min = 6.5h US session) |
| `volume_profile[]` | `double[48]` | U-curve | Historical % of daily volume per bucket. **Must sum to 1.0**. Default: classic US equity U-curve (heavy open/close, light midday) |
| `limit_price` | `Price` | 0 | Outer limit price (0 = no limit / market). Never fills worse than this. |
| `max_participation` | `double` | 0.20 | Cap the algo to at most 20% of real-time market volume in any bucket. Prevents price impact. Typical: 0.15–0.25 |
| `min_order_size` | `Qty` | 100 | Skip bucket if computed child qty < this. Avoids odd-lot noise. |
| `urgency` | `uint8_t` | 1 | **0=passive** (post at bid/ask), **1=neutral** (midpoint), **2=aggressive** (cross spread/take). Determines limit price. |
| `catch_up_enabled` | `bool` | `true` | Redistribute unfilled residual from previous buckets pro-rata into remaining buckets |
| `randomise_bp` | `uint32_t` | 1000 | ±basis-points size jitter. 1000 bp = ±10%. Uses RDTSC-seeded LCG (no heap). Breaks predictable pattern. |

### When to use VWAP

- **Yes**: Large-cap liquid equities (AAPL, MSFT, SPY) with predictable intraday volume patterns
- **Yes**: You have 30–90 days of clean volume data to build the profile
- **Yes**: Benchmark is VWAP and performance attribution tracks slippage vs VWAP
- **Yes**: Execution window is ≥ 30 minutes (enough buckets to spread impact)
- **No**: Illiquid or newly-listed instruments (volume profile unreliable)
- **No**: Execution window < 10 minutes (TWAP or aggressive IOC more appropriate)
- **No**: You need to finish by a strict deadline regardless of volume (use TWAP with MOC sweep)

### Compared to TWAP / POV

```
VWAP:  schedule ∝ historical volume  → heavy at open and close, light midday
TWAP:  schedule ∝ time               → perfectly flat, no volume weighting
POV:   schedule ∝ real-time volume   → adapts live, no historical data needed
```

---

## 3. TWAP — Time Weighted Average Price

### Concept

Divide the window into N equal slices and send the same quantity in each slice —
regardless of market volume.  The simplest execution benchmark; purely clock-driven.

### Execution flow

```
At start():
  1. slice_duration = (end - start) / num_slices
  2. For each slice i:
       base_qty = total_qty / num_slices  (+ remainder on last slice)
       fire_ns  = start + i × duration ± timing_jitter  (pre-randomised with LCG)
  3. carry_qty_ = 0

On timer (every ~50ms):
  While next_slice_.fire_ns <= now_ns:
    qty = slice.target + carry_qty_
    Apply ±randomise_bp jitter (LCG)
    Round to lot, clip at max_clip_bp
    Submit IOC child at urgency-based limit
    Advance next_slice_

At end_time (optional MOC sweep):
  If remaining > 0 and allow_moc_sweep: submit MOC for remainder
```

### All Parameters

| Parameter | Type | Default | Description |
|---|---|---|---|
| `symbol` | `char[8]` | `"MSFT"` | Instrument identifier |
| `side` | `Side` | `Buy` | Buy or Sell |
| `total_qty` | `Qty` | 60,000 | Parent order quantity |
| `start_time_ns` | `NsTime` | 10:00 | Window start |
| `end_time_ns` | `NsTime` | 15:00 | Window end |
| `num_slices` | `uint32_t` | 60 | Number of equal time slices. More slices → smaller impact per order but more messages. Typical: 20–120 |
| `limit_price` | `Price` | 0 | Outer worst price (0 = no limit) |
| `urgency` | `uint8_t` | 1 | 0=passive, 1=neutral, 2=aggressive |
| `randomise_bp` | `uint32_t` | 1000 | ±basis-points **size** jitter per slice (LCG, no heap) |
| `timing_jitter_bp` | `uint32_t` | 2000 | ±basis-points **fire-time** jitter within each slice. e.g. 2000 bp = ±20% of slice duration. Pre-computed at start(). |
| `catch_up_enabled` | `bool` | `true` | Carry unfilled residual (`carry_qty_`) into next slice's target |
| `max_clip_bp` | `uint32_t` | 500 | Hard cap per child = N bp of total_qty (500 bp = 5% per slice max) |
| `min_order_size` | `Qty` | 100 | Skip slice (accumulate as carry) if qty < this |
| `allow_moc_sweep` | `bool` | `true` | Submit MOC for any remaining quantity at end_time |

### When to use TWAP

- **Yes**: Illiquid stocks, emerging markets, crypto (volume data unreliable or absent)
- **Yes**: Regulatory/compliance mandates uniform participation (equal-time slices)
- **Yes**: Basket rebalancing with many legs having different intraday volume patterns
- **Yes**: Short execution windows (< 30 min) where VWAP profile bucketing is too coarse
- **Yes**: Dark-pool iceberg strategies where predictability is acceptable
- **No**: When you must benchmark vs VWAP (use VWAP algo)
- **No**: When market volume is very uneven (open/close heavy) and you care about impact

### Compared to VWAP / POV

```
TWAP front-loads vs VWAP at open: TWAP sends same qty at 09:30 as at 12:00.
VWAP sends 12% of order at 09:30, only 5% at 12:00 (matching market rhythm).

TWAP is simpler to predict (no volume profile dependency) but may trade into
thin midday liquidity, slightly widening spreads on larger orders.

Timing jitter (timing_jitter_bp) is the key anti-gaming parameter:
without it, a predictable slice every 5min is easily front-run by HFTs.
```

---

## 4. Participate / POV — Percentage of Volume

### Concept

The Participate (POV = Percentage Of Volume) algo subscribes to every public trade
print and maintains a rolling window of market turnover.  It sends child orders so
that the algo's cumulative filled quantity stays at a fixed fraction of total market
volume — the **participation rate**.

Unlike VWAP/TWAP (pre-scheduled), POV is **purely reactive**:
- Fast market → algo sends more
- Slow/illiquid market → algo sends less or nothing
- No historical volume profile needed

### Execution flow

```
On every trade print (hot path):
  rolling_vol_buf_.push(timestamp, qty)   ← O(1), zero alloc

On every quote update (hot path):
  last_bid_ = bid;  last_ask_ = ask       ← 2 atomic stores

On timer (every check_interval_ns, default 5s):
  mkt_vol  = rolling_vol_buf_.query(now)   ← evict stale, return sum
  algo_vol = algo_fill_buf_.query(now)     ← our fills in same window
  deficit  = mkt_vol × target_pov_bp / 10000 - algo_vol
  qty      = clamp(deficit, vol_min, vol_max)  ← all integer arithmetic
  if qty >= min_order_size: submit IOC child
  else: accumulate as deficit_carry_

On fill:
  algo_fill_buf_.push(fill_time, fill_qty)
  if self_fill_dedupe: rolling_vol_buf_.subtract(fill_qty)
  ← removes own fill from market vol denominator (prevents double-counting)
```

### All Parameters

| Parameter | Type | Default | Description |
|---|---|---|---|
| `symbol` | `char[8]` | `"SPY"` | Instrument identifier |
| `side` | `Side` | `Buy` | Buy or Sell |
| `total_qty` | `Qty` | 20,000 | Parent order quantity |
| `start_time_ns` | `NsTime` | 09:30 | Window start |
| `end_time_ns` | `NsTime` | 15:30 | Hard deadline |
| `target_pov_bp` | `uint32_t` | 1000 | **Target participation rate in basis points**. 1000 = 10% of all market trades. This is the most important parameter. |
| `min_pov_bp` | `uint32_t` | 500 | Floor participation (5%). In a very quiet market, algo still sends at least this rate of observed volume. |
| `max_pov_bp` | `uint32_t` | 2500 | Ceiling participation (25%). Caps price impact. Never take more than 25% of market volume in any window. |
| `check_interval_ns` | `NsTime` | 5s | How often to evaluate and send. Shorter = more responsive but more orders/messages. Typical: 1–5 seconds. |
| `volume_window_ns` | `NsTime` | 60s | Rolling window width for market volume measurement. Longer = smoother but slower to react to liquidity changes. Typical: 30–60s. |
| `min_order_size` | `Qty` | 100 | Minimum child order. Deficit accumulated in `deficit_carry_` if below this. |
| `max_order_size` | `Qty` | 5,000 | Hard clip per child order (absolute maximum regardless of deficit). |
| `limit_price` | `Price` | 0 | Outer worst price (0 = urgency-based) |
| `urgency` | `uint8_t` | 1 | 0=passive, 1=neutral, 2=aggressive |
| `self_fill_dedupe` | `bool` | `true` | Subtract own fills from market vol denominator. **Critical**: without this, the algo thinks it is behind because its own fills count as market vol it should participate in. |
| `allow_end_sweep` | `bool` | `true` | Market-order sweep of remaining qty at end_time |

### When to use Participate/POV

- **Yes**: Illiquid or volatile instruments where volume is unpredictable intraday
- **Yes**: Risk-reduction trades: e.g. exiting a position over hours where you want impact ∝ liquidity
- **Yes**: Newly-listed securities with no historical volume profile (VWAP impossible)
- **Yes**: When you care more about price impact than finishing by a specific time
- **Yes**: Instruments with high volume variance (biotech, small-cap, news-driven flow)
- **No**: Index rebalance requiring exact participation at a specific time (use MOC)
- **No**: When you must benchmark vs VWAP and have the volume data
- **No**: Extremely illiquid instruments where market volume is near-zero (algo never sends)

### Compared to VWAP / TWAP

```
VWAP:  schedule determined by HISTORICAL data  → can be wrong if today ≠ average
TWAP:  schedule determined by CLOCK only       → ignores all market signals
POV:   schedule determined by REAL-TIME tape   → adapts, but no fixed deadline guarantee

Key trade-off:
  POV might not complete by end_time in a slow market.
  VWAP/TWAP guarantee completion rate (subject to fill rate).

Self-fill dedup is the key correctness issue:
  Without it: algo fills → counted in market vol → algo thinks it needs to
  participate more → sends more → inflates participation → price impact.
  With it:   own fills subtracted from denominator → accurate POV tracking.
```

---

## 5. MOO — Market On Open

### Concept

A MOO (Market On Open) order executes at the single clearing price determined by
the exchange's **opening auction** — the price that maximises matched volume from
all accumulated pre-open interest.

```
Timeline (US equities, NYSE/NASDAQ):

  08:00   Brokers accept MOO/LOO client orders (pre-market)
  09:28   Exchange opens submission window — orders enter auction book
  09:29:50 NYSE cancel/replace deadline (NASDAQ: 09:29:55)
    │      ← window for on_imbalance() reactions
  09:30:00 Opening auction executes — single clearing price
    │      All MOO orders fill at this price (no price guarantee)
    │      LOO orders fill only if price ≤ limit (buy) or ≥ limit (sell)
    └──────── on_fill() called with opening print
```

**NOII (Net Order Imbalance Indicator)**: Published every 5s from ~09:28.
Contains: paired qty, imbalance qty, imbalance side, reference price, near/far price.
Used to decide whether to switch MOO→LOO or cancel before deadline.

### State Machine

```
Idle → AwaitingSubmit → OrderLive → AuctionPending → Complete
                           │
                     (cancel_deadline)
                     [NOII reactions happen here]
```

### All Parameters

| Parameter | Type | Default | Description |
|---|---|---|---|
| `symbol` | `char[8]` | `"GOOG"` | Instrument identifier |
| `side` | `Side` | `Buy` | Buy or Sell |
| `total_qty` | `Qty` | 5,000 | Parent order quantity |
| `order_type` | `OrderType` | `MOO` | `MOO` = market at open (no price limit). `LOO` = limit at open (fill only if price is within limit). |
| `limit_price` | `Price` | 0 | LOO limit price. For MOO: ignored (fills at any opening price). |
| `submit_time_ns` | `NsTime` | 09:28 | When to submit order to exchange auction book. Must be ≥ exchange submission open. Must be < cancel_deadline_ns. |
| `cancel_deadline_ns` | `NsTime` | 09:29:50 | **Exchange-enforced last cancel/replace time**. NYSE: 09:29:50. NASDAQ: 09:29:55. After this: order is locked. |
| `auction_time_ns` | `NsTime` | 09:30:00 | Expected auction execution time. Used for fill confirmation window. |
| `post_open_timeout_ns` | `NsTime` | 09:30:30 | If no fill received by this time, mark as stale/cancelled. |
| `imbalance_action` | `MOOImbalanceAction` | `DoNothing` | **`DoNothing`**: accept fill at any opening price (pure MOO). **`SwitchToLOO`**: replace with LOO at ref_price if adverse imbalance — caps downside. **`CancelOrder`**: cancel and reroute to continuous session. |
| `imbalance_threshold` | `uint32_t` | 200 | Act when imbalance_qty on our side ≥ threshold% × our qty. e.g. 200 = act when imbalance ≥ 2× our order. |
| `pre_open_pov_bp` | `uint32_t` | 0 | (Reserved) POV rate in pre-market continuous session. 0 = disabled. |

### When to use MOO

- **Yes**: Index rebalance on opening day (funds must participate at opening print)
- **Yes**: Earnings release repositioning (avoid pre-market spread of 5–50× normal)
- **Yes**: Large orders where opening auction has historically high paired volume
- **Yes**: When price is less important than guaranteeing participation at market open
- **No**: When reference price matters and opening print may gap significantly
- **No**: Small-cap/illiquid instruments where opening auction has low paired volume
- **No**: When continuous session after open is fine (use VWAP from 09:30)

### Compared to continuous-session execution

```
Opening auction advantages:
  • Single clearing price — no intraday slippage within the order
  • Highest liquidity pool of the morning
  • No bid-ask spread crossing cost (all orders match at one price)

Opening auction risks:
  • Price gap risk: opening print can be far from previous close
  • LOO may not fill at all if price gaps beyond limit
  • Cancel deadline is hard: cannot react after 09:29:50

LOO vs MOO:
  MOO:  guaranteed fill at opening price (whatever it is)
  LOO:  fill only if price within limit → may NOT fill (risk: missed execution)
```

---

## 6. MOC — Market On Close

### Concept

A MOC order executes at the **closing auction print** (16:00:00 US equities) — the
highest-volume event of the day (typically >10% of daily volume in a single print).

```
Timeline (US equities):

  NYSE                             NASDAQ
  ─────────────────────────────    ─────────────────────────────
  15:45  MOC/LOC submit deadline   15:55  MOC submit deadline
  15:45+ Closing imbalance feed    15:55+ Imbalance published
  15:58  Cancel/replace deadline   15:59:55 Cancel deadline
  16:00  Closing auction           16:00  Closing Cross
```

**Optional pre-close participation**: Run a continuous-session POV sub-algo
(e.g. 15:30–15:45) to fill part of the order before the auction, reducing
auction risk. The remainder is submitted as MOC at the deadline.

### State Machine

```
Idle
 └─► PreCloseParticipating  ← (if pre_close_participation=true)
      └─► AwaitingSubmit    ← (at submit_deadline_ns)
           └─► OrderLive    ← MOC/LOC on exchange; NOII monitoring; can replace
                └─► AuctionPending ← (after cancel_deadline_ns)
                     └─► Complete  ← (on fill)
```

### All Parameters

| Parameter | Type | Default | Description |
|---|---|---|---|
| `symbol` | `char[8]` | `"QQQ"` | Instrument identifier |
| `side` | `Side` | `Buy` | Buy or Sell |
| `total_qty` | `Qty` | 8,000 | Parent order quantity |
| `order_type` | `OrderType` | `MOC` | `MOC` = market at close. `LOC` = limit at close (fill only if price within limit). |
| `limit_price` | `Price` | 0 | LOC limit price. Ignored for MOC. |
| `submit_deadline_ns` | `NsTime` | 15:45 | **Must submit BEFORE this time**. NYSE: 15:45. NASDAQ: 15:55. Submit earlier to avoid latency risk at the deadline. |
| `cancel_deadline_ns` | `NsTime` | 15:58 | Last cancel/replace time. NYSE: 15:58. NASDAQ: 15:59:55. Algo can react to imbalance until this time. |
| `close_time_ns` | `NsTime` | 16:00 | Closing auction execution time. |
| `post_close_timeout_ns` | `NsTime` | 16:00:30 | If fill not received within 30s of close: mark stale. |
| `pre_close_participation` | `bool` | `false` | Enable continuous-session POV sub-algo before submitting MOC |
| `pre_close_start_ns` | `NsTime` | 15:30 | Start pre-close POV participation at this time |
| `pre_close_pov_bp` | `uint32_t` | 500 | POV rate (5%) during pre-close continuous session |
| `pre_close_target_pct` | `double` | 0.30 | Fraction of total_qty to fill pre-close (30%). Remaining 70% submitted as MOC. |
| `imbalance_action` | `MOCImbalanceAction` | `DoNothing` | **`DoNothing`**: fill at any closing price. **`SwitchToLOC`**: replace with LOC at ref_price if adverse imbalance. **`CancelOrder`**: cancel MOC, keep pre-close fills only. |
| `imbalance_threshold` | `uint32_t` | 200 | Act when closing imbalance_qty on our side ≥ threshold% × remaining. |

### When to use MOC

- **Yes**: Index/ETF funds tracking NAV benchmarks (must execute at closing price)
- **Yes**: End-of-day portfolio rebalancing where closing price is the reference
- **Yes**: Large institutional orders where closing auction has deepest liquidity
- **Yes**: Risk management: close residual positions by end of day (guaranteed fill)
- **No**: Intraday strategies that need immediate execution
- **No**: When closing price is expected to deviate significantly (use LOC instead)
- **No**: Non-US markets with different auction mechanics (parameters differ)

### LOC vs MOC

```
MOC:  guaranteed fill at whatever the closing price is → no price risk, but
      can be expensive if closing print is adverse (large imbalance, news event)

LOC:  fill only if closing price is within limit → protects against adverse price,
      but may NOT fill → unhedged position overnight (execution risk)

Best practice: use MOC for index tracking (must fill); use LOC for directional
trades where price matters more than guaranteed execution.

Pre-close participation (pre_close_participation=true):
  Reduces auction concentration risk by filling part of order in continuous
  session before the MOC deadline.  Typical: fill 20–30% pre-close, 70–80% at MOC.
  Trade-off: pre-close fills may be at worse prices than closing print.
```

---

## 7. Side-by-Side Comparison

### Decision Matrix

| | VWAP | TWAP | POV | MOO | MOC |
|---|---|---|---|---|---|
| **Benchmark** | Intraday VWAP | Time-uniform avg | None | Opening print | Closing print |
| **Volume data needed** | Historical profile | None | Real-time tape | None | None |
| **Guaranteed finish time** | Yes (end_time) | Yes (end_time) | No (rate-based) | Yes (open) | Yes (close) |
| **Price guarantee** | No | No | No | No (MOO) / Yes cap (LOO) | No (MOC) / Yes cap (LOC) |
| **Adapts to live market** | Partially (participation cap) | No | Yes (fully reactive) | Partially (NOII) | Partially (imbalance) |
| **Historical data required** | Yes | No | No | No | No |
| **Hot path latency** | <500ns | <200ns | <200ns | <100ns (pre-staged) | <100ns (pre-staged) |
| **Best for** | Liquid large-caps | Illiquid/compliance | Position reduction | Open repositioning | NAV/index close |

### Parameter Priority Guide

```
Urgency parameter (all algos):
  urgency=0 (passive)    → post at bid (buy) or ask (sell)
                           Best: quiet intraday, thick book, long execution window
  urgency=1 (neutral)    → post at midpoint
                           Best: balanced; most common default
  urgency=2 (aggressive) → take at ask (buy) or bid (sell)
                           Best: end-of-window catch-up, important benchmark

Randomise parameter (VWAP/TWAP):
  randomise_bp=0         → predictable pattern; easy to front-run
  randomise_bp=500–1500  → safe range; ±5–15% size variation per order
  randomise_bp>2000      → too much variance; may miss bucket targets

Participation rate (POV only):
  target_pov_bp < 500    → slow; may not complete within window
  target_pov_bp 500–1500 → sweet spot for most liquid instruments
  target_pov_bp > 2000   → aggressive; may move price on illiquid stocks

max_participation (VWAP):
  This caps the algo's share of real-time market volume PER BUCKET.
  If historical profile says "send 1000 shares in bucket 5" but only
  100 shares traded in the market, the algo sends max 20 (20% of 100)
  — not 1000. Catches cases where today's volume is far below profile.
```

### Typical Execution Window Choice

```
Order Size vs Daily Volume    Recommended Algo
─────────────────────────     ────────────────────────────────────────
< 0.5% of ADV                 Single IOC or limit order — no algo needed
0.5–2% of ADV                 TWAP or short VWAP window (1–2 hours)
2–5% of ADV                   VWAP or POV full session
5–15% of ADV                  POV (patient) + MOC sweep for residual
> 15% of ADV                  MOC/MOO (auction) + multi-day VWAP
```

---

## 8. ULL Architecture (shared across all algos)

All algos share `algo_common.hpp` which enforces:

### Hot path (on_trade / on_quote / on_timer / on_fill)

```
Performance budget per call:
  on_trade  → <200ns    (per tick; called thousands of times per second)
  on_quote  → <50ns     (per quote; 2 atomic relaxed stores)
  on_timer  → <500ns    (every 100ms–5s; includes order sizing logic)
  on_fill   → <300ns    (per fill; atomic metric updates)
```

| Constraint | Implementation |
|---|---|
| Zero heap allocation | `OrderPool<N>` pre-allocated; `RollingVolBuf<N>` fixed circular; no `new`/`malloc`/`std::vector` |
| Integer prices | `Price = uint32_t × 10000`; midpoint = `(bid+ask)>>1`; no float division |
| Lock-free metrics | `std::atomic<uint64_t>` with `memory_order_relaxed`; no mutex |
| No I/O in hot path | `ALGO_LOG` → SPSC `LogRing`; flushed only at cold-path events |
| Inline PRNG | LCG (`state = state × C + D`) for jitter; no `std::mt19937` in hot path |
| RDTSC timestamps | Every `submit()` stamped; `fill_tsc - submit_tsc` = wire-to-fill latency |
| `__builtin_expect` | All unlikely branches (`state != Active`, ring full, pool exhausted) |
| Cache alignment | `alignas(64)` on all hot data structures; hot atomics on own cache lines |

### RollingVolBuf<N>

Replaces `std::deque` for rolling volume window:
```
push(ts, qty)  → single entry write + sum += qty           O(1)
query(now)     → evict head entries older than window_ns    O(k) amortised ≈ O(1)
subtract(qty)  → sum -= qty  (self-fill dedup for POV)      O(1)
```
Power-of-2 capacity uses bitmask index (`buf_[tail_ & MASK]`) — no modulo.

### OrderPool<N>

Replaces `std::vector<ChildOrder>` and dynamic allocation:
```
alloc()  → scan N=64 slots for live=false, return pointer  O(N) ≈ O(1) at steady state
find(id) → scan for matching order_id                       O(N)
cancel_all(router) → cancel all live orders                 O(N)
```
All `N×sizeof(ChildOrder)` bytes in BSS — never paged out under `mlockall`.

### LogRing (SPSC)

```
push(msg, len)  → hot path: memcpy 128 bytes + seq store   ~20ns
flush()         → cold path: drain ring to stdout           once per event
```
If ring is full, message is **dropped** — hot path never blocks on I/O.

---

## 9. Build & Run

### Compile each algo standalone

```bash
cd 03_trading_apps/execution_algos

# VWAP
g++ -std=c++17 -O3 -march=native -DNDEBUG vwap_algo.cpp -lpthread -o vwap_algo && ./vwap_algo

# TWAP
g++ -std=c++17 -O3 -march=native -DNDEBUG twap_algo.cpp -lpthread -o twap_algo && ./twap_algo

# Participate / POV
g++ -std=c++17 -O3 -march=native -DNDEBUG participate_algo.cpp -lpthread -o participate_algo && ./participate_algo

# MOO
g++ -std=c++17 -O3 -march=native -DNDEBUG moo_algo.cpp -lpthread -o moo_algo && ./moo_algo

# MOC
g++ -std=c++17 -O3 -march=native -DNDEBUG moc_algo.cpp -lpthread -o moc_algo && ./moc_algo
```

### Build all at once

```bash
for f in vwap twap participate moo moc; do
  g++ -std=c++17 -O3 -march=native -DNDEBUG ${f}_algo.cpp -lpthread -o ${f}_algo
done
```

### RHEL 8/9 production build

```bash
g++ -std=c++17 -O3 -march=native -mtune=native -flto -ffast-math \
    -DNDEBUG -D_GNU_SOURCE \
    vwap_algo.cpp -lpthread -o vwap_algo

# Run with real-time priority (requires CAP_SYS_NICE):
sudo setcap cap_sys_nice+eip ./vwap_algo && ./vwap_algo
```

### Injecting a real order router (production)

Replace `MockRouter` with your FIX/OUCH/binary router:

```cpp
class MyFIXRouter final : public IOrderRouter {
public:
    uint64_t submit(const ChildOrder& o) override {
        // encode FIX NewOrderSingle → send via TCP socket
        return fix_session_.send_new_order(o);
    }
    bool cancel(uint64_t id) override {
        return fix_session_.send_cancel(id);
    }
    bool replace(uint64_t id, Qty qty, Price px) override {
        return fix_session_.send_replace(id, qty, px);
    }
private:
    FIXSession fix_session_;
};

// Wire up:
MyFIXRouter router;
VWAPAlgo    algo(params, router);
```

### File list

| File | Purpose |
|---|---|
| `algo_common.hpp` | Shared ULL types: `RollingVolBuf`, `OrderPool`, `LogRing`, `AlgoMetrics`, TSC calibration |
| `vwap_algo.cpp` | VWAP algo + simulation harness |
| `twap_algo.cpp` | TWAP algo + simulation harness |
| `participate_algo.cpp` | POV/Participate algo + simulation harness |
| `moo_algo.cpp` | MOO/LOO algo + simulation harness |
| `moc_algo.cpp` | MOC/LOC algo + pre-close POV + simulation harness |

