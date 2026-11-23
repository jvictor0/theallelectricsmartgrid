// Binary64 smooth-map diagnostics for the rational-saturation survey model.
// Coefficients and forcing are the exact binary32 values exported by the GPU host.
// This differentiates the real-valued recurrence, not floating-point rounding.
//
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
double Saturation(double x)
{
    return std::clamp(x*(x*x+27)/std::fma(x*9,x,27),-1.0,1.0);
}

double Derivative(double x)
{
    if (std::abs(x) >= 3)
    {
        return 0;
    }

    const double square = x*x;
    return (square-9)*(square-9)/(9*(square+3)*(square+3));
}
}

extern "C" void LadderRationalCycle(const double* p, const double* input, int count,
    double* state, double* jacobian, double* output)
{
    std::fill_n(jacobian,16,0.0);
    for (int i = 0; i < 4; ++i)
    {
        jacobian[4*i+i] = 1;
    }

    for (int n = 0; n < count; ++n)
    {
        state[0] = std::fma(p[0],input[n]-p[2]*state[3],p[1]*state[0]);
        for (int col = 0; col < 4; ++col)
        {
            jacobian[col] = p[1]*jacobian[col]-p[0]*p[2]*jacobian[12+col];
        }

        for (int row = 1; row < 4; ++row)
        {
            const double x = p[3]*state[row-1];
            const double slope = p[0]*p[3]*Derivative(x)/p[4];
            state[row] = std::fma(p[0],Saturation(x)/p[4],p[1]*state[row]);
            for (int col = 0; col < 4; ++col)
            {
                jacobian[4*row+col] = p[1]*jacobian[4*row+col]+slope*jacobian[4*(row-1)+col];
            }
        }

        output[n] = (1+p[2])*Saturation(p[3]*state[3])/p[4];
    }
}
