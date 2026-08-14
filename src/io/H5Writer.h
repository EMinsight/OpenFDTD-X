// H5Writer.h — HDF5 export of the project description + convergence data.
//
// このフォークのソルバーは ofd.out と並行して HDF5 時系列を直接出力できる
// (sol/solve.c)。GUI 側の H5Writer は補助的に「プロジェクト定義+収束履歴」を
// .h5 へ書き出す。-DUSE_HDF5=ON のときのみ有効。
#pragma once
#include <QString>
#include <QVector>

#include "KernelResultReader.h"

namespace ofd {

class Project;

class H5Writer {
public:
    static bool available();
    static bool write(const QString &path, const Project &project,
                      const QVector<int> &steps,
                      const QVector<double> &eAvg,
                      const QVector<double> &hAvg,
                      QString *err = nullptr);

    // ── アンテナ特性 (給電点掃引 + 遠方界パターン) の書き出し ──────────────
    // AntennaCharTab の CSV 書出と **同じ読み取り結果** (io/KernelResultReader)
    // をそのまま HDF5 へ入れる。数値を作り直さないので、共通する列は CSV と
    // 必ず一致する (単一の出所)。CSV に無い偏波成分 (E-theta / E-phi) は
    // far1d.log に列があるときだけ足す。
    //
    // 構成 (自己記述的にするため単位は属性で持たせる):
    //   /                     format="OpenFDTD-X antenna pattern", version=1
    //   /feed/feed<N>/        属性 feed_index, z0[ohm]
    //       frequency, rin, xin, ref_db, vswr        (各 1 次元, 属性 units)
    //   /pattern/cut<N>/      属性 plane (文字列), frequency_hz
    //       angle_deg, e_abs_db [, e_theta_db, e_phi_db]
    //
    // 中身が空 (給電点も切断面も無い) なら **書かずに false** を返す
    // (「書けた」と言える中身が無いのに 0 バイトのファイルを残さない)。
    static bool writeAntennaPattern(const QString &path,
                                    const QVector<FeedSweep> &feeds,
                                    const QVector<FarPattern> &cuts,
                                    const QString &projectName,
                                    QString *err = nullptr);
};

} // namespace ofd
