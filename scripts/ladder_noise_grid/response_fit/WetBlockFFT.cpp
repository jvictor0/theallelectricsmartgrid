#include <algorithm>
#include <array>
#include <complex>
#include <cstdint>

// Explicit radix-two butterflies used to enclose the mathematical analyzer.
// Table entries and the absolute coefficient error are checked by the caller.
//
extern "C" void WetFramePower(const double* first, int stride, int frames,
    const double* window, const double* twiddle, const int* reversed, double* powers)
{
    constexpr int x_length = 4096;
    std::fill_n(powers, x_length/2+1, 0.);
    std::array<std::complex<double>, x_length> data;
    for (int frame = 0; frame < frames; ++frame)
    {
        const double* start = first+frame*stride;
        for (int j = 0; j < x_length; ++j)
        {
            data[reversed[j]] = start[j]*window[j];
        }

        for (int size = 2; size <= x_length; size *= 2)
        {
            for (int k = 0; k < x_length; k += size)
            {
                for (int j = 0; j < size/2; ++j)
                {
                    const int angle = 2*j*(x_length/size);
                    const std::complex<double> w(twiddle[angle], twiddle[angle+1]);
                    const auto a = data[k+j];
                    const auto b = w*data[k+j+size/2];
                    data[k+j] = a+b;
                    data[k+j+size/2] = a-b;
                }
            }
        }

        for (int j = 0; j <= x_length/2; ++j)
        {
            powers[j] += std::norm(data[j]);
        }
    }
}
