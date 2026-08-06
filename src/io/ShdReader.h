// ShdReader.h — BELLHOP の音場ファイル (SHDFIL / <case>.shd) の読み取り。
//
// bellhopcxx が RunType 'C'/'S'/'I' (TL 計算) で書き出す固定長レコード
// 形式のバイナリ。レイアウトは bellhopcuda src/mode/tl.cpp WriteHeader() /
// GetRecNum() が正:
//
//   レコード長 = 先頭 int32 LRecl [4 byte 語] × 4 byte
//   rec 0 : int32 LRecl, char[80] Title
//   rec 1 : char[10] PlotType ("rectilin  " / "irregular ")
//   rec 2 : int32 Nfreq, Ntheta, NSx, NSy, NSz, NRz, NRr, ...
//   rec 3..9 : 周波数・方位・音源座標・受波器座標
//   rec 10 + (((isx*NSy + isy)*Ntheta + itheta)*NSz + isz)*NRz_per_range + irz
//          : complex<float32> × NRr (受波器距離方向の複素音圧)
//
// GUI は 2 次元 (音源 1 点・方位 1 本) の断面だけを使うので、
// 音源/方位インデックスは 0 に固定して深度×距離の TL 行列を作る。
// 受波器の座標は .env を書いたのが GUI 自身なので、そちらの
// (0..depth, 0..range) 等間隔を使う (レコード 8/9 の実数型は
// ビルド構成 (BHC_USE_FLOATS) で float/double が変わるため読まない)。
#pragma once
#include <QString>
#include <QVector>

namespace ofd {

struct ShdField {
    QString title;
    QString plotType;
    int nfreq = 0, ntheta = 0, nsx = 0, nsy = 0, nsz = 0, nrz = 0, nrr = 0;
    // 伝搬損失 TL = -20 log10 |p| [dB]。nrz 行 × nrr 列、行 0 = 海面側。
    // 音圧 0 (レイが届かない領域) は +inf ではなく kNoField を入れる。
    QVector<float> tl_dB;
    double minTL = 0.0, maxTL = 0.0;   // 有効値の範囲 (kNoField を除く)

    static constexpr float kNoField = 999.0f;

    bool isValid() const
    {
        return nrz > 0 && nrr > 0 && tl_dB.size() == qsizetype(nrz) * nrr;
    }
};

class ShdReader {
public:
    // <case>.shd を読み、音源 #0・方位 #0 の TL 断面を作る。
    static bool read(const QString &path, ShdField &out, QString *err = nullptr);
};

} // namespace ofd
