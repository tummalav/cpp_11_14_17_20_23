/**
 * =============================================================================
 * ETF MARKET MAKING, ETF ARBITRAGE, INDEX ARBITRAGE, CUSTOM BASKETS,
 * LONG/SHORT -- DEEP STRATEGY PARAMETERS & TARGET PRICE CALCULATIONS
 * =============================================================================
 * BUILD:
 *   g++ -std=c++20 -O3 -march=native -DNDEBUG \
 *       etf_strategies_pricing_deep.cpp -o etf_strategies -lm
 * =============================================================================
 */

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

static constexpr double BPS = 1e-4;

void section(const char* t) {
    std::cout << "\n" << std::string(70,'=') << "\n  " << t << "\n"
              << std::string(70,'=') << "\n";
}
void sub(const char* t) {
    int pad = std::max(0, (int)(64 - (int)strlen(t)));
    std::cout << "\n--- " << t << " " << std::string(pad,'-') << "\n";
}

// =============================================================================
// DATA TYPES
// =============================================================================
struct Constituent {
    std::string symbol;
    double weight;       // index weight (sum=1.0)
    double bid, ask;     // live L1
    double last;         // last trade
    double beta;         // beta to benchmark
    double fx;           // FX to ETF base currency
    double shares_in_cu; // shares per Creation Unit
    double adv;          // avg daily volume
};

// =============================================================================
// SECTION 1: FAIR FUTURE PRICE -- ALL FORMULA VARIANTS
// =============================================================================
/*
 *  CONTINUOUS COMPOUNDING (standard):
 *    F_fair = S * exp((r - d) * T)
 *
 *  DISCRETE COMPOUNDING:
 *    F_fair = S * (1 + r - d)^T
 *
 *  WITH KNOWN DISCRETE DIVIDENDS (most accurate):
 *    PV_Div = SUM[ Div_i * exp(-r * t_i) ]
 *    F_fair = (S - PV_Div) * exp(r * T)
 *
 *  FAIR VALUE BASIS (FVB):
 *    FVB = F_fair - S  ~= S * (r - d) * T
 *    FVB > 0: futures at PREMIUM  (r > d, typical US/EU)
 *    FVB < 0: futures at DISCOUNT (d > r, high-div APAC/UK)
 *
 *  MARKET BASIS:
 *    Basis = F_market_mid - F_fair
 *    Basis > 0: future RICH  --> sell future, buy cash (Cash and Carry)
 *    Basis < 0: future CHEAP --> buy future, sell cash (Reverse C&C)
 *
 *  IMPLIED REPO RATE:
 *    repo = ln(F_market / S) / T + d
 *    If repo > actual: future is rich
 *    If repo < actual: future is cheap
 *
 *  CALENDAR SPREAD FAIR VALUE:
 *    CSV = F_fair(T2) - F_fair(T1)
 *        = S * exp((r-d)*T2) - S * exp((r-d)*T1)
 */
struct FairFuture {
    static double continuous(double S, double r, double d, double T)
        { return S * std::exp((r - d) * T); }

    static double discrete(double S, double r, double d, double T)
        { return S * std::pow(1.0 + r - d, T); }

    static double with_known_divs(double S, double r, double T,
                                  const std::vector<std::pair<double,double>>& divs) {
        double pv = 0;
        for (auto& [t,d] : divs) pv += d * std::exp(-r * t);
        return (S - pv) * std::exp(r * T);
    }
    static double fvb(double S, double r, double d, double T)
        { return continuous(S,r,d,T) - S; }

    static double market_basis(double F_mid, double F_fair)
        { return F_mid - F_fair; }

    static double basis_bps(double basis, double S)
        { return basis / S * 10000.0; }

    static double implied_repo(double F_mkt, double S, double d, double T)
        { return std::log(F_mkt / S) / T + d; }

    static double calendar_spread_fair(double S, double r, double d, double T1, double T2)
        { return continuous(S,r,d,T2) - continuous(S,r,d,T1); }
};

// =============================================================================
// SECTION 2: iNAV CALCULATION -- ALL VARIANTS
// =============================================================================
/*
 *  DEFINITION: real-time per-share estimate of ETF fair value
 *
 *  BOTTOM-UP (all constituents):
 *    iNAV = [ SUM(shares_i * price_i * fx_i) + cash ] / ETF_shares_outstanding
 *
 *  TOP-DOWN (index proxy -- preferred for 500+ stock ETFs):
 *    iNAV = Index_Level * ETF_multiplier * FX - accrued_fee_per_share
 *    Avoids recalculating 500 stocks on every tick (~100us vs <5ns)
 *
 *  DELTA-BASED FAST UPDATE (for large ETFs):
 *    iNAV_new = iNAV_prev + weight_i * delta_price_i * fx_i
 *    One multiply + add = <5ns per L1 tick
 *    Periodic full recalc every N seconds to prevent drift
 *
 *  PRICE CHOICES FOR CONSTITUENTS:
 *    Mid price        = (bid+ask)/2          -- cleanest, most common
 *    Near-touch bid   = best bid              -- conservative for ETF sell quotes
 *    Near-touch ask   = best ask              -- conservative for ETF buy quotes
 *    Far-touch        = second-level bid/ask  -- more conservative
 *    Last trade                               -- stale risk during gaps/halts
 *    VWAP                                     -- for illiquid constituents
 *
 *  NEAR-TOUCH iNAV (for MM sell quotes -- conservative):
 *    iNAV_NT_sell = SUM(shares_i * bid_i  * fx_i) / etf_shares
 *
 *  BASKET HALF-SPREAD COST (round-trip cost to trade basket):
 *    half_spread_cost = SUM(shares_i * (ask_i - bid_i)/2 * fx_i) / etf_shares
 *    spread_cost_bps  = half_spread_cost / iNAV * 10000
 */
struct iNAV_Calc {
    static double mid(const std::vector<Constituent>& cs, double cash, double shares) {
        double v = cash;
        for (auto& c : cs) v += c.shares_in_cu * (c.bid+c.ask)*0.5 * c.fx;
        return v / shares;
    }
    static double near_touch_sell(const std::vector<Constituent>& cs,
                                   double cash, double shares) {
        double v = cash;
        for (auto& c : cs) v += c.shares_in_cu * c.bid * c.fx;
        return v / shares;
    }
    static double near_touch_buy(const std::vector<Constituent>& cs,
                                  double cash, double shares) {
        double v = cash;
        for (auto& c : cs) v += c.shares_in_cu * c.ask * c.fx;
        return v / shares;
    }
    static double from_index(double idx_level, double etf_mult, double fx, double accrued_fee)
        { return idx_level * etf_mult * fx - accrued_fee; }

    static double half_spread_bps(const std::vector<Constituent>& cs,
                                   double shares, double nav) {
        double v = 0;
        for (auto& c : cs) v += c.shares_in_cu * (c.ask-c.bid)*0.5 * c.fx;
        return (v / shares) / nav * 10000.0;
    }
    static double delta_update(double prev_inav, double weight_i,
                                double delta_price_i, double fx_i)
        { return prev_inav + weight_i * delta_price_i * fx_i; }
};

// =============================================================================
// SECTION 3: ETF MARKET MAKING -- COMPLETE PARAMETERS & TARGET PRICE
// =============================================================================
/*
 *  STRATEGY: Post continuous two-sided bid/ask quotes. Earn spread.
 *            Hedge inventory with futures or basket.
 *
 *  STRATEGY PARAMETERS:
 *
 *  [REFERENCE PRICE PARAMS]
 *    inav              -- real-time basket NAV per ETF share  [PRIMARY]
 *    future_basis      -- market future mid - fair price (+rich, -cheap)
 *    beta_to_future    -- ETF sensitivity to hedging future (e.g. 0.10 for SPY)
 *    alpha_signal      -- directional signal [-1.0=bear, +1.0=bull]
 *    alpha_scale_bps   -- TV shift per unit alpha (e.g. 5 bps)
 *
 *  [SPREAD PARAMS]
 *    base_spread_bps   -- minimum half-spread floor (e.g. 2 bps)
 *    max_spread_bps    -- maximum spread cap (e.g. 20 bps)
 *    basket_illiq_bps  -- SUM(weight_i * constituent_spread_i) / 2
 *    vol_scale         -- spread sensitivity: daily_vol * vol_scale * 10000
 *    realized_vol      -- 30-day realized vol annualized (e.g. 0.15=15%)
 *    event_premium_bps -- extra spread near earnings / index events
 *
 *  [INVENTORY PARAMS]
 *    inventory_shares  -- current net position (+long, -short)
 *    inv_skew_scale    -- bps of quote skew per 1000 shares inventory
 *    max_inventory     -- hard position limit (stop quoting if breached)
 *
 *  [EXECUTION PARAMS]
 *    tick_size         -- minimum price increment
 *    quote_size        -- bid/ask quantity
 *    refresh_rate_us   -- quote refresh frequency
 *
 *  TARGET PRICE CALCULATION -- STEP BY STEP:
 *
 *  STEP 1: Basis-adjusted iNAV
 *    iNAV_adj = iNAV + future_basis * beta_to_future
 *
 *    WHY: If future is RICH (+basis) the hedge is expensive.
 *         Offset iNAV slightly to compensate.
 *         If future is CHEAP (-basis), hedge is cheap, raise TV.
 *
 *    Example:
 *      iNAV=520.40, future_basis=+5.65, beta=0.10
 *      iNAV_adj = 520.40 + (5.65 * 0.10) = 520.40 + 0.565 = 520.965
 *
 *  STEP 2: Alpha-adjusted TV
 *    alpha_adj = alpha * alpha_scale_bps * BPS * iNAV_adj
 *    TV        = iNAV_adj + alpha_adj
 *
 *    WHY: Bullish model output -> shift TV up -> attract sellers at ask
 *         Bearish model output -> shift TV down -> attract buyers at bid
 *
 *    Example:
 *      alpha=+0.20, scale=5bps
 *      alpha_adj = 0.20 * 5 * 0.0001 * 520.965 = +0.0521
 *      TV = 520.965 + 0.052 = 521.017
 *
 *  STEP 3: Dynamic spread
 *    daily_vol   = realized_vol / sqrt(252)
 *    vol_bps     = daily_vol * vol_scale * 10000
 *    inv_bps     = (|inventory| / 1000) * inv_skew_scale
 *    half_spread = min(max_spread/2,
 *                      base + basket_illiq + vol_bps + inv_bps + event)
 *
 *    Components:
 *      base:         regulatory/competitive minimum
 *      basket_illiq: cost to delta-hedge 1 ETF unit with basket
 *      vol_bps:      compensation for wider intraday range
 *      inv_bps:      wider spread when large inventory (more risk)
 *      event:        uncertainty during news/dividends/index events
 *
 *  STEP 4: Inventory skew
 *    skew_bps = (inventory / 1000) * inv_skew_scale
 *
 *    BOTH bid and ask shift together by skew (not spread widening):
 *    Long  (+inventory): skew > 0, both DOWN -> attract sellers -> reduce long
 *    Short (-inventory): skew < 0, both UP   -> attract buyers  -> reduce short
 *
 *  STEP 5: Final quotes
 *    Bid = TV * (1 - (half_spread + skew_bps) * BPS)
 *    Ask = TV * (1 + (half_spread - skew_bps) * BPS)
 *    Round bid DOWN, ask UP to nearest tick
 *
 *  SPREAD RANGES BY MARKET CONDITION:
 *    Normal market:          2-5 bps
 *    High volatility:        5-10 bps
 *    Index rebalance event:  10-20 bps
 *    Large inventory:        base + inv_scale * |inventory|/1000
 *    Illiquid ETF basket:    basket_illiq * 2 + base
 *
 *  NEAR-TOUCH vs FAR-TOUCH:
 *    Near Touch Bid = best bid in market (highest existing bid)
 *    Near Touch Ask = best ask in market (lowest existing ask)
 *    Far  Touch Bid = second-level bid
 *    Far  Touch Ask = second-level ask
 *
 *    Aggressive (join near touch): use when low vol, inventory near zero
 *      Bid = near_touch_bid
 *      Ask = near_touch_ask
 *
 *    Passive (behind far touch): use when high vol, large inventory
 *      Bid = far_touch_bid - 1 tick
 *      Ask = far_touch_ask + 1 tick
 *
 *    TV-constrained blended:
 *      Bid = min(TV_bid, near_touch_bid)
 *      Ask = max(TV_ask, near_touch_ask)
 */
struct ETFMMParams {
    double inav              = 0.0;
    double future_basis      = 0.0;
    double beta_to_future    = 0.10;
    double alpha_signal      = 0.0;
    double alpha_scale_bps   = 5.0;
    double base_spread_bps   = 2.0;
    double max_spread_bps    = 20.0;
    double basket_illiq_bps  = 1.5;
    double vol_scale         = 2.0;
    double realized_vol      = 0.15;
    double event_premium_bps = 0.0;
    double inventory         = 0.0;
    double inv_skew_scale    = 0.5;
    double near_touch_bid    = 0.0;
    double near_touch_ask    = 0.0;
    double tick_size         = 0.01;
};

struct QuoteResult {
    double tv, bid, ask, spread_bps, skew_bps, half_spread;
    double inav_adj, alpha_adj, basis_adj, vol_bps, inv_bps;
};

QuoteResult etf_mm_quote(const ETFMMParams& p) {
    QuoteResult q = {};
    q.basis_adj  = p.future_basis * p.beta_to_future;
    q.inav_adj   = p.inav + q.basis_adj;
    q.alpha_adj  = p.alpha_signal * p.alpha_scale_bps * BPS * q.inav_adj;
    q.tv         = q.inav_adj + q.alpha_adj;
    double dv    = p.realized_vol / std::sqrt(252.0);
    q.vol_bps    = dv * p.vol_scale * 10000.0;
    q.inv_bps    = (std::abs(p.inventory) / 1000.0) * p.inv_skew_scale;
    q.half_spread= std::min(p.max_spread_bps / 2.0,
                            p.base_spread_bps + p.basket_illiq_bps
                            + q.vol_bps + q.inv_bps + p.event_premium_bps);
    q.skew_bps   = (p.inventory / 1000.0) * p.inv_skew_scale;
    q.bid        = q.tv * (1.0 - (q.half_spread + q.skew_bps) * BPS);
    q.ask        = q.tv * (1.0 + (q.half_spread - q.skew_bps) * BPS);
    q.bid        = std::floor(q.bid / p.tick_size) * p.tick_size;
    q.ask        = std::ceil (q.ask / p.tick_size) * p.tick_size;
    q.spread_bps = (q.ask - q.bid) / q.tv * 10000.0;
    return q;
}

// =============================================================================
// SECTION 4: ETF ARBITRAGE -- PARAMETERS & P&L
// =============================================================================
/*
 *  STRATEGY: Exploit mispricing between ETF market price and iNAV.
 *
 *  PARAMETERS:
 *    etf_bid / etf_ask     -- live ETF L1
 *    inav                  -- real-time basket NAV per share
 *    creation_cost_bps     -- AP creation fee per CU (e.g. 3 bps)
 *    redemption_cost_bps   -- AP redemption fee per CU (e.g. 3 bps)
 *    basket_spread_bps     -- round-trip cost to execute basket
 *    market_impact_bps     -- expected market impact executing basket
 *    etf_spread_bps        -- ETF execution cost
 *    funding_bps           -- overnight repo/financing
 *    tax_bps               -- stamp duty (0 US, 50bps UK)
 *    min_profit_bps        -- minimum net P&L threshold
 *    cu_size               -- Creation Unit size (shares, e.g. 50,000)
 *
 *  TARGET PRICE CALCULATION:
 *
 *  PREMIUM/DISCOUNT (entry signal):
 *    premium_bps = (ETF_mid - iNAV) / iNAV * 10000
 *    premium > 0: ETF RICH  --> creation arb (sell ETF, buy basket)
 *    premium < 0: ETF CHEAP --> redemption arb (buy ETF, sell basket)
 *
 *  CREATION ARB (ETF RICH):
 *    Gross_bps = (ETF_ask - iNAV) / iNAV * 10000
 *    Net_bps   = Gross - creation_cost - basket_spread/2
 *                      - etf_spread/2 - impact - funding - tax
 *    Trigger: Net_bps > min_profit_bps
 *    Mechanics:
 *      1. Buy basket (proportional to ETF index) at constituent asks
 *      2. Deliver basket to ETF issuer --> receive ETF shares (creation)
 *      3. Sell ETF shares in market @ ETF_ask
 *      4. Locked P&L = ETF_ask - basket_cost
 *
 *  REDEMPTION ARB (ETF CHEAP):
 *    Gross_bps = (iNAV - ETF_bid) / iNAV * 10000
 *    Net_bps   = Gross - redemption_cost - basket_spread/2
 *                      - etf_spread/2 - impact - funding - tax
 *    Mechanics:
 *      1. Buy ETF shares in market @ ETF_bid
 *      2. Deliver ETF to issuer --> receive basket (redemption)
 *      3. Sell basket stocks in market
 *      4. Locked P&L = basket_proceeds - ETF_bid
 *
 *  SECONDARY MARKET ARB (no AP needed):
 *    Buy cheap ETF_A + Sell rich ETF_B (same index, expect convergence)
 *    Cost: execution only (no creation/redemption fee)
 *    P&L: (ETF_B_ask - ETF_A_bid) / mid * 10000 - exec_costs
 *
 *  ARBITRAGE BAND (no-trade zone):
 *    Lower = iNAV - total_cost * BPS * iNAV  [below = redemption arb]
 *    Upper = iNAV + total_cost * BPS * iNAV  [above = creation arb]
 *    Typical band: 7-15 bps for S&P 500 ETF
 */
struct ETFArbParams {
    double etf_bid, etf_ask;
    double inav;
    double creation_cost_bps   = 3.0;
    double redemption_cost_bps = 3.0;
    double basket_spread_bps   = 2.0;
    double market_impact_bps   = 1.0;
    double etf_spread_bps      = 1.0;
    double funding_bps         = 0.5;
    double tax_bps             = 0.0;
    double min_profit_bps      = 1.0;
    double cu_size             = 50000;
};

struct ArbResult {
    double etf_mid, premium_bps;
    bool   creation_viable, redemption_viable;
    double creation_gross_bps, creation_net_bps;
    double redemption_gross_bps, redemption_net_bps;
    double pnl_usd;
    std::string action;
};

ArbResult etf_arb_evaluate(const ETFArbParams& p) {
    ArbResult r = {};
    r.etf_mid     = (p.etf_bid + p.etf_ask) * 0.5;
    r.premium_bps = (r.etf_mid - p.inav) / p.inav * 10000.0;
    double ec     = p.basket_spread_bps/2 + p.etf_spread_bps/2
                  + p.market_impact_bps + p.funding_bps + p.tax_bps;
    r.creation_gross_bps   = (p.etf_ask - p.inav) / p.inav * 10000.0;
    r.creation_net_bps     = r.creation_gross_bps - p.creation_cost_bps - ec;
    r.creation_viable      = r.creation_net_bps > p.min_profit_bps;
    r.redemption_gross_bps = (p.inav - p.etf_bid) / p.inav * 10000.0;
    r.redemption_net_bps   = r.redemption_gross_bps - p.redemption_cost_bps - ec;
    r.redemption_viable    = r.redemption_net_bps > p.min_profit_bps;
    if (r.creation_viable) {
        r.pnl_usd = r.creation_net_bps * BPS * p.inav * p.cu_size;
        r.action  = "CREATION: SELL ETF@ask + BUY basket";
    } else if (r.redemption_viable) {
        r.pnl_usd = r.redemption_net_bps * BPS * p.inav * p.cu_size;
        r.action  = "REDEMPTION: BUY ETF@bid + SELL basket";
    } else {
        r.pnl_usd = 0;
        r.action  = "NO ARB: within transaction cost band";
    }
    return r;
}

// =============================================================================
// SECTION 5: INDEX ARBITRAGE -- PARAMETERS & TARGET PRICE
// =============================================================================
/*
 *  STRATEGY: Exploit mispricing between index futures and cash basket.
 *            P&L locked in at entry, realized at futures expiry.
 *
 *  PARAMETERS:
 *    spot              -- index cash level (e.g. S&P 5220)
 *    r                 -- risk-free rate (SOFR), annualized
 *    d                 -- dividend yield, annualized
 *    T                 -- years to expiry = DTE/365
 *    future_bid/ask    -- live future L1
 *    multiplier        -- contract $ per point (ES=$50, NQ=$20)
 *    commission_bps    -- per leg execution commission
 *    basket_spread_bps -- round-trip to trade index basket
 *    impact_bps        -- market impact (larger index = more impact)
 *    financing_bps     -- repo cost for short basket
 *    dividend_risk_bps -- uncertainty in dividend forecast (+/-2 bps)
 *    arb_threshold_bps -- minimum net P&L to trigger (typically 5-15 bps)
 *
 *  TARGET PRICE CALCULATION:
 *
 *  STEP 1: Fair Future Price
 *    F_fair = S * exp((r - d) * T)
 *    Example: S=5220, r=5.25%, d=1.30%, T=87/365=0.2384
 *    F_fair = 5220 * exp((0.0525-0.0130)*0.2384) = 5269.40
 *
 *  STEP 2: Fair Value Basis
 *    FVB = F_fair - S = 5269.40 - 5220 = 49.40 pts
 *    FVB tells us: futures SHOULD be 49.40 pts above spot
 *
 *  STEP 3: Market Basis
 *    Basis     = F_market_mid - F_fair
 *    Basis_bps = Basis / S * 10000
 *    Basis > 0: future RICH  --> Cash and Carry opportunity
 *    Basis < 0: future CHEAP --> Reverse C&C opportunity
 *
 *  STEP 4: Transaction cost
 *    Total_cost = basket_spread + commission + impact + financing + div_risk
 *    Typical: 3.0 + 1.0 + 2.0 + 1.0 + 1.5 = 8.5 bps
 *
 *  CASH AND CARRY (future RICH):
 *    Entry:    SELL future @ future_bid  +  BUY basket @ ask prices
 *    P&L/bps = (future_bid - F_fair) / S * 10000 - total_cost
 *    P&L/$   = P&L_bps * BPS * S * multiplier * num_contracts
 *    Realize at expiry: futures converge to spot
 *
 *  REVERSE CASH AND CARRY (future CHEAP):
 *    Entry:    BUY future @ future_ask  +  SELL basket @ bid prices
 *    P&L/bps = (F_fair - future_ask) / S * 10000 - total_cost
 *
 *  HEDGE RATIO:
 *    basket_notional = F_price * multiplier * num_contracts
 *    num_contracts = basket_value / (F_price * multiplier)
 *
 *  ROLL COST:
 *    Roll_fair = F_fair(T2) - F_fair(T1)  (calendar spread fair value)
 *    Roll_bps  = (market_calendar_spread - Roll_fair) / S * 10000
 *    +ve roll cost: expensive to roll (pay up)
 *    -ve roll cost: cheap to roll (receive premium)
 *
 *  DIVIDEND RISK:
 *    If actual dividends > forecast: F_fair understated, arb less profitable
 *    Hedge with dividend futures or dividend swap for large positions
 */
struct IndexArbParams {
    double spot;
    double future_bid, future_ask;
    double r, d, T;
    double multiplier        = 50.0;
    int    num_contracts     = 1;
    double commission_bps    = 1.0;
    double basket_spread_bps = 3.0;
    double impact_bps        = 2.0;
    double financing_bps     = 1.0;
    double dividend_risk_bps = 1.5;
    double arb_threshold_bps = 8.0;
};

struct IndexArbResult {
    double fair, fvb, market_mid, basis, basis_bps;
    double total_cost;
    bool   cc_viable, rcc_viable;
    double cc_net_bps, rcc_net_bps;
    double pnl_per_contract;
    std::string action;
};

IndexArbResult index_arb_evaluate(const IndexArbParams& p) {
    IndexArbResult r = {};
    r.fair        = FairFuture::continuous(p.spot, p.r, p.d, p.T);
    r.fvb         = r.fair - p.spot;
    r.market_mid  = (p.future_bid + p.future_ask) * 0.5;
    r.basis       = r.market_mid - r.fair;
    r.basis_bps   = r.basis / p.spot * 10000.0;
    r.total_cost  = p.basket_spread_bps + p.commission_bps
                  + p.impact_bps + p.financing_bps + p.dividend_risk_bps;
    double cc_g   = (p.future_bid - r.fair) / p.spot * 10000.0;
    r.cc_net_bps  = cc_g - r.total_cost;
    r.cc_viable   = cc_g > r.total_cost;
    double rcc_g  = (r.fair - p.future_ask) / p.spot * 10000.0;
    r.rcc_net_bps = rcc_g - r.total_cost;
    r.rcc_viable  = rcc_g > r.total_cost;
    double net    = r.cc_viable ? r.cc_net_bps : r.rcc_net_bps;
    r.pnl_per_contract = net * BPS * p.spot * p.multiplier;
    if      (r.cc_viable)  r.action = "C&C: SELL future + BUY basket";
    else if (r.rcc_viable) r.action = "Rev C&C: BUY future + SELL basket";
    else                   r.action = "NO ARB";
    return r;
}

// =============================================================================
// SECTION 6: CUSTOM BASKET -- PARAMETERS & TARGET PRICE
// =============================================================================
/*
 *  STRATEGY: User-defined portfolio for execution, hedging, or replication.
 *
 *  PARAMETERS:
 *    constituents[]        -- stocks with weights, prices, betas, lots
 *    total_notional        -- portfolio $ size
 *    rebalance_threshold   -- rebalance when weight drift > N%
 *    tracking_error_target -- max TE annualized (e.g. 0.5%)
 *    hedge_instrument      -- future to beta-hedge basket
 *    adv_participation     -- max % of ADV for execution (e.g. 10%)
 *    lot_sizes[]           -- minimum tradeable lots
 *    fx_hedge              -- hedge FX for cross-listed names
 *
 *  TARGET PRICE CALCULATION:
 *
 *  BASKET NAV:
 *    Mid_NAV = SUM(shares_i * (bid_i+ask_i)/2 * fx_i)
 *    Bid_NAV = SUM(shares_i * bid_i * fx_i)    [conservative sell value]
 *    Ask_NAV = SUM(shares_i * ask_i * fx_i)    [conservative buy cost]
 *
 *  BASKET SPREAD COST:
 *    Spread_cost = Ask_NAV - Bid_NAV
 *    Spread_bps  = Spread_cost / Mid_NAV * 10000
 *
 *  PORTFOLIO BETA:
 *    beta = SUM(weight_i * beta_i)  where weight_i = pos_value_i / Mid_NAV
 *
 *  FUTURES HEDGE:
 *    N_futures = (Mid_NAV * portfolio_beta) / (F_price * multiplier)
 *
 *  EXECUTION TARGET PRICES:
 *    Aggressive: buy at ask, sell at bid (cross spread)
 *    Passive:    buy at bid+1tick, sell at ask-1tick
 *    VWAP:       target prev_N_day_VWAP, slice proportional to volume curve
 *    TWAP:       target arrival_mid, slice equally over N time intervals
 *    POV:        shares_per_interval = ADV * pov_rate / intervals_per_day
 *
 *  TRACKING ERROR:
 *    TE_annual = TE_daily * sqrt(252)
 *    Sources: lot rounding, missing names, FX slippage, div timing
 */
struct BasketResult {
    double mid_nav, bid_nav, ask_nav;
    double spread_bps, portfolio_beta, futures_hedge_n;
};

BasketResult basket_evaluate(const std::vector<Constituent>& cs,
                              double future_price, double multiplier) {
    BasketResult r = {};
    for (auto& c : cs) {
        r.mid_nav += c.shares_in_cu * (c.bid+c.ask)*0.5 * c.fx;
        r.bid_nav += c.shares_in_cu * c.bid * c.fx;
        r.ask_nav += c.shares_in_cu * c.ask * c.fx;
    }
    for (auto& c : cs) {
        double w = c.shares_in_cu * (c.bid+c.ask)*0.5 * c.fx / r.mid_nav;
        r.portfolio_beta += w * c.beta;
    }
    r.spread_bps      = (r.ask_nav - r.bid_nav) / r.mid_nav * 10000.0;
    r.futures_hedge_n = (r.mid_nav * r.portfolio_beta) / (future_price * multiplier);
    return r;
}

// =============================================================================
// SECTION 7: LONG / SHORT EQUITY -- PARAMETERS & TARGET PRICE
// =============================================================================
/*
 *  STRATEGY: Long undervalued stocks, short overvalued. Generate alpha.
 *            Control market/factor exposure.
 *
 *  PAIR TRADE PARAMETERS:
 *    symbol_A / symbol_B   -- two correlated/cointegrated assets
 *    hedge_ratio           -- OLS: Cov(A,B) / Var(B)  [cointegration slope]
 *    spread_mean           -- long-run equilibrium spread (rolling mean)
 *    spread_std            -- rolling std dev of spread
 *    entry_z               -- z-score to enter trade (+/-2.0 typical)
 *    exit_z                -- z-score to close (+/-0.2 typical)
 *    stop_z                -- stop-loss z-score (+/-4.0 typical)
 *    lookback_days         -- window for mean/std estimation (e.g. 60d)
 *    notional_per_leg      -- $ per leg
 *    beta_neutral          -- size for beta-neutrality
 *
 *  FACTOR L/S PARAMETERS:
 *    factor_targets[]      -- target beta per factor (mkt, size, value, mom)
 *    max_gross_pct         -- max gross exposure / NAV (e.g. 200%)
 *    max_net_pct           -- max net exposure / NAV   (e.g. 20%)
 *    max_single_name_pct   -- max weight per stock     (e.g. 5%)
 *    rebalance_freq        -- daily / weekly / on-signal
 *
 *  TARGET PRICE CALCULATION:
 *
 *  SPREAD:
 *    spread  = price_A - hedge_ratio * price_B
 *    z_score = (spread - spread_mean) / spread_std
 *
 *  SIGNAL RULES:
 *    z < -entry_z: spread CHEAP, expect to rise -> LONG A / SHORT B
 *    z > +entry_z: spread RICH,  expect to fall -> SHORT A / LONG B
 *    |z| < exit_z: mean reverted -> EXIT both legs
 *    |z| > stop_z: trend, not reversion -> EXIT (stop-loss)
 *
 *  ENTRY TARGET PRICES (urgency: 0=passive, 1=aggressive):
 *    Long  leg: target = mid + urgency * half_spread  [pay up to get filled]
 *    Short leg: target = mid - urgency * half_spread  [receive less]
 *
 *  BETA-NEUTRAL SIZING:
 *    shares_A = notional / price_A
 *    shares_B = shares_A * (beta_A / beta_B) * (price_A / price_B)
 *    Net_beta = (shares_A*price_A*beta_A - shares_B*price_B*beta_B) / notional
 *    Target: Net_beta ~= 0
 *
 *  P&L:
 *    P&L = (spread_exit - spread_entry) * shares_A
 *    Per 1-sigma mean reversion: E[P&L] = spread_std * shares_A
 *
 *  PERFORMANCE EXPECTATIONS:
 *    Hit rate:  65-75% (well-cointegrated pair)
 *    Avg win:   1.5-2.0 * spread_std * shares_A
 *    Avg loss:  0.5-1.0 * spread_std * shares_A
 *    Sharpe:    1.5-2.5 annualized (before costs)
 *
 *  PORTFOLIO METRICS:
 *    Gross_exposure = SUM(|pos_i * price_i|)
 *    Net_exposure   = SUM(pos_i * price_i)
 *    Gross_pct      = Gross / NAV * 100  [typical: 150-200%]
 *    Net_pct        = Net   / NAV * 100  [typical: -20% to +20%]
 *    Portfolio_beta = SUM(weight_i * beta_i)  [target: ~0 for mkt-neutral]
 */
struct PairParams {
    std::string sym_a, sym_b;
    double price_a, price_b;
    double beta_a, beta_b;
    double hedge_ratio;
    double spread_mean;
    double spread_std;
    double entry_z  = 2.0;
    double exit_z   = 0.2;
    double stop_z   = 4.0;
    double notional = 1e6;
    bool   beta_neutral = true;
};

struct PairResult {
    double spread, z_score;
    double shares_a, shares_b, net_beta;
    bool   enter_long_a, enter_short_a, exit_signal, stop_signal;
    double target_px_a, target_px_b;
    double expected_pnl_per_sigma;
};

PairResult pair_evaluate(const PairParams& p, double urgency = 0.5) {
    PairResult r = {};
    r.spread  = p.price_a - p.hedge_ratio * p.price_b;
    r.z_score = (r.spread - p.spread_mean) / p.spread_std;
    r.shares_a = p.notional / p.price_a;
    if (p.beta_neutral)
        r.shares_b = r.shares_a * (p.beta_a / p.beta_b) * (p.price_a / p.price_b);
    else
        r.shares_b = p.hedge_ratio * r.shares_a * (p.price_a / p.price_b);
    r.net_beta = (r.shares_a*p.price_a*p.beta_a
                - r.shares_b*p.price_b*p.beta_b) / p.notional;
    r.enter_long_a  = r.z_score < -p.entry_z;
    r.enter_short_a = r.z_score >  p.entry_z;
    r.exit_signal   = std::abs(r.z_score) < p.exit_z;
    r.stop_signal   = std::abs(r.z_score) > p.stop_z;
    double hs_a = p.price_a * 5.0 * BPS;
    double hs_b = p.price_b * 5.0 * BPS;
    if (r.enter_long_a) {
        r.target_px_a = p.price_a + urgency * hs_a;
        r.target_px_b = p.price_b - urgency * hs_b;
    } else {
        r.target_px_a = p.price_a - urgency * hs_a;
        r.target_px_b = p.price_b + urgency * hs_b;
    }
    r.expected_pnl_per_sigma = p.spread_std * r.shares_a;
    return r;
}

// =============================================================================
// DEMOS
// =============================================================================
void demo_etf_mm() {
    section("1. ETF MARKET MAKING -- TARGET QUOTE CALCULATION");

    sub("Scenario A: Normal market, zero inventory");
    ETFMMParams p;
    p.inav=520.40; p.future_basis=5.65; p.beta_to_future=0.10;
    p.alpha_signal=0.20; p.alpha_scale_bps=5.0;
    p.base_spread_bps=2.0; p.basket_illiq_bps=1.5;
    p.vol_scale=2.0; p.realized_vol=0.15;
    p.inventory=0; p.tick_size=0.01;
    auto q = etf_mm_quote(p);
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  iNAV             = " << p.inav << "\n";
    std::cout << "  Future basis     = +" << p.future_basis << " pts (RICH)\n";
    std::cout << "  Beta             = " << p.beta_to_future << "\n";
    std::cout << "  Basis adj        = " << q.basis_adj << " --> iNAV_adj = " << q.inav_adj << "\n";
    std::cout << "  Alpha adj        = " << q.alpha_adj  << " --> TV = " << q.tv << "\n";
    std::cout << std::setprecision(2);
    std::cout << "  Vol bps          = " << q.vol_bps     << " bps\n";
    std::cout << "  Half spread      = " << q.half_spread << " bps\n";
    std::cout << "  Inventory skew   = " << q.skew_bps    << " bps\n";
    std::cout << "  BID = " << std::setprecision(4) << q.bid
              << "   ASK = " << q.ask << "   Spread = "
              << std::setprecision(2) << q.spread_bps << " bps\n";

    sub("Scenario B: Long 15,000 shares (lower quotes to sell)");
    p.inventory=15000;
    q=etf_mm_quote(p);
    std::cout << "  Skew = +" << std::setprecision(2) << q.skew_bps
              << " bps (shift DOWN to attract sellers)\n";
    std::cout << "  BID = " << std::setprecision(4) << q.bid
              << "   ASK = " << q.ask << "\n";

    sub("Scenario C: Event day (3x vol + event premium)");
    p.inventory=0; p.realized_vol=0.45; p.event_premium_bps=5.0;
    q=etf_mm_quote(p);
    std::cout << "  Half spread = " << std::setprecision(2) << q.half_spread
              << " bps (wider for high vol)\n";
    std::cout << "  BID = " << std::setprecision(4) << q.bid
              << "   ASK = " << q.ask << "\n";
}

void demo_etf_arb() {
    section("2. ETF ARBITRAGE");
    sub("QQQ trading at premium");
    ETFArbParams p;
    p.etf_bid=445.80; p.etf_ask=445.90; p.inav=445.50;
    auto r=etf_arb_evaluate(p);
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  ETF mid        = " << r.etf_mid << "\n";
    std::cout << "  iNAV           = " << p.inav << "\n";
    std::cout << "  Premium        = " << r.premium_bps << " bps (RICH)\n";
    std::cout << "  Create gross   = " << r.creation_gross_bps << " bps\n";
    std::cout << "  Exec cost      = "
              << (p.basket_spread_bps/2+p.etf_spread_bps/2+p.market_impact_bps+p.funding_bps)
              << " bps\n";
    std::cout << "  Net P&L        = " << r.creation_net_bps << " bps\n";
    std::cout << "  P&L per CU     = $" << r.pnl_usd << "\n";
    std::cout << "  Viable?        = " << (r.creation_viable ? "YES" : "NO") << "\n";
    std::cout << "  Action: " << r.action << "\n";
}

void demo_index_arb() {
    section("3. INDEX ARBITRAGE");
    sub("S&P 500, 87 days to expiry");
    IndexArbParams p;
    p.spot=5220.0; p.future_bid=5226.0; p.future_ask=5226.5;
    p.r=0.0525; p.d=0.0130; p.T=87.0/365.0; p.multiplier=50.0;
    auto r=index_arb_evaluate(p);
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  Spot         = " << p.spot << "\n";
    std::cout << "  r=" << p.r*100 << "%  d=" << p.d*100 << "%  T=" << p.T << "y\n";
    std::cout << "  Fair future  = " << r.fair << "\n";
    std::cout << "  FVB          = " << r.fvb << " pts\n";
    std::cout << "  Market mid   = " << r.market_mid << "\n";
    std::cout << std::setprecision(2);
    std::cout << "  Basis        = " << r.basis << " pts (" << r.basis_bps << " bps)\n";
    std::cout << "  Total cost   = " << r.total_cost << " bps\n";
    std::cout << "  C&C net P&L  = " << r.cc_net_bps << " bps\n";
    std::cout << "  P&L/contract = $" << r.pnl_per_contract << "\n";
    std::cout << "  Action: " << r.action << "\n";
}

void demo_basket() {
    section("4. CUSTOM BASKET");
    std::vector<Constituent> basket = {
        {"AAPL", 0.30, 174.98, 175.02, 174.99, 1.10, 1.0, 0, 5e6},
        {"MSFT", 0.25, 389.95, 390.05, 390.00, 0.95, 1.0, 0, 5e6},
        {"NVDA", 0.20, 849.90, 850.10, 849.95, 1.50, 1.0, 0, 5e6},
        {"GOOGL",0.15, 169.97, 170.03, 170.00, 1.05, 1.0, 0, 5e6},
        {"META", 0.10, 489.95, 490.05, 490.00, 1.20, 1.0, 0, 5e6}
    };
    double notional = 10e6;
    for (auto& c : basket)
        c.shares_in_cu = std::floor(c.weight * notional / ((c.bid+c.ask)*0.5));
    auto r=basket_evaluate(basket, 18500.0, 20.0);
    std::cout << "\n  Stock   Wt    Mid      Spread(bps)  Beta   Shares\n";
    std::cout <<   "  ------  ----  -------  -----------  -----  ------\n";
    for (auto& c : basket) {
        double mid=(c.bid+c.ask)*0.5;
        std::cout << "  " << std::setw(6) << c.symbol
                  << std::setw(5) << std::setprecision(0) << c.weight*100 << "%"
                  << std::setw(9) << std::setprecision(2) << mid
                  << std::setw(13) << std::setprecision(1) << (c.ask-c.bid)/mid*10000
                  << std::setw(7) << c.beta
                  << std::setw(8) << std::setprecision(0) << c.shares_in_cu << "\n";
    }
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Mid NAV      = $" << r.mid_nav << "\n";
    std::cout << "  Spread cost  = "  << r.spread_bps << " bps\n";
    std::cout << "  Port beta    = "  << std::setprecision(3) << r.portfolio_beta << "\n";
    std::cout << "  Futures hedge= "  << std::setprecision(1) << r.futures_hedge_n << " contracts\n";
}

void demo_long_short() {
    section("5. LONG/SHORT PAIR TRADE");
    sub("AAPL vs MSFT");
    PairParams p;
    p.sym_a="AAPL"; p.sym_b="MSFT";
    p.price_a=175.0; p.price_b=390.0;
    p.beta_a=1.10; p.beta_b=0.95;
    p.hedge_ratio=0.443; p.spread_mean=2.183; p.spread_std=1.520;
    p.notional=1e6; p.beta_neutral=true;
    auto r=pair_evaluate(p, 0.5);
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  Spread  = " << p.price_a << " - " << p.hedge_ratio
              << " x " << p.price_b << " = " << r.spread << "\n";
    std::cout << "  Z-score = (" << r.spread << " - " << p.spread_mean
              << ") / " << p.spread_std << " = "
              << std::setprecision(2) << r.z_score << "\n";
    std::cout << "  Enter Long A?   = " << (r.enter_long_a  ? "YES (z < -2)" : "NO") << "\n";
    std::cout << "  Enter Short A?  = " << (r.enter_short_a ? "YES (z > +2)" : "NO") << "\n";
    std::cout << "  Exit signal?    = " << (r.exit_signal   ? "YES"          : "NO") << "\n";
    std::cout << "  Stop signal?    = " << (r.stop_signal   ? "YES"          : "NO") << "\n";
    std::cout << "  Shares A = " << std::setprecision(0) << r.shares_a << "\n";
    std::cout << "  Shares B = " << r.shares_b << "\n";
    std::cout << "  Net beta = " << std::setprecision(4) << r.net_beta
              << (std::abs(r.net_beta)<0.05 ? " (beta-neutral)" : " (not neutral)") << "\n";
    std::cout << "  Target A = " << r.target_px_a << "\n";
    std::cout << "  Target B = " << r.target_px_b << "\n";
    std::cout << "  E[P&L] per sigma = $" << std::setprecision(0)
              << r.expected_pnl_per_sigma << "\n";
}

void summary_table() {
    section("REFERENCE PRICES SUMMARY -- ALL STRATEGIES");
    std::cout << R"(
  +----------------------------+-----------------------------------------------+
  | Reference Price            | Used For                                      |
  +----------------------------+-----------------------------------------------+
  | iNAV (mid)                 | ETF MM: primary TV, arb trigger               |
  | iNAV (near-touch bid)      | Conservative ETF ask quote pricing            |
  | iNAV (near-touch ask)      | Conservative ETF bid quote pricing            |
  | iNAV (delta-updated)       | Fast update for 500+ stock ETFs (<5ns)        |
  | Future Fair Price          | Index arb threshold, basis calculation        |
  | Future Market Mid          | Hedge cost, basis adjustment to iNAV          |
  | Future Basis               | Adjust iNAV for hedge richness/cheapness      |
  | Near Touch Bid             | Aggressive ETF bid quote reference            |
  | Near Touch Ask             | Aggressive ETF ask quote reference            |
  | Far Touch Bid              | Passive wide bid quote reference              |
  | Far Touch Ask              | Passive wide ask quote reference              |
  | Spot Index Level           | Cost of carry, index arbitrage                |
  | Basket Mid NAV             | Custom basket valuation, P&L mark             |
  | Basket Bid NAV             | Mark long basket conservatively               |
  | Basket Ask NAV             | Mark short basket, cost to execute            |
  | VWAP                       | L/S algo execution benchmark                  |
  | Alpha Signal               | Shift TV direction (bullish/bearish)          |
  | Beta to Future             | Scale future basis adjustment                 |
  | Realized Volatility        | Dynamic spread widening                       |
  | Inventory Position         | Quote skew to manage delta                    |
  | FX Rate                    | Cross-listed ETF currency conversion          |
  | Dividend Yield             | Fair price, FVB, cost of carry                |
  | Risk-free Rate             | Cost of carry, fair price, funding            |
  | Pair Spread (A - ratio*B)  | L/S z-score entry/exit signal                 |
  | OLS Hedge Ratio            | L/S beta-neutral pair sizing                  |
  +----------------------------+-----------------------------------------------+

  FORMULA QUICK REFERENCE:
  =======================================================================
  iNAV(mid)      = SUM(shares_i * mid_i * fx_i) / etf_shares
  iNAV(index)    = Index_Level * ETF_mult * FX - accrued_fee
  F_fair         = S * exp((r - d) * T)
  FVB            = F_fair - S
  Market_Basis   = F_mid - F_fair  [+ = rich, - = cheap]

  TV (ETF MM)    = iNAV
                 + future_basis * beta
                 + alpha * alpha_scale_bps * BPS * iNAV

  half_spread    = min(max/2,
                   base + basket_illiq + daily_vol*vol_scale*10000
                   + (|inv|/1000)*inv_scale + event)

  Bid = TV * (1 - (half_spread + inv_skew) * BPS)
  Ask = TV * (1 + (half_spread - inv_skew) * BPS)

  ETF_premium    = (ETF_mid - iNAV) / iNAV * 10000  [bps]
  Create_net     = (ETF_ask - iNAV)/iNAV*10000 - create_cost - exec
  CC_net_bps     = (F_bid - F_fair) / S * 10000 - total_cost

  Basket_NAV     = SUM(shares_i * mid_i * fx_i)
  Futures_hedge  = (NAV * portfolio_beta) / (F_price * multiplier)

  Pair_spread    = price_A - hedge_ratio * price_B
  Pair_z         = (spread - spread_mean) / spread_std
  =======================================================================
)";
}

int main() {
    std::cout << std::string(70,'=') << "\n";
    std::cout << " ETF/INDEX STRATEGIES: DEEP PARAMS & TARGET PRICE CALCULATIONS\n";
    std::cout << std::string(70,'=') << "\n";
    demo_etf_mm();
    demo_etf_arb();
    demo_index_arb();
    demo_basket();
    demo_long_short();
    summary_table();
    return 0;
}

