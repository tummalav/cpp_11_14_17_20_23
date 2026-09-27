/**
 * =============================================================================
 * COMPREHENSIVE MARKET MAKING STRATEGIES & QUOTE PRICING CALCULATIONS
 * =============================================================================
 * Asset Classes:
 *   1.  Single Stock
 *   2.  Single Stock Futures (SSF)
 *   3.  Single Stock Options
 *   4.  ETFs
 *   5.  Index Futures
 *   6.  Index Options
 *   7.  FX Spot
 *   8.  FX Futures
 *   9.  FX Options (Garman-Kohlhagen)
 *  10.  Dual Counter (HKEX mechanism)
 *  11.  Warrants
 *
 * For each asset class: strategy parameters explained, pricing formulae,
 * parameter structs, worked demo with realistic numbers.
 *
 * BUILD:
 *   g++ -std=c++20 -O3 -march=native -DNDEBUG \
 *       comprehensive_mm_strategies_pricing.cpp -o mm_strategies -lm
 * =============================================================================
 */

#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

static constexpr double BPS   = 1e-4;   // 1 basis point
static constexpr double SQRT252 = 15.8745;  // sqrt(252) trading days

// ── console helpers ──────────────────────────────────────────────────────────
void section(const char* t) {
    std::cout << "\n" << std::string(72,'=') << "\n  " << t
              << "\n" << std::string(72,'=') << "\n";
}
void sub(const char* t) {
    int pad = std::max(0, 60 - (int)strlen(t));
    std::cout << "\n--- " << t << " " << std::string(pad,'-') << "\n";
}
static double round_tick(double price, double tick) {
    return std::round(price / tick) * tick;
}
static double floor_tick(double price, double tick) {
    return std::floor(price / tick) * tick;
}
static double ceil_tick(double price, double tick) {
    return std::ceil(price / tick) * tick;
}

// =============================================================================
// SECTION 0: SHARED PRICING INFRASTRUCTURE
// =============================================================================
/*
 *  norm_cdf  – cumulative normal distribution (Abramowitz & Stegun)
 *  norm_pdf  – standard normal PDF
 *  BlackScholes::greeks  – full equity option Greeks (continuous div yield q)
 *  GarmanKohlhagen::greeks – FX option pricing (r_f replaces q)
 */
static double norm_cdf(double x) {
    static const double a1=0.319381530, a2=-0.356563782, a3=1.781477937,
                        a4=-1.821255978, a5=1.330274429;
    double k = 1.0 / (1.0 + 0.2316419 * std::fabs(x));
    double poly = k*(a1+k*(a2+k*(a3+k*(a4+k*a5))));
    double pdf  = std::exp(-0.5*x*x) / std::sqrt(2*M_PI);
    double val  = 1.0 - pdf * poly;
    return x >= 0 ? val : 1.0 - val;
}
static double norm_pdf(double x) {
    return std::exp(-0.5*x*x) / std::sqrt(2.0*M_PI);
}

struct BSGreeks {
    double price{}, delta{}, gamma{}, vega{}, theta{}, rho{};
};

struct BlackScholes {
    // call_or_put = +1 for call, -1 for put
    static BSGreeks greeks(double S, double K, double T, double r, double q,
                           double sigma, int cp = 1) {
        if (T <= 0 || sigma <= 0) return {};
        double sqT = std::sqrt(T);
        double d1  = (std::log(S/K) + (r - q + 0.5*sigma*sigma)*T) / (sigma*sqT);
        double d2  = d1 - sigma*sqT;
        double Nd1 = norm_cdf(cp*d1), Nd2 = norm_cdf(cp*d2);
        double nd1 = norm_pdf(d1);
        double eq  = std::exp(-q*T), er = std::exp(-r*T);

        BSGreeks g;
        g.price = cp*(S*eq*Nd1 - K*er*Nd2);
        g.delta = cp*eq*Nd1;
        g.gamma = eq*nd1 / (S*sigma*sqT);
        g.vega  = S*eq*nd1*sqT / 100.0;   // per 1-vol-pt move
        g.theta = (-(S*eq*nd1*sigma)/(2*sqT) - cp*r*K*er*Nd2 + cp*q*S*eq*Nd1) / 365.0;
        g.rho   = cp*K*T*er*Nd2 / 100.0;
        return g;
    }
};

struct GarmanKohlhagen {
    // Identical to BS but q → r_f (foreign risk-free rate)
    static BSGreeks greeks(double S, double K, double T, double r_d, double r_f,
                           double sigma, int cp = 1) {
        return BlackScholes::greeks(S, K, T, r_d, r_f, sigma, cp);
    }
};

struct FairFuture {
    static double continuous(double S, double r, double q, double T)
        { return S * std::exp((r-q)*T); }
    static double fvb(double S, double r, double q, double T)
        { return continuous(S,r,q,T) - S; }
    static double implied_repo(double F, double S, double q, double T)
        { return std::log(F/S)/T + q; }
    static double calendar_spread(double S, double r, double q, double T1, double T2)
        { return continuous(S,r,q,T2) - continuous(S,r,q,T1); }
    // FX forward (covered interest parity)
    static double fx_forward(double spot, double r_d, double r_f, double T)
        { return spot * std::exp((r_d-r_f)*T); }
};

// =============================================================================
// SECTION 1: SINGLE STOCK MARKET MAKING
// =============================================================================
/*
 *  STRATEGY: Post two-sided quotes around theoretical value (TV) derived
 *  from L1 mid. Hedge net delta with same stock or SSF.
 *
 *  TV CONSTRUCTION (5 steps):
 *    Step 1  base_tv       = (bid_L1 + ask_L1) / 2           [L1 mid]
 *    Step 2  tv_alpha      = base_tv * (1 + alpha * alpha_scale_bps * BPS)
 *                                                              [alpha signal]
 *    Step 3  daily_vol     = realized_vol / sqrt(252)
 *            half_spread   = max(base_spread_bps, daily_vol * vol_scale * 10000)
 *            + borrow_cost (ask side only)                    [vol/borrow spread]
 *    Step 4  inv_skew_bps  = (inventory / max_inventory) * inv_skew_scale_bps
 *            → shift BOTH bid & ask DOWN (long) or UP (short)  [inventory skew]
 *    Step 5  bid = floor_tick(tv_alpha - (half_spread + inv_skew) * BPS * tv_alpha)
 *            ask = ceil_tick (tv_alpha + (half_spread - inv_skew) * BPS * tv_alpha
 *                             + borrow_cost * BPS * tv_alpha)  [round to tick]
 */
struct SingleStockMMParams {
    double bid_L1{};            // live best bid
    double ask_L1{};            // live best ask
    double realized_vol{};      // annualised daily vol (e.g. 0.20 = 20%)
    double base_spread_bps{5};  // competitive floor spread (3 bps liquid, 15 illiquid)
    double vol_scale{0.5};      // daily_vol * vol_scale * 10000 → extra bps
    double alpha_signal{0.0};   // directional signal: +1 bullish, -1 bearish, 0 neutral
    double alpha_scale_bps{2};  // bps shift per unit alpha
    double inventory_shares{};  // net long(+) / short(-) shares held
    double max_inventory{10000};// stop-quoting threshold (shares)
    double inv_skew_scale_bps{10}; // max inventory skew in bps
    double borrow_cost_bps{};   // hard-to-borrow fee (added to ask only)
    double tick_size{0.01};     // exchange min increment
    double adv_pct_limit{0.05}; // position cap = 5% ADV
    double adv{};               // average daily volume
};

struct SingleStockQuote {
    double tv{}, bid{}, ask{}, half_spread_bps{}, inv_skew_bps{};
};

static SingleStockQuote single_stock_mm_quote(const SingleStockMMParams& p) {
    SingleStockQuote r;
    double mid     = 0.5*(p.bid_L1 + p.ask_L1);
    r.tv           = mid * (1.0 + p.alpha_signal * p.alpha_scale_bps * BPS);
    double daily_v = p.realized_vol / SQRT252;
    double half    = std::max(p.base_spread_bps, daily_v * p.vol_scale * 10000.0);
    double inv_f   = (p.max_inventory > 0)
                   ? std::clamp(p.inventory_shares / p.max_inventory, -1.0, 1.0) : 0;
    r.inv_skew_bps = inv_f * p.inv_skew_scale_bps;
    r.half_spread_bps = half;
    r.bid = floor_tick(r.tv * (1.0 - (half + r.inv_skew_bps) * BPS), p.tick_size);
    r.ask = ceil_tick (r.tv * (1.0 + (half - r.inv_skew_bps) * BPS
                               + p.borrow_cost_bps * BPS), p.tick_size);
    return r;
}

void demo_single_stock_mm() {
    section("1. SINGLE STOCK MARKET MAKING");

    auto print_q = [](const char* label, const SingleStockMMParams& p) {
        auto q = single_stock_mm_quote(p);
        std::cout << std::fixed << std::setprecision(4);
        sub(label);
        std::cout
            << "  TV               = " << q.tv << "\n"
            << "  Half spread      = " << q.half_spread_bps << " bps\n"
            << "  Inventory skew   = " << q.inv_skew_bps << " bps\n"
            << "  BID = " << q.bid << "   ASK = " << q.ask
            << "   Spread = "
            << (q.ask - q.bid) / q.tv * 10000 << " bps\n";
    };

    // --- AAPL: flat inventory
    SingleStockMMParams p;
    p.bid_L1=174.98; p.ask_L1=175.02; p.realized_vol=0.20;
    p.base_spread_bps=3; p.vol_scale=0.5; p.alpha_signal=0;
    p.inventory_shares=0; p.max_inventory=10000;
    p.inv_skew_scale_bps=10; p.borrow_cost_bps=0; p.tick_size=0.01;
    print_q("AAPL $175 – flat inventory, normal market", p);

    // --- AAPL: long 5,000 (skew bid down)
    p.inventory_shares = 5000;
    print_q("AAPL $175 – long 5,000 shares (skew DOWN to attract sellers)", p);

    // --- Earnings day: 3x vol
    p.inventory_shares = 0; p.realized_vol = 0.60; p.base_spread_bps = 15;
    print_q("AAPL – earnings day (3x vol, wider base spread)", p);

    // --- Hard-to-borrow stock
    p.realized_vol=0.35; p.base_spread_bps=5; p.borrow_cost_bps=150;
    print_q("HTB short-squeeze stock – 150 bps borrow cost on ask", p);
}

// =============================================================================
// SECTION 2: SINGLE STOCK FUTURES (SSF) MARKET MAKING
// =============================================================================
/*
 *  STRATEGY: Quote SSF contracts. Fair value via cost-of-carry.
 *  Hedge residual delta with spot or options.
 *
 *  FORMULAE:
 *    F_fair      = S * exp((r − q) * T)          [continuous compounding]
 *    FVB_pts     = F_fair − S                     [fair value basis]
 *    richness    = (F_mkt_mid − F_fair)/S * 10000 [bps]
 *    impl_repo   = ln(F_mkt/S)/T + q
 *    half_spread = max(base_bps, daily_vol_pts*vol_scale) + div_risk_bps/2
 *    inv_skew    = (contracts/10) * inv_skew_bps
 *    Bid = F_fair - (half_spread + inv_skew)*BPS*F_fair   → round to tick
 *    Ask = F_fair + (half_spread - inv_skew)*BPS*F_fair   → round to tick
 *
 *  KEY PARAMETERS:
 *    r               – risk-free rate (SOFR/SONIA)
 *    q               – continuous div yield or PV discrete divs / S
 *    div_risk_bps    – uncertainty in forward dividend, widens spread
 *    roll_cost_bps   – cost to roll expiry; embedded in calendar spread quote
 *    implied_repo    – if impl_repo > actual r: future is RICH (sell)
 *                      if impl_repo < actual r: future is CHEAP (buy)
 */
struct SSFMMParams {
    double spot{};              // underlying live price
    double r{0.0525};          // risk-free (e.g. SOFR)
    double q{0.005};           // continuous div yield
    double T{};                 // years to expiry
    int    multiplier{100};     // shares per contract
    double base_spread_bps{3};
    double vol_scale{0.5};
    double realized_vol{};
    double div_risk_bps{2};    // dividend forecast uncertainty
    double roll_cost_bps{1};
    double inventory_contracts{};
    double inv_skew_scale_bps{8};
    double tick_size{0.01};
};

struct SSFQuote {
    double f_fair{}, fvb{}, impl_repo{}, richness_bps{};
    double bid{}, ask{}, half_spread_bps{};
};

static SSFQuote ssf_mm_quote(const SSFMMParams& p, double f_market_mid) {
    SSFQuote r;
    r.f_fair     = FairFuture::continuous(p.spot, p.r, p.q, p.T);
    r.fvb        = r.f_fair - p.spot;
    r.impl_repo  = FairFuture::implied_repo(f_market_mid, p.spot, p.q, p.T);
    r.richness_bps = (f_market_mid - r.f_fair) / p.spot * 10000.0;
    double daily_vol_pts = (p.realized_vol / SQRT252) * p.spot;
    double half = std::max(p.base_spread_bps,
                           daily_vol_pts / p.spot * 10000.0 * p.vol_scale)
                + p.div_risk_bps / 2.0;
    double inv_skew = (p.inventory_contracts / 10.0) * p.inv_skew_scale_bps;
    inv_skew = std::clamp(inv_skew, -p.inv_skew_scale_bps, p.inv_skew_scale_bps);
    r.half_spread_bps = half;
    r.bid = floor_tick(r.f_fair * (1.0 - (half + inv_skew) * BPS), p.tick_size);
    r.ask = ceil_tick (r.f_fair * (1.0 + (half - inv_skew) * BPS), p.tick_size);
    return r;
}

void demo_ssf_mm() {
    section("2. SINGLE STOCK FUTURES (SSF) MARKET MAKING");

    SSFMMParams p;
    p.spot=175.0; p.r=0.0525; p.q=0.005; p.T=90.0/365.0;
    p.realized_vol=0.20; p.base_spread_bps=3; p.vol_scale=0.5;
    p.div_risk_bps=2; p.tick_size=0.01; p.inventory_contracts=0;

    double f_mid = FairFuture::continuous(p.spot, p.r, p.q, p.T) + 0.15; // slightly rich

    sub("AAPL 90-day SSF – flat inventory");
    auto q = ssf_mm_quote(p, f_mid);
    std::cout << std::fixed << std::setprecision(4)
        << "  Spot             = " << p.spot << "\n"
        << "  Fair Future      = " << q.f_fair << "\n"
        << "  Fair Value Basis = " << q.fvb << " pts\n"
        << "  Market mid       = " << f_mid << "\n"
        << "  Richness         = " << q.richness_bps << " bps\n"
        << "  Implied Repo     = " << q.impl_repo*100 << "%\n"
        << "  Half spread      = " << q.half_spread_bps << " bps\n"
        << "  BID = " << q.bid << "   ASK = " << q.ask << "\n";

    sub("AAPL 90-day SSF – short 20 contracts (skew UP)");
    p.inventory_contracts = -20;
    q = ssf_mm_quote(p, f_mid);
    std::cout << "  BID = " << q.bid << "   ASK = " << q.ask
              << "   (asks raised to attract buyers)\n";

    // Calendar spread fair value
    double csv = FairFuture::calendar_spread(p.spot, p.r, p.q, 90.0/365, 180.0/365);
    std::cout << "\n  Calendar Spread Fair Value (3M vs 6M) = " << csv << " pts\n";
}

// =============================================================================
// SECTION 3: SINGLE STOCK OPTIONS MARKET MAKING
// =============================================================================
/*
 *  STRATEGY: Quote calls & puts around BS TV. Delta-hedge with stock or SSF.
 *  Earn the volatility spread (vol_bid vs vol_ask).
 *
 *  FORMULAE:
 *    vol_bid = sigma - vol_spread/2          [quote vol space]
 *    vol_ask = sigma + vol_spread/2
 *    option_bid = BS(S,K,T,r,q, vol_bid)
 *    option_ask = BS(S,K,T,r,q, vol_ask)
 *    gamma_spread_$ = gamma * S^2 * 0.01 * gamma_scale
 *    pin_risk_$ = gamma * S * daily_vol * 2   [near expiry / near strike]
 *    half_spread_$ = max(min_spread_$, delta_lag + gamma_spread + pin_risk)
 *    vega_skew_vol = vega_inventory * inv_vega_scale  → shift vol_bid & vol_ask
 *
 *  KEY PARAMETERS:
 *    vol_spread_pts   – quoting half-spread in vol (0.5 liquid, 2.0 illiquid)
 *    gamma_scale      – extra $ spread for gamma risk (re-hedging cost)
 *    delta_hedge_lag  – slippage cost of delta hedge execution in bps
 *    pin_risk_premium – extra spread when gamma spikes near expiry + near ATM
 *    vega_inventory   – current net vega exposure (positive = long vol)
 *    inv_vega_scale   – bps of vol shift per unit vega exposure
 */
struct SSOptMMParams {
    double S{}, K{}, T{}, r{}, q{}, sigma{};
    int    cp{1};               // +1 call, -1 put
    double vol_spread_pts{0.5}; // quoting half-spread in vol pts
    double gamma_scale{1.0};
    double delta_hedge_lag_bps{2};
    double pin_risk_scale{2.0}; // multiplier for near-expiry gamma risk
    double vega_inventory{};    // net vega (long = positive)
    double inv_vega_scale{0.1}; // vol pts shift per 100 units vega (0.1 vol pt per 100 vega)
    double min_spread_dollars{0.01};
    double tick_size{0.01};
};

struct SSOptQuote {
    BSGreeks greeks{};
    double vol_bid{}, vol_ask{};
    double bid{}, ask{};
    double gamma_spread_d{}, pin_risk_d{}, half_spread_d{};
    double delta_hedge_shares{};
};

static SSOptQuote ssopt_mm_quote(const SSOptMMParams& p) {
    SSOptQuote r;
    // vega inventory skew: inv_vega_scale in vol pts per 100 vega → convert to decimal
    double vol_adj   = p.vega_inventory * p.inv_vega_scale * 0.01 / 100.0;
    r.vol_bid = p.sigma - p.vol_spread_pts * 0.01 - vol_adj; // vol_spread_pts in vol pts
    r.vol_ask = p.sigma + p.vol_spread_pts * 0.01 - vol_adj;
    r.greeks  = BlackScholes::greeks(p.S, p.K, p.T, p.r, p.q, p.sigma, p.cp);
    double daily_vol = p.sigma / SQRT252;
    r.gamma_spread_d = r.greeks.gamma * p.S * p.S * 0.01 * p.gamma_scale;
    double atm_dist  = std::fabs(p.S - p.K) / p.K;
    double pin       = (p.T < 5.0/365 && atm_dist < 0.02)
                     ? r.greeks.gamma * p.S * daily_vol * p.pin_risk_scale : 0;
    r.pin_risk_d     = pin;
    double delta_lag = p.delta_hedge_lag_bps * BPS * p.S * std::fabs(r.greeks.delta);
    r.half_spread_d  = std::max(p.min_spread_dollars,
                                delta_lag + r.gamma_spread_d + r.pin_risk_d);
    auto g_bid = BlackScholes::greeks(p.S, p.K, p.T, p.r, p.q, r.vol_bid, p.cp);
    auto g_ask = BlackScholes::greeks(p.S, p.K, p.T, p.r, p.q, r.vol_ask, p.cp);
    r.bid = floor_tick(std::max(0.0, g_bid.price - r.half_spread_d), p.tick_size);
    r.ask = ceil_tick (g_ask.price + r.half_spread_d, p.tick_size);
    r.delta_hedge_shares = -r.greeks.delta * 100; // per 1 contract (100 shares)
    return r;
}

void demo_ss_options_mm() {
    section("3. SINGLE STOCK OPTIONS MARKET MAKING");

    SSOptMMParams p;
    p.S=175; p.K=175; p.T=30.0/365; p.r=0.0525; p.q=0.005;
    p.sigma=0.25; p.cp=1;
    p.vol_spread_pts=0.5; p.gamma_scale=1.0;
    p.delta_hedge_lag_bps=2; p.vega_inventory=0; p.tick_size=0.01;

    auto print_q = [](const char* lbl, const SSOptMMParams& pm) {
        auto q = ssopt_mm_quote(pm);
        sub(lbl);
        std::cout << std::fixed << std::setprecision(4)
            << "  BS TV            = " << q.greeks.price << "\n"
            << "  Delta            = " << q.greeks.delta << "\n"
            << "  Gamma            = " << q.greeks.gamma << "\n"
            << "  Vega (per vol pt)= " << q.greeks.vega  << "\n"
            << "  Theta ($/day)    = " << q.greeks.theta << "\n"
            << "  Vol bid/ask      = " << q.vol_bid*100 << "% / " << q.vol_ask*100 << "%\n"
            << "  Half spread $    = " << q.half_spread_d << "\n"
            << "  BID = " << q.bid << "   ASK = " << q.ask << "\n"
            << "  Delta hedge      = " << q.delta_hedge_shares << " shares per contract\n";
    };

    print_q("AAPL 30d ATM call – flat vol inventory", p);

    p.vega_inventory = 500.0; // long vega → shift vol quotes down
    print_q("AAPL 30d ATM call – long 500 vega (shift vol BID & ASK down)", p);

    p.vega_inventory = 0; p.cp = -1; // put
    print_q("AAPL 30d ATM put – flat", p);

    p.K=185; p.sigma=0.22; p.cp=1; // OTM call
    print_q("AAPL 30d OTM call K=185 sigma=22%", p);
}

// =============================================================================
// SECTION 4: ETF MARKET MAKING
// =============================================================================
/*
 *  STRATEGY: Stream two-sided quotes. TV anchored to iNAV (basket fair value).
 *  Creation/redemption arbitrage defines no-arb band.
 *
 *  FORMULAE:
 *    iNAV        = SUM(w_i * mid_i * fx_i) + cash_component
 *    iNAV_adj    = iNAV + beta * future_basis          [hedge with index future]
 *    TV          = iNAV_adj * (1 + alpha)              [alpha signal]
 *    half_spread = max(base_bps, daily_vol * vol_scale * 10000)
 *    inv_skew    = (inventory / max_inv) * inv_skew_scale_bps
 *    Bid = TV * (1 - (half + inv_skew) * BPS)   → round to tick
 *    Ask = TV * (1 + (half - inv_skew) * BPS)   → round to tick
 *
 *  CREATION/REDEMPTION ARB BAND:
 *    arb_band_bps ≈ creation_redemption_fee + execution_cost (~3-8 bps)
 *    If ETF_ask > iNAV + arb_band: CREATION opportunity (AP buys basket, redeems)
 *    If ETF_bid < iNAV - arb_band: REDEMPTION opportunity (AP sells basket, creates)
 *
 *  KEY PARAMETERS:
 *    beta           – sensitivity of ETF price to index future basis (typically 0.1–1.0)
 *    future_basis   – (F_market_mid − F_fair) in pts; positive = futures rich
 *    alpha          – short-term directional signal (order flow, macro)
 *    arb_band_bps   – no-arbitrage corridor; tighter for liquid ETFs (SPY=2, illiquid=15)
 */
struct ETFMMParams {
    double inav{};              // basket iNAV (SUM of weighted constituent mids)
    double future_basis{};      // F_market_mid - F_fair in index pts normalised to ETF
    double beta{0.1};          // ETF sensitivity to future basis
    double alpha{0.0};         // directional signal
    double realized_vol{};     // annualised vol
    double base_spread_bps{5};
    double vol_scale{0.5};
    double inventory_shares{};
    double max_inventory{50000};
    double inv_skew_scale_bps{10};
    double arb_band_bps{5};    // creation/redemption no-arb corridor
    double tick_size{0.01};
};

struct ETFQuote {
    double inav_adj{}, tv{}, bid{}, ask{};
    double half_spread_bps{}, inv_skew_bps{};
    bool   arb_create{}, arb_redeem{};
};

static ETFQuote etf_mm_quote(const ETFMMParams& p) {
    ETFQuote r;
    r.inav_adj = p.inav + p.beta * p.future_basis;
    r.tv       = r.inav_adj * (1.0 + p.alpha);
    double daily_v = p.realized_vol / SQRT252;
    double half    = std::max(p.base_spread_bps, daily_v * p.vol_scale * 10000.0);
    double inv_f   = std::clamp(p.inventory_shares / p.max_inventory, -1.0, 1.0);
    r.inv_skew_bps = inv_f * p.inv_skew_scale_bps;
    r.half_spread_bps = half;
    r.bid = floor_tick(r.tv * (1.0 - (half + r.inv_skew_bps) * BPS), p.tick_size);
    r.ask = ceil_tick (r.tv * (1.0 + (half - r.inv_skew_bps) * BPS), p.tick_size);
    r.arb_create = (r.ask > p.inav * (1.0 + p.arb_band_bps * BPS));
    r.arb_redeem = (r.bid < p.inav * (1.0 - p.arb_band_bps * BPS));
    return r;
}

void demo_etf_mm() {
    section("4. ETF MARKET MAKING");

    ETFMMParams p;
    p.inav=524.10; p.future_basis=6.20; p.beta=0.1;
    p.realized_vol=0.15; p.base_spread_bps=3; p.vol_scale=0.5;
    p.max_inventory=50000; p.arb_band_bps=4; p.tick_size=0.01;

    auto show = [](const char* lbl, const ETFMMParams& pm) {
        auto q = etf_mm_quote(pm);
        sub(lbl);
        std::cout << std::fixed << std::setprecision(4)
            << "  iNAV             = " << pm.inav << "\n"
            << "  iNAV_adj         = " << q.inav_adj << "\n"
            << "  TV               = " << q.tv << "\n"
            << "  Half spread      = " << q.half_spread_bps << " bps\n"
            << "  Inv skew         = " << q.inv_skew_bps << " bps\n"
            << "  BID = " << q.bid << "   ASK = " << q.ask << "\n"
            << "  Arb: create=" << q.arb_create << " redeem=" << q.arb_redeem << "\n";
    };

    show("SPY – normal market, flat inventory", p);
    p.inventory_shares = 20000;
    show("SPY – long 20,000 (skew DOWN to sell)", p);
    p.inventory_shares = 0; p.realized_vol = 0.40; p.base_spread_bps = 12;
    show("SPY – vol spike event (VIX=40)", p);
}

// =============================================================================
// SECTION 5: INDEX FUTURES MARKET MAKING
// =============================================================================
/*
 *  STRATEGY: Quote front/back-month index futures (ES, NQ, N225, HSI, SGX).
 *  Hedge with ETF, basket, or options.
 *
 *  FORMULAE:
 *    F_fair    = Index * exp((r − d) * T)         [d = continuous div yield]
 *    FVB_pts   = F_fair − Index                   [e.g. ES ≈ +40-60 pts]
 *    richness_bps = (F_mid − F_fair)/Index * 10000
 *    half_spread_pts = max(1 tick, daily_vol_pts * vol_scale)
 *    skew_ticks = (contracts/10) * inv_skew_ticks
 *    Bid = round_down(F_fair − half*BPS*Index, tick)
 *    Ask = round_up  (F_fair + half*BPS*Index, tick)
 *    Calendar spread fair value: CSV = F(T2) − F(T1)
 *
 *  KEY PARAMETERS:
 *    multiplier   – contract value per index point (ES=$50, NQ=$20, N225=¥1000)
 *    tick_size    – min increment (ES=0.25 pts, N225=5 pts)
 *    FVB          – market tends to trade at FVB during non-event times
 *    richness     – deviation from fair; >10 bps → arb opportunity with ETF basket
 */
struct IdxFutMMParams {
    double spot_index{};
    double r{0.0525};
    double d{0.013};           // index div yield (SP500 ≈ 1.3%)
    double T{};
    double multiplier{50};     // e.g. ES = $50/pt
    double tick_size{0.25};    // ES min tick
    double base_spread_ticks{1};
    double vol_scale{0.3};
    double realized_vol{};
    double inventory_contracts{};
    double inv_skew_ticks{1};
};

struct IdxFutQuote {
    double f_fair{}, fvb{}, richness_bps{};
    double bid{}, ask{}, half_spread_pts{};
};

static IdxFutQuote idx_fut_quote(const IdxFutMMParams& p, double f_mid) {
    IdxFutQuote r;
    r.f_fair = FairFuture::continuous(p.spot_index, p.r, p.d, p.T);
    r.fvb    = r.f_fair - p.spot_index;
    r.richness_bps = (f_mid - r.f_fair) / p.spot_index * 10000.0;
    double daily_vol_pts = p.realized_vol / SQRT252 * p.spot_index;
    double half_pts = std::max(p.base_spread_ticks * p.tick_size,
                               daily_vol_pts * p.vol_scale * BPS * p.spot_index);
    half_pts = std::max(half_pts, p.base_spread_ticks * p.tick_size);
    double inv_skew = std::clamp(p.inventory_contracts / 10.0,
                                 -5.0, 5.0) * p.inv_skew_ticks * p.tick_size;
    r.half_spread_pts = half_pts;
    r.bid = floor_tick(r.f_fair - half_pts - inv_skew, p.tick_size);
    r.ask = ceil_tick (r.f_fair + half_pts - inv_skew, p.tick_size);
    return r;
}

void demo_idx_futures_mm() {
    section("5. INDEX FUTURES MARKET MAKING");

    IdxFutMMParams p;
    p.spot_index=5220; p.r=0.0525; p.d=0.013; p.T=87.0/365;
    p.multiplier=50; p.tick_size=0.25; p.base_spread_ticks=1;
    p.realized_vol=0.15; p.vol_scale=0.3; p.inventory_contracts=0;

    double fair = FairFuture::continuous(p.spot_index, p.r, p.d, p.T);
    double f_mid = fair + 2.5; // slightly rich

    sub("E-mini S&P 500 (ES) – 87 days, flat inventory");
    auto q = idx_fut_quote(p, f_mid);
    std::cout << std::fixed << std::setprecision(2)
        << "  Index spot       = " << p.spot_index << "\n"
        << "  Fair future      = " << q.f_fair << "\n"
        << "  FVB              = " << q.fvb << " pts\n"
        << "  Market mid       = " << f_mid << "\n"
        << "  Richness         = " << std::setprecision(2) << q.richness_bps << " bps\n"
        << "  Half spread      = " << q.half_spread_pts << " pts\n"
        << "  BID = " << q.bid << "   ASK = " << q.ask << "\n"
        << "  Contract value   = $" << q.f_fair * p.multiplier << "\n";

    p.inventory_contracts = 50;
    sub("ES – long 50 contracts (skew DOWN)");
    q = idx_fut_quote(p, f_mid);
    std::cout << "  BID = " << q.bid << "   ASK = " << q.ask << "\n";

    double csv = FairFuture::calendar_spread(p.spot_index, p.r, p.d,
                                              87.0/365, 177.0/365);
    std::cout << "\n  Calendar Spread (3M vs 6M) fair value = " << csv << " pts\n";
}

// =============================================================================
// SECTION 6: INDEX OPTIONS MARKET MAKING
// =============================================================================
/*
 *  STRATEGY: European index options (SPX/NDX/Nikkei 225).
 *  Hedge delta with index futures. Earn vol spread + manage skew risk.
 *
 *  FORMULAE:
 *    TV = BS(Index, K, T, r, q, IV)     [European, no early exercise]
 *    Vol surface (Malz 1997 approximation):
 *      sigma(Delta) = ATM + RR * (Delta − 0.5) + BF * (2*Delta − 1)^2
 *    vol_bid = IV − vol_spread/2
 *    vol_ask = IV + vol_spread/2
 *    option_bid = BS(..., vol_bid);  option_ask = BS(..., vol_ask)
 *    futures_hedge = delta * notional / (F_fair * multiplier)
 *    pin_premium = gamma * S * daily_vol * 2   [near expiry, near strike]
 *
 *  KEY PARAMETERS:
 *    atm_vol     – ATM straddle implied vol (e.g. 18% for SPX)
 *    rr_25d      – 25-delta risk reversal = call_vol − put_vol (equity < 0, skew)
 *    bf_25d      – 25-delta butterfly (smile curvature, usually > 0)
 *    vega_inv    – net portfolio vega; large long → vol quoted lower on ask
 *    gamma_inv   – net gamma; affects re-hedge frequency and spread
 */
struct IdxOptMMParams {
    double index_spot{}, K{}, T{}, r{}, q{}, IV{};
    int    cp{1};
    double vol_spread_pts{0.75};
    double atm_vol{}, rr_25d{}, bf_25d{};
    double vega_inventory{};
    double inv_vega_scale{0.005};
    double multiplier{100};    // SPX = $100
    double futures_multiplier{50}; // ES = $50
    double min_spread_d{0.10};
    double tick_size{0.05};
};

struct IdxOptQuote {
    BSGreeks greeks{};
    double vol_bid{}, vol_ask{}, bid{}, ask{};
    double vol_surface_sigma{};
    double futures_hedge_contracts{};
};

static IdxOptQuote idx_opt_quote(const IdxOptMMParams& p) {
    IdxOptQuote r;
    // Vol surface for a given delta
    auto g0   = BlackScholes::greeks(p.index_spot, p.K, p.T, p.r, p.q, p.IV, p.cp);
    double dlt = std::fabs(g0.delta);
    r.vol_surface_sigma = p.atm_vol
        + p.rr_25d * (dlt - 0.5)
        + p.bf_25d * std::pow(2*dlt - 1.0, 2);
    double eff_vol = (p.IV > 0) ? p.IV : r.vol_surface_sigma;
    // inv_vega_scale in vol pts per unit vega → convert to decimal
    double vol_adj = p.vega_inventory * p.inv_vega_scale * 0.01;
    // vol_spread_pts is half-spread in vol pts (e.g. 0.75 = 0.75%); convert to decimal
    r.vol_bid = eff_vol - p.vol_spread_pts * 0.01 - vol_adj;
    r.vol_ask = eff_vol + p.vol_spread_pts * 0.01 - vol_adj;
    auto gb = BlackScholes::greeks(p.index_spot, p.K, p.T, p.r, p.q, r.vol_bid, p.cp);
    auto ga = BlackScholes::greeks(p.index_spot, p.K, p.T, p.r, p.q, r.vol_ask, p.cp);
    r.greeks = g0;
    double daily_vol = eff_vol / SQRT252;
    double pin = (p.T < 5.0/365 && std::fabs(p.index_spot-p.K)/p.K < 0.01)
               ? g0.gamma * p.index_spot * daily_vol * 2.0 : 0;
    double hs = std::max(p.min_spread_d,
        std::fabs(ga.price - gb.price)/2.0 + pin);
    r.bid = floor_tick(std::max(0.0, gb.price - hs), p.tick_size);
    r.ask = ceil_tick (ga.price + hs, p.tick_size);
    double f_fair = FairFuture::continuous(p.index_spot, p.r, p.q, p.T);
    r.futures_hedge_contracts = -(g0.delta * p.multiplier)
                                / (p.futures_multiplier);
    return r;
}

void demo_idx_options_mm() {
    section("6. INDEX OPTIONS MARKET MAKING");

    IdxOptMMParams p;
    p.index_spot=5220; p.K=5200; p.T=30.0/365; p.r=0.0525; p.q=0.013;
    p.IV=0.18; p.cp=-1; // 30d slightly OTM put
    p.vol_spread_pts=0.75; p.atm_vol=0.18; p.rr_25d=-0.03; p.bf_25d=0.005;
    p.multiplier=100; p.futures_multiplier=50; p.tick_size=0.05;

    auto show = [](const char* lbl, const IdxOptMMParams& pm) {
        auto q = idx_opt_quote(pm);
        sub(lbl);
        std::cout << std::fixed << std::setprecision(4)
            << "  BS TV            = " << q.greeks.price << "\n"
            << "  Delta            = " << q.greeks.delta << "\n"
            << "  Gamma            = " << q.greeks.gamma << "\n"
            << "  Vega (per vol pt)= " << q.greeks.vega  << "\n"
            << "  Vol surface σ    = " << q.vol_surface_sigma*100 << "%\n"
            << "  Vol bid / ask    = " << q.vol_bid*100 << "% / " << q.vol_ask*100 << "%\n"
            << "  BID = " << q.bid << "   ASK = " << q.ask << "\n"
            << "  ES futures hedge = " << std::setprecision(2)
            << q.futures_hedge_contracts << " contracts\n";
    };

    show("SPX 30d put K=5200 – flat inventory", p);
    p.K=5000; p.IV=0.22; // deeper OTM put (higher vol due to skew)
    show("SPX 30d deep OTM put K=5000 – higher skew IV", p);
    p.cp=1; p.K=5400; p.IV=0.14;
    show("SPX 30d OTM call K=5400 – lower vol (negative skew)", p);
}

// =============================================================================
// SECTION 7: FX SPOT MARKET MAKING
// =============================================================================
/*
 *  STRATEGY: Stream continuous bid/ask in major/minor pairs. Hedge net delta
 *  via spot deals with other LPs or via FX forwards.
 *
 *  FORMULAE:
 *    TV              = composite_mid    [from LP stream or ECN BBO]
 *    daily_vol_rate  = sigma_fx / sqrt(252) * mid_rate
 *    tier_adj        = size_tier_bps[tier]
 *    half_spread_pip = max(base_pips, daily_vol_rate * vol_scale / pip_size)
 *    inv_skew_pip    = (inventory_base_ccy / 1e6) * inv_skew_pips_per_M
 *    Bid = TV - (half_spread + inv_skew) * pip_size   [floor to pip]
 *    Ask = TV + (half_spread - inv_skew) * pip_size   [ceil to pip]
 *    Triangulation: TV_cross = ccyA_mid * ccyB_mid
 *      if |TV_direct - TV_cross| > 0.3 pip → stale feed alert
 *
 *  KEY PARAMETERS:
 *    pip_size           – smallest quoted price unit (EURUSD: 0.00001, USDJPY: 0.001)
 *    base_pip_spread    – minimum competitive spread (EURUSD: 0.3, USDTRY: 5.0)
 *    size_tier_bps      – spread ladder [<1M: 0, 1-10M: 0.5, 10-50M: 1.5, >50M: 3.0]
 *    fixing_premium_pip – extra spread near WMR/ECB fixing windows (±5 min)
 *    inv_skew_pips_per_M – pips shift per $1M net base-ccy exposure
 */
struct FXSpotMMParams {
    double mid_rate{};          // composite LP mid (e.g. EURUSD 1.08500)
    double pip_size{0.00001};  // EURUSD 5dp; USDJPY 0.001
    double base_pip_spread{0.3};// half-spread in pips (EURUSD tier 1)
    double size_bps[4]{0,0.5,1.5,3.0}; // spread add-on by tier
    double sigma_fx{0.065};    // annualised FX vol (EURUSD ≈ 6.5%)
    double vol_scale{1.0};
    double fixing_premium_pips{1.5}; // near WMR fixing
    bool   near_fixing{false};
    double inventory_base_ccy{}; // net EUR held (positive = long)
    double max_inventory_usd{5e7};
    double inv_skew_pips_per_M{0.3}; // pips per $1M inventory
    int    size_tier{0};        // 0=<1M, 1=1-10M, 2=10-50M, 3=>50M
};

struct FXSpotQuote {
    double tv{}, bid{}, ask{};
    double half_spread_pips{}, inv_skew_pips{};
    bool   stale_flag{false};
};

static FXSpotQuote fx_spot_quote(const FXSpotMMParams& p,
                                  double cross_check = 0.0) {
    FXSpotQuote r;
    r.tv = p.mid_rate;
    double daily_vol = p.sigma_fx / SQRT252 * p.mid_rate;
    double tier_add  = p.size_bps[std::clamp(p.size_tier,0,3)];
    double half = std::max(p.base_pip_spread,
                           daily_vol * p.vol_scale / p.pip_size)
                + tier_add
                + (p.near_fixing ? p.fixing_premium_pips : 0.0);
    double inv_M = p.inventory_base_ccy / 1e6;
    r.inv_skew_pips = std::clamp(inv_M * p.inv_skew_pips_per_M,
                                 -5.0, 5.0);
    r.half_spread_pips = half;
    r.bid = std::floor((r.tv - (half + r.inv_skew_pips) * p.pip_size)
                       / p.pip_size) * p.pip_size;
    r.ask = std::ceil ((r.tv + (half - r.inv_skew_pips) * p.pip_size)
                       / p.pip_size) * p.pip_size;
    if (cross_check > 0)
        r.stale_flag = std::fabs(r.tv - cross_check) > 0.3 * p.pip_size;
    return r;
}

void demo_fx_spot_mm() {
    section("7. FX SPOT MARKET MAKING");

    FXSpotMMParams p;
    p.mid_rate=1.08500; p.pip_size=0.00001; p.base_pip_spread=0.3;
    p.sigma_fx=0.065; p.vol_scale=0.001; p.near_fixing=false;
    p.inventory_base_ccy=0; p.inv_skew_pips_per_M=0.3;

    auto show = [](const char* lbl, const FXSpotMMParams& pm, double cross=0) {
        auto q = fx_spot_quote(pm, cross);
        sub(lbl);
        std::cout << std::fixed << std::setprecision(5)
            << "  TV               = " << q.tv << "\n"
            << "  Half spread      = " << std::setprecision(2)
            << q.half_spread_pips << " pips\n"
            << "  Inv skew         = " << q.inv_skew_pips << " pips\n"
            << "  BID = " << std::setprecision(5) << q.bid
            << "   ASK = " << q.ask << "\n";
        if (q.stale_flag) std::cout << "  *** STALE FEED ALERT ***\n";
    };

    show("EURUSD 1M tier – flat inventory", p);
    p.size_tier=2; // 10-50M
    show("EURUSD 25M tier – large deal", p);
    p.size_tier=0; p.inventory_base_ccy=5e6; // long EUR 5M
    show("EURUSD – long EUR 5M (skew bids DOWN)", p);
    p.near_fixing=true; p.inventory_base_ccy=0;
    show("EURUSD – near WMR fixing (wider spread)", p);
    // Triangulation check
    double eurgbp=0.85820, gbpusd=1.26450;
    double cross=eurgbp*gbpusd;
    p.near_fixing=false;
    show("EURUSD – triangulation cross-check", p, cross);
}

// =============================================================================
// SECTION 8: FX FUTURES MARKET MAKING
// =============================================================================
/*
 *  STRATEGY: Quote CME/SGX currency futures. Fair value from covered interest
 *  parity (CIP). Hedge with spot + FX swap.
 *
 *  FORMULAE:
 *    F_fair     = S * exp((r_d − r_f) * T)        [CIP forward rate]
 *    Fwd_pts    = F_fair − S ≈ S*(r_d−r_f)*T      [forward points]
 *    CIP_basis  = F_market − F_fair               [+ = USD funding premium]
 *    impl_r_d   = ln(F_mkt/S)/T + r_f             [implied domestic rate]
 *    TV         = F_fair + CIP_weight * CIP_basis  [CIP-adjusted TV]
 *    half_spread = max(base_pips, daily_vol * vol_scale)
 *    Bid/Ask    = TV ± half_spread * pip_size ± inv_skew
 *
 *  KEY PARAMETERS:
 *    r_d / r_f  – domestic/foreign risk-free (SOFR / €STR / SONIA / TONAR)
 *    CIP_basis  – persistent in USD shortage; anomalous vs pure theory
 *    CIP_weight – 0 = ignore CIP anomaly; 1 = fully adjust TV for it
 *    swap_pts   – live 3M FX swap market (alternative to CIP formula)
 */
struct FXFutMMParams {
    double spot{};             // EURUSD spot
    double r_d{0.0525};        // domestic (USD SOFR)
    double r_f{0.0375};        // foreign (EUR €STR)
    double T{};                // years to expiry
    int    multiplier{125000}; // EUR/USD CME = 125,000 EUR
    double pip_size{0.00001};
    double base_pip_spread{0.5};
    double sigma_fx{0.065};
    double vol_scale{1.0};
    double CIP_basis{};        // market forward − CIP fair (in rate units)
    double CIP_weight{0.3};    // partial adjustment for persistent CIP deviation
    double inventory_contracts{};
    double inv_skew_pips{0.5};
};

struct FXFutQuote {
    double f_fair{}, fwd_pts{}, CIP_basis_pips{};
    double tv{}, bid{}, ask{}, half_pips{};
};

static FXFutQuote fx_fut_quote(const FXFutMMParams& p, double f_market_mid) {
    FXFutQuote r;
    r.f_fair  = FairFuture::fx_forward(p.spot, p.r_d, p.r_f, p.T);
    r.fwd_pts = r.f_fair - p.spot;
    r.CIP_basis_pips = (f_market_mid - r.f_fair) / p.pip_size;
    r.tv      = r.f_fair + p.CIP_weight * p.CIP_basis;
    double daily_vol = p.sigma_fx / SQRT252 * p.spot;
    double half_rate = std::max(p.base_pip_spread * p.pip_size,
                                daily_vol * p.vol_scale * BPS * p.spot);
    double half_pips = half_rate / p.pip_size;
    double inv_skew  = std::clamp(p.inventory_contracts / 10.0, -5.0, 5.0)
                     * p.inv_skew_pips;
    r.half_pips = half_pips;
    r.bid = floor_tick(r.tv - (half_pips + inv_skew)*p.pip_size, p.pip_size);
    r.ask = ceil_tick (r.tv + (half_pips - inv_skew)*p.pip_size, p.pip_size);
    return r;
}

void demo_fx_futures_mm() {
    section("8. FX FUTURES MARKET MAKING");

    FXFutMMParams p;
    p.spot=1.08500; p.r_d=0.0525; p.r_f=0.0375; p.T=90.0/365;
    p.pip_size=0.00001; p.base_pip_spread=0.5; p.sigma_fx=0.065;
    p.CIP_basis=-2*p.pip_size; p.CIP_weight=0.3;
    p.inventory_contracts=0;

    double fair = FairFuture::fx_forward(p.spot, p.r_d, p.r_f, p.T);
    double f_mid = fair - 1*p.pip_size;

    sub("EURUSD Dec futures – flat, CIP deviation −2 pips");
    auto q = fx_fut_quote(p, f_mid);
    std::cout << std::fixed << std::setprecision(5)
        << "  Spot             = " << p.spot << "\n"
        << "  CIP Fair Forward = " << q.f_fair << "\n"
        << "  Forward pts      = " << std::setprecision(5) << q.fwd_pts << "\n"
        << "  CIP basis        = " << std::setprecision(2)
        << q.CIP_basis_pips << " pips\n"
        << "  TV (adjusted)    = " << std::setprecision(5) << q.tv << "\n"
        << "  Half spread      = " << std::setprecision(2)
        << q.half_pips << " pips\n"
        << "  BID = " << std::setprecision(5) << q.bid
        << "   ASK = " << q.ask << "\n";

    p.inventory_contracts = 20;
    q = fx_fut_quote(p, f_mid);
    sub("EURUSD Dec futures – long 20 contracts");
    std::cout << "  BID = " << q.bid << "   ASK = " << q.ask << "\n";
}

// =============================================================================
// SECTION 9: FX OPTIONS MARKET MAKING (GARMAN-KOHLHAGEN)
// =============================================================================
/*
 *  STRATEGY: Quote FX vanilla options. Vol surface quoted in delta-vol space.
 *  Hedge delta with spot; hedge vega/gamma with vanilla portfolio.
 *
 *  GARMAN-KOHLHAGEN (1983) — FX option pricing:
 *    d1 = (ln(S/K) + (r_d - r_f + σ²/2)*T) / (σ√T)
 *    d2 = d1 − σ√T
 *    Call = S*e^{-r_f*T}*N(d1) − K*e^{-r_d*T}*N(d2)
 *    Put  = K*e^{-r_d*T}*N(−d2) − S*e^{-r_f*T}*N(−d1)
 *    Delta_call = e^{-r_f*T}*N(d1)      [spot-delta convention]
 *
 *  VOL SURFACE — Malz (1997) parametrisation:
 *    σ(Δ) = ATM + RR*(Δ−0.5) + BF*(2Δ−1)²
 *      ATM = at-the-money straddle vol
 *      RR  = 25Δ risk reversal (call_vol − put_vol); equity < 0 (skew)
 *      BF  = 25Δ butterfly (smile curvature, typically > 0)
 *
 *  QUOTING:
 *    vol_bid = σ(Δ) − vol_spread/2;   vol_ask = σ(Δ) + vol_spread/2
 *    vega_skew: vol_adj = vega_inventory * inv_vega_scale
 *    option_bid = GK(S,K,T,rd,rf, vol_bid)
 *    option_ask = GK(S,K,T,rd,rf, vol_ask)
 *
 *  KEY PARAMETERS:
 *    atm_vol      – ATM straddle implied vol (EURUSD 1M ≈ 7.5%)
 *    rr_25d       – 25Δ risk reversal (EURUSD typically −0.5% to +0.5%)
 *    bf_25d       – 25Δ butterfly (≈ 0.3% for G10 majors)
 *    delta_convention – spot-delta vs premium-adjusted (PA) for in-the-money
 *    vanna_volga  – correction to GK for smile (for illiquid pairs)
 */
struct FXOptMMParams {
    double S{}, K{}, T{}, r_d{}, r_f{}, sigma{};
    int    cp{1};
    double vol_spread_pts{0.20}; // half-spread in vol pts (0.2 liquid pair)
    double atm_vol{}, rr_25d{}, bf_25d{};
    double vega_inventory{};
    double inv_vega_scale{0.01};
    double min_spread_rate{0.0001}; // min $ spread in rate terms
    double pip_size{0.00001};
    double notional{1e6};      // notional in base ccy (1M EUR)
};

struct FXOptQuote {
    BSGreeks greeks{};
    double vol_surface_sigma{};
    double vol_bid{}, vol_ask{}, bid{}, ask{};
    double delta_notional_base{}; // hedge size in base ccy
};

static FXOptQuote fx_opt_quote(const FXOptMMParams& p) {
    FXOptQuote r;
    auto g0 = GarmanKohlhagen::greeks(p.S, p.K, p.T, p.r_d, p.r_f, p.sigma, p.cp);
    double dlt = std::fabs(g0.delta);
    r.vol_surface_sigma = p.atm_vol
        + p.rr_25d  * (dlt - 0.5)
        + p.bf_25d  * std::pow(2*dlt - 1.0, 2);
    double eff = (p.sigma > 0) ? p.sigma : r.vol_surface_sigma;
    double vol_adj = p.vega_inventory * p.inv_vega_scale * 0.01;
    // vol_spread_pts is half-spread in vol pts (e.g. 0.20 = 0.2%); convert to decimal
    r.vol_bid = eff - p.vol_spread_pts * 0.01 - vol_adj;
    r.vol_ask = eff + p.vol_spread_pts * 0.01 - vol_adj;
    r.greeks = g0;
    auto gb = GarmanKohlhagen::greeks(p.S,p.K,p.T,p.r_d,p.r_f,r.vol_bid,p.cp);
    auto ga = GarmanKohlhagen::greeks(p.S,p.K,p.T,p.r_d,p.r_f,r.vol_ask,p.cp);
    double hs = std::max(p.min_spread_rate, std::fabs(ga.price-gb.price)/2.0);
    r.bid = std::max(0.0, gb.price - hs);
    r.ask = ga.price + hs;
    r.delta_notional_base = -g0.delta * p.notional; // hedge in spot
    return r;
}

void demo_fx_options_mm() {
    section("9. FX OPTIONS MARKET MAKING (GARMAN-KOHLHAGEN)");

    FXOptMMParams p;
    p.S=1.08500; p.K=1.08500; p.T=30.0/365; p.r_d=0.0525; p.r_f=0.0375;
    p.sigma=0.075; p.cp=1;
    p.vol_spread_pts=0.20; p.atm_vol=0.075; p.rr_25d=-0.005; p.bf_25d=0.003;
    p.notional=1e6; p.pip_size=0.00001;

    auto show = [](const char* lbl, const FXOptMMParams& pm) {
        auto q = fx_opt_quote(pm);
        sub(lbl);
        std::cout << std::fixed << std::setprecision(6)
            << "  GK TV (rate)     = " << q.greeks.price << "\n"
            << "  Delta            = " << q.greeks.delta << "\n"
            << "  Vol surface σ    = " << q.vol_surface_sigma*100 << "%\n"
            << "  Vol bid / ask    = " << q.vol_bid*100 << "% / "
            << q.vol_ask*100 << "%\n"
            << "  BID (rate)       = " << q.bid << "\n"
            << "  ASK (rate)       = " << q.ask << "\n"
            << "  Delta hedge      = " << std::setprecision(0)
            << q.delta_notional_base << " EUR spot\n";
    };

    show("EURUSD 30d ATM call – flat", p);
    p.cp=-1; p.K=1.07000;  // OTM put
    show("EURUSD 30d OTM put K=1.0700 (more negative skew)", p);
    p.cp=1; p.K=1.10000; p.sigma=0.070; // OTM call
    show("EURUSD 30d OTM call K=1.1000", p);
}

// =============================================================================
// SECTION 10: DUAL COUNTER MARKET MAKING (HKEX MECHANISM)
// =============================================================================
/*
 *  STRATEGY: HKEX Dual Counter – same stock quoted simultaneously in HKD
 *  (primary) and USD (secondary). Maintain cross-counter price parity.
 *  Authorised intermediaries (AI) can convert between counters at low cost.
 *
 *  FORMULAE:
 *    TV_HKD         = hkd_mid                  [primary anchor]
 *    TV_USD_parity  = TV_HKD / usdhkd_rate      [theoretical USD price]
 *    arb_signal_hkd = usd_mid * usdhkd_rate − hkd_mid  [+ = USD rich]
 *    arb_viable     = |arb_signal| / hkd_mid * 10000 > conversion_cost_bps
 *
 *    combined_inv_hkd = inv_hkd + inv_usd * usdhkd_rate  [single exposure]
 *    skew_bps = clamp(combined_inv_hkd / 10000, ±1) * inv_skew_scale_bps
 *
 *    HKD quotes:
 *      bid_hkd = floor_tick(TV_HKD * (1 − (half + skew) * BPS), tick_hkd)
 *      ask_hkd = ceil_tick (TV_HKD * (1 + (half − skew) * BPS), tick_hkd)
 *    USD quotes:
 *      bid_usd = floor_tick(TV_USD_parity * (1 − (half + skew) * BPS), tick_usd)
 *      ask_usd = ceil_tick (TV_USD_parity * (1 + (half − skew) * BPS), tick_usd)
 *
 *  KEY PARAMETERS:
 *    usdhkd_rate       – FX rate (HKD peg band: 7.75–7.85, typically 7.825)
 *    conversion_cost_bps – round-trip AI conversion fee (≈ 3–6 bps)
 *    peg_risk_bps      – extra spread for HKD peg band risk (usually < 1 bps)
 *    tvr               – traded value ratio USD/HKD (monitors cross-counter
 *                        migration; high TVR = USD counter gaining liquidity)
 *    lot_size_hkd/usd  – different board lot sizes per counter (e.g. 100 HKD, 10 USD)
 */
struct DualCounterMMParams {
    double hkd_mid{};          // HKD counter L1 mid
    double usd_mid{};          // USD counter L1 mid
    double usdhkd_rate{7.825};
    double realized_vol{};
    double base_spread_bps{5};
    double vol_scale{0.5};
    double conversion_cost_bps{4}; // AI FX conversion cost
    double peg_risk_bps{0.5};
    double inventory_hkd{};    // net shares on HKD counter
    double inventory_usd{};    // net shares on USD counter
    double inv_skew_scale_bps{8};
    double tick_hkd{0.01};
    double tick_usd{0.001};
    int    lot_size_hkd{100};
    int    lot_size_usd{10};
};

struct DualCounterQuote {
    double tv_hkd{}, tv_usd_parity{};
    double arb_signal_hkd{}, arb_bps{};
    bool   arb_viable{};
    double bid_hkd{}, ask_hkd{};
    double bid_usd{}, ask_usd{};
    double skew_bps{}, half_spread_bps{};
};

static DualCounterQuote dual_counter_quote(const DualCounterMMParams& p) {
    DualCounterQuote r;
    r.tv_hkd        = p.hkd_mid;
    r.tv_usd_parity = r.tv_hkd / p.usdhkd_rate;
    r.arb_signal_hkd = p.usd_mid * p.usdhkd_rate - p.hkd_mid;
    r.arb_bps       = r.arb_signal_hkd / p.hkd_mid * 10000.0;
    r.arb_viable    = std::fabs(r.arb_bps) > p.conversion_cost_bps;
    double daily_v  = p.realized_vol / SQRT252;
    double half     = std::max(p.base_spread_bps, daily_v * p.vol_scale * 10000.0)
                    + p.peg_risk_bps;
    double combined_inv = p.inventory_hkd + p.inventory_usd * p.usdhkd_rate;
    r.skew_bps = std::clamp(combined_inv / 10000.0, -1.0, 1.0)
               * p.inv_skew_scale_bps;
    r.half_spread_bps = half;
    r.bid_hkd = floor_tick(r.tv_hkd * (1.0-(half+r.skew_bps)*BPS), p.tick_hkd);
    r.ask_hkd = ceil_tick (r.tv_hkd * (1.0+(half-r.skew_bps)*BPS), p.tick_hkd);
    r.bid_usd = floor_tick(r.tv_usd_parity*(1.0-(half+r.skew_bps)*BPS), p.tick_usd);
    r.ask_usd = ceil_tick (r.tv_usd_parity*(1.0+(half-r.skew_bps)*BPS), p.tick_usd);
    return r;
}

void demo_dual_counter_mm() {
    section("10. DUAL COUNTER MARKET MAKING (HKEX)");

    DualCounterMMParams p;
    p.hkd_mid=88.90; p.usd_mid=11.38; p.usdhkd_rate=7.825;
    p.realized_vol=0.25; p.base_spread_bps=5; p.vol_scale=0.5;
    p.conversion_cost_bps=4; p.peg_risk_bps=0.5;
    p.inventory_hkd=0; p.inventory_usd=0;
    p.inv_skew_scale_bps=8; p.tick_hkd=0.01; p.tick_usd=0.001;

    auto show = [](const char* lbl, const DualCounterMMParams& pm) {
        auto q = dual_counter_quote(pm);
        sub(lbl);
        std::cout << std::fixed << std::setprecision(3)
            << "  TV HKD           = " << q.tv_hkd << "\n"
            << "  TV USD (parity)  = " << q.tv_usd_parity << "\n"
            << "  Arb signal       = " << q.arb_signal_hkd
            << " HKD (" << std::setprecision(2) << q.arb_bps << " bps)  "
            << (q.arb_viable ? "VIABLE" : "not viable") << "\n"
            << "  Half spread      = " << q.half_spread_bps << " bps\n"
            << "  Inv skew         = " << q.skew_bps << " bps\n"
            << "  HKD: BID=" << std::setprecision(3) << q.bid_hkd
            << "  ASK=" << q.ask_hkd << "\n"
            << "  USD: BID=" << std::setprecision(4) << q.bid_usd
            << "  ASK=" << q.ask_usd << "\n";
        if (q.arb_viable) {
            if (q.arb_bps > 0) std::cout << "  Action: BUY USD counter + SELL HKD counter\n";
            else                std::cout << "  Action: BUY HKD counter + SELL USD counter\n";
        }
    };

    show("Alibaba (9988 HK / 9988U HK) – flat inventory", p);

    p.usd_mid = 11.45; // USD counter 6 bps rich
    show("USD counter RICH vs HKD – arb signal", p);

    p.usd_mid = 11.38; p.inventory_hkd = 5000; p.inventory_usd = 200;
    show("Long combined inventory – skewed DOWN", p);
}

// =============================================================================
// SECTION 11: WARRANTS MARKET MAKING
// =============================================================================
/*
 *  STRATEGY: Quote structured call/put warrants issued by banks (HKEX/SGX/KRX).
 *  Delta-hedge with underlying stock. Earn vol premium + spread.
 *
 *  FORMULAE:
 *    TV_per_share    = BS(S, K, T, r, q, sigma)       [vanilla BS call/put]
 *    TV_per_warrant  = TV_per_share / conversion_ratio [per warrant]
 *    intrinsic       = max(0, S − K) / conv_ratio      [call]
 *    time_value      = TV_per_warrant − intrinsic
 *    gearing         = S / (TV_per_warrant * conv_ratio) [leverage ratio]
 *    delta_per_warr  = BS_delta / conv_ratio           [hedge per warrant]
 *    eff_gearing     = gearing * BS_delta              [true leverage Ω×Δ]
 *    premium_pct     = (K + TV_per_warrant*conv_ratio − S) / S * 100
 *
 *  QUOTING (issuer embeds vol premium on ask):
 *    vol_ask = sigma + vol_premium_pts                 [issuer charges extra vol]
 *    vol_bid = sigma                                   [tighter on bid side]
 *    TV_ask  = BS(S,K,T,r,q,vol_ask) / conv_ratio
 *    TV_bid  = BS(S,K,T,r,q,vol_bid) / conv_ratio
 *    gamma_spread_$ = gamma * S^2 * 0.01 * gamma_scale
 *    half_spread = max(base_spread * TV, gamma_spread)
 *    Bid = floor_tick(TV_bid − half_spread, min_tick)
 *    Ask = ceil_tick (TV_ask + half_spread, min_tick)
 *
 *  KEY PARAMETERS:
 *    conversion_ratio – warrants per underlying share (e.g. 10 = 0.1 share right)
 *    vol_premium_pts  – issuer's embedded vol markup on ask (1–3 vol pts)
 *    funding_cost_bps – issuer's hedging/funding cost in spread
 *    gearing          – higher gearing = more leverage = wider spread needed
 *    effective_gearing (Ω×Δ) – actual sensitivity to underlying move
 *    premium_pct      – how far out-of-the-money + time value as % of spot
 *    is_american      – if true, apply early exercise premium (BAW approx)
 */
struct WarrantMMParams {
    double S{}, K{}, T{}, r{}, q{}, sigma{};
    int    cp{1};               // +1 call, -1 put
    double conversion_ratio{10};// warrants per share (10:1 = 0.1 share per warrant)
    double vol_premium_pts{1.5};// issuer vol markup on ask side (bps of vol)
    double funding_cost_bps{10};// issuer's hedging/funding cost in bps
    double base_spread_bps{8};
    double gamma_scale{2.0};
    double inventory_warrants{};
    double inv_skew_scale_bps{10};
    double min_tick{0.001};    // HKEX min warrant tick
    bool   is_american{false};
};

struct WarrantQuote {
    double tv_per_warrant{}, tv_bid{}, tv_ask{};
    double intrinsic{}, time_value{};
    double gearing{}, delta_per_warrant{}, eff_gearing{};
    double premium_pct{};
    double bid{}, ask{};
    double half_spread{};
    BSGreeks greeks{};
};

static WarrantQuote warrant_mm_quote(const WarrantMMParams& p) {
    WarrantQuote r;
    r.greeks          = BlackScholes::greeks(p.S, p.K, p.T, p.r, p.q, p.sigma, p.cp);
    r.tv_per_warrant  = r.greeks.price / p.conversion_ratio;
    double intrinsic_share = std::max(0.0, p.cp*(p.S - p.K));
    r.intrinsic       = intrinsic_share / p.conversion_ratio;
    r.time_value      = r.tv_per_warrant - r.intrinsic;
    r.gearing         = (r.tv_per_warrant > 0)
                      ? p.S / (r.tv_per_warrant * p.conversion_ratio) : 0;
    r.delta_per_warrant = r.greeks.delta / p.conversion_ratio;
    r.eff_gearing     = r.gearing * std::fabs(r.greeks.delta);
    r.premium_pct     = (p.cp == 1)
        ? (p.K + r.tv_per_warrant * p.conversion_ratio - p.S) / p.S * 100.0
        : (p.S - p.K + r.tv_per_warrant * p.conversion_ratio) / p.S * 100.0;

    // Ask side: vol + premium
    double vol_ask = p.sigma + p.vol_premium_pts / 100.0;
    auto g_ask = BlackScholes::greeks(p.S, p.K, p.T, p.r, p.q, vol_ask, p.cp);
    r.tv_ask   = g_ask.price / p.conversion_ratio;
    r.tv_bid   = r.tv_per_warrant; // bid at fair vol

    double gamma_spread = r.greeks.gamma * p.S * p.S * 0.01 * p.gamma_scale
                        / p.conversion_ratio;
    double funding      = p.funding_cost_bps * BPS * r.tv_per_warrant;
    double base         = p.base_spread_bps * BPS * r.tv_per_warrant;
    r.half_spread = std::max(base, gamma_spread + funding);

    double inv_f = std::clamp(p.inventory_warrants / 100000.0, -1.0, 1.0);
    double skew  = inv_f * p.inv_skew_scale_bps * BPS * r.tv_per_warrant;

    r.bid = floor_tick(r.tv_bid - r.half_spread - skew, p.min_tick);
    r.ask = ceil_tick (r.tv_ask + r.half_spread - skew, p.min_tick);
    r.bid = std::max(r.bid, p.min_tick);
    return r;
}

void demo_warrants_mm() {
    section("11. WARRANTS MARKET MAKING");

    WarrantMMParams p;
    // Tencent (700 HK) call warrant
    p.S=365.0; p.K=380.0; p.T=90.0/365; p.r=0.04; p.q=0.02;
    p.sigma=0.30; p.cp=1;
    p.conversion_ratio=10; p.vol_premium_pts=1.5;
    p.funding_cost_bps=10; p.base_spread_bps=8;
    p.gamma_scale=2.0; p.min_tick=0.001; p.inventory_warrants=0;

    auto show = [](const char* lbl, const WarrantMMParams& pm) {
        auto q = warrant_mm_quote(pm);
        sub(lbl);
        std::cout << std::fixed << std::setprecision(4)
            << "  BS TV per share  = " << q.greeks.price << "\n"
            << "  TV per warrant   = " << q.tv_per_warrant << "  (conv ratio "
            << pm.conversion_ratio << ":1)\n"
            << "  Intrinsic/warr   = " << q.intrinsic << "\n"
            << "  Time value/warr  = " << q.time_value << "\n"
            << "  Gearing          = " << q.gearing << "x\n"
            << "  Delta per warr   = " << q.delta_per_warrant << "\n"
            << "  Effective gearing= " << q.eff_gearing << "x  (Ω×Δ)\n"
            << "  Premium %        = " << q.premium_pct << "%\n"
            << "  Vol bid / ask    = " << pm.sigma*100 << "% / "
            << (pm.sigma + pm.vol_premium_pts/100)*100 << "%\n"
            << "  Half spread      = " << q.half_spread << "\n"
            << "  BID = " << q.bid << "   ASK = " << q.ask << "\n";
    };

    show("Tencent call warrant – OTM K=380, flat inventory", p);

    p.K = 365.0; // ATM
    show("Tencent call warrant – ATM K=365", p);

    p.cp = -1; p.K = 350.0; // put warrant, OTM
    show("Tencent put warrant – OTM K=350", p);

    // long inventory – skew DOWN
    p.cp=1; p.K=380; p.inventory_warrants=50000;
    show("Tencent call OTM – long 50,000 warrants (skew DOWN)", p);
}

// =============================================================================
// SUMMARY TABLE
// =============================================================================
void summary_table() {
    section("CROSS-ASSET MARKET MAKING REFERENCE TABLE");

    std::cout << std::left
        << std::setw(22) << "Asset Class"
        << std::setw(22) << "TV Anchor"
        << std::setw(20) << "Pricing Model"
        << std::setw(20) << "Primary Spread Driver"
        << std::setw(18) << "Hedge Instrument"
        << "Key Risk\n"
        << std::string(115,'-') << "\n"
        << std::setw(22) << "Single Stock"
        << std::setw(22) << "L1 mid"
        << std::setw(20) << "Mid ± spread"
        << std::setw(20) << "Realised vol, ADV"
        << std::setw(18) << "Same stock / SSF"
        << "Inventory, borrow\n"

        << std::setw(22) << "SS Futures"
        << std::setw(22) << "F_fair=S*e^(r-q)T"
        << std::setw(20) << "Cost-of-carry"
        << std::setw(20) << "Div risk, vol"
        << std::setw(18) << "Spot, swap"
        << "Div forecast, roll\n"

        << std::setw(22) << "SS Options"
        << std::setw(22) << "BS(S,K,T,r,q,IV)"
        << std::setw(20) << "Black-Scholes"
        << std::setw(20) << "Vol spread, gamma"
        << std::setw(18) << "Stock (delta)"
        << "Pin risk, gap risk\n"

        << std::setw(22) << "ETF"
        << std::setw(22) << "iNAV (basket)"
        << std::setw(20) << "iNAV + basis adj"
        << std::setw(20) << "Basket illiquidity"
        << std::setw(18) << "Index future"
        << "Creation/redeem arb\n"

        << std::setw(22) << "Index Futures"
        << std::setw(22) << "F=Index*e^(r-d)T"
        << std::setw(20) << "Cost-of-carry"
        << std::setw(20) << "1 tick, vol"
        << std::setw(18) << "ETF / basket"
        << "Dividend, roll\n"

        << std::setw(22) << "Index Options"
        << std::setw(22) << "BS(Index,K,T,r,q)"
        << std::setw(20) << "Black-Scholes"
        << std::setw(20) << "Vol surface, skew"
        << std::setw(18) << "Index futures"
        << "Skew, term structure\n"

        << std::setw(22) << "FX Spot"
        << std::setw(22) << "LP composite mid"
        << std::setw(20) << "Mid ± spread"
        << std::setw(20) << "Vol, size tier"
        << std::setw(18) << "Spot / forward"
        << "Fixing, inventory\n"

        << std::setw(22) << "FX Futures"
        << std::setw(22) << "S*e^(rd-rf)T"
        << std::setw(20) << "Covered int parity"
        << std::setw(20) << "Rate diff, CIP basis"
        << std::setw(18) << "Spot + FX swap"
        << "CIP anomaly, USD\n"

        << std::setw(22) << "FX Options"
        << std::setw(22) << "GK(S,K,T,rd,rf,IV)"
        << std::setw(20) << "Garman-Kohlhagen"
        << std::setw(20) << "Vol smile, delta"
        << std::setw(18) << "Spot (delta hedge)"
        << "Vanna, volga risk\n"

        << std::setw(22) << "Dual Counter"
        << std::setw(22) << "HKD mid / FX parity"
        << std::setw(20) << "Parity + FX rate"
        << std::setw(20) << "Conversion cost"
        << std::setw(18) << "Both counters"
        << "HKD peg, cross arb\n"

        << std::setw(22) << "Warrants"
        << std::setw(22) << "BS / conv_ratio"
        << std::setw(20) << "Black-Scholes"
        << std::setw(20) << "Vol premium, gamma"
        << std::setw(18) << "Underlying stock"
        << "Gearing, gap risk\n";

    std::cout << "\n  UNIVERSAL INVENTORY SKEW RULE:\n"
              << "    Long inventory  → shift BOTH bid & ask DOWN (attract sellers)\n"
              << "    Short inventory → shift BOTH bid & ask UP   (attract buyers)\n"
              << "    Max skew        ≤ half_spread (never cross the market)\n\n"
              << "  SPREAD HIERARCHY  (tightest to widest):\n"
              << "    Index Futures (1 tick) < FX Spot (0.3 pip) < ETF (3 bps)\n"
              << "    < Single Stock (3-15 bps) < Options (vol spread)\n"
              << "    < Warrants (8-20 bps) < Dual Counter (5-10 bps)\n";
}

// ...existing summary_table code...

// =============================================================================
// ULTRA LOW LATENCY (ULL) TECHNIQUES FOR MARKET MAKING
// =============================================================================
/*
 *  TARGET LATENCY BUDGET — tick-to-quote (market data → new bid/ask out):
 *
 *    Hop                             Latency
 *    ─────────────────────────────   ──────────────
 *    NIC DMA → kernel bypass (RDMA)   50–200 ns
 *    Solarflare ef_vi / TCPDirect       50–150 ns
 *    Ring buffer enqueue (SPSC)          5–15  ns
 *    Price decode + normalise           10–30  ns
 *    iNAV / fair value recalc           20–80  ns  ← THIS FILE
 *    Quote generation (bid/ask)         10–30  ns  ← THIS FILE
 *    Ring buffer dequeue (SPSC)          5–15  ns
 *    Order gateway serialise            15–40  ns
 *    NIC transmit                       50–200 ns
 *    ─────────────────────────────   ──────────────
 *    Total (kernel bypass)            200–800 ns
 *
 *  TECHNIQUES APPLIED BELOW:
 *    1.  alignas(64) on all hot structs       → cache-line per struct, no false sharing
 *    2.  [[gnu::always_inline]] / FORCE_INLINE → eliminate call overhead on hot path
 *    3.  [[likely]] / [[unlikely]]            → branch predictor hints
 *    4.  constexpr pre-computed constants     → no runtime division/sqrt
 *    5.  Object pool (pre-allocated)          → zero heap alloc on hot path
 *    6.  SPSC ring buffer (lock-free)         → tick → pricer → quote pipeline
 *    7.  SOA (Structure of Arrays) iNAV      → SIMD-friendly batch recalc
 *    8.  Read-only params passed by ref       → no copy, stays in L1
 *    9.  Avoid virtual dispatch in hot path   → use templates / function pointers
 *   10.  Latency probe (RDTSC)               → measure cycle-accurate hot path
 */

#include <atomic>
#include <array>
#include <cassert>
#include <chrono>
#include <functional>

// ── compiler portability ─────────────────────────────────────────────────────
#if defined(_MSC_VER)
#  define FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#  define FORCE_INLINE __attribute__((always_inline)) inline
#else
#  define FORCE_INLINE inline
#endif

#if defined(__x86_64__) || defined(_M_X64)
#  include <x86intrin.h>
   static FORCE_INLINE uint64_t read_tsc() { return __rdtsc(); }
#else
   static FORCE_INLINE uint64_t read_tsc() {
       return static_cast<uint64_t>(
           std::chrono::high_resolution_clock::now().time_since_epoch().count());
   }
#endif

static constexpr int CACHE_LINE = 64;

// ─────────────────────────────────────────────────────────────────────────────
// 1. PRE-COMPUTED ULL CONSTANTS  (no runtime sqrt/division on hot path)
// ─────────────────────────────────────────────────────────────────────────────
/*
 *  All values computed at compile time. Pricer uses these directly — no
 *  std::sqrt(252) call on every tick.
 */
namespace ull_const {
    static constexpr double INV_SQRT252  = 1.0 / 15.874507866;  // 1/√252
    static constexpr double SQRT252_VAL  = 15.874507866;
    static constexpr double INV_365      = 1.0 / 365.0;
    static constexpr double INV_10000    = 1.0 / 10000.0;       // 1 bps
    static constexpr double LN2          = 0.693147180559945;
    static constexpr double SQRT2PI      = 2.506628274631001;
    static constexpr double INV_SQRT2PI  = 1.0 / 2.506628274631001;
    static constexpr double TWO_PI       = 6.283185307179586;
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. CACHE-LINE ALIGNED HOT STRUCTS
// ─────────────────────────────────────────────────────────────────────────────
/*
 *  Each struct fits in exactly one or two cache lines (64 bytes each).
 *  Members ordered: hot (frequently read) first, cold (rarely written) last.
 *  alignas(64) guarantees the struct starts on a cache-line boundary — no
 *  accidental sharing between two structs on the same cache line (false sharing).
 *
 *  Rule: sizeof(struct) should be ≤ 64 bytes (1 cache line) for structs
 *  that live in SPSC ring buffers or are passed between threads.
 */

// Tick (incoming market data) — fits in 1 cache line
struct alignas(CACHE_LINE) ULLTick {
    double   bid;           //  8 bytes
    double   ask;           //  8 bytes
    double   last;          //  8 bytes
    double   vol;           //  8 bytes  annualised realised vol
    uint64_t recv_tsc;      //  8 bytes  TSC at kernel-bypass receive
    uint32_t symbol_id;     //  4 bytes  integer symbol lookup (no string hash)
    uint16_t venue_id;      //  2 bytes
    uint8_t  asset_class;   //  1 byte   1=stock,2=ssf,3=opt,4=etf,5=idx_fut,7=fx
    uint8_t  flags;         //  1 byte   stale/halted/auction flags
    // Total = 48 bytes < 64 → fits in 1 cache line
};
static_assert(sizeof(ULLTick) <= 64, "ULLTick must fit in 1 cache line");

// Quote output — fits in 1 cache line
struct alignas(CACHE_LINE) ULLQuote {
    double   bid;           //  8 bytes
    double   ask;           //  8 bytes
    double   tv;            //  8 bytes
    uint64_t gen_tsc;       //  8 bytes  TSC when quote was generated
    uint64_t tick_tsc;      //  8 bytes  TSC of source tick (latency = gen-tick)
    uint32_t symbol_id;     //  4 bytes
    uint16_t strategy_id;   //  2 bytes
    uint8_t  asset_class;   //  1 byte   1=stock,2=ssf,3=opt,4=etf,5=idx_fut...
    uint8_t  flags;         //  1 byte   0=normal, 1=skewed, 2=stale, 3=halted
    // Total = 48 bytes < 64
};
static_assert(sizeof(ULLQuote) <= 64, "ULLQuote must fit in 1 cache line");

// Pricing parameters (read-only, loaded once, stays in L1/L2 during trading)
struct alignas(CACHE_LINE) ULLStockParams {
    double base_spread_bps; //  8
    double vol_scale;       //  8
    double max_inventory;   //  8
    double inv_skew_scale;  //  8
    double borrow_cost_bps; //  8
    double tick_size;       //  8
    double alpha_scale_bps; //  8
    double adv;             //  8
    // Total = 64 bytes = exactly 1 cache line
};
static_assert(sizeof(ULLStockParams) == 64, "ULLStockParams must be exactly 1 cache line");

// ─────────────────────────────────────────────────────────────────────────────
// 3. ALWAYS-INLINE ULL PRICERS  (hot path functions, never call-overhead)
// ─────────────────────────────────────────────────────────────────────────────
/*
 *  These replace the demo functions for production hot-path use.
 *  Key changes vs demo version:
 *    - No heap allocations (no std::string, no std::vector in return)
 *    - No virtual dispatch
 *    - Parameters by const-ref (stay in registers/L1)
 *    - Results written to pre-allocated output struct
 *    - [[likely]]/[[unlikely]] on fast/slow paths
 *    - constexpr constants (no runtime division)
 */

// Single-stock quote — hot path, ~20-40 ns
FORCE_INLINE void ull_stock_quote(
    const ULLTick&        tick,
    const ULLStockParams& p,
    double                inventory,
    ULLQuote&             out) noexcept
{
    out.tick_tsc = tick.recv_tsc;
    const double mid    = 0.5 * (tick.bid + tick.ask);
    const double daily_v = tick.vol * ull_const::INV_SQRT252;
    // [[likely]]: vol-based spread > floor most of the time
    const double vol_half = daily_v * p.vol_scale * 10000.0;
    const double half     = vol_half > p.base_spread_bps ? vol_half : p.base_spread_bps;
    // inventory skew: clamp to [-1, +1]
    const double inv_f  = (p.max_inventory > 0.0)
                        ? std::clamp(inventory / p.max_inventory, -1.0, 1.0)
                        : 0.0;
    const double skew   = inv_f * p.inv_skew_scale;
    out.tv  = mid;
    out.bid = std::floor((mid * (1.0 - (half + skew) * ull_const::INV_10000))
                         / p.tick_size) * p.tick_size;
    out.ask = std::ceil ((mid * (1.0 + (half - skew) * ull_const::INV_10000
                                     + p.borrow_cost_bps * ull_const::INV_10000))
                         / p.tick_size) * p.tick_size;
    out.asset_class = 1;
    out.gen_tsc = read_tsc();
}

// ETF iNAV recalc on single constituent update — O(1), ~15-25 ns
/*
 *  SOA iNAV layout: weights[], mids[], fx[] stored as flat arrays.
 *  When constituent k ticks, only ONE multiply needed:
 *    delta_inav = weights[k] * (new_mid[k] - old_mid[k]) * fx[k]
 *    inav += delta_inav
 *  No full basket recompute. Amortised O(1) per tick.
 */
struct alignas(CACHE_LINE) ULLiNAV {
    double value{};             // current iNAV
    double cash_component{};    // accrued dividends, cash in CU
    uint32_t n_constituents{};  // number of underliers
    uint32_t pad{};
    double   weight[12]{};      // weights (SOA, fits in ~2 cache lines for ≤12)
    double   mid[12]{};
    double   fx[12]{};
};

FORCE_INLINE void ull_inav_update(ULLiNAV& nav, uint32_t k,
                                   double new_mid) noexcept
{
    if (k >= nav.n_constituents) [[unlikely]] return;
    const double delta = nav.weight[k] * (new_mid - nav.mid[k]) * nav.fx[k];
    nav.mid[k]   = new_mid;
    nav.value   += delta;
}

FORCE_INLINE void ull_etf_quote(
    const ULLiNAV&  nav,
    double          future_basis,   // F_market_mid - F_fair (pts)
    double          beta,
    double          half_bps,
    double          inv_skew_bps,
    double          tick,
    ULLQuote&       out) noexcept
{
    const double tv = (nav.value + nav.cash_component) + beta * future_basis;
    out.tv  = tv;
    out.bid = std::floor(tv * (1.0 - (half_bps + inv_skew_bps)*ull_const::INV_10000) / tick) * tick;
    out.ask = std::ceil (tv * (1.0 + (half_bps - inv_skew_bps)*ull_const::INV_10000) / tick) * tick;
    out.asset_class = 4;
    out.gen_tsc = read_tsc();
}

// Index future quote — hot path, ~15-20 ns
FORCE_INLINE void ull_idx_future_quote(
    double spot, double fvb,            // fvb = pre-computed fair value basis
    double half_pts, double inv_skew_pts,
    double tick, ULLQuote& out) noexcept
{
    const double f_fair = spot + fvb;
    out.tv  = f_fair;
    out.bid = std::floor((f_fair - half_pts - inv_skew_pts) / tick) * tick;
    out.ask = std::ceil ((f_fair + half_pts - inv_skew_pts) / tick) * tick;
    out.asset_class = 5;
    out.gen_tsc = read_tsc();
}

// ULL Black-Scholes (inline, pre-computed d1/d2, avoids std::log on hot path)
FORCE_INLINE double ull_bs_call(double S, double K_inv, double T,
                                 double r, double q, double sigma) noexcept
{
    // K_inv = 1.0/K — pre-computed once, passed in to avoid division
    const double sqT   = std::sqrt(T);
    const double sig_sqT = sigma * sqT;
    const double d1    = (std::log(S * K_inv) + (r - q + 0.5*sigma*sigma)*T) / sig_sqT;
    const double d2    = d1 - sig_sqT;
    const double eq    = std::exp(-q*T);
    const double er    = std::exp(-r*T);
    return S*eq*norm_cdf(d1) - (1.0/K_inv)*er*norm_cdf(d2);
}

// FX spot quote — hot path, ~10-15 ns
FORCE_INLINE void ull_fx_spot_quote(
    double mid, double half_pip,        // half_pip pre-computed from vol + tier
    double inv_skew_pip, double pip,
    ULLQuote& out) noexcept
{
    out.tv  = mid;
    out.bid = std::floor((mid - (half_pip + inv_skew_pip)*pip) / pip) * pip;
    out.ask = std::ceil ((mid + (half_pip - inv_skew_pip)*pip) / pip) * pip;
    out.asset_class = 7;
    out.gen_tsc = read_tsc();
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. OBJECT POOL — zero heap allocation on hot path
// ─────────────────────────────────────────────────────────────────────────────
/*
 *  Pre-allocate a fixed pool of ULLQuote objects at startup.
 *  Hot path: acquire() returns pointer in O(1), no malloc/free.
 *  Pool is lock-free for single producer (pricer thread).
 *
 *  Sizing: pool_size = max_quotes_in_flight × 2 (double-buffer safety)
 *  Typical: 256 quotes in flight → pool = 512 entries × 48 bytes = 24 KB (fits L1/L2)
 */
template<typename T, size_t N>
class alignas(CACHE_LINE) ObjectPool {
    static_assert((N & (N-1)) == 0, "N must be power of 2");
    std::array<T, N> pool_{};
    alignas(CACHE_LINE) std::atomic<size_t> head_{0};

public:
    // Returns pointer to next slot (cycles through pool).
    // Single-producer safe. For MPMC: use fetch_add.
    [[nodiscard]] FORCE_INLINE T* acquire() noexcept {
        const size_t idx = head_.fetch_add(1, std::memory_order_relaxed) & (N-1);
        return &pool_[idx];
    }
    // No release needed: slots are reused cyclically (ring-pool pattern)
    // Caller must finish using T* before N more acquires() are made.
    constexpr size_t capacity() const noexcept { return N; }
};

// ─────────────────────────────────────────────────────────────────────────────
// 5. LOCK-FREE SPSC RING BUFFER — tick → pricer → quote pipeline
// ─────────────────────────────────────────────────────────────────────────────
/*
 *  SPSC = Single Producer Single Consumer.
 *  Producer: market data thread (writes ULLTick)
 *  Consumer: pricer thread (reads ULLTick, writes ULLQuote)
 *
 *  Memory ordering:
 *    push: store data → release-store tail (consumer sees complete tick)
 *    pop : acquire-load tail → read data   (compiler fence, no cache flush)
 *
 *  Latency: 5-15 ns (same core L1 cache), 20-50 ns (cross-core L3)
 *  Throughput: 50-200 M msgs/sec
 *
 *  TWO SEPARATE PIPELINES:
 *    tick_queue:  MD thread → pricer thread   (ULLTick)
 *    quote_queue: pricer thread → OMS thread  (ULLQuote)
 */
template<typename T, size_t N>
class alignas(CACHE_LINE) SPSCQueue {
    static_assert((N & (N-1)) == 0, "N must be power of 2");
    static constexpr size_t MASK = N - 1;

    alignas(CACHE_LINE) std::array<T, N> buf_{};
    alignas(CACHE_LINE) std::atomic<size_t> head_{0};  // consumer reads here
    alignas(CACHE_LINE) std::atomic<size_t> tail_{0};  // producer writes here

public:
    // Producer side — called from market data thread
    [[nodiscard]] FORCE_INLINE bool push(const T& item) noexcept {
        const size_t t = tail_.load(std::memory_order_relaxed);
        const size_t next = (t + 1) & MASK;
        if (next == head_.load(std::memory_order_acquire)) [[unlikely]]
            return false;  // full
        buf_[t] = item;
        tail_.store(next, std::memory_order_release); // release: consumer sees full item
        return true;
    }

    // Consumer side — called from pricer thread
    [[nodiscard]] FORCE_INLINE bool pop(T& out) noexcept {
        const size_t h = head_.load(std::memory_order_relaxed);
        if (h == tail_.load(std::memory_order_acquire)) [[unlikely]]
            return false;  // empty
        out = buf_[h];
        head_.store((h + 1) & MASK, std::memory_order_release);
        return true;
    }

    FORCE_INLINE bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }
    constexpr size_t capacity() const noexcept { return N; }
};

// ─────────────────────────────────────────────────────────────────────────────
// 6. LATENCY PROBE — cycle-accurate hot path measurement
// ─────────────────────────────────────────────────────────────────────────────
/*
 *  Use TSC timestamps embedded in ULLTick (recv_tsc) and ULLQuote (gen_tsc).
 *  latency_ns = (gen_tsc - recv_tsc) / tsc_freq_ghz
 *
 *  Typical TSC frequency: 3.0-4.0 GHz (read from /proc/cpuinfo or CPUID).
 *  Use rdtsc for sub-nanosecond precision — gettimeofday is too slow (200 ns).
 */
struct LatencyStats {
    uint64_t min_cycles{UINT64_MAX};
    uint64_t max_cycles{};
    uint64_t sum_cycles{};
    uint64_t count{};

    FORCE_INLINE void record(uint64_t cycles) noexcept {
        if (cycles < min_cycles) [[likely]]  min_cycles = cycles;
        if (cycles > max_cycles) [[unlikely]] max_cycles = cycles;
        sum_cycles += cycles;
        ++count;
    }

    void print(double tsc_ghz = 3.5) const {
        if (!count) return;
        double f = 1000.0 / tsc_ghz;  // cycles → ns
        std::cout << std::fixed << std::setprecision(1)
            << "  Latency (tick→quote): "
            << "min=" << min_cycles*f/1000 << " µs  "
            << "avg=" << (sum_cycles/count)*f/1000 << " µs  "
            << "max=" << max_cycles*f/1000 << " µs  "
            << "n="   << count << "\n";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// 7. COMPLETE ULL TICK-TO-QUOTE PIPELINE DEMO
// ─────────────────────────────────────────────────────────────────────────────
/*
 *  Simulates the hot path:
 *    1. Market data arrives (ULLTick, TSC stamped at receive)
 *    2. Pushed onto SPSC tick queue (lock-free, ~5 ns)
 *    3. Pricer consumes tick, recalculates TV, generates bid/ask (ULLQuote)
 *    4. Quote pushed onto SPSC quote queue (lock-free, ~5 ns)
 *    5. OMS consumes quote, sends to exchange
 *  Measures tick→quote latency in CPU cycles.
 *
 *  ASSET CLASS DISPATCH:
 *    Use integer asset_class field + jump table (function pointer array)
 *    instead of virtual dispatch. Jump table: O(1), no vtable lookup,
 *    branch predictor learns the pattern.
 */

// Global queues and pools (static = no heap alloc)
static SPSCQueue<ULLTick,  256> g_tick_queue;
static SPSCQueue<ULLQuote, 256> g_quote_queue;
static ObjectPool<ULLQuote, 512> g_quote_pool;

// Pre-loaded params (one per symbol, loaded at startup from config)
static ULLStockParams g_stock_params = {
    .base_spread_bps = 3.0,
    .vol_scale       = 0.5,
    .max_inventory   = 10000.0,
    .inv_skew_scale  = 10.0,
    .borrow_cost_bps = 0.0,
    .tick_size       = 0.01,
    .alpha_scale_bps = 2.0,
    .adv             = 5e6,
};

// Pricer function type (no virtual, no inheritance — function pointer array)
using PricerFn = void(*)(const ULLTick&, ULLQuote&) noexcept;

static void price_stock(const ULLTick& t, ULLQuote& q) noexcept {
    static double inventory = 0.0;  // per-symbol state (in real system: per-symbol map)
    ull_stock_quote(t, g_stock_params, inventory, q);
}
static void price_etf(const ULLTick& t, ULLQuote& q) noexcept {
    // Simplified: use tick mid as iNAV proxy for demo
    const double half  = 47.0;  // bps (pre-computed from vol)
    const double skew  = 0.0;
    const double tick  = 0.01;
    const double tv    = 0.5*(t.bid + t.ask);
    q.tv  = tv;
    q.bid = std::floor(tv*(1.0-(half+skew)*ull_const::INV_10000)/tick)*tick;
    q.ask = std::ceil (tv*(1.0+(half-skew)*ull_const::INV_10000)/tick)*tick;
    q.asset_class = 4; q.gen_tsc = read_tsc();
}
static void price_idx_future(const ULLTick& t, ULLQuote& q) noexcept {
    const double fvb  = 49.38;   // pre-computed fair value basis (pts)
    const double half = 0.25;    // 1 tick for ES
    ull_idx_future_quote(0.5*(t.bid+t.ask), fvb, half, 0.0, 0.25, q);
}
static void price_fx_spot(const ULLTick& t, ULLQuote& q) noexcept {
    ull_fx_spot_quote(0.5*(t.bid+t.ask), 0.44, 0.0, 0.00001, q);
}

// Jump table: asset_class → pricer function (O(1), no virtual dispatch)
static const PricerFn g_pricer_table[8] = {
    nullptr,            // 0 = unused
    price_stock,        // 1 = single stock
    price_stock,        // 2 = SSF (reuse for demo)
    price_stock,        // 3 = options (reuse for demo)
    price_etf,          // 4 = ETF
    price_idx_future,   // 5 = index future
    price_stock,        // 6 = index options
    price_fx_spot,      // 7 = FX spot
};

// Pricer hot loop — runs on dedicated pinned core
// In production: while(running) { if(tick_queue.pop(t)) process(t); }
// Here: single-pass demo for latency measurement
static void ull_pricer_loop(const std::vector<ULLTick>& ticks,
                             LatencyStats& stats)
{
    for (const auto& tick : ticks) {
        // ── hot path start ────────────────────────────────────────────
        ULLQuote* q_ptr = g_quote_pool.acquire();   // O(1), no malloc
        q_ptr->tick_tsc = tick.recv_tsc;
        q_ptr->symbol_id = tick.symbol_id;

        const uint8_t ac = tick.asset_class;
        if (ac > 0 && ac < 8 && g_pricer_table[ac]) [[likely]]
            g_pricer_table[ac](tick, *q_ptr);       // jump table, no virtual

        const uint64_t lat = q_ptr->gen_tsc - tick.recv_tsc;
        stats.record(lat);
        (void)g_quote_queue.push(*q_ptr);                 // SPSC push, ~5 ns (queue won't overflow in demo)
        // ── hot path end ──────────────────────────────────────────────
    }
}

void demo_ull_pipeline() {
    section("ULL: TICK-TO-QUOTE PIPELINE BENCHMARK");

    // Build test ticks (5 asset classes)
    std::vector<ULLTick> ticks;
    ticks.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        ULLTick t{};
        t.bid       = 174.99 + 0.001*(i%100);
        t.ask       = 175.01 + 0.001*(i%100);
        t.vol       = 0.20;
        t.recv_tsc  = read_tsc();
        t.symbol_id = i % 5;
        t.asset_class = (i % 5) + 1;  // 1=stock,2=SSF,3=opt,4=ETF,5=idx_fut
        ticks.push_back(t);
    }

    LatencyStats stats;
    ull_pricer_loop(ticks, stats);

    sub("Pipeline latency (tick → bid/ask generated)");
    stats.print(3.5);  // assume 3.5 GHz TSC

    sub("ULL Technique Checklist");
    std::cout
        << "  [OK] alignas(64) on ULLTick, ULLQuote, ULLStockParams, SPSCQueue\n"
        << "  [OK] FORCE_INLINE on ull_stock_quote, ull_etf_quote, ull_inav_update\n"
        << "  [OK] [[likely]]/[[unlikely]] branch hints on hot/cold paths\n"
        << "  [OK] constexpr INV_SQRT252, INV_10000 – no runtime division\n"
        << "  [OK] ObjectPool<ULLQuote,512> – zero heap alloc on hot path\n"
        << "  [OK] SPSCQueue<ULLTick,256>  – lock-free, acquire/release ordering\n"
        << "  [OK] Jump table (PricerFn[8]) – no virtual dispatch, O(1) dispatch\n"
        << "  [OK] SOA iNAV (ULLiNAV::weight[]/mid[]/fx[]) – O(1) delta update\n"
        << "  [OK] read_tsc() – cycle-accurate latency, not gettimeofday()\n"
        << "  [OK] No std::string, no std::vector on hot path structs\n"
        << "  [OK] Static queues/pools – no heap, resident in L2/L3\n";

    sub("Hot Path Latency Budget (per asset class)");
    std::cout
        << "  Single Stock quote:   ~20-40  ns  (vol calc + tick rounding)\n"
        << "  ETF quote:            ~15-25  ns  (O(1) iNAV delta + spread)\n"
        << "  Index Future quote:   ~10-20  ns  (precomputed FVB + 1 tick)\n"
        << "  FX Spot quote:        ~8-15   ns  (mid + pip floor/ceil)\n"
        << "  Options quote (BS):   ~80-150 ns  (exp/log/sqrt in BS)\n"
        << "  SPSC push/pop:        ~5-15   ns  (same-core L1)\n"
        << "  Object pool acquire:  ~3-5    ns  (atomic fetch_add)\n"
        << "  Total tick→quote:     ~50-300 ns  (kernel bypass path)\n";

    sub("Production Deployment Checklist");
    std::cout
        << "  [ ] CPU affinity: taskset / pthread_setaffinity_np to isolated core\n"
        << "  [ ] Disable SMT (hyper-threading): echo off > /sys/devices/.../smt/control\n"
        << "  [ ] NUMA: allocate memory on same NUMA node as NIC\n"
        << "  [ ] Huge pages: mmap(MAP_HUGETLB) for ring buffers (no TLB misses)\n"
        << "  [ ] IRQ affinity: pin NIC IRQ to separate core (not pricer core)\n"
        << "  [ ] Kernel bypass: Solarflare ef_vi or TCPDirect (skip kernel stack)\n"
        << "  [ ] Disable C-states: /dev/cpu_dma_latency = 0\n"
        << "  [ ] Compile: -O3 -march=native -DNDEBUG -fno-exceptions\n"
        << "  [ ] Profile: perf stat -e cache-misses,branch-misses,cycles\n";
}

// =============================================================================
// MAIN
// =============================================================================
int main() {
    std::cout << std::string(72,'=') << "\n"
              << "  COMPREHENSIVE MARKET MAKING STRATEGIES & PRICING CALCULATIONS\n"
              << "  Asset Classes: Single Stock, SSF, Options, ETF, Index Futures,\n"
              << "  Index Options, FX Spot, FX Futures, FX Options, Dual Counter,\n"
              << "  Warrants\n"
              << std::string(72,'=') << "\n";

    demo_single_stock_mm();
    demo_ssf_mm();
    demo_ss_options_mm();
    demo_etf_mm();
    demo_idx_futures_mm();
    demo_idx_options_mm();
    demo_fx_spot_mm();
    demo_fx_futures_mm();
    demo_fx_options_mm();
    demo_dual_counter_mm();
    demo_warrants_mm();
    summary_table();
    demo_ull_pipeline();   // ← NEW: ULL hot path demo

    return 0;
}



