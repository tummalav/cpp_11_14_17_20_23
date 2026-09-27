# Market Making Complete Guide
## ETF, Single Stock, Dual Counter, Options, Index & ETF Arbitrage

---

## Table of Contents

1. [Single Stock Market Making](#1-single-stock-market-making)
2. [ETF Market Making](#2-etf-market-making)
3. [Dual Counter Market Making (HKEX)](#3-dual-counter-market-making-hkex)
4. [Options Market Making](#4-options-market-making)
5. [Index Arbitrage](#5-index-arbitrage)
6. [ETF Arbitrage](#6-etf-arbitrage)
7. [Cross-ETF Arbitrage](#7-cross-etf-arbitrage)
8. [Strategy Parameter Reference](#8-strategy-parameter-reference)

---

# 1. Single Stock Market Making

## Purpose
Continuously quote two-sided markets (bid and ask) on individual stocks to earn the bid-ask spread, while managing directional inventory risk.

## Revenue Model
```
P&L = (Spread captured × # fills) - (Inventory × price adverse move) - (Adverse selection losses)
```

## TV (Theoretical Value) Construction — 5 Steps

```
Step 1: base_tv = (best_bid + best_ask) / 2        [L1 mid as anchor]

Step 2: tv_alpha = base_tv × (1 + alpha × alpha_scale_bps × 1e-4)
                                                    [directional signal tilt]

Step 3: daily_vol = realized_vol_annual / sqrt(252)
        half_spread = max(base_spread_bps,
                          daily_vol × vol_scale × 10000)
                    + borrow_cost_bps (ask side only)
                                                    [vol-adjusted spread]

Step 4: inv_fraction = inventory / max_inventory    [clamp to ±1]
        inv_skew_bps = inv_fraction × inv_skew_scale_bps
                                                    [inventory skew — shifts BOTH quotes]

Step 5: bid = floor_tick(tv_alpha × (1 - (half_spread + inv_skew) × 1e-4))
        ask = ceil_tick (tv_alpha × (1 + (half_spread - inv_skew) × 1e-4
                                      + borrow_cost × 1e-4))
```

## All Parameters

| Parameter | Type | Typical Range | Description |
|-----------|------|--------------|-------------|
| `bid_L1` | price | market | Live best bid (NBBO or exchange) |
| `ask_L1` | price | market | Live best ask |
| `realized_vol` | float | 0.10–0.60 | Annualized realized volatility |
| `base_spread_bps` | float | 2–20 | Competitive spread floor |
| `vol_scale` | float | 0.3–1.0 | Daily vol → bps multiplier |
| `alpha_signal` | float | -1 to +1 | Directional signal: +1=bullish, -1=bearish |
| `alpha_scale_bps` | float | 1–5 | bps shift per unit alpha |
| `inventory_shares` | int | varies | Net long(+)/short(-) inventory |
| `max_inventory` | int | varies | Soft cap: above this → stop quoting one side |
| `inv_skew_scale_bps` | float | 5–20 | Max skew bps at max inventory |
| `borrow_cost_bps` | float | 0–200 | Short borrow fee (HTB stocks only) |
| `tick_size` | float | 0.01 | Exchange price increment |
| `adv_pct_limit` | float | 0.01–0.10 | Position hard cap as % of ADV |
| `max_order_size` | int | varies | Maximum single child order |
| `quote_size` | int | varies | Volume displayed at each level |
| `cancel_replace_threshold` | float | 1–5 bps | Reprice if TV moves more than this |

## Quote Logic Decision Tree

```
if |inventory| >= max_inventory:
    → pull quotes on the side adding to inventory
    → keep quote only on the reducing side

elif spread < min_competitive_spread:
    → back off (don't undercut below floor)

elif adverse_selection_score > threshold:
    → widen spread
    → reduce quote size

elif volatility > vol_threshold:
    → widen spread
    → reduce quote size

else:
    → standard quote at computed bid/ask
```

## Worked Example

- Stock MSFT, bid/ask L1 = $415.80 / $415.85
- Realized vol = 22%, base spread = 3 bps, vol scale = 0.5
- Alpha = +0.6 (bullish signal), alpha scale = 2 bps
- Inventory = -5000 shares (short), max = 10000, skew scale = 8 bps

```
base_tv = (415.80 + 415.85) / 2 = 415.825
tv_alpha = 415.825 × (1 + 0.6 × 2 × 1e-4) = 415.825 × 1.00012 = 415.875

daily_vol = 0.22 / 15.87 = 0.01387 = 1.387%
vol_component = 1.387 × 0.5 × 10000 = 6.94 bps
half_spread = max(3, 6.94) = 6.94 bps

inv_fraction = -5000 / 10000 = -0.5 (short)
inv_skew = -0.5 × 8 = -4.0 bps  (negative → shift quotes HIGHER)

bid = 415.875 × (1 - (6.94 + (-4.0)) × 1e-4)
    = 415.875 × (1 - 2.94e-4)
    = 415.875 × 0.999706
    = 415.752 → $415.75

ask = 415.875 × (1 + (6.94 - (-4.0)) × 1e-4)
    = 415.875 × (1 + 10.94e-4)
    = 415.875 × 1.001094
    = 416.330 → $416.33

Spread = $416.33 - $415.75 = $0.58 = 13.9 bps (skewed up due to short inventory)
```

---

# 2. ETF Market Making

## Purpose
Quote two-sided markets in ETF shares, using the basket (iNAV) as theoretical anchor, and hedge fills by trading the underlying basket constituents or futures.

## Revenue Sources
1. Spread capture on ETF quotes
2. Basis compression (buy ETF cheap vs basket / sell ETF rich vs basket)
3. Liquidity program rebates from exchanges

## iNAV (Indicative NAV) Calculation

```
iNAV = (Σ weight_i × spot_i × FX_i + cash + accrued_div - accrued_fees) / units_per_share
```

For a domestic ETF (e.g., SPY):
```
iNAV = Σ (shares_i / CU_size × price_i) + cash_per_share - fee_accrual_per_share
```

Where `CU_size` = Creation Unit size (typically 50,000 ETF shares).

## ETF MM Quote Formula

```
TV = iNAV

half_spread = max(
    base_spread_bps,
    daily_vol × vol_scale × 10000,
    cr_cost_bps / 2            ← creation/redemption cost
)

near_bid = floor_tick(TV × (1 - (half_spread + inv_skew) × 1e-4))
near_ask = ceil_tick (TV × (1 + (half_spread - inv_skew) × 1e-4))

far_bid  = floor_tick(TV × (1 - far_multiple × half_spread × 1e-4 + far_skew × 1e-4))
far_ask  = ceil_tick (TV × (1 + far_multiple × half_spread × 1e-4 - far_skew × 1e-4))
```

## All Parameters

| Parameter | Typical Range | Description |
|-----------|--------------|-------------|
| `inav` | real-time | Basket fair value per ETF share |
| `base_spread_bps` | 1–15 | Competitive floor spread |
| `cr_cost_bps` | 2–100 | Creation/redemption transaction cost |
| `vol_scale` | 0.3–0.8 | Daily vol → bps multiplier |
| `inv_skew_scale_bps` | 3–10 | Max skew in bps at max inventory |
| `far_multiple` | 1.5–3.0 | Far touch = far_multiple × near half spread |
| `far_skew_amplifier` | 1.5–2.0 | Extra skew for far touch quotes |
| `inav_staleness_threshold` | 5–30s | Max age before iNAV is replaced by proxy |
| `premium_cap_bps` | 5–50 | Stop posting asks above this premium |
| `discount_cap_bps` | 5–50 | Stop posting bids below this discount |
| `hedge_ratio` | 0.8–1.0 | Fraction of fill hedged in basket |
| `hedge_instrument` | futures/basket | Hedge via basket trade or index futures |
| `max_inventory_cus` | 1–20 | Max inventory in creation unit multiples |

## Quote Bounds

```
ask_ceiling = iNAV × (1 + cr_cost_bps × 1e-4)    ← above this, creation is cheaper
bid_floor   = iNAV × (1 - cr_cost_bps × 1e-4)    ← below this, redemption is cheaper

near_ask = min(computed_ask, ask_ceiling)
near_bid = max(computed_bid, bid_floor)
```

## Hedge Logic After Fill

```
On ETF buy fill (we buy ETF, become long):
  → Sell basket constituents (or sell index futures as proxy)
  → Sell in proportion to basket weights

On ETF sell fill (we sell ETF, become short):
  → Buy basket constituents (or buy index futures)
  → Create ETF units if inventory falls below -1 CU
```

## Worked Example

See `ETF_NEAR_TOUCH_FAR_TOUCH_PRICES.md` for detailed worked example.

---

# 3. Dual Counter Market Making (HKEX)

## What is Dual Counter?

HKEX introduced Dual Counter (2022) allowing certain large-cap HK stocks and ETFs to trade in both:
- **HKD Counter** — settled in Hong Kong Dollars
- **RMB Counter** — settled in Chinese Renminbi

The same underlying security has two separate quote streams, and a Market Maker quotes both.

Example: Tencent (0700.HK in HKD, 80700.HK in RMB)

## Core Mechanism

```
HKD_TV = reference_price_HKD
RMB_TV = HKD_TV × USDHKD / USDRMB
       = HKD_TV / HKD_RMB_cross

where HKD_RMB_cross = USDRMB / USDHKD
```

## Dual Counter Quote Formula

```
For HKD counter:
  bid_HKD = floor_tick(HKD_TV × (1 - half_spread_bps × 1e-4 + inv_skew_hkd × 1e-4), hkd_tick)
  ask_HKD = ceil_tick (HKD_TV × (1 + half_spread_bps × 1e-4 - inv_skew_hkd × 1e-4), hkd_tick)

For RMB counter:
  RMB_TV  = HKD_TV / hkd_rmb_fx
  bid_RMB = floor_tick(RMB_TV × (1 - half_spread_bps × 1e-4 + inv_skew_rmb × 1e-4), rmb_tick)
  ask_RMB = ceil_tick (RMB_TV × (1 + half_spread_bps × 1e-4 - inv_skew_rmb × 1e-4), rmb_tick)
```

## FX Rate Dynamics

The key risk: HKD/RMB rate moves (even small) create arbitrage between the two counters:

```
Implied FX rate = ETF_price_HKD / ETF_price_RMB
Market FX rate  = actual USD/HKD / USD/RMB

If implied_fx > market_fx:
  → HKD counter expensive vs RMB → sell HKD, buy RMB
If implied_fx < market_fx:
  → RMB counter expensive vs HKD → sell RMB, buy HKD
```

## All Parameters

| Parameter | Description |
|-----------|-------------|
| `hkd_rmb_mid` | Current HKD/RMB cross rate |
| `hkd_spread_bps` | HKD counter half spread |
| `rmb_spread_bps` | RMB counter half spread |
| `fx_drift_bps` | Expected FX drift cost (adds to spread) |
| `fx_vol` | Intraday FX volatility (widens spread) |
| `inv_hkd` | Net inventory on HKD counter |
| `inv_rmb` | Net inventory on RMB counter |
| `net_inventory` | Combined position in shares (HKD + RMB) |
| `max_fx_mismatch_bps` | Cross-counter arbitrage alert threshold |
| `rmb_liquidity_discount` | Extra spread for RMB counter (lower liquidity) |
| `hkd_tick` | HKD minimum price increment |
| `rmb_tick` | RMB minimum price increment |
| `lot_size_hkd` | Board lot size for HKD counter |
| `lot_size_rmb` | Board lot size for RMB counter |

## Inventory Netting

A key advantage: inventories from both counters **net against each other**:

```
total_net_inventory = inv_HKD_shares + inv_RMB_shares

skew_factor = total_net_inventory / max_net_inventory

Both bid HKD and bid RMB are shifted by skew_factor simultaneously
```

## Cross-Counter Arbitrage

When a fill arrives on one counter, the MM may hedge on the other:
1. Buy RMB counter → immediately sell HKD counter (cross-counter hedge)
2. Or hedge via underlying
3. Exchange provides inter-counter transfer mechanism (settlement in T+2)

## Worked Example

```
Stock:  HSBC Holdings
HKD_TV: HKD 65.90
HKD/RMB: 0.9185

RMB_TV = 65.90 × 0.9185 = HKD→RMB = RMB 60.53

Spread = 8 bps each counter
Net inventory = +200,000 shares (long across both counters)
Max inventory = 1,000,000, skew scale = 5 bps
inv_skew = (200000/1000000) × 5 = 1.0 bps

HKD: bid = 65.90 × (1 - (8+1)×1e-4) = 65.90 × 0.9991 = 65.84 → HKD 65.84
     ask = 65.90 × (1 + (8-1)×1e-4) = 65.90 × 1.0007 = 65.95 → HKD 65.95

RMB: bid = 60.53 × (1 - (8+1)×1e-4) = 60.53 × 0.9991 = 60.48 → RMB 60.48
     ask = 60.53 × (1 + (8-1)×1e-4) = 60.53 × 1.0007 = 60.57 → RMB 60.57

Cross-check: ask_HKD / bid_RMB = 65.95 / 60.48 = 1.0905...
             vs HKD/RMB market rate = 1/0.9185 = 1.0887
             → small 18bps premium on HKD counter (within tolerance)
```

---

# 4. Options Market Making

## Purpose
Quote two-sided markets in options (calls and puts) using Black-Scholes or SABR model for TV, managing delta/gamma/vega/theta risk dynamically.

## Black-Scholes Pricing

### Formula
```
d1 = (ln(S/K) + (r - q + σ²/2) × T) / (σ × √T)
d2 = d1 - σ × √T

Call price = S × e^(-qT) × N(d1) - K × e^(-rT) × N(d2)
Put price  = K × e^(-rT) × N(-d2) - S × e^(-qT) × N(-d1)

Where:
  S = underlying spot price
  K = option strike price
  T = time to expiry (years)
  r = risk-free rate (continuous)
  q = dividend yield (continuous)
  σ = implied volatility
  N() = cumulative standard normal CDF
```

### Greeks

```
Delta  = ∂Price/∂S  = e^(-qT) × N(d1)          [call]  /  e^(-qT) × (N(d1)-1) [put]
Gamma  = ∂²Price/∂S² = e^(-qT) × n(d1) / (S × σ × √T)
Vega   = ∂Price/∂σ  = S × e^(-qT) × n(d1) × √T   (per 1% vol move = /100)
Theta  = ∂Price/∂T  = -(S×e^(-qT)×n(d1)×σ)/(2√T) - r×K×e^(-rT)×N(d2)   [call]
Rho    = ∂Price/∂r  = K×T×e^(-rT)×N(d2) / 100     [call]

where n(x) = standard normal PDF = (1/√2π) × e^(-x²/2)
```

## Options MM Quote Construction

### Volatility Spread → Price Spread

```
// Quote in vol space first, then convert to price
vol_bid = iv_mid - vol_half_spread
vol_ask = iv_mid + vol_half_spread

price_bid = BS(S, K, T, r, q, vol_bid, cp)
price_ask = BS(S, K, T, r, q, vol_ask, cp)

// Round to tick
bid = floor_tick(price_bid, tick_size)
ask = ceil_tick (price_ask, tick_size)

// Inventory skew in vol space
inv_vol_skew = inv_fraction × inv_vol_skew_bps × 0.01  (vol %)
vol_bid -= inv_vol_skew
vol_ask -= inv_vol_skew
```

### Vol Half-Spread Formula

```
vol_half_spread = max(
    min_vol_spread,
    base_vol_spread × vega_scale_factor,    ← wider for high vega
    base_vol_spread × gamma_scale_factor,   ← wider near expiry
    base_vol_spread × time_decay_factor     ← wider when theta is high
)
```

## Volatility Surface (Skew + Term Structure)

### ATM Vol Surface
```
σ_ATM(T) = σ_term_structure_lookup(T)  [interpolated from market data]
```

### Skew Adjustment (SVI or Sticky-Delta)
```
// Simple linear skew model:
σ(K, T) = σ_ATM(T) + skew × (F - K) / F

// SABR model:
σ_SABR(K, T) = computed from (α, β, ρ, ν) SABR parameters

// Sticky-delta (most common in equity options MM):
moneyness = K / F        (F = forward price)
sigma(K,T) = σ_ATM(T) + skew_coeff × (1 - moneyness) + convexity × (1 - moneyness)²
```

## All Options MM Parameters

| Parameter | Typical Range | Description |
|-----------|--------------|-------------|
| `spot` | market | Underlying spot price |
| `strike` | varies | Option strike |
| `expiry_years` | 0.001–2.0 | Time to expiry in years |
| `risk_free_rate` | 0.02–0.06 | Risk-free rate (continuous) |
| `div_yield` | 0.0–0.05 | Continuous dividend yield |
| `iv_mid` | 0.10–1.0 | Implied vol at mid (from vol surface) |
| `vol_half_spread` | 0.1–2.0% | Half-spread in vol points |
| `min_price_spread` | tick_size | Floor on price spread |
| `inv_vol_skew` | ±0.5% | Vol skew per unit inventory |
| `max_gamma_position` | varies | Max net gamma across all strikes |
| `max_vega_position` | varies | Max net vega across all strikes |
| `delta_hedge_threshold` | 0.1–0.5Δ | Hedge when net delta exceeds this |
| `hedge_instrument` | stock/future | What to use for delta hedge |
| `vol_surface_refresh_ms` | 100–500ms | How often to re-fetch vol surface |
| `skew_coeff` | -0.1 to 0.0 | Linear skew (negative for equity = downside skew) |
| `convexity_coeff` | 0–0.05 | Smile convexity |
| `term_struct_alpha` | varies | SVI/SABR surface alpha |

## Delta Hedging Logic

```
// After each option fill, compute net delta
net_delta = Σ position_i × delta_i     (across all options in book)

if |net_delta| > delta_hedge_threshold:
    hedge_qty = -round(net_delta / hedge_ratio)
    submit_hedge(hedge_instrument, hedge_qty)

// Gamma scalping: when underlying moves, rebalance delta
gamma_pnl_estimate = 0.5 × net_gamma × ΔS²
```

## Greeks Risk Limits

```
|net_delta|  ≤ max_delta_shares      ← first-order directional risk
|net_gamma|  ≤ max_gamma             ← second-order vol risk
|net_vega|   ≤ max_vega              ← sensitivity to vol moves
|net_theta|  ≤ max_theta_decay       ← max daily time decay loss
```

## Implied Vol Calculation (Newton-Raphson)

```
// Given market price P, find σ such that BS(S,K,T,r,q,σ) = P
σ_n+1 = σ_n - (BS(σ_n) - P) / Vega(σ_n)

// Repeat until |BS(σ) - P| < tolerance (typically 1e-6)
// Initial guess: σ_0 = sqrt(2π/T) × P/S  (Brenner-Subrahmanyam approximation)
```

## Worked Example (ATM Call)

```
Stock: AAPL @ $182.50
Strike: $182.50 (ATM)
Expiry: 30 days = 0.0822 years
r = 5.25%, q = 0.5%
IV mid = 28%

d1 = (ln(1) + (0.0525 - 0.005 + 0.28²/2) × 0.0822) / (0.28 × √0.0822)
   = (0 + 0.00635) / (0.28 × 0.2867)
   = 0.00635 / 0.0803
   = 0.0791

d2 = 0.0791 - 0.0803 = -0.0012

N(d1) = 0.5315, N(d2) = 0.4995

Call = 182.50 × e^(-0.005×0.0822) × 0.5315 - 182.50 × e^(-0.0525×0.0822) × 0.4995
     = 182.50 × 0.9996 × 0.5315 - 182.50 × 0.9957 × 0.4995
     = 96.99 - 90.82
     = $6.17

vol_half_spread = 1.5 vol pts
vol_bid = 26.5%, vol_ask = 29.5%

price_bid = BS(S=182.50, K=182.50, T=0.0822, r=0.0525, q=0.005, σ=0.265) = $5.83
price_ask = BS(S=182.50, K=182.50, T=0.0822, r=0.0525, q=0.005, σ=0.295) = $6.51

→ Quote: BID $5.80 / ASK $6.55 (after tick rounding)
   Spread = $0.75 = ~12% of premium
```

---

# 5. Index Arbitrage

## What is Index Arbitrage?

Exploiting the price difference between an **index future** and its **fair value** (derived from the constituent stocks' prices and cost of carry).

```
Fair Future Value (FFV) = Index_Spot × e^((r - q) × T)
                         (continuous compounding)

Or in discrete form:
FFV = (Index_Spot - PV_dividends) × (1 + r × T/365)
```

## The Basis

```
Basis = Futures_Price - Fair_Future_Value

Positive basis = futures too expensive → sell futures, buy basket
Negative basis = futures too cheap     → buy futures, sell basket
```

## Entry/Exit Conditions

```
// Long basis (buy futures, sell basket):
if (FFV - Futures_Price) > entry_threshold_bps × 1e-4 × Futures_Price:
    → BUY futures, SELL basket

// Short basis (sell futures, buy basket):
if (Futures_Price - FFV) > entry_threshold_bps × 1e-4 × Futures_Price:
    → SELL futures, BUY basket

// Exit when basis compresses or at futures expiry
if |Basis| < exit_threshold_bps × 1e-4 × Futures_Price:
    → unwind position
```

## Implied Repo Rate

```
// Back out the implied repo rate from futures vs spot:
implied_repo = ln(Futures / Spot) / T + dividend_yield

if implied_repo > market_repo:
    → futures cheap relative to cost of carry → buy futures, sell basket
if implied_repo < market_repo:
    → futures expensive                        → sell futures, buy basket
```

## All Parameters

| Parameter | Description |
|-----------|-------------|
| `spot_index` | Current spot index level (e.g., 5200 for S&P500) |
| `futures_price` | Current front-month futures price |
| `risk_free_rate` | Annualized risk-free rate |
| `dividend_yield` | Index dividend yield (annualized) |
| `days_to_expiry` | Futures days to expiry |
| `entry_threshold_bps` | Min basis in bps to enter trade |
| `exit_threshold_bps` | Basis bps to exit trade |
| `transaction_cost_bps` | One-way round-trip cost estimate |
| `max_position_size` | Max notional in futures contracts |
| `basket_hedge_ratio` | Fraction of index basket to hedge with |
| `num_constituents` | Number of stocks in index basket |
| `roll_threshold_days` | Days before expiry to roll to next contract |
| `slippage_model` | Linear/square-root impact model |

## Dividend Adjustment

```
PV_dividends = Σ Div_i × e^(-r × t_i)

FFV_adjusted = (Spot_Index - PV_dividends) × e^(r × T)

// For S&P 500, dividends are mostly quarterly → model 4 payment dates
// For TOPIX (Japan), concentrate dividends in March/September window
```

## Practical Execution

```
1. Receive index futures tick
2. Compute FFV using latest constituent prices
3. Compute basis = futures - FFV
4. If basis > threshold:
   a. Send futures sell order (1 contract = $250 × S&P500)
   b. Simultaneously send basket buy orders (500 stocks proportional)
   c. Use VWAP/MOO for basket to minimize market impact
5. Monitor delta: constituent prices move faster than futures sometimes
6. Rebalance when basis changes significantly
7. Unwind at expiry (or earlier if basis compresses)
```

## Settlement at Expiry

```
At futures expiry:
  Final Settlement Price = Special Opening Quotation (SOQ) = index at open
  All futures P&L locked in at SOQ
  Basket exits via MOO orders at same open
  Net P&L = basis × notional - transaction costs
```

## Worked Example

```
S&P 500 Index Spot = 5,200
ES Futures (front month, 31 days to expiry) = 5,228
Risk-free rate = 5.25%
Dividend yield = 1.40%

T = 31/365 = 0.0849 years

FFV = 5200 × e^((0.0525 - 0.014) × 0.0849)
    = 5200 × e^(0.003269)
    = 5200 × 1.003274
    = 5217.03

Basis = 5228 - 5217.03 = +10.97 index points = 21.1 bps

Transaction cost estimate = 3 bps round trip
Net opportunity = 21.1 - 3.0 = 18.1 bps = $1,175 per ES contract

Action: SELL 1 ES future @ 5228, BUY 500 stocks proportionally
Expected P&L at expiry: +18.1 bps × $1 = $94,120 on $50M notional
```

---

# 6. ETF Arbitrage

## What is ETF Arbitrage?

ETF arbitrage exploits mispricings between an ETF's market price and its Net Asset Value (NAV or iNAV) through the **creation/redemption mechanism**.

## Creation (ETF Trading at Premium)

```
if ETF_price > iNAV + creation_cost:

  Step 1: BUY basket (all constituents in correct proportions)
  Step 2: DELIVER basket to ETF issuer (Authorized Participant)
  Step 3: RECEIVE new ETF shares (typically T+2)
  Step 4: SELL ETF shares in market at premium

  Profit = ETF_price - iNAV - creation_cost - transaction_costs
```

## Redemption (ETF Trading at Discount)

```
if iNAV - ETF_price > redemption_cost:

  Step 1: BUY ETF shares at discount
  Step 2: DELIVER ETF shares to issuer
  Step 3: RECEIVE constituent basket
  Step 4: SELL basket constituents in market

  Profit = iNAV - ETF_price - redemption_cost - transaction_costs
```

## Soft Arb vs Hard Arb

| Type | Method | Risk | Speed |
|------|--------|------|-------|
| **Hard Arb** | Full creation/redemption | Very low (T+2 settlement risk) | Slow (minutes–hours) |
| **Soft Arb** | Trade ETF + hedge with futures/basket, no formal create/redeem | Basis risk | Fast (sub-second) |

Soft arbitrage:
```
ETF premium:
  short ETF + long futures (as proxy for basket)
  
ETF discount:
  long ETF + short futures

Unwind when premium/discount collapses
Profit = premium_change - transaction_costs
```

## All Parameters

| Parameter | Description |
|-----------|-------------|
| `etf_price` | Live ETF market price |
| `inav` | Real-time iNAV |
| `premium_bps` | (ETF - iNAV) / iNAV × 10000 |
| `creation_cost_bps` | Total cost to create 1 creation unit |
| `redemption_cost_bps` | Total cost to redeem 1 creation unit |
| `creation_unit_size` | Number of ETF shares per creation unit (e.g., 50,000) |
| `basket_impact_bps` | Estimated market impact of buying/selling basket |
| `etf_borrow_cost` | Short borrow cost for ETF (creation arb) |
| `basket_liquidity_score` | Fraction of basket that's liquid |
| `settlement_lag_days` | T+2 for equities (settlement risk window) |
| `inav_staleness_ms` | How stale the iNAV is |
| `min_premium_entry_bps` | Minimum premium to enter creation arb |
| `max_position_cus` | Max creation units outstanding at once |

## Risk Factors in ETF Arbitrage

```
1. Settlement risk: basket bought today, ETF received T+2
   → market can move against you during settlement window

2. Basket execution risk: large basket hard to execute at iNAV price
   → slippage erodes profit

3. iNAV staleness: international ETFs have stale iNAV during US hours
   → use futures proxy for iNAV

4. Short borrow availability: creation arb requires shorting ETF before receiving
   → HTB ETFs can have 50–200bps borrow cost

5. Redemption-in-kind: ETF issuer may substitute less liquid stocks
   → receive stocks that are hard to sell
```

## Premium/Discount Monitor

```
// Compute rolling premium
premium_bps = (etf_price - inav) / inav × 10000

// Compute z-score vs historical distribution
z_score = (premium_bps - premium_mean_30d) / premium_std_30d

if z_score > 2.0 and premium_bps > creation_cost_bps:
    ALERT: Hard creation opportunity
    
if z_score < -2.0 and -premium_bps > redemption_cost_bps:
    ALERT: Hard redemption opportunity

if z_score > 1.5 (soft arb threshold):
    EXECUTE: Short ETF + long basket/futures
```

## Worked Example

```
ETF: SPY
iNAV: $482.35
ETF market price: $482.72
Premium: +37 bps
Creation cost: 8 bps (transaction + impact)
Short borrow: 0.5 bps

Net opportunity = 37 - 8 - 0.5 = 28.5 bps

Creation Unit = 50,000 shares × $482.35 = $24.1M

Action:
  1. Buy S&P 500 basket ($24.1M notional, all 500 stocks)
  2. Submit to State Street (SSGA) for creation
  3. Receive 50,000 SPY shares in T+2
  4. Sell 50,000 SPY @ $482.72 (today or over T+2)

Gross P&L = 50,000 × ($482.72 - $482.35) = 50,000 × $0.37 = $18,500
Costs      = 50,000 × $482.35 × 8.5bps × 1e-4 = $20,500
Net P&L    = 18,500 - 20,500 = -$2,000 (NOT profitable at 8.5bps total cost)

→ Minimum profitable premium = creation_cost + borrow + slippage ≈ 10–15 bps for SPY
```

---

# 7. Cross-ETF Arbitrage

## Concept

Multiple ETFs track the same or similar index (SPY, IVV, VOO all track S&P 500). Temporary mispricings between them create arbitrage opportunities.

```
If SPY premium > IVV premium by threshold:
  → Sell SPY, Buy IVV (same underlying, different price)
  → Convergence trade: no fundamental risk, only execution risk

Profit = (SPY_premium - IVV_premium) - round_trip_costs
```

## Three-ETF Arbitrage

```
Instruments: SPY, IVV, VOO (all S&P 500 ETFs)

Model: all three should trade at approximately same premium to iNAV

Signal: z-score of premium spread across all three

If spread_SPY_IVV > 5 bps + transactions costs:
  → Long IVV (cheap), Short SPY (expensive)
  → Expect spread to converge within minutes
```

## Parameters

| Parameter | Description |
|-----------|-------------|
| `etf_a_price` / `etf_b_price` | Live prices of two ETFs |
| `inav_a` / `inav_b` | iNAV for each ETF |
| `premium_a_bps` / `premium_b_bps` | Individual premiums |
| `spread_bps` | premium_a - premium_b |
| `spread_mean` | Rolling mean of spread |
| `spread_std` | Rolling std of spread |
| `z_score` | (spread - mean) / std |
| `entry_z` | Z-score to enter (typically 2.0) |
| `exit_z` | Z-score to exit (typically 0.5) |
| `max_notional` | Position size cap |
| `hedge_ratio` | Relative size to equalize notional |
| `max_holding_minutes` | Stop-loss on duration |

## Worked Example

```
SPY iNAV: $482.35, price: $482.72 → premium = +7.7 bps
IVV iNAV: $482.41, price: $482.61 → premium = +4.1 bps

Spread = 7.7 - 4.1 = 3.6 bps (SPY expensive vs IVV)
30-day mean = 1.5 bps, std = 1.2 bps
Z-score = (3.6 - 1.5) / 1.2 = 1.75

If threshold = 2.0 → no trade yet
If threshold = 1.5 → ENTER: sell SPY, buy IVV

Position: 10,000 SPY short, 10,000 IVV long (notional-matched)
Target exit: spread returns to ~1.5 bps (mean)
Expected P&L: (3.6 - 1.5) bps × $482 × 10,000 = ~$10,122
Transaction costs: ~2 bps round trip = ~$9,600 net
```

---

# 8. Strategy Parameter Reference

## Universal Parameters (All Strategies)

| Parameter | Description |
|-----------|-------------|
| `symbol_id` | Interned integer ID (no strings on hot path) |
| `tick_size` | Exchange minimum price increment |
| `lot_size` | Minimum tradeable quantity |
| `max_order_size` | Maximum single order volume |
| `max_notional` | Maximum position value allowed |
| `cancel_replace_ms` | Max time before forced refresh |
| `quote_age_ms` | Max age of outstanding quote |
| `risk_kill` | Hard kill switch (disables all quoting) |

## Risk Parameter Hierarchy

```
Exchange Level:  position_limit, msg_rate_limit
Firm Level:      firm_max_notional, firm_max_loss
Strategy Level:  strat_max_inventory, strat_max_loss
Book Level:      book_pnl_limit, book_position_limit
Order Level:     fat_finger_check, max_order_size
```

## Spread Sizing Rule of Thumb

```
Equities (liquid large-cap):    spread = 2–5 bps
Equities (mid-cap):             spread = 5–15 bps
Equities (small/micro-cap):     spread = 15–50 bps
ETFs (liquid, domestic):        spread = 1–5 bps
ETFs (international):           spread = 5–20 bps
ETFs (niche/sector):            spread = 10–50 bps
Index options (ATM):            vol spread = 0.5–1.5 vol pts
Single stock options (ATM):     vol spread = 1.0–3.0 vol pts
Index futures (ES/NQ):          0.25 ticks (1 tick wide)
```

## Inventory Skew Sizing Rule of Thumb

```
inv_fraction = net_inventory / max_inventory  [clamp -1 to +1]
inv_skew_bps = inv_fraction × inv_skew_scale_bps

inv_skew_scale_bps:
  Liquid equities/ETFs:   3–8 bps
  Mid-cap equities:       5–15 bps
  Options (vol units):    0.5–2.0 vol pts
  FX:                     0.5–3.0 bps
```

## Alpha Signal Integration

```
// Alpha signal: +1 = strongly bullish, -1 = strongly bearish
// Applied as TV tilt (both quotes shift in signal direction)

tv_tilted = tv × (1 + alpha × alpha_scale_bps × 1e-4)

// With alpha:
//   bullish (+1): quotes shift up → better fill probability on bid side
//   bearish (-1): quotes shift down → better fill probability on ask side

// Alpha decay: signal decays exponentially
alpha_now = alpha_0 × exp(-decay_rate × time_elapsed_seconds)
```

---

## Files in This Directory

| File | Content |
|------|---------|
| `comprehensive_mm_strategies_pricing.cpp` | Single Stock, SSF, Options, ETF, Index, FX, Dual Counter, Warrants (1950 lines) |
| `ull_etf_index_strategies.cpp` | ULL ETF MM, ETF Arb, Index Arb, Basket (1516 lines) |
| `ull_pair_dualcounter_strategies.cpp` | Dual Counter MM, Pair Trading (1247 lines) |
| `ull_missing_strategies.cpp` | Cross-ETF Arb, Index Options MM, Vol Surface Arb, StatArb (1380 lines) |
| `ETF_NEAR_TOUCH_FAR_TOUCH_PRICES.md` | ETF near/far touch pricing detail |
| `MARKET_MAKING_COMPLETE_GUIDE.md` | This file — full strategy reference |
| `MARKET_MAKING_BACKTEST_README.md` | Backtesting framework documentation |

## Build Commands

```bash
# Comprehensive strategies (single stock, ETF, options, FX, dual counter):
g++ -std=c++20 -O3 -march=native -DNDEBUG \
    comprehensive_mm_strategies_pricing.cpp -o mm_strategies -lm

# ULL ETF + Index arbitrage:
g++ -std=c++20 -O3 -march=native -DNDEBUG \
    ull_etf_index_strategies.cpp \
    -lpthread -lm -o ull_etf_strategies

# Dual counter + pair strategies:
g++ -std=c++20 -O3 -march=native -DNDEBUG \
    ull_pair_dualcounter_strategies.cpp \
    -lpthread -lm -o ull_pair_strategies

# Missing/advanced strategies (Cross-ETF arb, vol surface, stat-arb):
g++ -std=c++20 -O3 -march=native -DNDEBUG \
    ull_missing_strategies.cpp \
    -lpthread -lm -o ull_missing_strategies
```

---

*Last updated: May 2026*

