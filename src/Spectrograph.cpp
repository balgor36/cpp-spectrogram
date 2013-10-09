#include "Spectrograph.h"
#include "Utility.h"
#include "FreeImage.h"

#include <iostream>
#include <cassert>
#include <cmath>

Spectrograph::Spectrograph(std::string fname, int width, int height) :
    fname_(fname), file_handle_(fname), width_(width), height_(height),
    window_(Utility::hann_function) {

    if (!file_handle_){
        std::cerr << "Error Loading file " << fname << std::endl;
    } else {
        read_in_data();
    }
}

void Spectrograph::set_window(std::function<double(int, int)> window){
    window_ = window;
}

bool Spectrograph::file_is_valid(){
    return file_handle_;
}

void Spectrograph::read_in_data(){
    std::cout << "Reading in file: " << fname_ << std::endl;

    int audio_length_sec = file_handle_.frames() / file_handle_.samplerate();
    std::cout << "Length (s): " << audio_length_sec << std::endl;

    const int data_size = file_handle_.channels() * file_handle_.frames();

    data_ = std::vector<short>(data_size, 0);
    file_handle_.read(&data_[0], data_.size());
    max_frequency_ = file_handle_.samplerate() * 0.5;
}

std::complex<double> Spectrograph::omega(float p, float q){

    const float trig_arg = 2 * M_PI * q / p;
    // VVV Comment out this line to use the cache VVVV
    return { cos(trig_arg), sin(trig_arg) };

    auto memo = omega_cache_.find(trig_arg);
    if (memo != omega_cache_.end()){
        return memo->second;
    } else {
        complex_d result = { cos(trig_arg), sin(trig_arg) };
        omega_cache_[trig_arg] = result;
        return result;
    }
}

void Spectrograph::save_image(
        std::string fname, 
        bool log_mode){
    //
    // Active for static linking
    #ifdef FREEIMAGE_LIB
        FreeImage_Initialise();
    #endif
    FIBITMAP *bitmap = FreeImage_Allocate(spectrogram_.size(), height_, 32); // RGBA

    const int data_size = spectrogram_.front().size();
    const int data_size_used = data_size * 0.25;

    const double log_coef = 
        (1.0/log(static_cast<double>(height_ + 1))) * static_cast<double>(data_size_used);

    std::cout << "Drawing." << std::endl;
    for (int x = 0; x < spectrogram_.size(); x++){
        int freq = 0;
        for (int y = 1; y <= height_; y++){

            RGBQUAD color = get_color(spectrogram_[x][freq], 10.0);
            FreeImage_SetPixelColor(bitmap, x, y - 1, &color);
            
            if (log_mode){
                freq = data_size_used - 1 - static_cast<int>(log_coef * log(height_ + 1 - y));
            } else {
                double ratio = static_cast<double>(y)/height_;
                freq = static_cast<int>(ratio * data_size * 0.5);
            }
        }
    }

    std::cout << "Saving to file " << fname << std::endl;
    FreeImage_Save(FIF_PNG, bitmap, fname.c_str(), PNG_DEFAULT);
    FreeImage_Unload(bitmap);

    #ifdef FREEIMAGE_LIB
        FreeImage_Deinitialise();
    #endif
}

RGBQUAD Spectrograph::get_color(std::complex<double>& c, float threshold){
    double val = 0.5 * std::log10(Utility::mag(c) + 1);

    if (val > threshold){
        val = threshold;
    }
    float ratio = val/threshold;
    uint8_t num = static_cast<uint8_t>(ratio * 255);

    return {num, num, num, 255};
}

void Spectrograph::compute(const int CHUNK_SIZE, const float OVERLAP){
    assert(0.0 <= OVERLAP && OVERLAP < 1.0);
    const int STEP = static_cast<int>(CHUNK_SIZE * (1.0 - OVERLAP));

    // Print out computation info
    std::cout << "Computing spectrogram..." << std::endl;
    std::cout << "Chunk: " << CHUNK_SIZE << " Overlap: " 
              << OVERLAP * CHUNK_SIZE << std::endl;

    std::cout << "Step Size: " << STEP << std::endl;
    std::cout << "Data Size: " << data_.size() << std::endl;

    // Pad the data
    int new_size = 0;
    while (new_size + CHUNK_SIZE < data_.size()){
        new_size += STEP;
    }
    if (new_size != data_.size()){
        std::cout << "Padding data." << std::endl;
        new_size += CHUNK_SIZE;
        std::vector<short> padding(new_size - data_.size(), 0);
        data_.insert(data_.end(), padding.begin(), padding.end());
    }

    chunkify(CHUNK_SIZE, STEP);
}

void Spectrograph::chunkify(const int CHUNK_SIZE, const int STEP){
    spectrogram_.clear();
    spectrogram_.reserve((data_.size() - CHUNK_SIZE)/STEP + 1);

    std::cout << "Computing chunks." << std::endl;
    for (int i = 0; i + CHUNK_SIZE <= data_.size(); i += STEP){
        spectrogram_.push_back(
                std::vector<complex_d>(
                data_.begin() + i,
                data_.begin() + i + CHUNK_SIZE)
                );
        transform(spectrogram_.back());
    }
    std::cout << "Done." << std::endl;
}

// TODO: Cache calculations of omega
void Spectrograph::transform(std::vector<std::complex<double>>& signal, int min_size){
    if (min_size < 0){
        min_size = signal.size();
    }
    int power = pad_to_power2(signal, min_size);

    if (!power){
        return;
    }

    std::vector<std::complex<double>> transformed(signal.size(), 0);
    // Apply window function and sort by bit-reversed index
    for (int i = 0; i < signal.size(); i++){
        transformed[Utility::reverse_bits(i, power)] = 
            signal[i] * window_(i, signal.size());
    }

    int n = 2;
    while (n <= transformed.size()){
        // Iterate over length-n segments
        for (int i = 0; i <= transformed.size() - n; i += n){
            // Combine each half of the segment
            for (int m = i; m < i + n/2; m++){
                complex_d term1 = transformed[m];
                complex_d term2 = omega(n, -m) * transformed[m + n/2];

                transformed[m]       = term1 + term2;
                transformed[m + n/2] = term1 - term2;
            }
        }
        n *= 2;
    }
    signal.assign(transformed.begin(), transformed.end());
}


void Spectrograph::transform_recursive(
        std::vector<std::complex<double>>& signal, 
        int min_size){
    pad_to_power2(signal, signal.size());
    for (int i = 0; i < signal.size(); i++){
        signal[i] *= window_(i, signal.size());
    }
    _transform(signal);
}

void Spectrograph::_transform(
        std::vector<std::complex<double>>& signal, 
        int min_size){

    int n = signal.size();
    if (n == 1){
        return;
    }

    std::vector<std::complex<double>> f_even;
    std::vector<std::complex<double>> f_odd;
    f_even.reserve(signal.size());
    f_odd.reserve(signal.size());

    for (int i = 0; i < signal.size(); i++){
        if (i % 2){
            f_odd.push_back(signal[i]);
        } else {
            f_even.push_back(signal[i]);
        }
    }

    _transform(f_odd);
    _transform(f_even);

    for (int m = 0; m < n/2; m++){
        complex_d w = omega(n, -m);
        signal[m] = f_even[m] + w * f_odd[m];
        signal[m + n/2] = f_even[m] - w * f_odd[m];
    }
}

void Spectrograph::pad(std::vector<std::complex<double>>& signal, int new_size){
    if (new_size > signal.size()){
        std::vector<std::complex<double>> padding(new_size - signal.size(), 0);
        signal.insert(signal.end(), padding.begin(), padding.end());
    }
}

int Spectrograph::pad_to_power2(std::vector<std::complex<double>>& signal, int min_size){
    int power = 1;
    int new_size = 2;

    while (new_size < min_size){
        new_size *= 2;
        power++;
    }
    pad(signal, new_size);
    return power;
}