// IlluminationScene.h — IlluminationOpts → 非順次レイトレースの系 (illum::Scene)。
//
// 照明タブと光タブの **両方**がこの写像を使う。片方にだけ書くと同じ設定から
// 違う系が組み上がるので、共有ヘルパーとして 1 か所に置く
// (`.claude/rules/gui.md`「タブ間で共有できるヘルパーはコピーせず抽出」)。
//
// I18n キーはここに持たない。追跡できない理由は**識別子**で返し、各タブが
// 自分の接頭辞 (ilm_ / optray_) の文言へ割り当てる。
#pragma once
#include "../optics/IlluminationTrace.h"

namespace ofd {

struct IlluminationOpts;

namespace illum {

// 追跡できない理由の識別子 (成り立つときは nullptr):
//   "raydata" 実測レイデータ  / "bsdf" 実測 BSDF / "elem" TIR・導光板・蛍光体
//   "rays" / "flux" / "focal" / "radius" / "target" / "cells"  (traceBlocker と同じ)
// maxRays は「編集のたびに追跡する」画面向けの本数上限 (0 以下で無制限)。
const char *sceneFromOpts(const IlluminationOpts &o, Scene *sc, long long *nRays,
                          long long maxRays = 200000);

} // namespace illum
} // namespace ofd
