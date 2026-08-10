// PostPrereq.cpp — ポスト作図の前提条件 (出典と対応は PostPrereq.h)
#include "PostPrereq.h"
#include "Project.h"

namespace ofd {

PostInputs postInputsOf(const Project &p)
{
    PostInputs in;
    in.feeds  = int(p.feeds().size());
    in.probes = int(p.probes().size());

    // NFreq1 / NFreq2 = 分割数 + 1 (OpenFDTD sol/input_data.c:420)。
    // キー自体が無いファイルでは 0 点 = その系統の作図は出ない。
    const GeneralOpts &g = p.general();
    in.freq1Points = g.hasF1 ? (g.f1div + 1) : 0;
    in.freq2Points = g.hasF2 ? (g.f2div + 1) : 0;

    const PostOpts &po = p.post();
    in.far1dCount  = int(po.far1d.size());
    in.near1dCount = int(po.near1d.size());
    in.near2dCount = int(po.near2d.size());
    in.far0d = po.far0d;
    in.far2d = po.far2d;
    return in;
}

PostBlocker postBlocker(PostItem item, const PostInputs &in)
{
    const bool feed  = (in.feeds > 0);
    const bool point = (in.probes > 0);
    const bool f1    = (in.freq1Points > 0);
    const bool f2    = (in.freq2Points > 0);

    switch (item) {
    case PostItem::Iter:
        // post/post.c:21 — 収束履歴は常に出る
        return PostBlocker::None;

    case PostItem::Feed:
        // post/plot2dFeed.c:14 — NFeed > 0
        return feed ? PostBlocker::None : PostBlocker::NoFeed;

    case PostItem::Point:
        // post/plot2dPoint.c:14 — NPoint > 0
        return point ? PostBlocker::None : PostBlocker::NoPoint;

    case PostItem::Smith:
    case PostItem::Zin:
    case PostItem::Yin:
    case PostItem::Ref:
        // post/plot2dFreq.c:29/36/43/50 — NFeed > 0、かつ post.c:33 で NFreq1 > 0
        if (!feed) return PostBlocker::NoFeed;
        return f1 ? PostBlocker::None : PostBlocker::NoFreq1;

    case PostItem::Spara:
        // post/plot2dFreq.c:57 — **NPoint** > 0 (給電点ではない点に注意)
        if (!point) return PostBlocker::NoPoint;
        return f1 ? PostBlocker::None : PostBlocker::NoFreq1;

    case PostItem::Coupling:
        // post/plot2dFreq.c:64 — NFeed > 0 かつ NPoint > 0
        if (!feed && !point) return PostBlocker::NoFeedAndPoint;
        if (!feed)  return PostBlocker::NoFeed;
        if (!point) return PostBlocker::NoPoint;
        return f1 ? PostBlocker::None : PostBlocker::NoFreq1;

    case PostItem::Far0d:
        // post/post.c:37-38 — NFreq2 > 0 かつ IFar0d
        if (!f2) return PostBlocker::NoFreq2;
        return in.far0d ? PostBlocker::None : PostBlocker::NoEntry;

    case PostItem::Far1d:
        if (!f2) return PostBlocker::NoFreq2;
        return (in.far1dCount > 0) ? PostBlocker::None : PostBlocker::NoEntry;

    case PostItem::Far2d:
        if (!f2) return PostBlocker::NoFreq2;
        return in.far2d ? PostBlocker::None : PostBlocker::NoEntry;

    case PostItem::Near1d:
        if (!f2) return PostBlocker::NoFreq2;
        return (in.near1dCount > 0) ? PostBlocker::None : PostBlocker::NoEntry;

    case PostItem::Near2d:
        if (!f2) return PostBlocker::NoFreq2;
        return (in.near2dCount > 0) ? PostBlocker::None : PostBlocker::NoEntry;
    }
    return PostBlocker::None;
}

} // namespace ofd
