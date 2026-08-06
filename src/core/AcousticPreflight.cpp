// AcousticPreflight.cpp — 外部音響ソルバー起動前の入力点検。
#include "AcousticPreflight.h"

#include "Project.h"
#include "../I18n.h"

namespace ofd {
namespace preflight {

namespace {
const bool s_i18n = [] {
    I18n::reg("pfl_mesh",
        "ソルバ領域 (メッシュ) が不正です。③ ソルバ領域タブで "
        "X/Y/Z の範囲と分割数を設定してください。",
        "The solver region (mesh) is invalid. Set the X/Y/Z extents and "
        "divisions in the Solver region tab.");
    I18n::reg("pfl_nofeed",
        "音源 (feed) がありません。④ 音源 (励振) タブで点音源を追加するか、"
        "🎤 音源/WAV/指向性 タブの「⚡ 有効な音源をソルバ波源へ反映」を"
        "使ってください。",
        "There is no source (feed). Add a point source in the Sources "
        "(excitation) tab, or use “Apply enabled sources to solver feeds” in "
        "the Source/WAV/Directivity tab.");
    I18n::reg("pfl_nopoint",
        "受音点 (観測点) がありません。④ 音源 (励振) タブの「観測点」に "
        "1 つ以上追加してください (RIR はここで受音します)。",
        "There is no receiver (observation point). Add at least one in the "
        "“Observation points” list of the Sources tab — that is where the RIR "
        "is picked up.");
    I18n::reg("pfl_feed_out",
        "音源 #%1 の位置 (%2, %3, %4) が室の外です。室は "
        "X [%5, %6] · Y [%7, %8] · Z [%9, %10] m です — 位置を室内へ直すか、"
        "③ ソルバ領域を広げてください。",
        "Source #%1 at (%2, %3, %4) is outside the room. The room is "
        "X [%5, %6] · Y [%7, %8] · Z [%9, %10] m — move it inside or enlarge "
        "the solver region.");
    I18n::reg("pfl_point_out",
        "受音点 #%1 の位置 (%2, %3, %4) が室の外です。室は "
        "X [%5, %6] · Y [%7, %8] · Z [%9, %10] m です。",
        "Receiver #%1 at (%2, %3, %4) is outside the room. The room is "
        "X [%5, %6] · Y [%7, %8] · Z [%9, %10] m.");
    I18n::reg("pfl_cells",
        "セル総数 %1 がソルバーの上限 %2 を超えています。③ ソルバ領域の"
        "分割数を減らす (刻みを粗くする) か、室を小さくしてください。",
        "The cell count %1 exceeds the solver limit %2. Reduce the divisions "
        "(coarser grid) in the Solver region tab, or shrink the room.");
    return true;
}();

QString num(double v)
{
    return QString::number(v, 'g', 6);
}
} // namespace

long long maxCells() { return 30000000LL; }

QStringList acousticRunProblems(const Project &p)
{
    QStringList out;

    // 1) メッシュ
    bool meshOk = true;
    for (int a = 0; a < 3; ++a)
        if (!p.mesh(a).isValid()) meshOk = false;
    if (!meshOk) {
        out << I18n::tr("pfl_mesh");
        return out;    // 範囲が取れないので以降の判定は行わない
    }

    const double lo[3] = { p.mesh(0).min(), p.mesh(1).min(), p.mesh(2).min() };
    const double hi[3] = { p.mesh(0).max(), p.mesh(1).max(), p.mesh(2).max() };

    // 2) 音源 / 3) 受音点の有無
    if (p.feeds().isEmpty()) out << I18n::tr("pfl_nofeed");
    if (p.probes().isEmpty()) out << I18n::tr("pfl_nopoint");

    // 4) 音源が室内か
    const auto outside = [&lo, &hi](double x, double y, double z) {
        const double v[3] = { x, y, z };
        for (int a = 0; a < 3; ++a)
            if (v[a] < lo[a] || v[a] > hi[a]) return true;
        return false;
    };
    const auto rangeArgs = [&lo, &hi](QString s) {
        return s.arg(num(lo[0]), num(hi[0]), num(lo[1]), num(hi[1]),
                     num(lo[2]), num(hi[2]));
    };
    for (int i = 0; i < p.feeds().size(); ++i) {
        const Feed &f = p.feeds().at(i);
        if (!outside(f.x, f.y, f.z)) continue;
        out << rangeArgs(I18n::tr("pfl_feed_out")
                             .arg(QString::number(i + 1), num(f.x), num(f.y),
                                  num(f.z)));
    }
    // 5) 受音点が室内か
    for (int i = 0; i < p.probes().size(); ++i) {
        const Probe &r = p.probes().at(i);
        if (!outside(r.x, r.y, r.z)) continue;
        out << rangeArgs(I18n::tr("pfl_point_out")
                             .arg(QString::number(i + 1), num(r.x), num(r.y),
                                  num(r.z)));
    }

    // 6) セル総数
    const long long cells = static_cast<long long>(p.mesh(0).totalCells()) *
                            p.mesh(1).totalCells() * p.mesh(2).totalCells();
    if (cells > maxCells())
        out << I18n::tr("pfl_cells").arg(QString::number(cells),
                                         QString::number(maxCells()));
    return out;
}

} // namespace preflight
} // namespace ofd
