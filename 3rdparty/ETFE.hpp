// MIT License

// Copyright (c) 2022 Evan Pezent

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once
#include <kiss_fftr.h>
#include <complex>
#include <vector>
#include <algorithm>

namespace etfe {

/// pi
static constexpr double pi = 3.14159265358979323846264338327950288;
/// complex type
typedef std::complex<double> complex;

//-----------------------------------------------------------------------------
// FFT
//-----------------------------------------------------------------------------

/// Fast Fourier Transfrom (KISS FFT Wrapper)
class FFT {
public:

    /// Default Constructor
    FFT() : m_nfft(0), m_cfg(nullptr)  { }

    // Constructor (nfft must be even)
    FFT(std::size_t nfft) { resize(nfft); }

    // Destructor
    ~FFT() { if (m_cfg != nullptr) free(m_cfg); }

    // Perform the FFT
    void transform(const double* in, complex* out) {
        kiss_fft_cpx* kiss_out = reinterpret_cast<kiss_fft_cpx*>(out);
        kiss_fftr(m_cfg, in, kiss_out);        
    }

    // Gets the FFT size
    std::size_t size() const { return m_nfft; }

    // Resize the FFT (nfft must be even)
    void resize(std::size_t nfft) {
        if (nfft == 0)
            return;
        nfft = nfft % 2 != 0 ? nfft + 1 : nfft;
        m_nfft = nfft;
        if (m_cfg != nullptr) {
            free(m_cfg);
            m_cfg = nullptr;
        }
        m_cfg = kiss_fftr_alloc((int)m_nfft, 0, NULL, NULL);
    }

private:
    std::size_t    m_nfft; ///< FFT size
    kiss_fftr_cfg  m_cfg;  ///< KISSFFT plan
};

//-----------------------------------------------------------------------------
// WINDOWS
//-----------------------------------------------------------------------------

/// Generates hamming window of size W
inline std::vector<double> hamming(std::size_t L) {
    std::vector<double> window(L);
    for (std::size_t w = 0; w < L; ++w)
        window[w] = (0.54 - 0.46 * std::cos(2*pi*w/(L-1)));
    return window;
}

// Generates hann window of size L
inline std::vector<double> hann(std::size_t L) {
    std::vector<double> window(L);
    for (std::size_t w = 0; w < L; ++w)
        window[w] = (0.5 - 0.5 * std::cos(2*pi*w/(L-1)));
    return window;
}

// Generates winrect window of size L
inline std::vector<double> winrect(std::size_t L) {
    return std::vector<double>(L,1.0);
}

// TODO: add more windows

//-----------------------------------------------------------------------------
// ETFE
//-----------------------------------------------------------------------------

// Empirical Transfer-Function Estimate
class ETFE {
public:   

    /// Full results from estimating the experimental transfer function
    class Result {
    public:

        /// Default Constructor
        Result() { }

        /// Get the common size of all the result buffers
        std::size_t size() const {return m_size; }

        std::vector<double>  f;     ///< freqeuncies [Hz]
        std::vector<double>  pxx;   ///< power spectral density of input x
        std::vector<double>  pyy;   ///< power spectral density of output y
        std::vector<complex> pxy;   ///< cross power spectral density of input x and output y
        std::vector<complex> txy;   ///< transfer function estimate for input x and output y (i.e. conj(pxy) ./ pxx)
        std::vector<double>  mag;   ///< transfer function magnitude (i.e 20*log10(abs(txy)))
        std::vector<double>  phase; ///< transfer function phase (i.e. 180/pi*angle(txy))
        std::vector<double>  ampx;  ///< amplitude spectrum of x
        std::vector<double>  ampy;  ///< amplitude spectrum of y

    private:
        friend class ETFE;
        /// Constructor
        Result(std::size_t n) { resize(n); }
        /// Resets Result for next ETFE computation
        void reset() {
            std::fill(ampx.begin(), ampx.end(), 0);
            std::fill(ampy.begin(), ampy.end(), 0);
            std::fill(pxx.begin(), pxx.end(), 0);
            std::fill(pyy.begin(), pyy.end(), 0);
            std::fill(pxy.begin(), pxy.end(), complex(0,0));
        }
        /// Resize the result buffers
        void resize(std::size_t size) {
            m_size = size;
            f.resize(m_size);   pxx.resize(m_size);  pyy.resize(m_size); pxy.resize(m_size); txy.resize(m_size); 
            mag.resize(m_size); phase.resize(m_size); ampx.resize(m_size); ampy.resize(m_size);
        }
    private:
        std::size_t m_size; ///< size of buffers
    };

    /// Default Constructor
    ETFE() { }

    /// Constructor 1 (Quick Setup)
    ETFE(std::size_t nsamples, double fs) { 
        setup(nsamples, fs); 
    }

    /// Constructor 2 (Full Setup)
    ETFE(std::size_t nsamples, double fs, const std::vector<double>& window, std::size_t noverlap, std::size_t nfft)  {          
        setup(nsamples, fs, window, noverlap, nfft);
    }

    /// Estimate the experimental transfer function given input x and output y, each of length n provided to the constructor
    const Result& estimate(const double* x, const double* y) {
        // reset results
        m_r.reset();
        // iterate window segments
        for (std::size_t k = 0; k < m_k; ++k) {
            // zero pad and window kth segment of x and y
            std::fill(m_inx.begin(), m_inx.end(), 0);
            std::fill(m_iny.begin(), m_iny.end(), 0);
            for (std::size_t w = 0; w < m_win.size(); ++w) {
                const std::size_t idx = m_o*k+w;
                m_inx[w] = x[idx] * m_win[w];
                m_iny[w] = y[idx] * m_win[w];
            }
            // fft kth segment of x and y
            m_fft.transform(m_inx.data(), m_outx.data());
            m_fft.transform(m_iny.data(), m_outy.data());
            // calculate power spectral densities for window
            for (std::size_t i = 0; i < m_fft.size()/2+1; ++i)  {
                // compute abs values
                const double absxi = std::abs(m_outx[i]);
                const double absyi = std::abs(m_outy[i]);
                // power spectral densities
                const double psdScale = (1 + (i > 0 && i < m_fft.size()/2+1)) * m_pf;
                m_r.pxx[i] += absxi * absxi * psdScale;          
                m_r.pyy[i] += absyi * absyi * psdScale;  
                m_r.pxy[i] += m_outx[i] * std::conj(m_outy[i]) * psdScale;     
                // amplitudes
                const double ampScale = 1.0 / (m_fft.size()/2) * m_af; 
                m_r.ampx[i] += absxi * ampScale;
                m_r.ampy[i] += absyi * ampScale;
            }
        }
        // compute txy, mag, and phase
        for (std::size_t i = 0; i < m_fft.size()/2+1; ++i) {
            m_r.txy[i]   = std::conj(m_r.pxy[i]) / m_r.pxx[i];
            m_r.mag[i]   = 20*log10(std::abs(m_r.txy[i]));
            m_r.phase[i] = 180/pi * std::arg(m_r.txy[i]);
        } 
        // return result
        return m_r;
    }

    /// (Re)perform quick setup
    void setup(std::size_t nsamples, double fs) {
        // 8 windows w/ 50% overlap
        const std::size_t w = 2*nsamples/(8+1); 
        const std::size_t o = w / 2;            
        // nfft = max(256,2^p) where p = ceil(log2W)
        const int p  = static_cast<int>(std::pow(2,std::ceil(std::log2(w))));
        const std::size_t nfft = std::max(256, p);
        setup(nsamples, fs, hamming(w), o, nfft);
    }

    /// (Re)perform full setup
    void setup(std::size_t nsamples, double fs, const std::vector<double>& window, std::size_t noverlap, std::size_t nfft) {
        // make nfft even
        nfft = nfft % 2 != 0 ? nfft + 1 : nfft;
        // set provided variables
        m_n    = nsamples;
        m_fs   = fs;
        m_win  = window;
        m_o    = noverlap;
        // resize FFT and buffers
        m_fft.resize(nfft);
        m_inx.resize(nfft);
        m_iny.resize(nfft);
        m_outx.resize(nfft);
        m_outy.resize(nfft);
        // calculate window count 
        m_k = m_o == 0 ? m_n / m_win.size() : (m_o - m_win.size() + m_n) / m_o;
        // calculate scaling factors
        double sum   = 0, sumsq = 0;
        for (auto& w : m_win) {
            sum   += w;
            sumsq += w*w;
        }
        m_af = (1.0/m_k) * m_win.size() / sum;
        m_pf = 1.0 / (m_fs * sumsq) / m_k;
        // resize result buffer
        m_r.resize(nfft/2+1);
        // calculate frequency bins
        const double df = fs/nfft;
        for (std::size_t i = 0; i < m_fft.size()/2+1; ++i)
            m_r.f[i] = i * df;  
    }

    const Result& getResult() const            { return m_r; }
    std::size_t   getSampleSize() const        { return m_n; }
    std::size_t   getFftSize() const           { return m_fft.size(); }
    double        getSamplingFrequency() const { return m_fs; }
    std::size_t   getWindowCount() const       { return m_k; }
    std::size_t   getWindowSize()  const       { return m_win.size(); }
    std::size_t   getWindowOverlap() const     { return m_o; }    

private:
    std::size_t          m_n;     ///< number of samples expected in x and y
    std::size_t          m_o;     ///< number of overlapping samples between segments
    std::size_t          m_k;     ///< number of segments
    double               m_fs;    ///< sampling frequency
    std::vector<double>  m_win;   ///< window
    FFT                  m_fft;   ///< FFT 
    std::vector<double>  m_inx;   ///< FFT input for x
    std::vector<double>  m_iny;   ///< FFT input for y
    std::vector<complex> m_outx;  ///< FFT output for x
    std::vector<complex> m_outy;  ///< FFT output for y
    double               m_af;    ///< amplitude scaling factor
    double               m_pf;    ///< power scaling factor
    Result               m_r;     ///< result buffers
};

} // namespace etfe