/** @file TactileProcessor.cpp
 *  @brief  A real-time feature calculation utility for the ATI/Accelerometer sensor

 *  @author Michele Pompilio
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <fftw3.h>
#include "iir/Butterworth.h"
#include "WindowProcessor.hpp"

static const char TactileFeatureProcessorVersion[]="0.1";

/*
#ifdef __cplusplus
extern "C"
{
#endif
*/
//Unmangled interface for the code!
int tactile_add_force(unsigned long timestamp, double fX , double fY, double fZ);
int tactile_add_acc(unsigned long timestamp, double accX , double accY, double accZ);

int tactile_write_disk(FILE* FrFD, FILE* AsFD, FILE* ApsdFD , FILE* FpsdFD);
int tactile_write_shared_memory(void* mem, unsigned int mem_size,unsigned int window_elements);
/*
#ifdef __cplusplus
}
#endif
*/

// Classe per calcolare media e deviazione standard in modo incrementale
class IncrementalStats {
    private:
        double mean;
        double variance;
        size_t count;

    public:
        IncrementalStats() : mean(0.0), variance(0.0), count(0) {}

        void update(double new_value) {
            count++;
            if (count == 1) {
                mean = new_value;
                variance = 0.0;
            } else {
                double delta = new_value - mean;
                mean += delta / count;
                variance += delta * (new_value - mean);  // Welford’s method
            }
        }

        double getMean() const { return mean; }
        double getStdDev() const { return (count > 1) ? std::sqrt(variance / count) : 0.0; }
};

class TactileFeaturesProcessor{
    const int   F_INPUT_FS, F_TARGET_FS, F_DECIMATION_FACTOR,
                A_INPUT_FS, A_TARGET_FS, A_DECIMATION_FACTOR;

    const size_t    Fr_WINDOW_SIZE, Fr_OVERLAP_WND,
                    As_WINDOW_SIZE, As_OVERLAP_WND,
                    Apsd_WINDOW_SIZE, Apsd_OVERLAP_WND,
                    Fpsd_WINDOW_SIZE, Fpsd_OVERLAP_WND;

    const double    Fr_HIGH_CUT_F, Fr_LOW_CUT_F,
                    As_HIGH_CUT_F,
                    Apsd_LOW_CUT_F,
                    Fpsd_LOW_CUT_F;

    const std::string   Fr_output_file,
                        As_output_file,
                        Apsd_output_file,
                        Fpsd_output_file;


    mutable std::mutex fft_mutex_;

    // Coefficienti FIR passa-basso (500 Hz, ordine 21)
    const std::vector<double> FIR_COEFFS = {
        -0.0016, -0.0042, -0.0094, -0.0172, -0.0268, -0.0367, -0.0447, -0.0482, -0.0452,
        -0.0345, -0.0169, 0.0064, 0.0324, 0.0578, 0.0789, 0.0923, 0.0953, 0.0865, 0.0657,
        0.0340, -0.0043, -0.0448
    };

    // **Filtro FIR passa-basso anti-aliasing**
    std::vector<double> applyFIRFilter(const std::vector<double>& signal) {
        size_t N = signal.size();
        std::vector<double> output(N, 0.0);
        size_t filter_order = FIR_COEFFS.size();

        for (size_t i = 0; i < N; i++) {
            for (size_t j = 0; j < filter_order; j++) {
                if (i >= j) output[i] += FIR_COEFFS[j] * signal[i - j];
            }
        }
        return output;
    }

    // **Decimazione del segnale**
    std::vector<double> downsample(const std::vector<double>& signal, int decimation_factor) {
        if (decimation_factor <= 1) return signal;

        std::vector<double> downsampled;
        for (size_t i = 0; i < signal.size(); i += decimation_factor) {
            downsampled.push_back(signal[i]);
        }
        return downsampled;
    }

    // **Filtro Butterworth a fase zero (passa-basso o passa-alto)**
    void applyZeroPhaseButterworth(std::vector<double>& signal, bool lowpass, double cutoff_freq, float target_fs) {
        Iir::Butterworth::LowPass<4> lp_filter;
        Iir::Butterworth::HighPass<4> hp_filter;
        if (lowpass) lp_filter.setup(target_fs, cutoff_freq);
        else hp_filter.setup(target_fs, cutoff_freq);

        // Passaggio avanti
        for (double& sample : signal) sample = lowpass ? lp_filter.filter(sample) : hp_filter.filter(sample);

        // Reset filtro e passaggio indietro
        if (lowpass) lp_filter.reset();
        else hp_filter.reset();

        for (int i = signal.size() - 1; i >= 0; i--) {
            signal[i] = lowpass ? lp_filter.filter(signal[i]) : hp_filter.filter(signal[i]);
        }
    }

    // Funzione per il calcolo della media mobile (SMA)
    std::vector<double> moving_average(const std::vector<double>& data, size_t window_size) {
        std::vector<double> sma(data.size(), 0.0);
        double sum = 0.0;

        for (size_t i = 0; i < data.size(); ++i) {
            sum += data[i];
            if (i >= window_size) {
                sum -= data[i - window_size];
            }
            sma[i] = (i >= window_size - 1) ? sum / window_size : sum / (i + 1);
        }

        return sma;
    }

    void applyHannWindow(std::vector<double>& window) {
        int N = window.size();
        for (int i = 0; i < N; i++) {
            window[i] *= 0.5 * (1 - cos(2 * M_PI * i / (N - 1)));  // Finestra di Hann
        }
    }


    // Funzione di elaborazione compatibile con segnali mono/multi-canale
    std::vector<DataPoint> processFriction(const std::vector<DataPoint>& window) {
        size_t win_size = window.size();
        size_t num_channels = window[0].numChannels();

        // Estrarre i dati per ciascun canale
        std::vector<std::vector<double>> channels(num_channels, std::vector<double>(win_size));
        for (size_t i = 0; i < win_size; i++) {
            for (size_t j = 0; j < num_channels; j++) {
                channels[j][i] = window[i].values[j];
            }
        }

        // Filtraggio e decimazione per ogni canale
        for (size_t j = 0; j < num_channels; j++) {
            channels[j] = applyFIRFilter(channels[j]);
            applyZeroPhaseButterworth(channels[j], true, Fr_HIGH_CUT_F, F_TARGET_FS);
            applyZeroPhaseButterworth(channels[j], false, Fr_LOW_CUT_F, F_TARGET_FS);
            channels[j] = downsample(channels[j], F_DECIMATION_FACTOR);
        }

        // Creazione della finestra elaborata decimata
        size_t new_size = channels[0].size();
        std::vector<DataPoint> processed_window(new_size, DataPoint(0, num_channels));
        for (size_t i = 0; i < new_size; i++) {
            for (size_t j = 0; j < num_channels; j++) {
                processed_window[i].values[j] = channels[j][i];
            }
        }

        return processed_window;
    }

    // Callback per mostrare i risultati
    void FrResult(const std::vector<DataPoint>& result, WindowProcessor& processor) {
        std::vector<DataPoint> tmp = std::vector<DataPoint>(result.begin(), result.begin() + processor.step / F_DECIMATION_FACTOR);
        std::vector<DataPoint> friction;
        for (const auto& dp : tmp) {
            friction.push_back(DataPoint(dp.timestamp, std::vector<double>({abs(dp.values[0]-dp.values[1])})));
        }
        processor.addResults(friction);
        // Scrivi su file se abilitato
        if (processor.write_to_file) {
            processor.writeToFile(friction);
        }
    }

// Funzione di elaborazione compatibile con segnali mono/multi-canale
std::vector<DataPoint> processAccSpike(const std::vector<DataPoint>& window) {
    static IncrementalStats stats;

    size_t win_size = window.size();
    size_t num_channels = window[0].numChannels();

    // Estrarre i dati per ciascun canale
    std::vector<std::vector<double>> channels(num_channels, std::vector<double>(win_size));
    for (size_t i = 0; i < win_size; i++) {
        for (size_t j = 0; j < num_channels; j++) {
            stats.update(window[i].values[j]);
            channels[j][i] = window[i].values[j];
        }
    }

    // Filtraggio e decimazione per ogni canale
    for (size_t j = 0; j < num_channels; j++) {
        applyZeroPhaseButterworth(channels[j], true, As_HIGH_CUT_F, A_TARGET_FS);
        channels[j] = downsample(channels[j], A_DECIMATION_FACTOR);
    }

    std::vector<double> sma = moving_average(channels[0], As_WINDOW_SIZE);
    std::vector<double> acc_th(channels[0].size());
    std::vector<double> acc_delta(channels[0].size());

    for (size_t i = 0; i < channels[0].size(); ++i) {
        acc_th[i] = 2 * stats.getStdDev() + stats.getMean() + sma[i];
        acc_delta[i] = std::max(0.0, channels[0][i] - acc_th[i]); // Imposta a 0 se negativo
    }

    // Creazione della finestra elaborata decimata
    size_t new_size = channels[0].size();
    std::vector<DataPoint> processed_window(new_size, DataPoint(0, num_channels));
    for (size_t i = 0; i < new_size; i++) {
        for (size_t j = 0; j < num_channels; j++) {
            processed_window[i].values[j] = acc_delta[i];
        }
    }

    return processed_window;
}

// Callback per mostrare i risultati
void AccSpikesResult(const std::vector<DataPoint>& result, WindowProcessor& processor) {
    std::vector<DataPoint> AccSPikes(result.begin(), result.begin() + processor.step / A_DECIMATION_FACTOR);
    processor.addResults(AccSPikes);
    if (processor.write_to_file) {
        processor.writeToFile(AccSPikes);
    }
}

std::vector<DataPoint> processAccPSD(const std::vector<DataPoint>& window)
{
    size_t win_size = window.size();
    size_t num_channels = window[0].numChannels();

    // Estrarre i dati per ciascun canale
    std::vector<std::vector<double>> channels(num_channels, std::vector<double>(win_size));
    for (size_t i = 0; i < win_size; i++) {
        for (size_t j = 0; j < num_channels; j++) {
            channels[j][i] = window[i].values[j];
        }
    }

    // Filtraggio e decimazione per ogni canale
    for (size_t j = 0; j < num_channels; j++) {
        applyZeroPhaseButterworth(channels[j], false, Apsd_LOW_CUT_F, A_TARGET_FS);
        channels[j] = downsample(channels[j], A_DECIMATION_FACTOR);
        applyHannWindow(channels[j]);
    }


    // Creazione della finestra elaborata decimata
    size_t new_size = channels[0].size();
    std::vector<DataPoint> processed_window(new_size, DataPoint(0, num_channels));
    for (size_t i = 0; i < new_size; i++) {
        for (size_t j = 0; j < num_channels; j++) {
            processed_window[i].values[j] = channels[j][i];
        }
    }

    return processed_window;
}

// Callback per mostrare i risultati
void ApsdComputation(const std::vector<DataPoint>& result, WindowProcessor& processor)
{
    if (result.size()==0) { return; }

    static size_t step = 1;
    static double t0 = 0.0;
    double energy, time_sec;
    if(step == 1) t0 = result[0].timestamp;

    size_t win_size = result.size();
    fftw_complex *out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * win_size);
    double *in = (double*) fftw_malloc(sizeof(double) * win_size);

    if ((in!=NULL) && (out!=NULL))
    {
        {
            std::lock_guard<std::mutex> lock(fft_mutex_);
            fftw_plan plan = fftw_plan_dft_r2c_1d(win_size, in, out, FFTW_ESTIMATE);

            energy=0.0;
            time_sec = t0+((step*(Apsd_WINDOW_SIZE - Apsd_OVERLAP_WND)) / (double)(A_TARGET_FS*A_DECIMATION_FACTOR));

            for (size_t i = 0; i < win_size; i++) {
                in[i] = result[i].values[0];   // Prendi solo il primo canale
            }

            // Esegui la FFT
            fftw_execute(plan);

            // Calcola l'energia totale
            for (size_t i = 0; i < Apsd_WINDOW_SIZE / 2; i++) {
                double magnitude = out[i][0] * out[i][0] + out[i][1] * out[i][1];  // |X[k]|^2
                energy += magnitude;
            }

            energy=energy/win_size;

            fftw_destroy_plan(plan);
            fftw_free(in);
            fftw_free(out);
        }
        std::vector<DataPoint>ApsdResult({DataPoint(time_sec, std::vector<double>({energy}))});
        processor.addResults(ApsdResult);
        // Scrivi su file se abilitato
        if (processor.write_to_file) {
            processor.writeToFile(ApsdResult);
        }

        step += 1;
    }
}

std::vector<DataPoint> processForcePSD(const std::vector<DataPoint>& window)
{
    size_t win_size = window.size();
    size_t num_channels = window[0].numChannels();

    // Estrarre i dati per ciascun canale
    std::vector<std::vector<double>> channels(num_channels, std::vector<double>(win_size));
    for (size_t i = 0; i < win_size; i++) {
        for (size_t j = 0; j < num_channels; j++) {
            channels[j][i] = window[i].values[j];
        }
    }

    // Filtraggio e decimazione per ogni canale
    for (size_t j = 0; j < num_channels; j++) {
        applyZeroPhaseButterworth(channels[j], false, Fpsd_LOW_CUT_F, F_TARGET_FS);
        channels[j] = downsample(channels[j], F_DECIMATION_FACTOR);
        applyHannWindow(channels[j]);
    }


    // Creazione della finestra elaborata decimata
    size_t new_size = channels[0].size();
    std::vector<DataPoint> processed_window(new_size, DataPoint(0, num_channels));
    for (size_t i = 0; i < new_size; i++) {
        for (size_t j = 0; j < num_channels; j++) {
            processed_window[i].values[j] = channels[j][i];
        }
    }

    return processed_window;
}

// Callback per mostrare i risultati
void FpsdComputation(const std::vector<DataPoint>& result, WindowProcessor& processor)
{
    if (result.size()==0) { return; }

    static size_t step = 1;
    static double t0 = 0.0;
    if(step == 1) t0 = result[0].timestamp;
    double energy , time_sec;
    size_t win_size = result.size();

    fftw_complex *out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * win_size);
    double *in = (double*) fftw_malloc(sizeof(double) * win_size );


    if ((in!=NULL) && (out!=NULL))
    {
        {
            std::lock_guard<std::mutex> lock(fft_mutex_);
            fftw_plan plan = fftw_plan_dft_r2c_1d(win_size, in, out, FFTW_ESTIMATE);

            energy = 0.0;
            time_sec = t0+((step*(Fpsd_WINDOW_SIZE - Fpsd_OVERLAP_WND)) / (double)(F_TARGET_FS*F_DECIMATION_FACTOR));

            for (size_t i = 0; i < win_size; i++)
            {
                if (result[i].values.size()!=0)
                {
                in[i] = result[i].values[0];   // Prendi solo il primo canale
                }
            }

            // Esegui la FFT
            fftw_execute(plan);

            // Calcola l'energia totale
            for (size_t i = 0; i < win_size / 2; i++)
            {
                double magnitude = out[i][0] * out[i][0] + out[i][1] * out[i][1];  // |X[k]|^2
                energy += magnitude;
            }

            energy=energy/win_size;

            fftw_destroy_plan(plan);
            fftw_free(in);
            fftw_free(out);
        }

        std::vector<DataPoint>FpsdResult({DataPoint(time_sec, std::vector<double>({energy}))});
        processor.addResults(FpsdResult);
        // Scrivi su file se abilitato
        if (processor.write_to_file) { processor.writeToFile(FpsdResult); }
        step += 1;
    }
}

public:

    WindowProcessor Fr_processor;
    WindowProcessor As_processor;
    WindowProcessor Apsd_processor;
    WindowProcessor Fpsd_processor;

    TactileFeaturesProcessor(const int f_input_fs = 7000, const int f_dec_fact = 7, const int a_input_fs = 4000, const int a_dec_fact = 1,
        size_t fr_win_size=128, float fr_ovlp=0.5, float fr_highcutf = 450.0, float fr_lowcutf = 10.0, const std::string& fr_filename = "",
        size_t as_win_size=2000, float as_ovlp=0.5, float as_highcutf = 100.0, const std::string& as_filename = "",
        size_t fpsd_win_size=512, float fpsd_ovlp=0.8, float fpsd_lowcutf = 5.0, const std::string& fpsd_filename = "",
        size_t apsd_win_size=512, float apsd_ovlp=0.8, float apsd_lowcutf = 5.0, const std::string& apsd_filename = "")

        : F_INPUT_FS(f_input_fs), F_TARGET_FS(int(f_input_fs/f_dec_fact)), F_DECIMATION_FACTOR(f_dec_fact),
          A_INPUT_FS(a_input_fs), A_TARGET_FS(int(a_input_fs/a_dec_fact)), A_DECIMATION_FACTOR(a_dec_fact),
          Fr_WINDOW_SIZE(fr_win_size), Fr_OVERLAP_WND(size_t(fr_win_size*fr_ovlp)),
          As_WINDOW_SIZE(as_win_size), As_OVERLAP_WND(size_t(as_win_size*as_ovlp)),
          Apsd_WINDOW_SIZE(apsd_win_size), Apsd_OVERLAP_WND(size_t(apsd_win_size*apsd_ovlp)),
          Fpsd_WINDOW_SIZE(fpsd_win_size), Fpsd_OVERLAP_WND(size_t(fpsd_win_size*fpsd_ovlp)),
          Fr_HIGH_CUT_F(fr_highcutf), Fr_LOW_CUT_F(fr_lowcutf),
          As_HIGH_CUT_F(as_highcutf),
          Apsd_LOW_CUT_F(apsd_lowcutf),
          Fpsd_LOW_CUT_F(fpsd_lowcutf),
          Fr_output_file(fr_filename),
          As_output_file(as_filename),
          Apsd_output_file(apsd_filename),
          Fpsd_output_file(fpsd_filename),
          Fr_processor(fr_win_size, size_t(fr_win_size*fr_ovlp), 3, f_dec_fact, fr_filename),
          As_processor(as_win_size, size_t(as_win_size*as_ovlp), 1, a_dec_fact, as_filename),
          Apsd_processor(apsd_win_size, size_t(apsd_win_size*apsd_ovlp), 1, a_dec_fact, apsd_filename),
          Fpsd_processor(fpsd_win_size, size_t(fpsd_win_size*fpsd_ovlp), 1, f_dec_fact, fpsd_filename) {

            //Initialize the Fr WindowProcessor
            Fr_processor.setProcessingFunction([this](const std::vector<DataPoint>& window) { return this->processFriction(window); });
            Fr_processor.setResultCallback([this](const std::vector<DataPoint>& result, WindowProcessor& processor) {
                this->FrResult(result, processor);
            });
            Fr_processor.setNormalizationFunction([](double value) { return pow(10, value-1); });

            //Initialize the As WindowProcessor
            As_processor.setProcessingFunction([this](const std::vector<DataPoint>& window) { return this->processAccSpike(window); });
            As_processor.setResultCallback([this](const std::vector<DataPoint>& result, WindowProcessor& processor) {
                this->AccSpikesResult(result, processor);
            });
            As_processor.setNormalizationFunction([](double value) { return pow(10, value-1); });

            //Initialize the Apsd WindowProcessor
            Apsd_processor.setProcessingFunction([this](const std::vector<DataPoint>& window) { return this->processAccPSD(window); });
            Apsd_processor.setResultCallback([this](const std::vector<DataPoint>& result, WindowProcessor& processor) {
                this->ApsdComputation(result, processor);
            });
            Apsd_processor.setNormalizationFunction([](double value) { return pow(10, value-1); });

            //Initialize the Fpsd WindowProcessor
            Fpsd_processor.setProcessingFunction([this](const std::vector<DataPoint>& window) { return this->processForcePSD(window); });
            Fpsd_processor.setResultCallback([this](const std::vector<DataPoint>& result, WindowProcessor& processor) {
                this->FpsdComputation(result, processor);
            });
            Fpsd_processor.setNormalizationFunction([](double value) { return pow(10, value-1); });
    }

    void addForceData(const DataPoint& new_sample) {
        Fr_processor.addData(new_sample);
        Fpsd_processor.addData({new_sample.timestamp, std::vector<double>({static_cast<double>(sqrt(new_sample.values[0]*new_sample.values[0] + new_sample.values[1]*new_sample.values[1] + new_sample.values[2]*new_sample.values[2]))})});
    }
    void addAccelerationData(const DataPoint& new_sample)
    {
        //Check here ?
        As_processor.addData({new_sample.timestamp, std::vector<double>({static_cast<double>(sqrt(new_sample.values[0]*new_sample.values[0] + new_sample.values[1]*new_sample.values[1] + new_sample.values[2]*new_sample.values[2]))})});
        Apsd_processor.addData({new_sample.timestamp, std::vector<double>({static_cast<double>(sqrt(new_sample.values[0]*new_sample.values[0] + new_sample.values[1]*new_sample.values[1] + new_sample.values[2]*new_sample.values[2]))})});
    }

    std::vector<DataPoint> getFrictionResults(size_t n) {
        return Fr_processor.getOutputBuffer(n);
    }

    std::vector<DataPoint> getAccSpikeResults(size_t n) {
        return As_processor.getOutputBuffer(n);
    }

    std::vector<DataPoint> getAccPSDResults(size_t n) {
        return Apsd_processor.getOutputBuffer(n);
    }

    std::vector<DataPoint> getForcePSDResults(size_t n) {
        return Fpsd_processor.getOutputBuffer(n);
    }

    bool isProcessing() const {
        return Fr_processor.isProcessing() || As_processor.isProcessing() || Apsd_processor.isProcessing() || Fpsd_processor.isProcessing();
    }
};
