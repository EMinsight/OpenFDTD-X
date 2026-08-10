// PostPrereq.h — ポスト処理のチェック項目が「実際に作図を生むか」の判定
//                (Qt Core のみ / GUI 非依存)
//
// ポストタブのチェックは `.ofd` のキーになり、`ofd_post` がそれを見て作図する。
// しかしカーネル側には**チェックとは別の前提条件**があり、満たしていなければ
// チェックが入っていても図は 1 枚も出ない。実例:
//
//   RCS (平面波入射) のプロジェクトは給電点が無いので、スミスチャート・
//   入力インピーダンス・入力アドミタンス・反射係数は**全部出ない**。
//   観測点が無ければ S パラメータ・結合係数も出ない。
//   それでも GUI はチェックを受け付けて何も言わないため、「チェックしたのに
//   反映されない」ように見える (実際にそう報告された)。
//
// ここはその前提条件を GUI 側に写したもの。**出典は本家 OpenFDTD の
// post/ のコードそのもの**で、下記の行を根拠にしている:
//
//   post/post.c:21        plotiter  → 無条件
//   post/post.c:25        plotfeed  → Pfeed
//   post/plot2dFeed.c:14    〃      → NFeed > 0 かつ Ntime >= 2
//   post/post.c:29        plotpoint → Ppoint
//   post/plot2dPoint.c:14   〃      → NPoint > 0 かつ Ntime >= 2
//   post/post.c:33        plot2dFreq() は NFreq1 > 0 のときだけ呼ばれる
//   post/plot2dFreq.c:29    smith   → NFeed > 0
//   post/plot2dFreq.c:36    zin     → NFeed > 0
//   post/plot2dFreq.c:43    yin     → NFeed > 0
//   post/plot2dFreq.c:50    ref     → NFeed > 0
//   post/plot2dFreq.c:57    spara   → **NPoint > 0** (給電点ではない)
//   post/plot2dFreq.c:64    coupling→ NFeed > 0 かつ NPoint > 0
//   post/post.c:37        遠方界・近傍界は **NFreq2 > 0** のときだけ
//   post/post.c:38-56     さらに各項目の行数 (far1d/near1d/near2d) が要る
//
// **カーネル側を直したらここも直す。** 判定がずれると、出ない図を「出る」と
// 言うか、出る図を「出ない」と言うことになり、どちらも害がある。
#ifndef OFD_CORE_POSTPREREQ_H
#define OFD_CORE_POSTPREREQ_H

namespace ofd {

class Project;

// ポストタブのチェック項目
enum class PostItem {
    Iter,       // 収束状況        (plotiter)
    Feed,       // 給電点波形      (plotfeed)
    Point,      // 観測点波形      (plotpoint)
    Smith,      // スミスチャート  (plotsmith)
    Zin,        // 入力インピーダンス (plotzin)
    Yin,        // 入力アドミタンス   (plotyin)
    Ref,        // 反射係数        (plotref)
    Spara,      // S パラメータ    (plotspara)
    Coupling,   // 結合係数        (plotcoupling)
    Far0d,      // 遠方界周波数特性 (plotfar0d)
    Far1d,      // 遠方界指向性     (plotfar1d)
    Far2d,      // 遠方界全方向     (plotfar2d)
    Near1d,     // 近傍界 1D        (plotnear1d)
    Near2d,     // 近傍界 2D        (plotnear2d)
};

// 出力を妨げているもの (None = このプロジェクトなら図が出る)
enum class PostBlocker {
    None = 0,
    NoFeed,          // 給電点 (feed) が無い
    NoPoint,         // 観測点 (point) が無い
    NoFeedAndPoint,  // 両方要るのに両方無い
    NoFreq1,         // frequency1 が無い (周波数特性が出ない)
    NoFreq2,         // frequency2 が無い (遠方界・近傍界が出ない)
    NoEntry,         // その項目の行が 1 つも無い (far1d / near1d / near2d)
};

// 判定に要るプロジェクトの数量だけを抜き出したもの (selftest から直接叩ける)
struct PostInputs {
    int  feeds = 0;         // .ofd の feed 行の数   → NFeed
    int  probes = 0;        // .ofd の point 行の数  → NPoint
    int  freq1Points = 0;   // frequency1 の点数     → NFreq1 (div + 1、キー無しは 0)
    int  freq2Points = 0;   // frequency2 の点数     → NFreq2
    int  far1dCount = 0;    // plotfar1d の行数      → NFar1d
    int  near1dCount = 0;   // plotnear1d の行数     → NNear1d
    int  near2dCount = 0;   // plotnear2d の行数     → NNear2d
    bool far0d = false;     // plotfar0d が有効か    → IFar0d
    bool far2d = false;     // plotfar2d が有効か    → NFar2d
};

// プロジェクトから数量を取る (GUI から使う入口)
PostInputs postInputsOf(const Project &p);

// item がこのプロジェクトで作図されるか。されないなら理由を返す。
PostBlocker postBlocker(PostItem item, const PostInputs &in);

// 便宜: 妨げが無い = 図が出る
inline bool postWillPlot(PostItem item, const PostInputs &in)
{
    return postBlocker(item, in) == PostBlocker::None;
}

} // namespace ofd

#endif // OFD_CORE_POSTPREREQ_H
