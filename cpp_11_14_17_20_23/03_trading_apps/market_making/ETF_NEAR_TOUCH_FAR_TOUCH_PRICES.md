# ETF Near Touch / Far Touch Prices — Complete Reference

## What Are Near Touch and Far Touch?

In market making, **two sides of a two-sided quote** are named relative to the theoretical value (TV) or the market mid:

| Term | Definition |
|------|-----------|
| **Near Touch Bid** | Highest price a MM will pay — closest to TV from below |
| **Near Touch Ask** | Lowest price a MM will sell — closest to TV from above |
| **Far Touch Bid** | Passive bid posted deeper in the book (wider than near touch) |
| **Far Touch Ask** | Passive ask posted deeper in the book (wider than near touch) |

```
Price axis (ascending)

Far Touch Bid    ←────────────────────────────────────
Near Touch Bid       ←──────────────────────────────
                              TV / iNAV
Near Touch Ask                         ──────────────→
Far Touch Ask                                   ──────────────→
```

---

## Why ETF Market Makers Quote Two Levels

1. **Near touch** captures the most aggressive fills — earns the spread but faces highest adverse selection
2. **Far touch** provides depth and passive inventory management — fills only when price moves past near touch
3. Two-level quoting signals liquidity to the market, improving ETF fund rating
4. Many exchange liquidity programs pay rebates for maintaining both near and far touch

---

## ETF iNAV (Indicative NAV) — The Theoretical Anchor

The iNAV is the real-time estimated fair value of the ETF based on its underlying basket:

### Full iNAV Formula

```
iNAV = (Σ [weight_i × spot_price_i × FX_rate_i] + cash_component - liabilities) / shares_outstanding
```

Where:
- `weight_i` = number of units of constituent i per creation unit
- `spot_price_i` = last trade / mid price of constituent i
- `FX_rate_i` = USD equivalent conversion (if multi-currency basket)
- `cash_component` = cash / dividends accrued in the fund
- `liabilities` = fees accrued since last NAV (management fee daily accrual)
- `shares_outstanding` = total ETF shares in issue

### Practical iNAV

For a simple domestic equity ETF (e.g., SPY tracking S&P500):

```
iNAV = (Σ weight_i × price_i + accrued_dividends - accrued_fees) / units_per_share
```

For international ETFs:
```
iNAV = (Σ weight_i × price_i_local × FX_USD_local) / units_per_share
```

Note: International ETF iNAVs are "stale" during foreign market close hours;
market makers use futures or proxy hedges instead.

---

## Near Touch Pricing (ETF Market Maker)

### Base Formula

```
TV = iNAV (real-time basket value)

half_spread = max(
    base_spread_bps,
    daily_vol × vol_multiplier × 10000 bps,
    creation_redemption_cost_bps / 2
)

Near Touch Bid = floor_tick(TV × (1 - (half_spread + inv_skew) × 1e-4))
Near Touch Ask = ceil_tick (TV × (1 + (half_spread - inv_skew) × 1e-4))
```

### Parameter Descriptions

| Parameter | Typical Range | Description |
|-----------|--------------|-------------|
| `base_spread_bps` | 1–15 bps | Minimum competitive spread |
| `daily_vol` | 0.1%–2% | ETF daily volatility (annualized / √252) |
| `vol_multiplier` | 0.3–1.0 | Scale daily vol into spread contribution |
| `creation_redemption_cost_bps` | 1–100 bps | Cost to create/redeem the ETF (market impact + fees) |
| `inv_skew` | ±0–10 bps | Inventory bias: long → lower bid/ask, short → higher |
| `tick_size` | 0.001–0.01 | Exchange minimum price increment |

---

## Far Touch Pricing

```
far_touch_spread_multiple = 1.5 – 3.0  (vs near touch half spread)

Far Touch Bid = floor_tick(TV × (1 - far_spread_bps × 1e-4 + inv_skew_far × 1e-4))
Far Touch Ask = ceil_tick (TV × (1 + far_spread_bps × 1e-4 - inv_skew_far × 1e-4))
```

Where:
```
far_spread_bps = near_spread_bps × far_touch_spread_multiple
inv_skew_far   = inv_skew × far_skew_amplifier  (typically 1.5–2×)
```

### Why Far Touch Has Larger Skew

The far touch acts as an **inventory drain** mechanism:
- If long inventory → far touch ask pulled closer to TV, far bid pushed deeper
- If short inventory → far touch bid pulled closer to TV, far ask pushed deeper
- This ensures inventory mean-reverts while still earning spread

---

## Complete ETF MM Quote Pricing Example

### Setup
- ETF: SPY (S&P 500 ETF)
- iNAV: $482.35
- Realized annual vol: 18% → daily vol = 18% / √252 = 1.134%
- Creation/redemption cost: 8 bps
- Base spread: 2 bps
- Inventory: +50,000 shares long (max: 200,000) → inventory fraction = 0.25
- Inv skew scale: 6 bps

### Near Touch Calculation

```
daily_vol_component = 1.134% × 0.5 × 10000 = 5.67 bps
half_spread = max(2, 5.67, 8/2) = max(2, 5.67, 4) = 5.67 bps
inv_skew = 0.25 × 6 = 1.5 bps  (long → push quotes lower)

Near Touch Bid = 482.35 × (1 - (5.67 + 1.5) × 1e-4)
              = 482.35 × (1 - 7.17e-4)
              = 482.35 × 0.999283
              = 481.004 → floor to tick → $481.00

Near Touch Ask = 482.35 × (1 + (5.67 - 1.5) × 1e-4)
              = 482.35 × (1 + 4.17e-4)
              = 482.35 × 1.000417
              = 482.551 → ceil to tick  → $482.56

Spread: $482.56 - $481.00 = $1.56 = 3.24 bps (asymmetric due to skew)
```

### Far Touch Calculation

```
far_multiple = 2.5
far_spread = 5.67 × 2.5 = 14.18 bps
far_skew = 1.5 × 1.5 = 2.25 bps

Far Touch Bid = 482.35 × (1 - (14.18 + 2.25) × 1e-4)
             = 482.35 × 0.998357
             = 481.56 → $481.55

Far Touch Ask = 482.35 × (1 + (14.18 - 2.25) × 1e-4)
             = 482.35 × 1.001193
             = 482.925 → $482.93
```

### Final Quotes

```
Level    BID       ASK      Width (bps)
─────────────────────────────────────────
Near     $481.00   $482.56   3.24 bps
Far      $481.55   $482.93   2.89 bps (asymmetric skew toward ask — long inventory)
iNAV                         $482.35
```

---

## Premium / Discount to iNAV

```
Premium(%) = (ETF market price - iNAV) / iNAV × 100

> 0  → ETF trading expensive vs basket → creation units (buy basket, create ETF, sell ETF)
< 0  → ETF trading cheap vs basket  → redemption (buy ETF, redeem for basket, sell basket)
```

MM's near touch ask will **not** be posted above `iNAV + creation_cost_bps` — above that it's cheaper for arbitrageurs to create new ETF shares, suppressing price.

MM's near touch bid will **not** be posted below `iNAV - redemption_cost_bps` — below that arbitrageurs redeem ETF for basket.

This creates **natural bounds** on MM quotes:

```
Lower bound (bid floor) = iNAV - redemption_cost_bps × 1e-4 × iNAV
Upper bound (ask ceiling) = iNAV + creation_cost_bps × 1e-4 × iNAV
```

---

## Stale iNAV Handling

When underlying stocks are in auction or in non-overlapping time zones:

```
Adjust iNAV using liquid proxy prices (futures or correlated ETFs):

iNAV_adjusted = iNAV_last × (1 + proxy_return)

proxy_return = (futures_now - futures_close) / futures_close

Example: EM ETF with closed Asian markets
  iNAV_last = 45.20 (Asian close)
  ES futures up 1.2% during US session
  iNAV_adjusted = 45.20 × 1.012 = 45.74
```

---

## Quote Refresh Triggers

| Trigger | Action |
|---------|--------|
| iNAV moves > threshold | Reprice both near and far touch |
| Underlying constituent price moves > threshold | Recalculate iNAV then reprice |
| Fill received | Update inventory, recompute skew |
| Inventory crosses level | Widen or narrow quotes |
| Vol spike | Widen half_spread immediately |
| Market microstructure signal | Apply alpha tilt |
| Timer (max_quote_age) | Force refresh even if no trigger |

