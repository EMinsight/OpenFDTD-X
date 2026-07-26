// Fft.h — 自前 radix-2 反復 FFT (Cooley & Tukey 1965)。Qt 非依存 / C++14。
// 外部 FFT ライブラリは使用しない (ライセンス方針)。
//
// 規約: 順変換 X[k] = Σ x[n]·exp(-j2πkn/N) (正規化なし)、
// 逆変換で 1/N 正規化する (順→逆の往復で恒等)。
// 長さは 2 の冪であること (実信号ヘルパはゼロ詰めで 2 の冪に揃える)。
#pragma once
#include <complex>
#include <cstddef>
#include <vector>

#include "AcousticError.h"
#include "ArrayView.h"

namespace ofd {
namespace acoustics {

// n 以上で最小の 2 の冪 (n = 0 は 1 を返す)
std::size_t nextPowerOfTwo(std::size_t n);

// n が 2 の冪か (0 は false)
bool isPowerOfTwo(std::size_t n);

// in-place 順変換。x.size() が 2 の冪でなければ false を返し x は変更しない。
bool fftForward(std::vector<std::complex<double>> &x);

// in-place 逆変換 (1/N 正規化)。x.size() が 2 の冪でなければ false。
bool fftInverse(std::vector<std::complex<double>> &x);

// 実信号をゼロ詰めして 2 の冪長 (>= max(x.size(), minLength)) にし、
// 複素スペクトル (全長 N) を返す。入力が空なら EmptyInput。
AcousticResult<std::vector<std::complex<double>>>
realFft(ArrayView<const double> x, std::size_t minLength = 0);

} // namespace acoustics
} // namespace ofd
