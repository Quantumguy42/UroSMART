#include "SignalFiltering.h"

Regression::Regression() {}

void Regression::init(double sample_rate, int inp_window_size){
    // ================= CONFIG =================
    window_size = inp_window_size;
    fs = sample_rate;                 // sample rate Hz
    dt = 1.0 / fs;

    // ================= STORAGE =================
    // Init buffer with zeros
    buffer.resize(inp_window_size, 0.0); 
    // buffer = (double*)calloc(window_size, sizeof(double));
    // for (int i = 0; i < window_size; i++) buffer[i] = 0.0;
    idx = 0;                  // circular write index
    buffer_full = false;

    // ================= PRECOMPUTED CONSTANTS =================
    // for t = [0, dt, 2dt, ... (N-1)dt]
    S_t  = dt * (window_size * (window_size - 1) / 2.0);
    S_tt = dt*dt * ((window_size - 1) * window_size * (2*window_size - 1) / 6.0);

    // ================= STATE =================
    S_v  = 0.0;   // sum(v)
    S_tv = 0.0;   // sum(t*v)

}

double Regression::update_regression(double v_new) {

    double v_old = buffer[idx];

    // store new
    buffer[idx] = v_new;

    if (!buffer_full) {
        // ----------- filling phase -----------
        int k = idx;           // position in time order
        double t_new = k * dt;

        S_v  += v_new;
        S_tv += t_new * v_new;

        idx++;
        if (idx == window_size) {
            idx = 0;
            buffer_full = true;
        }

        return 0.0; // not enough data yet
    }

    // ----------- steady-state sliding window -----------

    // Remove old sample contribution
    S_v  -= v_old;
    S_tv -= 0.0 * v_old;   // old sample was at t=0

    // Shift time basis for all existing samples:
    // t_i -> t_i - dt
    S_tv -= dt * S_v;

    // Add new sample at t = (N-1)dt
    double t_new = (window_size - 1) * dt;
    S_v  += v_new;
    S_tv += t_new * v_new;

    // advance circular index
    idx = (idx + 1) % window_size;

    // ----------- slope -----------
    double denom = (window_size * S_tt - S_t * S_t);
    if (denom == 0) return 0.0;

    double m = (window_size * S_tv - S_t * S_v) / denom;
    return m;
}

double Regression::update_regression_O_N(double v_new) {

    // Old value being overwritten
    double v_old = buffer[idx];

    // Store new value
    buffer[idx] = v_new;

    // Update sum(v)
    if (buffer_full) S_v -= v_old;
    S_v += v_new;

    // ---------- Recompute S_tv safely ----------
    // Because time mapping changes with idx,
    // S_tv must be recomputed with correct time ordering
    S_tv = 0.0;

    int N = buffer_full ? window_size : idx + 1;

    for (int i = 0; i < N; i++) {
        // circular index → chronological time
        int bi = (idx + 1 + i) % window_size;  
        double ti = i * dt;                  // 0 .. (N-1)dt
        S_tv += ti * buffer[bi];
    }

    // Advance index
    idx = (idx + 1) % window_size;
    if (idx == 0) buffer_full = true;

    // Not enough samples yet
    if (!buffer_full) return 0.0;

    // ---------- Regression slope ----------
    double denom = (window_size * S_tt - S_t * S_t);
    if (denom == 0) return 0.0;

    double m = (window_size * S_tv - S_t * S_v) / denom;

    return m;   // flow (volume units per second)
}
