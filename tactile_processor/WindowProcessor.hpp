/** @file WindowProcessor.hpp
 *  @brief  A real-time feature calculation utility for the ATI/Accelerometer sensor

 *  @author Michele Pompilio
 */

#include <vector>
#include <deque>
#include <functional>
#include <iostream>
#include <cmath>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

// Class to represent a data point with N channels
class DataPoint {
    public:
        double timestamp;
        std::vector<double> values;

        // Default constructor
        DataPoint() : timestamp(0.0), values() {}

        // Constructor with a specified number of channels
        DataPoint(double t, size_t num_channels) : timestamp(t), values(num_channels, 0.0) {}

        // Constructor with initialized values
        DataPoint(double t, const std::vector<double>& v) : timestamp(t), values(v) {}

        // Method to set the values
        void setValues(const std::vector<double>& v) {
            values = v;
        }

        // Method to get the number of channels
        size_t numChannels() const {
            return values.size();
        }

        // Method to print the content of the DataPoint
        void print() const {
            std::cout << "Timestamp: " << timestamp << " | Values: ";
            for (double v : values) std::cout << v << " ";
            std::cout << "\n";
        }
};

// Class to manage signals with a variable number of channels
class WindowProcessor {
private:
    size_t window_size, overlap, downsampled_size, num_channels;
    const int DECIMATION_FACTOR;
    std::deque<DataPoint> buffer;
    std::vector<DataPoint> output_buffer;
    std::vector<DataPoint> overlap_buffer;
    std::vector<double> normalization_buffer;
    std::ofstream output_file;

    std::function<std::vector<DataPoint>(const std::vector<DataPoint>&)> processing_function;
    std::function<void(const std::vector<DataPoint>&, WindowProcessor&)> result_callback;
    std::function<double(double)> normalization_function;

    mutable std::mutex buff_mutex_;
    mutable std::mutex output_mutex_;
    std::condition_variable buff_cond_;
    std::condition_variable processing_cond_;

    std::thread worker_thread_;
    std::atomic<bool> running_{false}, processing_{false};

    // Start the worker thread with a processing function
    void start_processor() {
        if (running_) return;  // Avoid starting multiple workers
        running_ = true;
        processing_ = true;
        worker_thread_ = std::thread([this]() {
            while (running_ || processing_) {
                std::unique_lock<std::mutex> lock(buff_mutex_);
                buff_cond_.wait(lock, [this] { return buffer.size() >= window_size || !running_; });

                if(buffer.size() >= window_size){
                    std::vector<DataPoint> window(buffer.begin(), buffer.begin() + window_size);

                    // Process the window
                    if (processing_function) {
                        std::vector<DataPoint> processed_window = processing_function(window);

                        // Overlap-Add with normalization
                        for (size_t i = 0; i < downsampled_size; i++) {
                            for (size_t j = 0; j < num_channels; j++) {
                                overlap_buffer[i].values[j] += processed_window[i].values[j];
                            }
                            normalization_buffer[i] += 1.0;
                        }

                        // Normalization and output
                        std::vector<DataPoint> normalized_output(downsampled_size);
                        for (size_t i = 0; i < downsampled_size; i++) {
                            normalized_output[i].timestamp = window[i * DECIMATION_FACTOR].timestamp;
                            normalized_output[i].values.resize(num_channels);

                            if (normalization_buffer[i] >= 1.0) {
                                for (size_t j = 0; j < num_channels; j++) {
                                    if(normalization_function)
                                        normalized_output[i].values[j] = overlap_buffer[i].values[j] / normalization_function(normalization_buffer[i]);
                                    else
                                        normalized_output[i].values[j] = overlap_buffer[i].values[j] / normalization_buffer[i];
                                }
                            }
                        }

                        if (result_callback) {
                            result_callback(normalized_output, *this);
                        }

                        // Shift the buffer for the next window
                        buffer.erase(buffer.begin(), buffer.begin() + step);
                        for (size_t i = 0; i < downsampled_size - step / DECIMATION_FACTOR; i++) {
                            overlap_buffer[i] = overlap_buffer[i + step / DECIMATION_FACTOR];
                            normalization_buffer[i] = normalization_buffer[i + step / DECIMATION_FACTOR];
                        }
                        std::fill(overlap_buffer.begin() + downsampled_size - step / DECIMATION_FACTOR, overlap_buffer.end(), DataPoint(0, num_channels));
                        std::fill(normalization_buffer.begin() + downsampled_size - step / DECIMATION_FACTOR, normalization_buffer.end(), 0);
                    }
                }
                if(buffer.size() < window_size && !running_){
                    processing_ = false;
                    processing_cond_.notify_all();
                }
                lock.unlock();
            }
        });
    }

    // Safely stop the worker thread
    void stop_processor() {
        if (running_) {
            running_ = false;
            buff_cond_.notify_all();  // Unlock any waiting threads
            std::unique_lock<std::mutex> lock(buff_mutex_);
            processing_cond_.wait(lock, [this] { return buffer.size()<window_size && !processing_; });
            lock.unlock();
            if (worker_thread_.joinable()) {
                worker_thread_.join();
            }
        }
    }
public:
    bool write_to_file;
    size_t step;
    WindowProcessor(size_t win_size, size_t ovlp, size_t channels, const int dec_fact, const std::string& filename = "")
        : window_size(win_size), overlap(ovlp), step(win_size - ovlp),
          DECIMATION_FACTOR(dec_fact),
          num_channels(channels),
          write_to_file(!filename.empty()) {
            downsampled_size = (win_size / DECIMATION_FACTOR);
            overlap_buffer = std::vector<DataPoint>(downsampled_size, DataPoint(0.0, channels));
            normalization_buffer = std::vector<double>(downsampled_size, 0.0);

            if (write_to_file) {
                output_file.open(filename);
                if (!output_file.is_open()) {
                    throw std::runtime_error("Error opening the output file.");
                }
            }
          }

    ~WindowProcessor() {
        stop_processor();
        if (write_to_file && output_file.is_open()) {
            output_file.close();
        }
    }

    void writeToFile(const std::vector<DataPoint>& window) {
        if (!write_to_file || !output_file.is_open()) return;

        for (const auto& data_point : window) {
            output_file << data_point.timestamp<<" ";
            for (const auto& value : data_point.values) {
                output_file << " " << value;
            }
            output_file << "\n";
        }
    }

    void setProcessingFunction(std::function<std::vector<DataPoint>(const std::vector<DataPoint>&)> func) {
        processing_function = func;
    }

    void setResultCallback(std::function<void(const std::vector<DataPoint>&, WindowProcessor&)> callback) {
        result_callback = callback;
    }

    void setNormalizationFunction(std::function<double(double)> func) {
        normalization_function = func;
    }

    void addData(const DataPoint& new_sample)
    {
        if (processing_function && result_callback && normalization_function && !running_) {
            start_processor();
        }
        std::lock_guard<std::mutex> lock(buff_mutex_);
        buffer.push_back(new_sample);

        if (buffer.size() >= window_size) {
            buff_cond_.notify_one();
        }
    }

    void addResults(const std::vector<DataPoint>& results) {
        std::lock_guard<std::mutex> lock(output_mutex_);
        output_buffer.insert(output_buffer.end(), results.begin(), results.end());
    }

    std::vector<DataPoint> getOutputBuffer(size_t n) {
        std::lock_guard<std::mutex> lock(output_mutex_);
        std::vector<DataPoint> buffer;
        if (n <= output_buffer.size()) {
            buffer.assign(output_buffer.begin(), output_buffer.begin() + n);
            output_buffer.erase(output_buffer.begin(), output_buffer.begin() + n);
        } else {
            buffer.assign(output_buffer.begin(), output_buffer.end());
            output_buffer.clear();
        }
        return buffer;
    }

    int size()
    {
        return output_buffer.size();
    }


    bool isProcessing() const {
        return processing_;
    }
};
