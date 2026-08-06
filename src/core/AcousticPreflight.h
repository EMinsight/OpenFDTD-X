// AcousticPreflight.h — 外部音響ソルバーを起動する前の入力点検。
//
// ソルバー (OpenAcoustics の ofdx_acoustic_fdtd / ofdx_acoustic_ga) は
// 契約どおり不正入力を非零終了で弾くが、その理由はソルバーログにしか
// 出ない。GUI 側で同じ条件を先に見て、**起動する前に**画面で直せる形の
// メッセージにするのがここの役目。
//
// 実際に踏んだ例: 音源リストの既定位置 (L_main = (-3, 4.5, 5) — 大ホールの
// ステレオ配置) を「⚡ ソルバ波源へ反映」でそのまま feed にすると、5×4×3 m の
// 部屋では室外になり `feed position ... is outside the room` で落ちる。
//
// Qt には依存するが GUI (Widgets) には依存しない — Project を読むだけの
// 純関数なので selftest から直接検証できる。
#pragma once
#include <QString>
#include <QStringList>

namespace ofd {

class Project;

namespace preflight {

// ソルバー起動前の点検。空なら起動してよい。
// 返るのは利用者向けの文で、**どのタブで直すかを文中に書く** (押した場所に
// 出したいので、呼び出し側でタブを自動で切り替えたりはしない — 切り替えると
// 理由のメッセージ自体が見えなくなる)。
// 検査項目 (いずれもソルバー側が非零終了する条件と対応):
//   1. メッシュ (ソルバ領域) が不正
//   2. 音源 (feed) が 1 つも無い
//   3. 受音点 (観測点 point) が 1 つも無い
//   4. 音源が室外にある (座標と室の範囲を併記)
//   5. 受音点が室外にある
//   6. セル総数が 3,000 万を超える (OpenAcoustics の上限)
QStringList acousticRunProblems(const Project &p);

// セル総数の上限 (OpenAcoustics の ac_setup / ga_setup と同じ値)
long long maxCells();

} // namespace preflight
} // namespace ofd
