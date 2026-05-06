#ifndef SIGNAL_FILTERING_H
#define SIGNAL_FILTERING_H
#include <vector>

class Regression {
    private: 
        // ================= CONFIG =================
        // #define WINDOW_SIZE 12 
        int window_size;
        double fs;                 // sample rate Hz
        double dt;

        // ================= STORAGE =================
        // double* buffer;
        std::vector<double> buffer;

        // double buffer[WINDOW_SIZE];   // volume samples
        int idx;                  // circular write index
        bool buffer_full;

        // ================= PRECOMPUTED CONSTANTS =================
        // for t = [0, dt, 2dt, ... (N-1)dt]
        double S_t;
        double S_tt;

        // ================= STATE =================
        double S_v;   // sum(v)
        double S_tv;   // sum(t*v)

    public:
        Regression();
        void init(double sample_rate, int inp_window_size);

        double update_regression(double v_new);
        double update_regression_O_N(double v_new);

};

#endif 
