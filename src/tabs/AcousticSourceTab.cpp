// AcousticSourceTab.cpp
#include "AcousticSourceTab.h"
#include "../core/Project.h"
#include "../acoustics/core/ArrayDirectivity.h"
#include "../acoustics/qt/QtAcousticAdapter.h"
#include "../audio/AudioEditEngine.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTabWidget>
#include <QThread>
#include <QVBoxLayout>
#include <cmath>
#include <memory>
#include <vector>

using namespace ofd;

// ── file-local vocabulary (asrc_) ───────────────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // sub tabs
    I18n::reg("asrc_tab_sources", "音源リスト", "Sources");
    I18n::reg("asrc_tab_signal", "入力信号 (WAV)", "Input signal (WAV)");
    I18n::reg("asrc_tab_directivity", "指向性", "Directivity");
    I18n::reg("asrc_tab_array", "アレイ・ライン音源", "Array / line sources");
    I18n::reg("asrc_tab_aural", "可聴化 / Auralization", "Auralization");
    // sources
    I18n::reg("asrc_sources_section", "音源一覧", "Sources");
    I18n::reg("asrc_src_hint_uw",
              "ソナー送信源・水中音源を配置。WAV/PCM・チャープ・トーンを直接入力可能。",
              "Place sonar transmitters and underwater sources. WAV/PCM, chirp "
              "and tone signals can be fed directly.");
    I18n::reg("asrc_src_hint_room",
              "AFMG EASE / Odeon 風のスピーカー配置。CLF/GLL指向性ファイルは"
              "パーサ未実装 — ファイル名の記録のみ。",
              "AFMG EASE / Odeon style loudspeaker placement. CLF/GLL "
              "directivity files: parser not implemented — only the file name "
              "is recorded.");
    I18n::reg("asrc_col_name", "名前", "Name");
    I18n::reg("asrc_col_kind", "種類", "Type");
    I18n::reg("asrc_col_pos", "位置 (x,y,z)", "Position (x,y,z)");
    I18n::reg("asrc_col_dir", "向き", "Aim");
    I18n::reg("asrc_col_sig", "信号", "Signal");
    I18n::reg("asrc_kind_card", "カーディオイド", "Cardioid");
    I18n::reg("asrc_kind_omni", "無指向性", "Omni");
    I18n::reg("asrc_kind_bipolar", "バイポーラ", "Bipolar");
    I18n::reg("asrc_kind_directional", "指向性", "Directional");
    I18n::reg("asrc_add_row", "＋ 音源を追加…", "+ Add source…");
    I18n::reg("asrc_btn_addfile", "＋ ファイルから追加", "+ Add from file");
    I18n::reg("asrc_btn_delrow", "− 選択行を削除", "− Remove selected");
    I18n::reg("asrc_btn_clflib", "CLF/GLL ライブラリ", "CLF/GLL library");
    // ボタンが「押せない理由」を文言とツールチップの両方で示す
    // (無効化されているだけでは理由が分からない — CLAUDE.md 絶対規則 5)
    I18n::reg("asrc_btn_clflib_ni", "CLF/GLL (未実装)",
              "CLF/GLL (not impl.)");
    I18n::reg("asrc_clflib_note",
              "▸ CLF/GLL パーサは未実装です。指向性は「種別」列の"
              "解析モデル (無指向 / カーディオイド / 指向性) を"
              "使ってください。",
              "▸ The CLF/GLL parser is not implemented. Use the analytic "
              "directivity models in the \"Kind\" column "
              "(omni / cardioid / directional) instead.");
    I18n::reg("asrc_clflib_tip",
              "CLF/GLL パーサは未実装です — 指向性は解析モデル "
              "(無指向 / カーディオイド等) を使ってください。"
              "下の「CLF/GLL ファイル」欄はファイル名を記録するだけで、"
              "指向性データは読み込みません。",
              "The CLF/GLL parser is not implemented — use an analytic "
              "directivity model (omni / cardioid / …) instead. The "
              "\"CLF/GLL file\" field below only records a file name; no "
              "directivity data is read from it.");
    // 信号 (WAV) の選択導線 — 「信号」列は自由記述だがファイル選択の導線が
    // 無かったため、選択行に対してファイルを割り当てられるようにする
    I18n::reg("asrc_btn_sigpick", "🎵 信号 (WAV) を選択…",
              "🎵 Choose signal (WAV)…");
    I18n::reg("asrc_btn_sigclear", "信号を解除", "Clear signal");
    I18n::reg("asrc_sig_title", "音源の入力信号 (WAV)",
              "Source input signal (WAV)");
    I18n::reg("asrc_sig_filter",
              "音声ファイル (*.wav *.flac *.aiff *.ogg);;すべてのファイル (*)",
              "Audio files (*.wav *.flac *.aiff *.ogg);;All files (*)");
    I18n::reg("asrc_sig_norow",
              "音源の行が選択されていません — 信号を設定したい行をクリックして"
              "選んでから、もう一度押してください (最終行の「＋ 音源を追加…」は"
              "対象外です)。",
              "No source row is selected — click the row you want to set the "
              "signal for, then press again (the trailing “+ Add source…” row "
              "does not count).");
    I18n::reg("asrc_sig_noclear",
              "この音源には信号が設定されていません (解除するものがありません)。",
              "This source has no signal set (nothing to clear).");
    I18n::reg("asrc_sig_missing", "ファイルが見つかりません: %1",
              "File not found: %1");
    I18n::reg("asrc_sig_btn_tip",
              "選択した行の「信号」列に音声ファイル (WAV 等) を設定します。"
              "設定した信号は「🔊 可聴化」タブの「🎵 音源リストから」で"
              "ドライ音源として取り込めます (信号自体はソルバへは渡りません)。",
              "Sets an audio file (WAV etc.) as the “Signal” of the selected "
              "row. A signal set here can be picked up as the dry source with "
              "“🎵 From source list” in the Auralization tab (the signal "
              "itself is not passed to the solver).");
    I18n::reg("asrc_sig_clear_tip",
              "選択した行の「信号」列を空にします。",
              "Clears the “Signal” cell of the selected row.");
    I18n::reg("asrc_btn_preset", "プリセット", "Presets");
    I18n::reg("asrc_col_spl_room", "SPL@1m [dB]", "SPL@1m [dB]");
    I18n::reg("asrc_col_spl_uw", "SL [dB re μPa·m]", "SL [dB re μPa·m]");
    I18n::reg("asrc_new_src", "新規音源", "New source");
    // 音源一覧はモデル (.ofdx) に保存されるが、まだソルバー入力には
    // 渡していない — 何が有効かを正しく述べる (絶対規則 5)
    I18n::reg("asrc_count_fmt", "有効な音源 %1 / %2 件。",
              "%1 of %2 sources enabled.");
    I18n::reg("asrc_count_none",
              "1 件も有効でないため、可聴化・指標とも計算できません。",
              "None are enabled, so neither auralisation nor the metrics "
              "can be computed.");
    I18n::reg("asrc_count_ok",
              "室内応答は音源ごとに独立に求めて重ね合わせるので、"
              "計算量は音源数に比例します (急に破綻はしません)。",
              "Room responses are computed per source and summed, so the "
              "cost grows linearly with the source count.");
    I18n::reg("asrc_count_many",
              "%1 件は多めです — 可聴化は音源ごとに IR 畳み込みを行うため、"
              "書き出し時間とメモリが音源数に比例して増えます。"
              "確認用には一部だけ有効にすることを勧めます。",
              "%1 is a lot — auralisation convolves an IR per source, so "
              "render time and memory grow with the count. Enable a "
              "subset while checking.");
    I18n::reg("asrc_src_model_note",
              "▸ 音源一覧はプロジェクト (.ofdx) に保存され、次回読み込み時に"
              "復元されます。現在の音響計算 (統計推定・ソルバー連携) は"
              "「音響」タブの単一音源設定 (位置・基準SPL) を使うため、"
              "この一覧が自動でカーネル入力へ渡されることはありません "
              "(位置のみ、下の反映ボタンで明示的に波源へ書き込めます)。",
              "▸ The source list is stored in the project (.ofdx) and restored "
              "on reload. The present acoustic computations (statistical "
              "estimate / solver hand-off) use the single source (position and "
              "reference SPL) from the Acoustic tab — this list is never "
              "passed to the kernel input automatically (only the positions "
              "can be written into the feeds explicitly with the apply button "
              "below).");
    // 音源リスト → ソルバ波源 (feed) への反映導線 (室内音響のみ)
    I18n::reg("asrc_btn_sync", "⚡ 有効な音源をソルバ波源へ反映",
              "⚡ Apply enabled sources to solver feeds");
    I18n::reg("asrc_sync_title", "ソルバ波源へ反映", "Apply to solver feeds");
    I18n::reg("asrc_sync_confirm",
              "現在の波源 %1 個を、音源リストの有効な音源 %2 個で"
              "置き換えます。\n\n反映されるのは位置のみです — 指向性・WAV・"
              "レベルはソルバへは渡りません (可聴化・解析側で使用します)。"
              "振幅・位相・内部抵抗は既定値になります。よろしいですか？",
              "Replace the current %1 feed(s) with the %2 enabled source(s) of "
              "this list?\n\nOnly the positions are applied — directivity, WAV "
              "and level are not passed to the solver (they are used by "
              "auralization / analysis). Amplitude, phase and internal "
              "impedance are set to their defaults.");
    I18n::reg("asrc_sync_outside",
              "⚠ そのうち %1 個はソルバ領域の外です "
              "(室は X [%2, %3] · Y [%4, %5] · Z [%6, %7] m)。"
              "このまま反映するとソルバーが「音源が室外」で停止します — "
              "音源の座標を室内へ直すか、③ ソルバ領域を広げてください。",
              "⚠ %1 of them are outside the solver region "
              "(the room is X [%2, %3] · Y [%4, %5] · Z [%6, %7] m). "
              "Applying them as-is makes the solver stop with “source outside "
              "the room” — move the sources inside or enlarge the region.");
    I18n::reg("asrc_sync_none",
              "有効な音源がありません — 反映するには行の左端のチェックを"
              "有効にしてください。波源 (feed) は変更していません。",
              "No enabled sources — tick the checkbox of the rows to apply. "
              "The feeds were not changed.");
    I18n::reg("asrc_sync_note",
              "▸ ソルバ (計算実行) が使う点音源は「④波源」タブの feed です。"
              "この一覧の指向性・WAV・レベルは可聴化/解析用でソルバへは"
              "渡りません — 上の反映ボタンで有効行の位置だけを feed へ"
              "書き込めます (レベル [dB] は校正が無いため振幅へ換算しません)。",
              "▸ The point sources the solver (run button) uses are the feeds "
              "of the ④ Sources tab. Directivity, WAV and level in this list "
              "are for auralization / analysis and are not passed to the "
              "solver — the apply button above writes only the positions of "
              "the enabled rows into the feeds (level [dB] is not converted "
              "to an amplitude because there is no calibration).");
    I18n::reg("asrc_col_solver", "ソルバ", "Solver");
    I18n::reg("asrc_solver_feed", "ソルバ波源 #%1", "Solver feed #%1");
    I18n::reg("asrc_preset_sonar", "ソナー", "Sonar");
    I18n::reg("asrc_preset_speaker", "スピーカー", "Speakers");
    I18n::reg("asrc_common_section", "共通設定", "Common");
    I18n::reg("asrc_base_spl", "基準SPL @1m", "Reference SPL @1m");
    I18n::reg("asrc_normalize", "正規化", "Normalisation");
    I18n::reg("asrc_clip_prevent", "同時駆動でクリップ防止",
              "Prevent clipping when driven together");
    I18n::reg("asrc_delay", "遅延", "Delay");
    I18n::reg("asrc_dist_comp", "距離自動補正",
              "Automatic distance compensation");
    I18n::reg("asrc_phase", "位相", "Phase");
    I18n::reg("asrc_coherence", "音源間相互コヒーレンス",
              "Inter-source mutual coherence");
    // signal
    I18n::reg("asrc_signal_section", "入力信号", "Source signal");
    I18n::reg("asrc_sig_kind", "信号種別", "Signal type");
    I18n::reg("asrc_sig_impulse", "インパルス", "Impulse");
    I18n::reg("asrc_sig_sweep", "スイープ (TSP/ESS)", "Sweep (TSP/ESS)");
    I18n::reg("asrc_sig_chirp", "チャープ", "Chirp");
    I18n::reg("asrc_sig_tone", "正弦波", "Sine tone");
    I18n::reg("asrc_sig_noise", "ノイズ", "Noise");
    I18n::reg("asrc_sig_wav", "WAV ファイル", "WAV file");
    I18n::reg("asrc_wav_section", "WAV ファイル入力", "Audio file");
    I18n::reg("asrc_file", "ファイル", "File");
    I18n::reg("asrc_browse", "📁 参照…", "📁 Browse…");
    I18n::reg("asrc_listen", "▶ 試聴", "▶ Preview");
    I18n::reg("asrc_prep_note",
              "▸ トリム・ゲイン・ハイパスは「トリム → HPF → ゲイン」の順で"
              "適用され、下の波形プレビューと、可聴化 (単発・一括・音響タブ) が"
              "畳み込むドライ音源の両方に効く。"
              "トリムは終了 ≤ 開始のとき全長を使う。HPF は 2 次バタワース "
              "(RBJ biquad, Q=1/√2)。",
              "▸ Trim, gain and the high-pass filter are applied in the order "
              "trim → HPF → gain, and affect both the waveform preview "
              "below and the dry source convolved by the auralisation (single, "
              "batch and the acoustics tab alike). The trim is "
              "ignored when the end is not after the start. The HPF is a "
              "2nd-order Butterworth (RBJ biquad, Q=1/√2).");
    I18n::reg("asrc_prep_applied", "前処理あり", "pre-processed");
    I18n::reg("asrc_prep_empty",
              "トリム範囲が信号の外にあるため、前処理後の信号が空になりました "
              "(設定を見直してください)。",
              "The trim range lies outside the signal, so nothing is left after "
              "pre-processing — please revise the settings.");
    I18n::reg("asrc_formats", "対応形式", "Supported formats");
    I18n::reg("asrc_channels", "チャンネル", "Channels");
    I18n::reg("asrc_ch_mono", "モノ → 1音源", "Mono → 1 source");
    I18n::reg("asrc_ch_stereo", "ステレオ → 2音源", "Stereo → 2 sources");
    I18n::reg("asrc_ch_51", "5.1ch → 6音源", "5.1ch → 6 sources");
    I18n::reg("asrc_srate", "サンプリングレート", "Sample rate");
    // ファイル未選択のうちは値を出さない (実測値と誤認させない — 絶対規則 5)
    I18n::reg("asrc_srate_none", "— (ファイル未選択)", "— (no file selected)");
    I18n::reg("asrc_resample", "自動リサンプル", "Auto resample");
    I18n::reg("asrc_loop", "ループ", "Loop");
    I18n::reg("asrc_loop_chk", "ファイル末尾でループ再生",
              "Loop playback at end of file");
    I18n::reg("asrc_trim", "トリミング", "Trim");
    I18n::reg("asrc_gain", "ゲイン", "Gain");
    I18n::reg("asrc_hpf", "ハイパス", "High-pass");
    I18n::reg("asrc_hpf_chk", "DC除去 20Hz", "DC removal 20 Hz");
    I18n::reg("asrc_lib_section", "ライブラリ", "Library");
    I18n::reg("asrc_lib_hint",
              "可聴化・明瞭度評価に使う無響録音 (anechoic recordings) の"
              "推奨素材一覧 (用途の指針)。",
              "Recommended anechoic recording material for auralization and "
              "intelligibility work (guidance, not files).");
    I18n::reg("asrc_col_bundled", "同梱", "Bundled");
    // 音源データを同梱していない事実と入手先・登録方法を述べる
    I18n::reg("asrc_lib_note",
              "▸ 音源ファイルは同梱していません (再配布ライセンスのため) — "
              "上表は用途別の推奨素材の一覧です。無響録音は配布元 "
              "(EBU SQAM / Openair (Univ. of York) / Bang & Olufsen 無響録音CD / "
              "ODEON・EASE の無響音源など) から入手し、下のボタンで登録すると"
              "入力信号として使えます。",
              "▸ No audio files are bundled (redistribution licences) — the "
              "table above lists recommended material per use case. Obtain "
              "anechoic recordings from their publishers (EBU SQAM, Openair "
              "(Univ. of York), the Bang & Olufsen anechoic CD, the ODEON / "
              "EASE anechoic sets, …) and register one with the button below "
              "to use it as the input signal.");
    I18n::reg("asrc_lib_register", "📁 ファイルを登録して入力信号にする",
              "📁 Register a file as the input signal");
    I18n::reg("asrc_col_material", "素材", "Material");
    I18n::reg("asrc_col_len", "長さ", "Length");
    I18n::reg("asrc_col_use", "用途", "Use");
    I18n::reg("asrc_lib1", "男性スピーチ (日本語)", "Male speech (Japanese)");
    I18n::reg("asrc_lib1u", "STI評価", "STI evaluation");
    I18n::reg("asrc_lib2", "女性スピーチ (英語)", "Female speech (English)");
    I18n::reg("asrc_lib2u", "明瞭度比較", "Intelligibility comparison");
    I18n::reg("asrc_lib3", "クラシック (オーケストラ片チャネル)",
              "Classical (orchestra, one channel)");
    I18n::reg("asrc_lib3u", "音楽残響評価", "Music reverberation evaluation");
    I18n::reg("asrc_lib4", "ピアノ独奏", "Piano solo");
    I18n::reg("asrc_lib4u", "過渡応答試聴", "Transient response listening");
    I18n::reg("asrc_lib5", "クリック (極短パルス)", "Click (very short pulse)");
    I18n::reg("asrc_lib5u", "IRF直接観察", "Direct IRF observation");
    I18n::reg("asrc_lib6", "MLS / TSP スイープ", "MLS / TSP sweep");
    I18n::reg("asrc_lib6u", "正確なIRF測定", "Accurate IRF measurement");
    I18n::reg("asrc_lib7", "ピンクノイズ", "Pink noise");
    I18n::reg("asrc_lib7u", "RT60 / SPL測定", "RT60 / SPL measurement");
    I18n::reg("asrc_lib8_room", "拍手", "Applause");
    I18n::reg("asrc_lib8u_room", "残響推定", "Reverberation estimate");
    I18n::reg("asrc_lib8_uw", "クジラ鳴音", "Whale call");
    I18n::reg("asrc_lib8u_uw", "生物音響", "Bioacoustics");
    I18n::reg("asrc_preview_section", "信号プレビュー", "Waveform preview");
    I18n::reg("asrc_preview_fail", "読み込み失敗: %1", "Load failed: %1");
    // 未読込時は数値を出さない ("—")
    I18n::reg("asrc_preview_none",
              "RMS: — · Peak: — · Crest factor: —",
              "RMS: — · Peak: — · Crest factor: —");
    I18n::reg("asrc_preview_placeholder",
              "▸ ファイル未選択 — 波形は形状のプレースホルダで、実測値では"
              "ありません。ファイルを選ぶと包絡線と RMS/Peak/Crest を"
              "実読込値で表示します。",
              "▸ No file selected — the waveform is a shape placeholder, not "
              "measured data. Choose a file to show the real envelope and "
              "RMS / Peak / crest factor.");
    // 書式は日英共通 (数値と単位のみ)
    I18n::reg("asrc_preview_stats",
              "RMS: %1 dBFS · Peak: %2 dBFS · Crest factor: %3 dB",
              "RMS: %1 dBFS · Peak: %2 dBFS · Crest factor: %3 dB");
    // WAV 試聴 (外部 CLI プレイヤ — QtMultimedia は使わない)
    I18n::reg("asrc_listen_title", "WAV 試聴", "WAV preview");
    I18n::reg("asrc_play_nofile", "ファイルが見つかりません: %1",
              "File not found: %1");
    I18n::reg("asrc_play_noplayer",
              "再生用プレイヤが見つかりません (PATH を確認)。ffplay / aplay / "
              "afplay のいずれかが必要です — macOS: 標準の afplay または "
              "brew install ffmpeg / Linux: apt install ffmpeg または "
              "alsa-utils。",
              "No audio player found on PATH. One of ffplay / aplay / afplay "
              "is required — macOS: built-in afplay or brew install ffmpeg / "
              "Linux: apt install ffmpeg or alsa-utils.");
    // directivity
    I18n::reg("asrc_dir_section", "指向性パターン", "Directivity");
    I18n::reg("asrc_dir_hint",
              "音源の放射特性。CLF (Common Loudspeaker Format) / GLL (Generic "
              "Loudspeaker Library) / 測定 polar データ / 解析モデルから選択 "
              "(CLF/GLL はパーサ未実装 — ファイル名の記録のみ)。",
              "Radiation characteristics of the source. Choose from CLF (Common "
              "Loudspeaker Format) / GLL (Generic Loudspeaker Library) / "
              "measured polar data / analytic models. (CLF/GLL parser not "
              "implemented — only the file name is recorded.)");
    I18n::reg("asrc_model_section", "モデル選択", "Model");
    I18n::reg("asrc_m_omni", "無指向性", "Omni");
    I18n::reg("asrc_m_card", "カーディオイド", "Cardioid");
    I18n::reg("asrc_m_super", "スーパーカーディオイド", "Supercardioid");
    I18n::reg("asrc_m_hyper", "ハイパーカーディオイド", "Hypercardioid");
    I18n::reg("asrc_m_fig8", "双指向性 (Fig-8)", "Figure-8");
    I18n::reg("asrc_s_piston", "円形ピストン", "Circular piston");
    I18n::reg("asrc_s_horn", "ホーン (CD型)", "Horn (CD type)");
    I18n::reg("asrc_s_line", "ラインアレイ要素", "Line array element");
    I18n::reg("asrc_s_clf", "CLF/GLL ファイル", "CLF/GLL file");
    I18n::reg("asrc_s_measured", "測定 polar (.txt)", "Measured polar (.txt)");
    I18n::reg("asrc_gll_section", "CLF/GLL ファイル", "Loudspeaker file");
    I18n::reg("asrc_origin", "参照点 (origin)", "Reference point (origin)");
    I18n::reg("asrc_o_acoustic", "音響中心", "Acoustic centre");
    I18n::reg("asrc_o_physical", "物理中心", "Physical centre");
    I18n::reg("asrc_o_custom", "カスタム", "Custom");
    I18n::reg("asrc_freqres", "周波数分解能", "Frequency resolution");
    I18n::reg("asrc_f_oct", "1オクターブ", "1 octave");
    I18n::reg("asrc_f_third", "1/3オクターブ", "1/3 octave");
    I18n::reg("asrc_f_sixth", "1/6オクターブ", "1/6 octave");
    I18n::reg("asrc_f_narrow", "狭帯域", "Narrow band");
    I18n::reg("asrc_angres", "角度分解能", "Angular resolution");
    I18n::reg("asrc_angres_val", "5° × 5° (球面)", "5° × 5° (spherical)");
    I18n::reg("asrc_band_section", "周波数別指向性", "Per-band directivity");
    // 帯域表の出所を明示する (解析式の実計算 / ファイル未解析)
    I18n::reg("asrc_band_note_model",
              "一次指向性モデル r(θ) = a + b·cosθ は周波数非依存のため、"
              "全帯域で同じ値になります (表の値は閉形式の実計算: "
              "-6dB 全角 = 2·acos((10^(-6/20)(a+b)−a)/b)、"
              "Q = (a+b)²/(a²+b²/3)、DI = 10·log10 Q)。"
              "実機の帯域別指向性には CLF/GLL または測定 polar が必要です"
              " (パーサ未実装)。",
              "The first-order model r(θ) = a + b·cosθ is frequency "
              "independent, so every band shares the same value (computed in "
              "closed form: -6 dB full angle = 2·acos((10^(-6/20)(a+b)−a)/b), "
              "Q = (a+b)²/(a²+b²/3), DI = 10·log10 Q). Per-band data of a real "
              "loudspeaker needs CLF/GLL or measured polar data (parser not "
              "implemented).");
    I18n::reg("asrc_band_note_file",
              "CLF/GLL・測定 polar はパーサ未実装のため、帯域別の値を算出"
              "できません (「—」)。解析モデル (円形ピストン / ホーン / "
              "ラインアレイ要素) を選ぶと、選択中の一次指向性モデルの理論値を"
              "表示します。",
              "The CLF/GLL and measured-polar parsers are not implemented, so "
              "per-band values cannot be computed (shown as “—”). Selecting an "
              "analytic source (circular piston / horn / line-array element) "
              "shows the theoretical values of the selected first-order model.");
    I18n::reg("asrc_col_band", "帯域", "Band");
    I18n::reg("asrc_col_h6", "水平 -6dB", "Horizontal -6dB");
    I18n::reg("asrc_col_v6", "垂直 -6dB", "Vertical -6dB");
    I18n::reg("asrc_col_q", "Q値", "Q");
    I18n::reg("asrc_polar_section", "可視化", "Polar plot preview");
    I18n::reg("asrc_polar_hint",
              "水平面 (azimuth) ポーラパターン — 一次指向性モデル "
              "r = a + b·cosθ (周波数非依存)",
              "Horizontal (azimuth) polar pattern — first-order model "
              "r = a + b·cosθ (frequency-independent)");
    I18n::reg("asrc_polar_note_clf",
              "CLF/GLL・測定 polar のパーサは未実装 — 表示は選択中の解析モデル"
              " (一次指向性) の理論パターンです。",
              "CLF/GLL and measured-polar parsers are not implemented — the "
              "plot shows the theoretical pattern of the selected analytic "
              "(first-order) model.");
    I18n::reg("asrc_fr_section", "周波数特性",
              "Frequency response (on-axis)");
    I18n::reg("asrc_fr_note_model",
              "一次指向性モデルの軸上応答は定義上フラット (0 dB, 相対) — "
              "周波数依存性を持ちません。実機の周波数特性を表示するには "
              "CLF/GLL または測定データが必要です (パーサ未実装)。",
              "The on-axis response of a first-order model is flat by "
              "definition (0 dB, relative) — it has no frequency dependence. "
              "Showing a real device response needs CLF/GLL or measured data "
              "(parser not implemented).");
    I18n::reg("asrc_fr_note_file",
              "CLF/GLL・測定 polar は未解析のため、軸上周波数特性を表示"
              "できません (グラフは空)。",
              "CLF/GLL and measured polar data are not parsed, so the on-axis "
              "frequency response cannot be shown (empty plot).");
    // array
    I18n::reg("asrc_array_section", "ラインアレイ (EASE / D&B / L-Acoustics 風)",
              "Line array (EASE / D&B / L-Acoustics style)");
    I18n::reg("asrc_array_hint",
              "複数スピーカーをライン状に配置し、各要素の遅延・ゲインで指向性を制御。"
              "コンサートホール・PA システム設計に必須。",
              "Multiple loudspeakers placed in a line; directivity is controlled "
              "by per-element delay and gain. Essential for concert hall and PA "
              "system design.");
    I18n::reg("asrc_elems", "要素数", "Elements");
    I18n::reg("asrc_spacing", "要素間距離", "Element spacing");
    I18n::reg("asrc_curve", "カーブ", "Curve");
    I18n::reg("asrc_c_straight", "直線", "Straight");
    I18n::reg("asrc_c_j", "Jカーブ", "J-curve");
    I18n::reg("asrc_c_banana", "バナナ", "Banana");
    I18n::reg("asrc_c_custom", "カスタム", "Custom");
    I18n::reg("asrc_splay", "各要素の傾斜", "Per-element splay");
    I18n::reg("asrc_splay_unit", "° (上から)", "° (from top)");
    I18n::reg("asrc_dg_section", "遅延・ゲイン", "Delay & gain");
    I18n::reg("asrc_steer_auto", "自動算出 (受聴範囲指定)",
              "Automatic (specify listening range)");
    I18n::reg("asrc_coverage", "目標カバレッジ", "Target coverage");
    I18n::reg("asrc_uniform", "均一性目標", "Uniformity target");
    I18n::reg("asrc_grating", "蜂巣構造防止 (グレーティングローブ抑制)",
              "Grating lobe suppression");
    I18n::reg("asrc_airabs", "Air absorption 補償",
              "Air absorption compensation");
    I18n::reg("asrc_steer_unit", "° (下向き正)", "° (down positive)");
    // ── 合成された指向性 (実計算) ────────────────────────────────────────
    I18n::reg("asrc_beam_section", "合成された指向性 (鉛直面)",
              "Synthesised directivity (vertical plane)");
    I18n::reg("asrc_beam_hint",
              "上の素子数・素子間隔・splay・ステアリングから遠方界の和を取った"
              "ものです。素子は箱の高さぶんの連続線音源とみなしています。"
              "レベルは最大を 0 dB とした相対値で、絶対 SPL ではありません。",
              "The far-field sum of the element count, spacing, splay and "
              "steering above. Each cabinet is modelled as a continuous line "
              "source as tall as the box spacing. Levels are relative to the "
              "maximum, not absolute SPL.");
    I18n::reg("asrc_beam_freq", "評価周波数", "Evaluation frequency");
    I18n::reg("asrc_beam_x", "角度 [° 下向き正]", "Angle [deg, down positive]");
    I18n::reg("asrc_beam_item", "項目", "Quantity");
    I18n::reg("asrc_beam_value", "値", "Value");
    I18n::reg("asrc_beam_peak", "最大方向", "Peak direction");
    I18n::reg("asrc_beam_hpbw", "主ローブの 3 dB 幅", "Main-lobe 3 dB width");
    I18n::reg("asrc_beam_cov", "−6 dB が及ぶ角度範囲",
              "Angles covered within 6 dB");
    I18n::reg("asrc_beam_sll", "主ローブ外の最大", "Highest lobe outside");
    I18n::reg("asrc_beam_len", "アレイ長 (素子数 × 間隔)",
              "Array length (elements x spacing)");
    I18n::reg("asrc_beam_gl", "グレーティングローブ出現周波数",
              "Grating-lobe onset frequency");
    I18n::reg("asrc_beam_glwarn",
              "評価周波数がグレーティングローブの出現周波数を超えています — "
              "主ビームと同じ高さのローブが別方向に立ちます。素子間隔を "
              "狭めるか、評価周波数を下げてください。",
              "The evaluation frequency is above the grating-lobe onset, so a "
              "lobe as strong as the main beam appears in another direction. "
              "Use a smaller spacing or a lower frequency.");
    I18n::reg("asrc_beam_bad",
              "素子数・素子間隔が数値として読めないため合成できません。",
              "The element count or spacing is not a number, so nothing can be "
              "synthesised.");
    // ── サブアレイ ───────────────────────────────────────────────────────
    I18n::reg("asrc_sub_single",
              "単発なので指向性は生まれません (低域では無指向)。",
              "A single box has no directivity (omnidirectional at low "
              "frequencies).");
    I18n::reg("asrc_sub_fb",
              "遅延に見合う間隔 %1 m (= c·τ) としたときの前後比 %2 dB "
              "(前方 %3 dB / 後方 %4 dB、%5 Hz)。前方が最大になるのは %6 Hz "
              "(間隔 = λ/4) です。",
              "With the spacing %1 m that matches the delay (= c·tau), the "
              "front-to-back ratio is %2 dB (front %3 dB / back %4 dB at "
              "%5 Hz). The front output peaks at %6 Hz (spacing = lambda/4).");
    I18n::reg("asrc_sub_needrev",
              "後方を全周波数で打ち消すには「リアを逆相にする」が要ります "
              "(同相のままだと打ち消せる周波数が限られます)。",
              "Cancelling the rear at every frequency needs the rear box "
              "reversed in polarity; in phase, only some frequencies cancel.");
    I18n::reg("asrc_sub_bad",
              "リア遅延が数値として読めないため計算できません。",
              "The rear delay is not a number, so nothing can be computed.");
    I18n::reg("asrc_sub_section", "サブウーファアレイ", "Sub array");
    I18n::reg("asrc_layout", "配置", "Layout");
    I18n::reg("asrc_l_single", "単発", "Single");
    I18n::reg("asrc_l_endfire", "エンドファイア", "End-fire");
    I18n::reg("asrc_l_cardioid", "カーディオイド", "Cardioid");
    I18n::reg("asrc_l_gradient", "勾配", "Gradient");
    I18n::reg("asrc_rev_rear", "位相反転 (rear)", "Polarity invert (rear)");
    I18n::reg("asrc_rev_chk", "ON (前方放射重視)",
              "ON (front radiation priority)");
    I18n::reg("asrc_delay_rear", "遅延 (rear)", "Delay (rear)");
    // aural
    I18n::reg("asrc_aural_section", "可聴化", "Auralization");
    I18n::reg("asrc_aural_hint",
              "シミュレーション結果のIRF (Impulse Response) と入力WAVを畳み込み、"
              "その空間で聴いた音を再現。<br/>畳み込み処理は可聴化タブで実行 "
              "(このページは設定のみ・未実装)。",
              "Convolves the simulated IRF (impulse response) with the input WAV "
              "to reproduce the sound heard in that space.<br/>Convolution runs "
              "in the Auralization tab (this page is settings only — not "
              "implemented).");
    I18n::reg("asrc_conv_section", "畳み込み設定", "Convolution");
    I18n::reg("asrc_input_wav", "入力WAV", "Input WAV");
    I18n::reg("asrc_recv_irf", "受音点 (IRF)", "Receiver (IRF)");
    I18n::reg("asrc_p1", "P1_center (中央前列)", "P1_center (front centre)");
    I18n::reg("asrc_p2", "P2_left  (左サイド)", "P2_left (left side)");
    I18n::reg("asrc_p3", "P3_back  (後方)", "P3_back (rear)");
    I18n::reg("asrc_p4", "P4_balcony (バルコニー)", "P4_balcony (balcony)");
    I18n::reg("asrc_outch", "出力チャネル", "Output channels");
    I18n::reg("asrc_oc_mono", "モノ", "Mono");
    I18n::reg("asrc_oc_stereo", "ステレオ", "Stereo");
    I18n::reg("asrc_oc_binaural", "バイノーラル (HRTF)", "Binaural (HRTF)");
    I18n::reg("asrc_hrtf", "HRTFデータベース", "HRTF database");
    I18n::reg("asrc_hrtf_personal", "個人化 HRTF (.sofa)",
              "Personalised HRTF (.sofa)");
    I18n::reg("asrc_convmode", "畳み込み方式", "Convolution method");
    I18n::reg("asrc_cv_direct", "直接畳み込み", "Direct convolution");
    I18n::reg("asrc_cv_fft", "FFT (オフライン)", "FFT (offline)");
    I18n::reg("asrc_cv_part", "分割畳み込み (リアルタイム)",
              "Partitioned convolution (real-time)");
    I18n::reg("asrc_render_section", "出力", "Render");
    I18n::reg("asrc_outfile", "出力ファイル", "Output file");
    I18n::reg("asrc_bits", "ビット深度", "Bit depth");
    I18n::reg("asrc_btn_render", "🎧 レンダリング", "🎧 Render");
    I18n::reg("asrc_btn_listen2", "▶ 試聴 (ヘッドホン推奨)",
              "▶ Listen (headphones recommended)");
    I18n::reg("asrc_btn_ab", "A/B 比較 (素音 vs 残響付)",
              "A/B compare (dry vs reverberant)");
    I18n::reg("asrc_norender",
              "レンダリング済みの WAV がありません。畳み込み (レンダリング) は"
              "可聴化 (Auralization) タブで実行してください。",
              "No rendered WAV yet. Run the convolution (render) in the "
              "Auralization tab first.");
    I18n::reg("asrc_ab_handoff",
              "A/B 比較 (dry / wet の書き出しと波形比較) は可聴化 "
              "(Auralization) タブで実行します。入力WAV を可聴化タブの dry "
              "ファイルとして設定しました。",
              "A/B comparison (dry / wet rendering and waveform comparison) "
              "runs in the Auralization tab. The input WAV has been handed "
              "over as the Auralization tab's dry file.");
    I18n::reg("asrc_ab_delegate",
              "A/B 比較 (dry / wet の書き出しと波形比較) は可聴化 "
              "(Auralization) タブで実行します。入力WAV に実在するファイルを"
              "指定すると、ここから dry ファイルとして引き渡します。",
              "A/B comparison (dry / wet rendering and waveform comparison) "
              "runs in the Auralization tab. Point the input WAV to an "
              "existing file to hand it over as the dry file.");
    I18n::reg("asrc_ab_section", "A/B 試聴", "Listening test");
    I18n::reg("asrc_ab_target", "比較対象", "Compare");
    I18n::reg("asrc_ab_dry", "無響原音 (dry)", "Anechoic original (dry)");
    I18n::reg("asrc_ab_wet", "畳み込み済み (wet)", "Convolved (wet)");
    I18n::reg("asrc_ab_revonly", "シミュレーション残響のみ",
              "Simulated reverberation only");
    I18n::reg("asrc_seat", "座席切替", "Seat select");
    I18n::reg("asrc_seat1", "P1 — 中央前列", "P1 — front centre");
    I18n::reg("asrc_seat2", "P2 — サイド席", "P2 — side seat");
    I18n::reg("asrc_seat3", "P3 — 後方", "P3 — rear");
    I18n::reg("asrc_seat4", "P4 — バルコニー", "P4 — balcony");
    I18n::reg("asrc_play", "▶ 再生", "▶ Play");
    I18n::reg("asrc_abx", "ABXテスト", "ABX test");
    I18n::reg("asrc_abx_chk", "二重盲検モード", "Double-blind mode");
    I18n::reg("asrc_quality_section", "可聴化品質指標",
              "Auralization quality metrics");
    I18n::reg("asrc_col_item", "項目", "Item");
    I18n::reg("asrc_col_value", "値", "Value");
    I18n::reg("asrc_col_verdict", "判定", "Verdict");
    I18n::reg("asrc_q_irflen", "IRF長", "IRF length");
    I18n::reg("asrc_q_enough", "十分", "Sufficient");
    I18n::reg("asrc_q_short", "不足 (T30 未満)", "Too short (< T30)");
    I18n::reg("asrc_q_density", "初期反射密度 (0–50 ms)",
              "Early reflection density (0–50 ms)");
    I18n::reg("asrc_q_good", "良好", "Good");
    I18n::reg("asrc_q_lf", "LF (側方音エネルギー)", "LF (lateral energy)");
    I18n::reg("asrc_q_phase", "位相応答", "Phase response");
    // 可聴化品質指標 — 実測 RIR からの実計算 / 未算出の説明
    I18n::reg("asrc_q_btn", "🔄 実測 RIR から算出",
              "🔄 Compute from measured RIR");
    I18n::reg("asrc_q_idle",
              "▸ この表は「RIR 分析」タブで指定した実測 RIR (WAV) から算出"
              "します — 未算出です。LF (側方音エネルギー) は ISO 3382-1 の "
              "2ch 測定 (無指向性 + 双指向性マイク) が必要で、モノ RIR からは"
              "算出できません。位相応答の最小位相補正は未実装です。",
              "▸ This table is computed from the measured RIR (WAV) selected "
              "in the RIR analysis tab — not computed yet. LF (lateral energy) "
              "requires the ISO 3382-1 two-channel measurement (omni + "
              "figure-of-eight) and cannot be derived from a mono RIR. "
              "Minimum-phase correction of the phase response is not "
              "implemented.");
    I18n::reg("asrc_q_done",
              "▸ 実測 RIR %1 の分析結果 (IRF長 / 初期反射密度 / ITDG は実計算)。"
              "LF は 2ch 測定が必要、位相応答補正は未実装のため「—」です。",
              "▸ Computed from the measured RIR %1 (IRF length / early "
              "reflection density / ITDG are real results). LF needs a "
              "two-channel measurement and phase correction is not "
              "implemented, hence “—”.");
    I18n::reg("asrc_q_norir",
              "実測 RIR が未設定です。「RIR 分析」タブで RIR ファイル (WAV) を"
              "指定してください。",
              "No measured RIR is set. Select a RIR file (WAV) in the RIR "
              "analysis tab first.");
    I18n::reg("asrc_q_nofile", "RIR ファイルが見つかりません: %1",
              "RIR file not found: %1");
    I18n::reg("asrc_q_fail", "分析に失敗しました: %1", "Analysis failed: %1");
    I18n::reg("asrc_q_busy", "分析中…", "Analysing…");
    I18n::reg("asrc_q_title", "可聴化品質指標", "Auralization quality metrics");
    I18n::reg("asrc_q_refl_n", "%1 個", "%1 events");
    I18n::reg("asrc_q_lf_need", "2ch 測定が必要 (ISO 3382-1)",
              "needs 2-channel measurement (ISO 3382-1)");
    I18n::reg("asrc_uw_norm", "正規化・遅延・位相のチェック",
              "the normalisation / delay / phase check boxes");
    I18n::reg("asrc_uw_norm_ok", "基準 SPL",
              "the reference SPL");
    I18n::reg("asrc_uw_array",
              "このページの設定 (ソルバ入力に対応するキーがありません)。"
              "画面の「合成された指向性」は、この設定から GUI が実計算した"
              "ものです",
              "the settings on this page (there is no matching key in the "
              "solver input). The synthesised directivity shown above is "
              "computed by the GUI from these very settings");
    I18n::reg("asrc_uw_conv", "畳み込み設定 (レンダリング品質・HRTF・出力形式)",
              "the convolution settings (render quality, HRTF, output format)");
    return true;
}();

// 音響アクセント色 (mock の var(--acc-acoustic))
const QColor kAcc("#2E8B57");

// バッジ風 QLabel (badge / badge ok / badge acc 相当, スタイルは最小限)
QLabel *makeBadge(const QString &text, QWidget *parent,
                  const char *color = nullptr)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(QStringLiteral(
        "QLabel { border: 1px solid palette(mid); border-radius: 3px;"
        " padding: 1px 6px; %1 }")
        .arg(color ? QStringLiteral("color: %1;")
                         .arg(QString::fromLatin1(color))
                   : QString()));
    return l;
}

// 読取専用データ表 (q-table 相当) の共通初期化
void setupTable(QTableWidget *t, const QStringList &headers, int minH)
{
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(22);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(minH);
}

// 値が無いことを表す表示 (捏造値を出さない箇所は必ずこれ)
const QString kDash = QString::fromUtf8("—");

// 座標セル "x, y, z" の解釈 (区切りはカンマ / 空白どちらでも)。
// 3 個の数値として読めたときだけ true を返し、呼び出し側はそのときだけ
// モデルを書き換える (読めない入力でモデルと表示が食い違わないように、
// 呼び出し側は false のときセル表示をモデル値へ戻す)。
bool parsePosText(const QString &text, double *x, double *y, double *z)
{
    const QStringList parts =
        text.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
    if (parts.size() != 3) return false;
    bool ok[3] = { false, false, false };
    const double v0 = parts[0].toDouble(&ok[0]);
    const double v1 = parts[1].toDouble(&ok[1]);
    const double v2 = parts[2].toDouble(&ok[2]);
    if (!ok[0] || !ok[1] || !ok[2]) return false;
    *x = v0; *y = v1; *z = v2;
    return true;
}

QString posText(const AcousticSourceRow &r)
{
    return QStringLiteral("%1, %2, %3")
        .arg(QString::number(r.x_m, 'g', 6), QString::number(r.y_m, 'g', 6),
             QString::number(r.z_m, 'g', 6));
}

// 信号欄がファイルらしいか (自由記述 "chirp 3-5kHz" と区別する)。
// パス区切りを含むか、音声ファイルの拡張子で終わるものをファイルとみなす。
bool looksLikeAudioPath(const QString &s)
{
    const QString t = s.trimmed();
    if (t.isEmpty()) return false;
    if (t.contains(QLatin1Char('/')) || t.contains(QLatin1Char('\\')))
        return true;
    static const char *kExt[] = { ".wav", ".flac", ".aiff", ".aif", ".ogg" };
    for (const char *e : kExt)
        if (t.endsWith(QLatin1String(e), Qt::CaseInsensitive)) return true;
    return false;
}

// 信号セルの表示を整える。ファイルは列が狭いのでファイル名だけを表示し、
// ツールチップにフルパスを出す。実在しないパスはグレー + 「見つかりません」
// (存在しないファイルを設定済みのように見せない)。自由記述はそのまま。
void decorateSignalCell(QTableWidgetItem *it, const QString &signal,
                        const QBrush &missingBrush, const QBrush &normalBrush)
{
    if (!looksLikeAudioPath(signal)) {
        it->setText(signal);
        it->setToolTip(QString());
        it->setForeground(normalBrush);
        return;
    }
    const QFileInfo fi(signal.trimmed());
    it->setText(fi.fileName());
    if (fi.exists() && fi.isFile()) {
        it->setToolTip(fi.absoluteFilePath());
        it->setForeground(normalBrush);
    } else {
        it->setToolTip(ofd::I18n::tr("asrc_sig_missing").arg(signal.trimmed()));
        it->setForeground(missingBrush);
    }
}

// 行の位置と一致する feed の番号 (1 始まり)。無ければ 0。
// 許容誤差 1e-9 m の厳密比較 — 反映ボタンの直後に「ソルバ波源 #n」の対応が
// 見えることが目的なので、これで足りる (近接判定は不要)。
int matchingFeedIndex(const ofd::Project &p, const AcousticSourceRow &r)
{
    const QVector<Feed> &fs = p.feeds();
    for (int i = 0; i < fs.size(); ++i) {
        if (std::fabs(fs[i].x - r.x_m) <= 1e-9 &&
            std::fabs(fs[i].y - r.y_m) <= 1e-9 &&
            std::fabs(fs[i].z - r.z_m) <= 1e-9)
            return i + 1;
    }
    return 0;
}

// ── 一次指向性 r(θ) = a + b·cosθ の閉形式 (ポーラ図・帯域表で共用) ─────────
// 係数は omni / cardioid / super / hyper / fig-8 の順。
const double kAB[5][2] = {
    { 1.0, 0.0 }, { 0.5, 0.5 }, { 0.37, 0.63 }, { 0.25, 0.75 }, { 0.0, 1.0 },
};

// |r(θ)| が軸上値から dropDb 落ちる全角 [deg]。
// そこまで落ちない (omni 等) 場合は 0 を返し、呼び出し側は「—」を表示する。
double beamWidthDeg(double a, double b, double dropDb)
{
    if (b <= 0.0) return 0.0;
    const double c = (std::pow(10.0, -dropDb / 20.0) * (a + b) - a) / b;
    if (c <= -1.0 || c >= 1.0) return 0.0;
    return 2.0 * std::acos(c) * 180.0 / M_PI;
}

// 指向性係数 (回転対称 3D): Q = (a+b)² / (a² + b²/3)
// (∫|a+b·cosθ|² dΩ / 4π = a² + b²/3 — 一次指向性の標準的な閉形式)
double directivityFactor(double a, double b)
{
    return (a + b) * (a + b) / (a * a + b * b / 3.0);
}

QString beamText(double deg)
{
    return deg > 0.0 ? QStringLiteral("%1°").arg(qRound(deg)) : kDash;
}

// WAV を外部 CLI プレイヤで再生する (QtMultimedia は依存に追加しない —
// H5ViewerTab の ffmpeg 探索と同じ流儀で PATH から実行ファイルを探す)。
// ffplay → aplay → afplay の順で最初に見つかったものを非同期起動し、
// どれも無ければ導入方法を案内する。
void playWavExternal(QWidget *parent, const QString &path)
{
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(parent, ofd::I18n::tr("asrc_listen_title"),
                             ofd::I18n::tr("asrc_play_nofile").arg(path));
        return;
    }
    const QString ffplay =
        QStandardPaths::findExecutable(QStringLiteral("ffplay"));
    if (!ffplay.isEmpty()) {
        // ウィンドウを開かず末尾で自動終了
        QProcess::startDetached(ffplay,
            { QStringLiteral("-nodisp"), QStringLiteral("-autoexit"),
              QStringLiteral("-loglevel"), QStringLiteral("quiet"), path });
        return;
    }
    const QString aplay =
        QStandardPaths::findExecutable(QStringLiteral("aplay"));
    if (!aplay.isEmpty()) {
        QProcess::startDetached(aplay, { QStringLiteral("-q"), path });
        return;
    }
    const QString afplay =
        QStandardPaths::findExecutable(QStringLiteral("afplay"));
    if (!afplay.isEmpty()) {
        QProcess::startDetached(afplay, { path });
        return;
    }
    QMessageBox::information(parent, ofd::I18n::tr("asrc_listen_title"),
                             ofd::I18n::tr("asrc_play_noplayer"));
}
} // namespace

// ── PolarPatternView — 一次指向性 r = a + b·cosθ を QPainter で描画 ────────
PolarPatternView::PolarPatternView(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(200, 200);
}

void PolarPatternView::setPattern(double a, double b)
{
    m_a = a;
    m_b = b;
    update();
}

void PolarPatternView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    // viewBox -110..110 → widget 座標
    const double s = qMin(width(), height()) / 220.0;
    p.translate(width() / 2.0, height() / 2.0);
    p.scale(s, s);

    // 同心円 (dashed) + 放射線
    QPen grid(palette().mid().color(), 1);
    grid.setDashPattern({ 2, 2 });
    p.setPen(grid);
    p.setBrush(Qt::NoBrush);
    for (int r : { 20, 40, 60, 80, 100 })
        p.drawEllipse(QPointF(0, 0), double(r), double(r));
    grid.setDashPattern({ 1, 2 });
    p.setPen(grid);
    for (int a : { 0, 30, 60, 90, 120, 150 }) {
        const double rad = a * M_PI / 180.0;
        p.drawLine(QPointF(0, 0),
                   QPointF(100 * std::cos(rad), 100 * std::sin(rad)));
    }

    // 一次指向性 r = 100·|a + b·cosθ| / (a+b)  (θ = 0° を上向きに描く。
    // fig-8 の後方ローブは絶対値で表す — 音圧振幅の極座標表示)
    const double norm = std::max(std::fabs(m_a + m_b), 1e-9);
    QPainterPath path;
    for (int i = 0; i <= 180; ++i) {
        const double th = i * 2.0 * M_PI / 180.0;
        const double r =
            100.0 * std::fabs(m_a + m_b * std::cos(th)) / norm;
        const QPointF pt(r * std::sin(th), -r * std::cos(th));
        if (i == 0) path.moveTo(pt); else path.lineTo(pt);
    }
    path.closeSubpath();
    QColor fill = kAcc;
    fill.setAlphaF(0.3);
    p.setPen(QPen(kAcc, 1.5));
    p.setBrush(fill);
    p.drawPath(path);

    // 角度ラベル
    p.setPen(palette().text().color());
    QFont f = p.font();
    f.setPointSizeF(7.5);
    p.setFont(f);
    p.drawText(QRectF(-20, -100, 40, 12), Qt::AlignCenter, "0°");
    p.drawText(QRectF(70, -3, 40, 12), Qt::AlignCenter, "90°");
}

// ── AcousticSourceTab ───────────────────────────────────────────────────────
AcousticSourceTab::AcousticSourceTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    m_tabs = new QTabWidget(body);
    m_tabs->setDocumentMode(true);
    m_tabs->addTab(buildSourcesPage(),     I18n::tr("asrc_tab_sources"));
    m_tabs->addTab(buildSignalPage(),      I18n::tr("asrc_tab_signal"));
    m_tabs->addTab(buildDirectivityPage(), I18n::tr("asrc_tab_directivity"));
    m_tabs->addTab(buildArrayPage(),       I18n::tr("asrc_tab_array"));
    m_tabs->addTab(buildAuralPage(),       I18n::tr("asrc_tab_aural"));
    v->addWidget(m_tabs);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::loaded, this, &AcousticSourceTab::refresh);
    connect(project, &Project::domainChanged,
            this, &AcousticSourceTab::onDomainChanged);
    onDomainChanged();
    refresh();
}

bool AcousticSourceTab::isUnderwater() const
{
    return m_p->activeDomain() == Domain::Underwater;
}

// ── page: sources ───────────────────────────────────────────────────────────
QWidget *AcousticSourceTab::buildSourcesPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("asrc_sources_section"), page);
    m_srcHint = new QLabel(s);
    m_srcHint->setWordWrap(true);
    s->vbox()->addWidget(m_srcHint);

    m_srcTable = new QTableWidget(0, 9, s);   // 末尾列 = ソルバ波源マーカー
    m_srcTable->setObjectName(QStringLiteral("asrcSourceTable"));
    m_srcTable->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    m_srcTable->horizontalHeader()->setStretchLastSection(true);
    m_srcTable->verticalHeader()->setVisible(false);
    m_srcTable->verticalHeader()->setDefaultSectionSize(22);
    m_srcTable->setMinimumHeight(190);
    s->vbox()->addWidget(m_srcTable);
    // 音源一覧はモデル (.ofdx) と接続済み — どこまで効くかを注記する
    m_srcModelNote = new QLabel(I18n::tr("asrc_src_model_note"), s);
    m_srcModelNote->setWordWrap(true);
    m_srcModelNote->setStyleSheet("font-size:11px; color:palette(mid);");
    s->vbox()->addWidget(m_srcModelNote);
    // 有効な音源の数と、その数が解析にどう効くかを出す。
    // 「音源が多くて大丈夫か」は数が見えないと判断できない。
    m_srcCountNote = new QLabel(s);
    m_srcCountNote->setWordWrap(true);
    m_srcCountNote->setStyleSheet("font-size:11px;");
    s->vbox()->addWidget(m_srcCountNote);

    auto *row = new QHBoxLayout();
    auto *addBtn = new QPushButton(I18n::tr("asrc_btn_addfile"), s);
    auto *delBtn = new QPushButton(I18n::tr("asrc_btn_delrow"), s);
    auto *libBtn = new QPushButton(I18n::tr("asrc_btn_clflib"), s);
    m_presetBtn = new QPushButton(s);
    // CLF/GLL ライブラリはパーサ未実装のまま (ファイル名の記録のみ)。
    // 無効化だけでは「なぜ押せないのか」が分からないので、ボタン文言と
    // ツールチップの両方に理由と代替手段 (解析モデル) を書く。
    tabhelp::markNotImplemented(libBtn);
    libBtn->setText(I18n::tr("asrc_btn_clflib_ni"));
    libBtn->setToolTip(I18n::tr("asrc_clflib_tip"));
    row->addWidget(addBtn);
    row->addWidget(delBtn);
    row->addStretch(1);
    s->vbox()->addLayout(row);
    // ボタンを 1 行に並べると左ペインの幅で文言が切れて読めないため、
    // ライブラリ/プリセットは 2 行目へ分ける。
    auto *row2 = new QHBoxLayout();
    row2->addWidget(libBtn);
    row2->addWidget(m_presetBtn);
    row2->addStretch(1);
    s->vbox()->addLayout(row2);
    // 無効ボタンの理由と代替手段 (ツールチップは隠れて気付かれにくい)
    auto *clfNote = new QLabel(I18n::tr("asrc_clflib_note"), s);
    clfNote->setWordWrap(true);
    clfNote->setStyleSheet("font-size:11px; color:#B8860B;");
    s->vbox()->addWidget(clfNote);

    // 信号 (WAV) の割り当て — 「信号」列は直接編集もできるが、ファイル選択の
    // 導線が無いと実質設定できないため選択行に対するボタンを置く
    auto *sigRow = new QHBoxLayout();
    m_sigPickBtn = new QPushButton(I18n::tr("asrc_btn_sigpick"), s);
    m_sigPickBtn->setObjectName(QStringLiteral("asrcSigPickBtn"));
    m_sigPickBtn->setToolTip(I18n::tr("asrc_sig_btn_tip"));
    m_sigClearBtn = new QPushButton(I18n::tr("asrc_btn_sigclear"), s);
    m_sigClearBtn->setObjectName(QStringLiteral("asrcSigClearBtn"));
    m_sigClearBtn->setToolTip(I18n::tr("asrc_sig_clear_tip"));
    sigRow->addWidget(m_sigPickBtn);
    sigRow->addWidget(m_sigClearBtn);
    sigRow->addStretch(1);
    s->vbox()->addLayout(sigRow);

    // 音源リスト → ソルバ波源 (feed) への反映導線 (室内音響のみ —
    // 水中は BELLHOP が feed を使わないため onDomainChanged() で隠す)
    auto *syncRow = new QHBoxLayout();
    m_syncBtn = new QPushButton(I18n::tr("asrc_btn_sync"), s);
    syncRow->addWidget(m_syncBtn);
    syncRow->addStretch(1);
    s->vbox()->addLayout(syncRow);
    m_syncNote = new QLabel(I18n::tr("asrc_sync_note"), s);
    m_syncNote->setWordWrap(true);
    m_syncNote->setStyleSheet("font-size:11px; color:palette(mid);");
    s->vbox()->addWidget(m_syncNote);
    v->addWidget(s);

    // 表の編集 → モデル (セル単位。不正入力はセル表示をモデル値へ戻す)
    connect(m_srcTable, &QTableWidget::cellChanged, this,
            [this](int r, int c) {
                if (m_updating) return;
                applySourceCell(r, c);
            });
    // 最終行「＋ 音源を追加…」のクリックで 1 行追加 (mock の追加行)
    connect(m_srcTable, &QTableWidget::cellClicked, this, [this](int r, int) {
        if (r == sourceList().size()) {
            AcousticSourceRow n;
            n.name = I18n::tr("asrc_new_src");
            n.level_dB = isUnderwater() ? m_p->underwater().sonarSL_dB
                                        : m_p->acoustic().srcSPL_dB;
            addSourceRow(n);
        }
    });
    // ファイルから追加: 実ファイルを選び、信号欄にそのファイルを持つ音源を作る
    connect(addBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("asrc_btn_addfile"), QString(),
            "Audio (*.wav *.flac *.aiff *.ogg);;All files (*)");
        if (path.isEmpty()) return;
        AcousticSourceRow n;
        n.name = QFileInfo(path).completeBaseName();
        n.signal = path;
        n.level_dB = isUnderwater() ? m_p->underwater().sonarSL_dB
                                    : m_p->acoustic().srcSPL_dB;
        addSourceRow(n);
    });
    // 🎵 信号 (WAV) を選択 — 選択行の signal にフルパスを保存する。
    // 行が選ばれていない場合はボタンを無効化せず理由を表示する
    // (行選択の度に有効/無効を持ち回らずに済み、理由も伝わる)。
    connect(m_sigPickBtn, &QPushButton::clicked, this, [this] {
        QVector<AcousticSourceRow> &list = sourceList();
        const int r = m_srcTable->currentRow();
        if (r < 0 || r >= list.size()) {
            QMessageBox::information(this, I18n::tr("asrc_sig_title"),
                                     I18n::tr("asrc_sig_norow"));
            return;
        }
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("asrc_sig_title"), list[r].signal,
            I18n::tr("asrc_sig_filter"));
        if (path.isEmpty()) return;   // キャンセル — モデルは変えない
        list[r].signal = path;
        refreshSourceTable();
        m_p->touch();
    });
    connect(m_sigClearBtn, &QPushButton::clicked, this, [this] {
        QVector<AcousticSourceRow> &list = sourceList();
        const int r = m_srcTable->currentRow();
        if (r < 0 || r >= list.size()) {
            QMessageBox::information(this, I18n::tr("asrc_sig_title"),
                                     I18n::tr("asrc_sig_norow"));
            return;
        }
        if (list[r].signal.isEmpty()) {
            QMessageBox::information(this, I18n::tr("asrc_sig_title"),
                                     I18n::tr("asrc_sig_noclear"));
            return;
        }
        list[r].signal.clear();
        refreshSourceTable();
        m_p->touch();
    });
    connect(delBtn, &QPushButton::clicked, this, [this] {
        const int r = m_srcTable->currentRow();
        QVector<AcousticSourceRow> &list = sourceList();
        if (r >= 0 && r < list.size()) {
            list.removeAt(r);
            refreshSourceTable();
            m_p->touch();
        }
    });
    // プリセット投入: ドメイン既定の構成を追記する (既存行は消さない)
    connect(m_presetBtn, &QPushButton::clicked, this, [this] {
        QVector<AcousticSourceRow> &list = sourceList();
        const QVector<AcousticSourceRow> preset =
            isUnderwater() ? defaultSonarSources() : defaultAcousticSources();
        for (const AcousticSourceRow &r : preset) list.push_back(r);
        refreshSourceTable();
        m_p->touch();
    });
    // 反映: 有効行の位置のみを feed へ書き込む (確認ダイアログ付き)。
    // 有効行 0 のときはボタン無効化ではなくメッセージで理由を示す
    // (チェック切替の度の再計算を持ち回らずに済み、理由も伝わる)。
    connect(m_syncBtn, &QPushButton::clicked, this, [this] {
        int enabled = 0;
        for (const AcousticSourceRow &r : m_p->acoustic().sources)
            if (r.enabled) ++enabled;
        if (enabled == 0) {
            QMessageBox::information(this, I18n::tr("asrc_sync_title"),
                                     I18n::tr("asrc_sync_none"));
            return;
        }
        const int nOld = int(m_p->feeds().size());
        QString msg = I18n::tr("asrc_sync_confirm").arg(nOld).arg(enabled);
        // 室外の音源をそのまま feed にするとソルバーが弾く (既定の音源リストは
        // 大ホール向けの座標なので、小さい室では実際にこれで落ちる)。
        // 反映する前にここで気づけるよう、件数と室の範囲を確認文に足す。
        int outside = 0;
        bool meshOk = true;
        for (int a = 0; a < 3; ++a)
            if (!m_p->mesh(a).isValid()) meshOk = false;
        if (meshOk) {
            const double lo[3] = { m_p->mesh(0).min(), m_p->mesh(1).min(),
                                   m_p->mesh(2).min() };
            const double hi[3] = { m_p->mesh(0).max(), m_p->mesh(1).max(),
                                   m_p->mesh(2).max() };
            for (const AcousticSourceRow &r : m_p->acoustic().sources) {
                if (!r.enabled) continue;
                const double v[3] = { r.x_m, r.y_m, r.z_m };
                for (int a = 0; a < 3; ++a)
                    if (v[a] < lo[a] || v[a] > hi[a]) { ++outside; break; }
            }
            if (outside > 0) {
                const auto n = [](double v) {
                    return QString::number(v, 'g', 6);
                };
                msg += QStringLiteral("\n\n") +
                       I18n::tr("asrc_sync_outside")
                           .arg(QString::number(outside))
                           .arg(n(lo[0]), n(hi[0]), n(lo[1]), n(hi[1]),
                                n(lo[2]), n(hi[2]));
            }
        }
        if (QMessageBox::question(this, I18n::tr("asrc_sync_title"), msg,
                QMessageBox::Ok | QMessageBox::Cancel,
                QMessageBox::Cancel) != QMessageBox::Ok)
            return;   // キャンセル — feed は変更しない
        syncFeedsFromSources(*m_p);
        m_p->touch();
        refreshSourceTable();   // 「ソルバ波源 #n」マーカーを即時更新
    });

    auto *sc = new SectionBox(I18n::tr("asrc_common_section"), page);
    auto *splRow = new QHBoxLayout();
    m_baseSpl = new QLineEdit(sc);
    m_baseSpl->setMaximumWidth(80);
    m_baseSplUnit = new QLabel(sc);
    splRow->addWidget(m_baseSpl);
    splRow->addWidget(m_baseSplUnit);
    splRow->addStretch(1);
    sc->form()->addRow(I18n::tr("asrc_base_spl"), splRow);
    auto *clip = new QCheckBox(I18n::tr("asrc_clip_prevent"), sc);
    clip->setChecked(true);
    sc->form()->addRow(I18n::tr("asrc_normalize"), clip);
    auto *dist = new QCheckBox(I18n::tr("asrc_dist_comp"), sc);
    dist->setChecked(true);
    sc->form()->addRow(I18n::tr("asrc_delay"), dist);
    auto *coh = new QCheckBox(I18n::tr("asrc_coherence"), sc);
    sc->form()->addRow(I18n::tr("asrc_phase"), coh);
    // 正規化/遅延/位相のチェックはどこにも読まれない — 注記 (基準SPLのみ有効)
    sc->vbox()->addWidget(tabhelp::unwiredNote(sc, I18n::tr("asrc_uw_norm"), I18n::tr("asrc_uw_norm_ok")));
    v->addWidget(sc);
    v->addStretch(1);

    connect(m_baseSpl, &QLineEdit::editingFinished, this,
            [this] { apply(); });
    return page;
}

// 表示中のドメインに対応する音源リスト (室内 / 水中で別リスト)
QVector<AcousticSourceRow> &AcousticSourceTab::sourceList()
{
    return isUnderwater() ? m_p->underwater().sources
                          : m_p->acoustic().sources;
}

// 1 セルの編集をモデルへ書き戻す。数値として読めない入力はモデルを変えず、
// セル表示をモデル値へ戻す (UI と保存内容を乖離させない — gui.md)。
void AcousticSourceTab::applySourceCell(int row, int col)
{
    QVector<AcousticSourceRow> &list = sourceList();
    if (row < 0 || row >= list.size()) return;   // 最終行 = 追加行
    AcousticSourceRow &r = list[row];
    QTableWidgetItem *it = m_srcTable->item(row, col);
    if (!it) return;
    bool restore = false;
    switch (col) {
    case 0: r.enabled = (it->checkState() == Qt::Checked); break;
    case 2: r.name = it->text(); break;
    case 4: {
        double x = 0, y = 0, z = 0;
        if (parsePosText(it->text(), &x, &y, &z)) {
            r.x_m = x; r.y_m = y; r.z_m = z;
        } else {
            restore = true;
        }
        break;
    }
    case 5: r.aim = it->text(); break;
    case 6: {
        // 表示はファイル名だけなので、表示と同じ文字列のまま確定した
        // (実際には編集していない) ときはフルパスを保持する。
        const QString t = it->text();
        const bool unchangedDisplay =
            looksLikeAudioPath(r.signal) && t == QFileInfo(r.signal).fileName();
        if (!unchangedDisplay) r.signal = t;
        // 実在チェックとツールチップを新しい値に合わせる (グレー表示のまま
        // 別のファイルに書き換えられた、等の食い違いを残さない)。
        m_updating = true;   // 再入 (cellChanged) を止める
        decorateSignalCell(it, r.signal, palette().mid(), palette().text());
        m_updating = false;
        break;
    }
    case 7: {
        bool ok = false;
        const double v = it->text().toDouble(&ok);
        if (ok) r.level_dB = v; else restore = true;
        break;
    }
    default: return;   // 3 = 種別 (QComboBox セルウィジェット)
    }
    if (restore) {
        m_updating = true;
        it->setText(col == 4 ? posText(r)
                             : QString::number(r.level_dB, 'g', 6));
        m_updating = false;
        return;   // モデルは変わっていない
    }
    m_p->touch();
}

// 音源リスト (モデル) → 表。最終行は mock の「＋ 音源を追加…」行。
// 有効な音源の数を出し、数に応じた注意を添える。
// 室内音響の応答は音源ごとに独立に求めて重ね合わせるので、計算量は
// 音源数に **比例** する (指数的に増えるわけではない)。ただし可聴化は
// 音源ごとに IR 畳み込みが要るので、そこが実際の重さになる。
void AcousticSourceTab::refreshSourceCount()
{
    if (!m_srcCountNote) return;
    int enabled = 0;
    for (const AcousticSourceRow &r : m_p->acoustic().sources)
        if (r.enabled) ++enabled;

    QString text = I18n::tr("asrc_count_fmt")
                       .arg(enabled).arg(m_p->acoustic().sources.size());
    const char *kind = "";
    if (enabled == 0) {
        text += QLatin1Char(' ') + I18n::tr("asrc_count_none");
        kind = "background:#FFF4CE; color:#9D5D00;";
    } else if (enabled > 16) {
        text += QLatin1Char(' ') + I18n::tr("asrc_count_many").arg(enabled);
        kind = "background:#FFF4CE; color:#9D5D00;";
    } else {
        text += QLatin1Char(' ') + I18n::tr("asrc_count_ok");
    }
    m_srcCountNote->setText(text);
    m_srcCountNote->setStyleSheet(
        QStringLiteral("font-size:11px; border-radius:3px; padding:2px 6px; %1")
            .arg(QLatin1String(kind)));
}

void AcousticSourceTab::refreshSourceTable()
{
    // 表を作り直したら件数の注記も必ず更新する (呼び忘れを作らない)
    struct CountSync { AcousticSourceTab *t; ~CountSync() {
        t->refreshSourceCount(); } } sync{ this };
    const bool prevUpdating = m_updating;   // refresh() から入れ子で呼ばれる
    m_updating = true;
    const QVector<AcousticSourceRow> &list = sourceList();
    const int n = list.size();
    m_srcTable->clearSpans();
    m_srcTable->setRowCount(n + 1);
    for (int i = 0; i < n; ++i) {
        const AcousticSourceRow &r = list[i];
        auto *ck = new QTableWidgetItem;
        ck->setCheckState(r.enabled ? Qt::Checked : Qt::Unchecked);
        ck->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        m_srcTable->setItem(i, 0, ck);
        auto *num = new QTableWidgetItem(QString::number(i + 1));
        num->setFlags(num->flags() & ~Qt::ItemIsEditable);
        m_srcTable->setItem(i, 1, num);
        m_srcTable->setItem(i, 2, new QTableWidgetItem(r.name));
        // 種別はコンボ (Omni / Cardioid / Bipolar / Directional)
        auto *kind = new QComboBox(m_srcTable);
        kind->addItem(I18n::tr("asrc_kind_omni"));
        kind->addItem(I18n::tr("asrc_kind_card"));
        kind->addItem(I18n::tr("asrc_kind_bipolar"));
        kind->addItem(I18n::tr("asrc_kind_directional"));
        kind->setCurrentIndex(qBound(0, r.kind, 3));
        connect(kind, &QComboBox::currentIndexChanged, this,
                [this, i](int idx) {
                    if (m_updating) return;
                    QVector<AcousticSourceRow> &l = sourceList();
                    if (i < 0 || i >= l.size()) return;
                    l[i].kind = idx;
                    m_p->touch();
                });
        m_srcTable->setCellWidget(i, 3, kind);
        m_srcTable->setItem(i, 4, new QTableWidgetItem(posText(r)));
        m_srcTable->setItem(i, 5, new QTableWidgetItem(r.aim));
        // 信号: ファイルならファイル名のみ表示 + ツールチップにフルパス
        // (実在しないパスはグレー + 「見つかりません」)
        auto *sig = new QTableWidgetItem;
        decorateSignalCell(sig, r.signal, palette().mid(), palette().text());
        m_srcTable->setItem(i, 6, sig);
        auto *lv = new QTableWidgetItem(QString::number(r.level_dB, 'g', 6));
        lv->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_srcTable->setItem(i, 7, lv);
        // ソルバ波源マーカー: この行の位置と一致する feed があれば
        // 「ソルバ波源 #n」— 反映結果 (どの行がソルバに効くか) を可視化する
        const int fi = matchingFeedIndex(*m_p, r);
        auto *sv = new QTableWidgetItem(
            fi > 0 ? I18n::tr("asrc_solver_feed").arg(fi) : kDash);
        sv->setFlags(Qt::ItemIsEnabled);   // 読取専用 (表示のみ)
        if (fi > 0) sv->setForeground(QBrush(kAcc));
        m_srcTable->setItem(i, 8, sv);
    }
    // 追加行 (＋ 音源を追加…) — クリックで 1 行増える
    m_srcTable->setCellWidget(n, 3, nullptr);
    auto *blank = new QTableWidgetItem;
    blank->setFlags(Qt::ItemIsEnabled);
    m_srcTable->setItem(n, 0, blank);
    auto *add = new QTableWidgetItem(I18n::tr("asrc_add_row"));
    QFont f = add->font();
    f.setItalic(true);
    add->setFont(f);
    add->setForeground(palette().mid());
    add->setFlags(Qt::ItemIsEnabled);
    m_srcTable->setItem(n, 1, add);
    m_srcTable->setSpan(n, 1, 1, 8);
    m_updating = prevUpdating;
}

void AcousticSourceTab::addSourceRow(const AcousticSourceRow &row)
{
    sourceList().push_back(row);
    refreshSourceTable();
    m_p->touch();
}

// ── page: signal ────────────────────────────────────────────────────────────
QWidget *AcousticSourceTab::buildSignalPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("asrc_signal_section"), page);
    m_sigKind = new QComboBox(s);
    m_sigKind->addItem(I18n::tr("asrc_sig_impulse"));
    m_sigKind->addItem(I18n::tr("asrc_sig_sweep"));
    m_sigKind->addItem(I18n::tr("asrc_sig_chirp"));
    m_sigKind->addItem(I18n::tr("asrc_sig_tone"));
    m_sigKind->addItem(I18n::tr("asrc_sig_noise"));
    m_sigKind->addItem(I18n::tr("asrc_sig_wav"));
    m_sigKind->setCurrentIndex(5);   // mock: value="wav"
    s->form()->addRow(I18n::tr("asrc_sig_kind"), m_sigKind);
    v->addWidget(s);

    auto *sw = new SectionBox(I18n::tr("asrc_wav_section"), page);
    auto *fileRow = new QHBoxLayout();
    // 実在しないファイル名を初期値に置かない (「選択済み」に見えてしまう) —
    // 例はプレースホルダで示す
    m_wavFile = new QLineEdit(sw);
    m_wavFile->setPlaceholderText("anechoic_speech_48k.wav");
    auto *browse = new QPushButton(I18n::tr("asrc_browse"), sw);
    auto *listen = new QPushButton(I18n::tr("asrc_listen"), sw);
    fileRow->addWidget(m_wavFile, 1);
    fileRow->addWidget(browse);
    fileRow->addWidget(listen);
    sw->form()->addRow(I18n::tr("asrc_file"), fileRow);

    auto *fmtRow = new QHBoxLayout();
    for (const char *b : { "WAV", "FLAC", "AIFF", "OGG", "PCM", "24/32-bit" })
        fmtRow->addWidget(makeBadge(QString::fromUtf8(b), sw));
    fmtRow->addStretch(1);
    sw->form()->addRow(I18n::tr("asrc_formats"), fmtRow);

    auto *ch = new QComboBox(sw);
    ch->addItem(I18n::tr("asrc_ch_mono"));
    ch->addItem(I18n::tr("asrc_ch_stereo"));
    ch->addItem(I18n::tr("asrc_ch_51"));
    ch->addItem("Ambisonics (B-format)");
    sw->form()->addRow(I18n::tr("asrc_channels"), ch);

    auto *srRow = new QHBoxLayout();
    // ファイル未解析の間は値を出さない
    // (プレビューで実読込に成功したら実測値へ置き換える)
    m_srateValue = new QLabel(I18n::tr("asrc_srate_none"), sw);
    srRow->addWidget(m_srateValue);
    auto *resample = new QCheckBox(I18n::tr("asrc_resample"), sw);
    resample->setChecked(true);
    srRow->addWidget(resample);
    srRow->addStretch(1);
    sw->form()->addRow(I18n::tr("asrc_srate"), srRow);

    auto *loop = new QCheckBox(I18n::tr("asrc_loop_chk"), sw);
    sw->form()->addRow(I18n::tr("asrc_loop"), loop);

    // ── 前処理 (トリム / ゲイン / HPF) — モデル (.ofdx acoustic.source_wav) の
    //    View。波形プレビューと可聴化へ渡すドライ音源の両方に効く。
    const AcousticOpts &a0 = m_p->acoustic();
    auto *trimRow = new QHBoxLayout();
    m_wavTrim0 = new QLineEdit(QString::number(a0.wavTrimStart_s, 'g', 6), sw);
    m_wavTrim0->setMaximumWidth(60);
    m_wavTrim1 = new QLineEdit(QString::number(a0.wavTrimEnd_s, 'g', 6), sw);
    m_wavTrim1->setMaximumWidth(60);
    trimRow->addWidget(m_wavTrim0);
    trimRow->addWidget(new QLabel(QString::fromUtf8("〜"), sw));
    trimRow->addWidget(m_wavTrim1);
    trimRow->addWidget(new QLabel("s", sw));
    trimRow->addStretch(1);
    sw->form()->addRow(I18n::tr("asrc_trim"), trimRow);

    auto *gainRow = new QHBoxLayout();
    m_wavGain = new QLineEdit(QString::number(a0.wavGain_dB, 'g', 6), sw);
    m_wavGain->setMaximumWidth(60);
    gainRow->addWidget(m_wavGain);
    gainRow->addWidget(new QLabel("dB", sw));
    gainRow->addStretch(1);
    sw->form()->addRow(I18n::tr("asrc_gain"), gainRow);

    auto *hpfRow = new QHBoxLayout();
    m_wavHpf = new QCheckBox(I18n::tr("asrc_hpf_chk"), sw);
    m_wavHpf->setChecked(a0.wavHighPass);
    m_wavHpfHz = new QLineEdit(QString::number(a0.wavHighPassHz, 'g', 6), sw);
    m_wavHpfHz->setMaximumWidth(60);
    hpfRow->addWidget(m_wavHpf);
    hpfRow->addWidget(m_wavHpfHz);
    hpfRow->addWidget(new QLabel("Hz", sw));
    hpfRow->addStretch(1);
    sw->form()->addRow(I18n::tr("asrc_hpf"), hpfRow);
    auto *prepNote = new QLabel(I18n::tr("asrc_prep_note"), sw);
    prepNote->setWordWrap(true);
    prepNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sw->vbox()->addWidget(prepNote);
    for (QLineEdit *e : { m_wavTrim0, m_wavTrim1, m_wavGain, m_wavHpfHz })
        connect(e, &QLineEdit::editingFinished, this,
                &AcousticSourceTab::applyWavPrep);
    connect(m_wavHpf, &QCheckBox::toggled, this,
            &AcousticSourceTab::applyWavPrep);
    v->addWidget(sw);

    auto *sl = new SectionBox(I18n::tr("asrc_lib_section"), page);
    auto *libHint = new QLabel(I18n::tr("asrc_lib_hint"), sl);
    libHint->setWordWrap(true);
    sl->vbox()->addWidget(libHint);
    m_libTable = new QTableWidget(8, 4, sl);
    setupTable(m_libTable, { I18n::tr("asrc_col_bundled"),
                             I18n::tr("asrc_col_material"),
                             I18n::tr("asrc_col_len"),
                             I18n::tr("asrc_col_use") }, 200);
    // レイアウトへの追加漏れがあると、表が親の左上に素置きされて
    // 上のヒントラベルに重なる (実際に表示崩れを起こしていた)
    sl->vbox()->addWidget(m_libTable);
    // 音源データは同梱していない — 入手先と登録方法を案内する (絶対規則 5)
    auto *libNote = new QLabel(I18n::tr("asrc_lib_note"), sl);
    libNote->setWordWrap(true);
    libNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sl->vbox()->addWidget(libNote);
    auto *libRegRow = new QHBoxLayout();
    auto *libReg = new QPushButton(I18n::tr("asrc_lib_register"), sl);
    libRegRow->addWidget(libReg);
    libRegRow->addStretch(1);
    sl->vbox()->addLayout(libRegRow);
    v->addWidget(sl);

    auto *sp = new SectionBox(I18n::tr("asrc_preview_section"), page);
    m_wavePlot = new MiniPlot(sp);
    m_wavePlot->setLabels("t [s]", "amplitude");
    m_wavePlot->setMinimumHeight(110);
    // ファイル未選択時のプレースホルダ波形。
    // モックは 440 Hz 搬送波だったが、5 s を 200 点 (= 40 Hz) で描くと
    // sin(2π·440·t) は全サンプルが節に当たって恒等的に 0 になり (実測 1e-13)、
    // 数値誤差だけの意味のない線が出ていた。描画点数で表現できる搬送波
    // (12 Hz) と点数 (1000) にして、波形として読める見本にする。
    MiniSeries wave;
    wave.color = kAcc;
    const int kPts = 1000;
    for (int i = 0; i < kPts; ++i) {
        const double t = i / double(kPts) * 5.0;
        const double env = std::exp(-std::pow((t - 1.5) / 0.6, 2.0));
        const double y = env * std::sin(2 * M_PI * 12 * t)
                       * (0.5 + 0.4 * std::sin(2 * M_PI * 1.5 * t));
        wave.pts.push_back({ t, y });
    }
    m_wavePlot->setSeries({ wave });
    sp->vbox()->addWidget(m_wavePlot);
    // 未読込のうちは数値を出さない (実測値と誤認させない — 絶対規則 5)
    m_wavStats = new QLabel(I18n::tr("asrc_preview_none"), sp);
    sp->vbox()->addWidget(m_wavStats);
    // ファイル未選択の状態表示 (実読込に成功したら隠す)
    m_previewNote = new QLabel(I18n::tr("asrc_preview_placeholder"), sp);
    m_previewNote->setWordWrap(true);
    m_previewNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sp->vbox()->addWidget(m_previewNote);
    v->addWidget(sp);
    v->addStretch(1);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("asrc_wav_section"), QString(),
            "Audio (*.wav *.flac *.aiff *.ogg);;All files (*)");
        if (path.isEmpty()) return;
        m_wavFile->setText(path);
        loadWavPreview(path);
    });
    // 手入力パスも、確定時に実在すればプレビューへ反映する
    connect(m_wavFile, &QLineEdit::editingFinished, this, [this] {
        const QString path = m_wavFile->text();
        if (path != m_previewPath && QFileInfo::exists(path))
            loadWavPreview(path);
    });
    // 試聴: 外部 CLI プレイヤ (ffplay/aplay/afplay) に委ねる
    connect(listen, &QPushButton::clicked, this, [this] {
        playWavExternal(this, m_wavFile->text());
    });
    // ライブラリ: 同梱音源が無いので、入手した実ファイルを入力信号として
    // 登録する導線を用意する (選択と同時に実読込プレビューも更新)
    connect(libReg, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("asrc_lib_register"), QString(),
            "Audio (*.wav *.flac *.aiff *.ogg);;All files (*)");
        if (path.isEmpty()) return;
        m_sigKind->setCurrentIndex(5);   // 信号種別 = WAV ファイル
        m_wavFile->setText(path);
        loadWavPreview(path);
    });
    return page;
}

// 選択 WAV を実読込し、包絡線 (min/max) と RMS/Peak/Crest を実計算して表示。
// 読込と解析は QThread で非同期 (gui.md: 秒単位処理を GUI スレッドで同期
// 実行しない)。失敗時は見本表示のまま、エラーだけ統計行へ出す。
// 前処理の設定 (モデル → AudioEditEngine の入力)
audioedit::SourcePrep AcousticSourceTab::wavPrep() const
{
    return tabhelp::sourcePrep(m_p->acoustic());
}

// 前処理の widgets → model。プレビュー済みなら同じファイルを再解析して
// 波形と指標を新しい設定で出し直す (設定と表示が乖離しないように)。
void AcousticSourceTab::applyWavPrep()
{
    if (m_updating || !m_wavTrim0) return;
    AcousticOpts a = m_p->acoustic();
    auto num = [](QLineEdit *e, double fallback) {
        bool ok = false;
        const double v = e->text().trimmed().toDouble(&ok);
        return ok ? v : fallback;
    };
    const AcousticOpts d;
    a.wavTrimStart_s = qMax(0.0, num(m_wavTrim0, d.wavTrimStart_s));
    a.wavTrimEnd_s   = qMax(0.0, num(m_wavTrim1, d.wavTrimEnd_s));
    a.wavGain_dB     = num(m_wavGain, d.wavGain_dB);
    a.wavHighPass    = m_wavHpf->isChecked();
    a.wavHighPassHz  = qMax(0.0, num(m_wavHpfHz, d.wavHighPassHz));
    if (a.wavTrimStart_s == m_p->acoustic().wavTrimStart_s
        && a.wavTrimEnd_s == m_p->acoustic().wavTrimEnd_s
        && a.wavGain_dB == m_p->acoustic().wavGain_dB
        && a.wavHighPass == m_p->acoustic().wavHighPass
        && a.wavHighPassHz == m_p->acoustic().wavHighPassHz)
        return;                                   // 変化なし (再解析しない)
    m_p->acoustic() = a;
    m_p->touch();

    if (!m_previewPath.isEmpty()) {
        const QString path = m_previewPath;
        m_previewPath.clear();                    // 再読込ガードを外す
        loadWavPreview(path);
    }
}

void AcousticSourceTab::loadWavPreview(const QString &path)
{
    if (m_previewBusy || path.trimmed().isEmpty()) return;
    m_previewBusy = true;

    struct PreviewData {
        bool ok = false;
        QString err;
        double fs = 0.0;
        std::vector<double> mono;          // 平均モノ (包絡線用)
        audioedit::LevelMetrics lv;
        bool prepped = false;              // 前処理を適用したか (注記用)
        bool prepEmpty = false;            // 前処理で空になった (トリム範囲が外)
    };
    auto d = std::make_shared<PreviewData>();
    const std::string p = path.toStdString();
    // プレビューは前処理 (トリム/HPF/ゲイン) を適用した後の信号を出す —
    // 設定を変えたら波形と RMS/Peak/Crest がその場で追従する。
    const audioedit::SourcePrep prep = wavPrep();
    QThread *th = QThread::create([p, d, prep] {
        const acoustics::AcousticResult<acoustics::AudioBuffer> res =
            acoustics::readWavFile(p);
        if (!res.success()) {
            d->err = QString::fromStdString(res.message());
            return;
        }
        d->fs = res.value().sampleRateHz;
        // 平均モノにしてから前処理する (指標と包絡線を同じ信号で測る)
        acoustics::AudioBuffer mb;
        mb.sampleRateHz = d->fs;
        mb.channels.push_back(QtAcousticAdapter::selectChannel(res.value(), 2));
        mb = audioedit::prepareSource(mb, prep);
        if (mb.channels.empty() || mb.channels[0].empty()) {
            d->err = QString();
            d->prepEmpty = true;
            return;
        }
        d->mono = mb.channels[0];
        d->lv = audioedit::analyzeLevels(mb, 0, 0);   // a >= z → 全範囲
        d->prepped = !prep.isIdentity();
        d->ok = true;
    });
    connect(th, &QThread::finished, this, [this, th, d, path] {
        th->deleteLater();
        m_previewBusy = false;
        if (!d->ok) {
            m_wavStats->setText(d->prepEmpty
                ? I18n::tr("asrc_prep_empty")
                : I18n::tr("asrc_preview_fail").arg(d->err));
            return;   // 見本波形と注記はそのまま
        }
        QVector<QPointF> top, bottom;
        tabhelp::envelopeSeries(d->mono, d->fs, 1200,
                                tabhelp::TimeUnit::Seconds, top, bottom);
        MiniSeries hi;  hi.pts = top;     hi.color = kAcc;
        MiniSeries lo;  lo.pts = bottom;  lo.color = kAcc;
        m_wavePlot->setSeries({ hi, lo });
        m_wavStats->setText(
            I18n::tr("asrc_preview_stats")
                .arg(QString::number(d->lv.rmsDbfs, 'f', 1),
                     QString::number(d->lv.peakDbfs, 'f', 1),
                     QString::number(d->lv.crestDb, 'f', 1))
            + QStringLiteral(" · %1 s")
                  .arg(QString::number(d->lv.durationSec, 'f', 2))
            + (d->prepped ? QStringLiteral(" · ") + I18n::tr("asrc_prep_applied")
                          : QString()));
        m_srateValue->setText(QStringLiteral("%1 Hz").arg(qRound(d->fs)));
        m_previewNote->setVisible(false);   // 実データ表示 — 見本注記を外す
        m_previewPath = path;
    });
    th->start();
}

// ── page: directivity ───────────────────────────────────────────────────────
QWidget *AcousticSourceTab::buildDirectivityPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("asrc_dir_section"), page);
    auto *hint = new QLabel(I18n::tr("asrc_dir_hint"), s);
    hint->setWordWrap(true);
    s->vbox()->addWidget(hint);
    v->addWidget(s);

    auto *sm = new SectionBox(I18n::tr("asrc_model_section"), page);
    m_dirModel = new QComboBox(sm);
    m_dirModel->addItem(I18n::tr("asrc_m_omni"));
    m_dirModel->addItem(I18n::tr("asrc_m_card"));
    m_dirModel->addItem(I18n::tr("asrc_m_super"));
    m_dirModel->addItem(I18n::tr("asrc_m_hyper"));
    m_dirModel->addItem(I18n::tr("asrc_m_fig8"));
    m_dirModel->setCurrentIndex(1);   // ポーラプレビューは Cardioid
    sm->vbox()->addWidget(m_dirModel);
    m_dirSource = new QComboBox(sm);
    m_dirSource->addItem(I18n::tr("asrc_s_piston"));
    m_dirSource->addItem(I18n::tr("asrc_s_horn"));
    m_dirSource->addItem(I18n::tr("asrc_s_line"));
    m_dirSource->addItem(I18n::tr("asrc_s_clf"));
    m_dirSource->addItem(I18n::tr("asrc_s_measured"));
    m_dirSource->setCurrentIndex(3);   // mock: value="clf"
    sm->vbox()->addWidget(m_dirSource);
    v->addWidget(sm);

    auto *sf = new SectionBox(I18n::tr("asrc_gll_section"), page);
    auto *fileRow = new QHBoxLayout();
    // 同上 (CLF/GLL はパーサ未実装 — ファイル名の記録のみ)
    m_gllFile = new QLineEdit(sf);
    m_gllFile->setPlaceholderText("EAW_KF730_v2.gll");
    auto *browse = new QPushButton(I18n::tr("asrc_browse"), sf);
    fileRow->addWidget(m_gllFile, 1);
    fileRow->addWidget(browse);
    sf->form()->addRow(I18n::tr("asrc_file"), fileRow);
    auto *fmtRow = new QHBoxLayout();
    fmtRow->addWidget(makeBadge(".CLF (CLF v2)", sf));
    fmtRow->addWidget(makeBadge(".GLL (Ease GLL)", sf, "#2E8B57"));
    fmtRow->addWidget(makeBadge(".XGLC", sf));
    fmtRow->addWidget(makeBadge(".SPK (Odeon)", sf));
    fmtRow->addWidget(makeBadge(".so8 (CATT)", sf));
    fmtRow->addStretch(1);
    sf->form()->addRow(I18n::tr("asrc_formats"), fmtRow);
    auto *origin = new QComboBox(sf);
    origin->addItem(I18n::tr("asrc_o_acoustic"));
    origin->addItem(I18n::tr("asrc_o_physical"));
    origin->addItem(I18n::tr("asrc_o_custom"));
    sf->form()->addRow(I18n::tr("asrc_origin"), origin);
    auto *freqres = new QComboBox(sf);
    freqres->addItem(I18n::tr("asrc_f_oct"));
    freqres->addItem(I18n::tr("asrc_f_third"));
    freqres->addItem(I18n::tr("asrc_f_sixth"));
    freqres->addItem(I18n::tr("asrc_f_narrow"));
    freqres->setCurrentIndex(1);   // mock: value="third"
    sf->form()->addRow(I18n::tr("asrc_freqres"), freqres);
    sf->form()->addRow(I18n::tr("asrc_angres"),
                       new QLabel(I18n::tr("asrc_angres_val"), sf));
    v->addWidget(sf);

    // 帯域別指向性: 解析モデル選択時は閉形式の実計算 (周波数非依存)、
    // CLF/GLL・測定 polar 選択時はファイル未解析なので "—" (偽値を出さない)
    auto *sb = new SectionBox(I18n::tr("asrc_band_section"), page);
    m_bandTable = new QTableWidget(5, 5, sb);
    setupTable(m_bandTable, { I18n::tr("asrc_col_band"), I18n::tr("asrc_col_h6"),
                              I18n::tr("asrc_col_v6"), I18n::tr("asrc_col_q"),
                              "DI [dB]" }, 150);
    static const char *kBandName[5] = {
        "125 Hz", "500 Hz", "1 kHz", "4 kHz", "16 kHz" };
    for (int r = 0; r < 5; ++r)
        m_bandTable->setItem(r, 0, new QTableWidgetItem(
            QString::fromUtf8(kBandName[r])));
    sb->vbox()->addWidget(m_bandTable);
    m_bandNote = new QLabel(sb);
    m_bandNote->setWordWrap(true);
    m_bandNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sb->vbox()->addWidget(m_bandNote);
    v->addWidget(sb);

    auto *sp = new SectionBox(I18n::tr("asrc_polar_section"), page);
    auto *polarHint = new QLabel(I18n::tr("asrc_polar_hint"), sp);
    polarHint->setWordWrap(true);
    sp->vbox()->addWidget(polarHint);
    auto *polarRow = new QHBoxLayout();
    m_polar = new PolarPatternView(sp);
    polarRow->addWidget(m_polar);
    m_polarInfo = new QLabel(sp);
    m_polarInfo->setTextFormat(Qt::RichText);
    m_polarInfo->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    polarRow->addSpacing(16);
    polarRow->addWidget(m_polarInfo);
    polarRow->addStretch(1);
    sp->vbox()->addLayout(polarRow);
    // CLF/GLL・測定 polar 選択時のみ「ファイルは未解析」の注記を出す
    m_polarClfNote = new QLabel(I18n::tr("asrc_polar_note_clf"), sp);
    m_polarClfNote->setWordWrap(true);
    m_polarClfNote->setStyleSheet("font-size:11px; color:#B8860B;");
    sp->vbox()->addWidget(m_polarClfNote);
    v->addWidget(sp);

    // 軸上周波数特性: 一次指向性モデルの軸上応答は定義上フラット (0 dB)。
    // モックにあった凹凸カーブ (低域/高域ロールオフ + sin リップル) は
    // どの音源にも対応しない作り物なので描かない (絶対規則 5)。
    auto *sr = new SectionBox(I18n::tr("asrc_fr_section"), page);
    m_freqResp = new MiniPlot(sr);
    m_freqResp->setLabels("f [Hz] (log)", "rel. level [dB]");
    m_freqResp->setXTickPow10(true);
    m_freqResp->setYRange(-12.0, 12.0);
    m_freqResp->setMinimumHeight(110);
    sr->vbox()->addWidget(m_freqResp);
    m_frNote = new QLabel(sr);
    m_frNote->setWordWrap(true);
    m_frNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sr->vbox()->addWidget(m_frNote);
    v->addWidget(sr);
    v->addStretch(1);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("asrc_gll_section"), QString(),
            "Loudspeaker (*.clf *.gll *.xglc *.spk *.so8);;All files (*)");
        if (!path.isEmpty()) m_gllFile->setText(path);
    });
    connect(m_dirModel, &QComboBox::currentIndexChanged, this,
            [this](int) { updateDirectivity(); });
    connect(m_dirSource, &QComboBox::currentIndexChanged, this,
            [this](int) { updateDirectivity(); });
    updateDirectivity();
    return page;
}

// 選択された一次指向性モデル r(θ) = a + b·cosθ をポーラ図・帯域別指向性表・
// 軸上周波数特性へ反映する。ビーム幅・F/B 比・指向性係数 Q は閉形式の実計算。
// 音源データが CLF/GLL・測定 polar のときはパーサ未実装なので、帯域表と
// 周波数特性は値を出さない ("—" / 空グラフ) — 偽の数値を出さない (絶対規則 5)。
// ── アレイページ: 設定 → 遠方界パターン (acoustics/ArrayDirectivity) ────────
// ソルバは要らない。素子位置と遅延が決まれば遠方界は和で書ける。
void AcousticSourceTab::updateArray()
{
    if (!m_arrPlot || !m_arrTable) return;
    const double c = 343.0;               // 20 °C の空気 (室内音響の既定)

    auto rowItem = [](const QString &t, bool right) {
        auto *it = new QTableWidgetItem(t);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        if (right) it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return it;
    };
    struct Line { QString name, value; };
    QVector<Line> lines;

    bool okN = false, okD = false;
    const int nEl = m_arrElems->text().trimmed().toInt(&okN);
    const double spacing = m_arrSpacing->text().trimmed().toDouble(&okD);
    const double freq = [this] {
        const double table[7] = { 125, 250, 500, 1000, 2000, 4000, 8000 };
        return table[qBound(0, m_arrFreq->currentIndex(), 6)];
    }();

    if (!okN || !okD || nEl <= 0 || spacing <= 0.0) {
        m_arrPlot->setSeries({});
        m_arrTable->clearContents();
        m_arrTable->setRowCount(1);
        m_arrTable->setItem(0, 0, rowItem(I18n::tr("asrc_beam_bad"), false));
        m_arrTable->setItem(0, 1, rowItem(QStringLiteral("—"), true));
        m_arrNote->setVisible(false);
        return;
    }

    // splay: ストレートを選んだら 0、それ以外は入力欄の並びを読む
    std::vector<double> splay;
    if (m_arrCurve->currentIndex() != 0) {
        const QStringList parts =
            m_arrSplay->text().split(QRegularExpression("[,\\s]+"),
                                     Qt::SkipEmptyParts);
        for (const QString &p : parts) {
            bool ok = false;
            const double v = p.toDouble(&ok);
            if (ok) splay.push_back(v);
        }
    }
    const double steer = m_arrSteer->isChecked()
                             ? m_arrSteerDeg->text().trimmed().toDouble()
                             : 0.0;

    const std::vector<acoustics::ArrayElement> els =
        acoustics::buildLineArray(nEl, spacing, splay, steer, c);
    const acoustics::ArrayPattern pat =
        acoustics::beamPattern(els, freq, c, spacing, -90.0, 90.0, 1801);
    const acoustics::BeamMetrics bm = acoustics::beamMetrics(pat);

    QVector<QPointF> pts;
    pts.reserve(int(pat.deg.size()));
    for (std::size_t i = 0; i < pat.deg.size(); ++i)
        pts.push_back(QPointF(pat.deg[i], std::max(-30.0, pat.db[i])));
    MiniSeries ms;
    ms.pts = pts;
    ms.color = QColor("#0078D4");
    m_arrPlot->setSeries({ ms });

    const QString dash = QStringLiteral("—");
    auto deg = [](double v) { return QString::number(v, 'f', 1)
                                     + QString::fromUtf8("°"); };
    lines.push_back({ I18n::tr("asrc_beam_peak"),
                      pat.valid ? deg(pat.peakDeg) : dash });
    lines.push_back({ I18n::tr("asrc_beam_hpbw"),
                      bm.hasHpbw ? deg(bm.hpbwDeg) : dash });
    lines.push_back({ I18n::tr("asrc_beam_cov"),
                      bm.hasCoverage
                          ? (deg(bm.coverageMinDeg) + QStringLiteral(" … ")
                             + deg(bm.coverageMaxDeg))
                          : dash });
    lines.push_back({ I18n::tr("asrc_beam_sll"),
                      bm.hasSll ? (QString::number(bm.sllDb, 'f', 1)
                                   + QStringLiteral(" dB @ ")
                                   + deg(bm.sllDeg))
                                : dash });
    lines.push_back({ I18n::tr("asrc_beam_len"),
                      QString::number(nEl * spacing, 'f', 2)
                          + QStringLiteral(" m") });
    const double fg = acoustics::gratingLobeFreq(spacing, steer, c);
    lines.push_back({ I18n::tr("asrc_beam_gl"),
                      fg > 0.0 ? (QString::number(fg, 'f', 0)
                                  + QStringLiteral(" Hz"))
                               : dash });

    m_arrTable->clearContents();
    m_arrTable->setRowCount(lines.size());
    for (int i = 0; i < lines.size(); ++i) {
        m_arrTable->setItem(i, 0, rowItem(lines[i].name, false));
        m_arrTable->setItem(i, 1, rowItem(lines[i].value, true));
    }
    // 「グレーティングローブ抑制」を入れているのに超えているなら言う
    const bool over = (fg > 0.0 && freq >= fg);
    m_arrNote->setVisible(over && m_arrGrating->isChecked());
    if (over) m_arrNote->setText(I18n::tr("asrc_beam_glwarn"));

    // ── サブアレイ ──────────────────────────────────────────────────────
    if (!m_subInfo) return;
    if (m_subLayout->currentIndex() == 0) {          // 単発
        m_subInfo->setText(I18n::tr("asrc_sub_single"));
        return;
    }
    bool okT = false;
    const double tauMs = m_subDelay->text().trimmed().toDouble(&okT);
    if (!okT || tauMs <= 0.0) {
        m_subInfo->setText(I18n::tr("asrc_sub_bad"));
        return;
    }
    const double tau = tauMs * 1e-3;
    const double dSub = c * tau;                     // 遅延に見合う間隔
    const bool rev = m_subRev->isChecked();
    const acoustics::EndfireResult ef =
        acoustics::endfire(dSub, tau, freq, c, rev);
    QString text = I18n::tr("asrc_sub_fb")
                       .arg(QString::number(dSub, 'f', 2),
                            QString::number(ef.frontBackDb, 'f', 1),
                            QString::number(ef.frontDb, 'f', 1),
                            QString::number(ef.backDb, 'f', 1),
                            QString::number(freq, 'f', 0),
                            QString::number(ef.bestFreqHz, 'f', 0));
    if (!rev) text += QStringLiteral(" ") + I18n::tr("asrc_sub_needrev");
    m_subInfo->setText(text);
}

void AcousticSourceTab::updateDirectivity()
{
    const int idx = qBound(0, m_dirModel->currentIndex(), 4);
    const double a = kAB[idx][0], b = kAB[idx][1];
    m_polar->setPattern(a, b);
    // 3 = CLF/GLL ファイル、4 = 測定 polar (.txt)
    const bool fileBased = (m_dirSource->currentIndex() == 3 ||
                            m_dirSource->currentIndex() == 4);
    m_polarClfNote->setVisible(fileBased);

    // F/B 比 = 20·log10(|r(0°)| / |r(180°)|)。背面ヌル (cardioid) は ∞
    const double back = std::fabs(a - b);
    const QString fb = back < 1e-9
        ? QStringLiteral("∞")
        : QStringLiteral("%1 dB").arg(QString::number(
              20.0 * std::log10((a + b) / back), 'f', 1));
    const double q = directivityFactor(a, b);
    const double di = 10.0 * std::log10(q);
    m_polarInfo->setText(QStringLiteral(
        "<b>Type:</b> %1<br>"
        "<b>-3 dB beam:</b> %2<br>"
        "<b>-6 dB beam:</b> %3<br>"
        "<b>F/B ratio:</b> %4<br>"
        "<b>Q:</b> %5 (DI %6 dB)")
        .arg(m_dirModel->currentText(), beamText(beamWidthDeg(a, b, 3.0)),
             beamText(beamWidthDeg(a, b, 6.0)), fb, QString::number(q, 'f', 1),
             QString::number(di, 'f', 1)));

    // ── 帯域別指向性 (列 1..4: 水平 -6dB / 垂直 -6dB / Q / DI) ──
    // 一次指向性モデルは回転対称かつ周波数非依存なので、水平 = 垂直、
    // 全帯域で同じ値になる。
    const QString h6 = fileBased ? kDash : beamText(beamWidthDeg(a, b, 6.0));
    const QString qs = fileBased ? kDash : QString::number(q, 'f', 1);
    const QString dis = fileBased ? kDash : QString::number(di, 'f', 1);
    for (int r = 0; r < 5; ++r) {
        m_bandTable->setItem(r, 1, new QTableWidgetItem(h6));
        m_bandTable->setItem(r, 2, new QTableWidgetItem(h6));
        m_bandTable->setItem(r, 3, new QTableWidgetItem(qs));
        m_bandTable->setItem(r, 4, new QTableWidgetItem(dis));
    }
    m_bandNote->setText(
        I18n::tr(fileBased ? "asrc_band_note_file" : "asrc_band_note_model"));

    // ── 軸上周波数特性 ──
    // 解析モデルの軸上応答はフラット (0 dB)。ファイル由来は未解析なので空。
    MiniSeries fr;
    fr.color = kAcc;
    if (!fileBased) {
        // 20 Hz – 20 kHz を対数軸で 2 点 (フラットなので端点だけで足りる)
        fr.pts.push_back({ std::log10(20.0), 0.0 });
        fr.pts.push_back({ std::log10(20000.0), 0.0 });
    }
    m_freqResp->setSeries({ fr });
    m_frNote->setText(
        I18n::tr(fileBased ? "asrc_fr_note_file" : "asrc_fr_note_model"));
}

// ── page: array ─────────────────────────────────────────────────────────────
QWidget *AcousticSourceTab::buildArrayPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("asrc_array_section"), page);
    auto *hint = new QLabel(I18n::tr("asrc_array_hint"), s);
    hint->setWordWrap(true);
    s->vbox()->addWidget(hint);
    m_arrElems = new QLineEdit("12", s);
    m_arrElems->setMaximumWidth(60);
    s->form()->addRow(I18n::tr("asrc_elems"), m_arrElems);
    auto *spacingRow = new QHBoxLayout();
    m_arrSpacing = new QLineEdit("0.35", s);
    m_arrSpacing->setMaximumWidth(60);
    spacingRow->addWidget(m_arrSpacing);
    spacingRow->addWidget(new QLabel("m", s));
    spacingRow->addStretch(1);
    s->form()->addRow(I18n::tr("asrc_spacing"), spacingRow);
    m_arrCurve = new QComboBox(s);
    m_arrCurve->addItem(I18n::tr("asrc_c_straight"));
    m_arrCurve->addItem(I18n::tr("asrc_c_j"));
    m_arrCurve->addItem(I18n::tr("asrc_c_banana"));
    m_arrCurve->addItem(I18n::tr("asrc_c_custom"));
    m_arrCurve->setCurrentIndex(1);   // mock: value="jcurve"
    s->form()->addRow(I18n::tr("asrc_curve"), m_arrCurve);
    auto *splayRow = new QHBoxLayout();
    m_arrSplay = new QLineEdit("0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14", s);
    splayRow->addWidget(m_arrSplay, 1);
    splayRow->addWidget(new QLabel(I18n::tr("asrc_splay_unit"), s));
    s->form()->addRow(I18n::tr("asrc_splay"), splayRow);
    v->addWidget(s);

    auto *sd = new SectionBox(I18n::tr("asrc_dg_section"), page);
    auto *steerRow = new QHBoxLayout();
    m_arrSteer = new QCheckBox(I18n::tr("asrc_steer_auto"), sd);
    m_arrSteer->setChecked(true);
    m_arrSteerDeg = new QLineEdit("0", sd);
    m_arrSteerDeg->setMaximumWidth(60);
    steerRow->addWidget(m_arrSteer);
    steerRow->addWidget(m_arrSteerDeg);
    steerRow->addWidget(new QLabel(I18n::tr("asrc_steer_unit"), sd));
    steerRow->addStretch(1);
    sd->form()->addRow("Beam steering", steerRow);
    auto *covRow = new QHBoxLayout();
    auto *cov0 = new QLineEdit("5", sd);  cov0->setMaximumWidth(60);
    auto *cov1 = new QLineEdit("30", sd); cov1->setMaximumWidth(60);
    covRow->addWidget(cov0);
    covRow->addWidget(new QLabel(QString::fromUtf8("〜"), sd));
    covRow->addWidget(cov1);
    covRow->addWidget(new QLabel("m", sd));
    covRow->addStretch(1);
    sd->form()->addRow(I18n::tr("asrc_coverage"), covRow);
    auto *uniRow = new QHBoxLayout();
    auto *uni = new QLineEdit("3", sd);
    uni->setMaximumWidth(60);
    uniRow->addWidget(new QLabel(QString::fromUtf8("±"), sd));
    uniRow->addWidget(uni);
    uniRow->addWidget(new QLabel("dB", sd));
    uniRow->addStretch(1);
    sd->form()->addRow(I18n::tr("asrc_uniform"), uniRow);
    auto *chkRow = new QHBoxLayout();
    m_arrGrating = new QCheckBox(I18n::tr("asrc_grating"), sd);
    m_arrGrating->setChecked(true);
    auto *airabs = new QCheckBox(I18n::tr("asrc_airabs"), sd);
    // 空気吸収は距離に依存する量で、遠方界パターン (相対値) には効かない
    tabhelp::markNotImplemented(airabs);
    chkRow->addWidget(m_arrGrating);
    chkRow->addWidget(airabs);
    chkRow->addStretch(1);
    sd->vbox()->addLayout(chkRow);
    v->addWidget(sd);

    // ── 合成された指向性 (実計算) ────────────────────────────────────────
    auto *sr = new SectionBox(I18n::tr("asrc_beam_section"), page);
    auto *beamHint = new QLabel(I18n::tr("asrc_beam_hint"), sr);
    beamHint->setWordWrap(true);
    sr->vbox()->addWidget(beamHint);
    m_arrFreq = new QComboBox(sr);
    m_arrFreq->addItems({ "125 Hz", "250 Hz", "500 Hz", "1 kHz", "2 kHz",
                          "4 kHz", "8 kHz" });
    m_arrFreq->setCurrentIndex(3);
    m_arrFreq->setMaximumWidth(120);
    sr->form()->addRow(I18n::tr("asrc_beam_freq"), m_arrFreq);
    m_arrPlot = new MiniPlot(sr);
    m_arrPlot->setLabels(I18n::tr("asrc_beam_x"), "dB");
    m_arrPlot->setYRange(-30.0, 3.0);
    m_arrPlot->setMinimumHeight(170);
    sr->vbox()->addWidget(m_arrPlot);
    m_arrTable = new QTableWidget(0, 2, sr);
    m_arrTable->setHorizontalHeaderLabels({ I18n::tr("asrc_beam_item"),
                                            I18n::tr("asrc_beam_value") });
    m_arrTable->verticalHeader()->setVisible(false);
    m_arrTable->verticalHeader()->setDefaultSectionSize(24);
    m_arrTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_arrTable->horizontalHeader()->setSectionResizeMode(0,
                                                         QHeaderView::Stretch);
    m_arrTable->horizontalHeader()
        ->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_arrTable->setMinimumHeight(160);
    sr->vbox()->addWidget(m_arrTable);
    m_arrNote = new QLabel(sr);
    m_arrNote->setWordWrap(true);
    m_arrNote->setStyleSheet("color:#B45309;");
    m_arrNote->setVisible(false);
    sr->vbox()->addWidget(m_arrNote);
    v->addWidget(sr);

    auto *ss = new SectionBox(I18n::tr("asrc_sub_section"), page);
    m_subLayout = new QComboBox(ss);
    m_subLayout->addItem(I18n::tr("asrc_l_single"));
    m_subLayout->addItem(I18n::tr("asrc_l_endfire"));
    m_subLayout->addItem(I18n::tr("asrc_l_cardioid"));
    m_subLayout->addItem(I18n::tr("asrc_l_gradient"));
    m_subLayout->setCurrentIndex(2);   // mock: value="cardioid"
    ss->form()->addRow(I18n::tr("asrc_layout"), m_subLayout);
    m_subRev = new QCheckBox(I18n::tr("asrc_rev_chk"), ss);
    m_subRev->setChecked(true);
    ss->form()->addRow(I18n::tr("asrc_rev_rear"), m_subRev);
    auto *delayRow = new QHBoxLayout();
    m_subDelay = new QLineEdit("3.5", ss);
    m_subDelay->setMaximumWidth(60);
    delayRow->addWidget(m_subDelay);
    delayRow->addWidget(new QLabel("ms", ss));
    delayRow->addStretch(1);
    ss->form()->addRow(I18n::tr("asrc_delay_rear"), delayRow);
    m_subInfo = new QLabel(ss);
    m_subInfo->setWordWrap(true);
    ss->vbox()->addWidget(m_subInfo);
    v->addWidget(ss);
    // 画面の合成結果は実計算だが、アレイの設定そのものはソルバ入力へは
    // 渡らない (.ofd/.ofdx に対応キーが無い) — そこを明示する
    v->addWidget(tabhelp::unwiredNote(page, I18n::tr("asrc_uw_array")));
    v->addStretch(1);

    // 入力が変わったら合成し直す
    const QLineEdit *edits[5] = { m_arrElems, m_arrSpacing, m_arrSplay,
                                  m_arrSteerDeg, m_subDelay };
    for (int i = 0; i < 5; ++i)
        connect(const_cast<QLineEdit *>(edits[i]), &QLineEdit::editingFinished,
                this, &AcousticSourceTab::updateArray);
    connect(m_arrCurve, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateArray(); });
    connect(m_arrFreq, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateArray(); });
    connect(m_subLayout, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateArray(); });
    connect(m_arrSteer, &QCheckBox::toggled, this, [this](bool) { updateArray(); });
    connect(m_arrGrating, &QCheckBox::toggled, this, [this](bool) { updateArray(); });
    connect(m_subRev, &QCheckBox::toggled, this, [this](bool) { updateArray(); });
    updateArray();
    return page;
}

// ── page: aural ─────────────────────────────────────────────────────────────
QWidget *AcousticSourceTab::buildAuralPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("asrc_aural_section"), page);
    auto *hint = new QLabel(I18n::tr("asrc_aural_hint"), s);
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::RichText);
    s->vbox()->addWidget(hint);
    v->addWidget(s);

    auto *sc = new SectionBox(I18n::tr("asrc_conv_section"), page);
    auto *inRow = new QHBoxLayout();
    auto *inWav = new QLineEdit(sc);
    inWav->setPlaceholderText("anechoic_speech_48k.wav");
    auto *inBtn = new QPushButton("📁", sc);
    inBtn->setMaximumWidth(36);
    inRow->addWidget(inWav, 1);
    inRow->addWidget(inBtn);
    sc->form()->addRow(I18n::tr("asrc_input_wav"), inRow);
    auto *recv = new QComboBox(sc);
    recv->addItem(I18n::tr("asrc_p1"));
    recv->addItem(I18n::tr("asrc_p2"));
    recv->addItem(I18n::tr("asrc_p3"));
    recv->addItem(I18n::tr("asrc_p4"));
    sc->form()->addRow(I18n::tr("asrc_recv_irf"), recv);
    auto *outch = new QComboBox(sc);
    outch->addItem(I18n::tr("asrc_oc_mono"));
    outch->addItem(I18n::tr("asrc_oc_stereo"));
    outch->addItem(I18n::tr("asrc_oc_binaural"));
    outch->addItem("Ambisonics (B-format)");
    outch->addItem("5.1ch");
    outch->addItem("22.2ch");
    outch->setCurrentIndex(2);   // mock: value="binaural"
    sc->form()->addRow(I18n::tr("asrc_outch"), outch);
    auto *hrtf = new QComboBox(sc);
    hrtf->addItem("KEMAR (MIT)");
    hrtf->addItem("CIPIC (UC Davis)");
    hrtf->addItem("SADIE II (York)");
    hrtf->addItem(I18n::tr("asrc_hrtf_personal"));
    sc->form()->addRow(I18n::tr("asrc_hrtf"), hrtf);
    auto *conv = new QComboBox(sc);
    conv->addItem(I18n::tr("asrc_cv_direct"));
    conv->addItem(I18n::tr("asrc_cv_fft"));
    conv->addItem(I18n::tr("asrc_cv_part"));
    conv->setCurrentIndex(2);   // mock: value="partition"
    sc->form()->addRow(I18n::tr("asrc_convmode"), conv);
    // 畳み込み設定はまだどこにも読まれない
    sc->vbox()->addWidget(tabhelp::unwiredNote(sc, I18n::tr("asrc_uw_conv")));
    v->addWidget(sc);

    auto *sr = new SectionBox(I18n::tr("asrc_render_section"), page);
    auto *outFile = new QLineEdit("aural_concert_hall_P1.wav", sr);
    sr->form()->addRow(I18n::tr("asrc_outfile"), outFile);
    m_renderRate = new QComboBox(sr);
    m_renderRate->addItems({ "44.1 kHz", "48 kHz", "96 kHz", "192 kHz" });
    m_renderRate->setCurrentIndex(1);   // mock: value="48"
    sr->form()->addRow(I18n::tr("asrc_srate"), m_renderRate);
    auto *bits = new QComboBox(sr);
    bits->addItems({ "16-bit", "24-bit", "32-bit float" });
    bits->setCurrentIndex(1);   // mock: value="24"
    sr->form()->addRow(I18n::tr("asrc_bits"), bits);
    auto *btnRow = new QHBoxLayout();
    auto *renderBtn = new QPushButton(I18n::tr("asrc_btn_render"), sr);
    auto *listenBtn = new QPushButton(I18n::tr("asrc_btn_listen2"), sr);
    auto *abBtn     = new QPushButton(I18n::tr("asrc_btn_ab"), sr);
    // レンダリング (畳み込み) 自体は可聴化タブが担う — このページでは未実装
    tabhelp::markNotImplemented(renderBtn);
    btnRow->addWidget(renderBtn);
    btnRow->addWidget(listenBtn);
    btnRow->addWidget(abBtn);
    btnRow->addStretch(1);
    sr->vbox()->addLayout(btnRow);
    v->addWidget(sr);

    auto *sa = new SectionBox(I18n::tr("asrc_ab_section"), page);
    auto *abRow = new QHBoxLayout();
    auto *dry = new QCheckBox(I18n::tr("asrc_ab_dry"), sa);
    dry->setChecked(true);
    auto *wet = new QCheckBox(I18n::tr("asrc_ab_wet"), sa);
    wet->setChecked(true);
    auto *revOnly = new QCheckBox(I18n::tr("asrc_ab_revonly"), sa);
    abRow->addWidget(dry);
    abRow->addWidget(wet);
    abRow->addWidget(revOnly);
    abRow->addStretch(1);
    sa->form()->addRow(I18n::tr("asrc_ab_target"), abRow);
    auto *seatRow = new QHBoxLayout();
    auto *seat = new QComboBox(sa);
    seat->addItem(I18n::tr("asrc_seat1"));
    seat->addItem(I18n::tr("asrc_seat2"));
    seat->addItem(I18n::tr("asrc_seat3"));
    seat->addItem(I18n::tr("asrc_seat4"));
    seatRow->addWidget(seat, 1);
    auto *playBtn = new QPushButton(I18n::tr("asrc_play"), sa);
    tabhelp::markNotImplemented(playBtn);
    seatRow->addWidget(playBtn);
    sa->form()->addRow(I18n::tr("asrc_seat"), seatRow);
    auto *abx = new QCheckBox(I18n::tr("asrc_abx_chk"), sa);
    sa->form()->addRow(I18n::tr("asrc_abx"), abx);
    v->addWidget(sa);

    // ── 可聴化品質指標 — 実測 RIR (RIR 分析タブで設定) からの実計算 ──
    auto *sq = new SectionBox(I18n::tr("asrc_quality_section"), page);
    m_qualTable = new QTableWidget(5, 3, sq);
    setupTable(m_qualTable,
               { I18n::tr("asrc_col_item"), I18n::tr("asrc_col_value"),
                 I18n::tr("asrc_col_verdict") }, 150);
    sq->vbox()->addWidget(m_qualTable);
    auto *qBtnRow = new QHBoxLayout();
    auto *qBtn = new QPushButton(I18n::tr("asrc_q_btn"), sq);
    qBtnRow->addWidget(qBtn);
    qBtnRow->addStretch(1);
    sq->vbox()->addLayout(qBtnRow);
    m_qualNote = new QLabel(I18n::tr("asrc_q_idle"), sq);
    m_qualNote->setWordWrap(true);
    m_qualNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sq->vbox()->addWidget(m_qualNote);
    clearAuralQuality();
    v->addWidget(sq);
    v->addStretch(1);

    connect(qBtn, &QPushButton::clicked, this,
            [this] { computeAuralQuality(); });

    // 入力WAV の参照ボタン (隣の QLineEdit にパスを反映)
    connect(inBtn, &QPushButton::clicked, this, [this, inWav] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("asrc_input_wav"), QString(),
            "Audio (*.wav *.flac *.aiff *.ogg);;All files (*)");
        if (!path.isEmpty()) inWav->setText(path);
    });
    connect(m_renderRate, &QComboBox::currentIndexChanged, this,
            [this] { apply(); });
    // 試聴: 可聴化タブがレンダリングした出力 WAV を外部プレイヤで再生。
    // まだ出力が無い場合は明示的にその旨を伝える (虚偽の動作表示をしない)
    connect(listenBtn, &QPushButton::clicked, this, [this] {
        const QString out =
            m_p->operaAcoustic().auralizationOutputFile.trimmed();
        if (out.isEmpty() || !QFileInfo::exists(out)) {
            QMessageBox::information(this, I18n::tr("asrc_listen_title"),
                                     I18n::tr("asrc_norender"));
            return;
        }
        playWavExternal(this, out);
    });
    // A/B 比較: dry/wet の書き出し・比較は可聴化タブで実装済み —
    // 入力WAV を dry ファイルとして引き渡し、実行場所を案内する
    connect(abBtn, &QPushButton::clicked, this, [this, inWav] {
        const QString wav = inWav->text().trimmed();
        if (!wav.isEmpty() && QFileInfo::exists(wav)) {
            m_p->operaAcoustic().auralizationDryFile = wav;
            m_p->operaAcoustic().enabled = true;
            m_p->touch();
            QMessageBox::information(this, I18n::tr("asrc_btn_ab"),
                                     I18n::tr("asrc_ab_handoff"));
        } else {
            QMessageBox::information(this, I18n::tr("asrc_btn_ab"),
                                     I18n::tr("asrc_ab_delegate"));
        }
    });
    return page;
}

// 可聴化品質指標を「未算出」状態にする。値は出さず ("—")、LF と位相応答は
// 算出できない理由を判定欄に書く (絶対規則 5・6: 偽の値を出さない)。
void AcousticSourceTab::clearAuralQuality()
{
    const QString items[5] = {
        I18n::tr("asrc_q_irflen"), I18n::tr("asrc_q_density"),
        QStringLiteral("ITDG (Initial Time Delay Gap)"),
        I18n::tr("asrc_q_lf"), I18n::tr("asrc_q_phase"),
    };
    const QString verdicts[5] = {
        kDash, kDash, kDash,
        I18n::tr("asrc_q_lf_need"),   // ISO 3382-1 の 2ch 測定が必要
        I18n::tr("th_notimpl"),       // 最小位相補正は未実装
    };
    for (int r = 0; r < 5; ++r) {
        m_qualTable->setItem(r, 0, new QTableWidgetItem(items[r]));
        m_qualTable->setItem(r, 1, new QTableWidgetItem(kDash));
        m_qualTable->setItem(r, 2, new QTableWidgetItem(verdicts[r]));
    }
    if (m_qualNote) m_qualNote->setText(I18n::tr("asrc_q_idle"));
}

// 実測 RIR (OperaAcousticSettings::rirPath) を分析し、IRF長・初期反射密度・
// ITDG を実計算して表へ入れる。分析は QThread で非同期 (gui.md)。
// LF (側方音エネルギー) は ISO 3382-1 で無指向性 + 双指向性マイクの 2ch 測定を
// 要求するためモノ RIR からは求まらない → 「—」のまま。
void AcousticSourceTab::computeAuralQuality()
{
    if (m_qualBusy) return;
    const OperaAcousticSettings settings = m_p->operaAcoustic();
    const QString path = settings.rirPath.trimmed();
    if (path.isEmpty()) {
        QMessageBox::information(this, I18n::tr("asrc_q_title"),
                                 I18n::tr("asrc_q_norir"));
        return;
    }
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, I18n::tr("asrc_q_title"),
                             I18n::tr("asrc_q_nofile").arg(path));
        return;
    }

    struct QualData {
        bool ok = false;
        QString err;
        double duration = 0.0;      // IRF 長 [s]
        bool   haveT30 = false;
        double t30 = 0.0;           // 有効帯域の最大 T30 [s]
        int    early = 0;           // 直接音後 0–50 ms の反射数
        bool   haveItdg = false;
        double itdgSec = 0.0;       // 直接音→初反射 [s]
    };
    auto d = std::make_shared<QualData>();
    m_qualBusy = true;
    m_qualNote->setText(I18n::tr("asrc_q_busy"));

    QThread *th = QThread::create([settings, d] {
        const acoustics::AcousticResult<acoustics::RirAnalysisResult> res =
            QtAcousticAdapter::analyzeFile(settings);
        if (!res.success()) {
            d->err = QString::fromStdString(res.message());
            return;
        }
        const acoustics::RirAnalysisResult &r = res.value();
        d->duration = r.preprocess.durationSeconds;
        // IRF 長の判定基準に使う T30 (有効な帯域の最大値)
        for (const acoustics::BandMetricsResult &b : r.bands) {
            if (!b.metrics.t30.valid) continue;
            if (!d->haveT30 || b.metrics.t30.value > d->t30) {
                d->t30 = b.metrics.t30.value;
                d->haveT30 = true;
            }
        }
        // 反射は時刻昇順 — 最初の正遅延が ITDG、0–50 ms の個数が初期反射密度
        for (const acoustics::ReflectionEvent &e : r.reflections) {
            if (e.delayFromDirect <= 0.0) continue;
            if (!d->haveItdg) {
                d->haveItdg = true;
                d->itdgSec = e.delayFromDirect;
            }
            if (e.delayFromDirect <= 0.05) ++d->early;
        }
        d->ok = true;
    });
    connect(th, &QThread::finished, this, [this, th, d, path] {
        th->deleteLater();
        m_qualBusy = false;
        if (!d->ok) {
            clearAuralQuality();
            m_qualNote->setText(I18n::tr("asrc_q_fail").arg(d->err));
            return;
        }
        clearAuralQuality();
        auto setCell = [this](int row, const QString &value,
                              const QString &verdict, bool positive) {
            m_qualTable->setItem(row, 1, new QTableWidgetItem(value));
            auto *ver = new QTableWidgetItem(verdict);
            if (positive) ver->setForeground(QBrush(kAcc));
            m_qualTable->setItem(row, 2, ver);
        };
        // IRF 長: 減衰を最後まで含むか (T30 に満たない収録は評価に不足)
        const bool longEnough = d->haveT30 && d->duration >= d->t30;
        setCell(0, QStringLiteral("%1 s")
                       .arg(QString::number(d->duration, 'f', 2)),
                d->haveT30 ? I18n::tr(longEnough ? "asrc_q_enough"
                                                 : "asrc_q_short")
                           : kDash,
                longEnough);
        // 初期反射密度: 直接音後 0–50 ms に検出した反射の個数 (実測)
        setCell(1, I18n::tr("asrc_q_refl_n").arg(d->early), kDash, false);
        // ITDG: Beranek の親密感の目安 (≲20 ms) を満たすかだけを述べる
        // (L. L. Beranek, "Concert Halls and Opera Houses", 2nd ed.)
        if (d->haveItdg) {
            const double ms = d->itdgSec * 1000.0;
            setCell(2, QStringLiteral("%1 ms").arg(QString::number(ms, 'f', 1)),
                    ms <= 20.0 ? I18n::tr("asrc_q_good") : kDash, ms <= 20.0);
        }
        m_qualNote->setText(
            I18n::tr("asrc_q_done").arg(QFileInfo(path).fileName()));
    });
    th->start();
}

// ── domain switch (音響 ⇔ 水中) ─────────────────────────────────────────────
void AcousticSourceTab::onDomainChanged()
{
    const bool uw = isUnderwater();
    m_srcHint->setText(I18n::tr(uw ? "asrc_src_hint_uw" : "asrc_src_hint_room"));
    m_presetBtn->setText(QStringLiteral("%1 (%2)")
        .arg(I18n::tr("asrc_btn_preset"),
             I18n::tr(uw ? "asrc_preset_sonar" : "asrc_preset_speaker")));
    m_baseSplUnit->setText(uw ? QStringLiteral("dB re μPa·m")
                              : QStringLiteral("dB SPL"));
    // 表示単位はドメインで変わる (室内 = SPL@1m / 水中 = SL)
    m_srcTable->setHorizontalHeaderLabels({
        "", "#", I18n::tr("asrc_col_name"), I18n::tr("asrc_col_kind"),
        I18n::tr("asrc_col_pos"), I18n::tr("asrc_col_dir"),
        I18n::tr("asrc_col_sig"),
        I18n::tr(uw ? "asrc_col_spl_uw" : "asrc_col_spl_room"),
        I18n::tr("asrc_col_solver") });
    // 反映導線は室内音響のみ (水中は BELLHOP — .ofd の feed を使わない)
    m_syncBtn->setVisible(!uw);
    m_syncNote->setVisible(!uw);
    refreshSourceTable();

    // ライブラリ表 (最終行のみドメイン依存)。音源ファイルは 1 つも同梱して
    // いないので「同梱」列は全行 "—" (○/● は同梱の誤認を招く)。
    const struct { const char *mat, *len, *use; } kLib[8] = {
        { "asrc_lib1", "15s",  "asrc_lib1u" },
        { "asrc_lib2", "15s",  "asrc_lib2u" },
        { "asrc_lib3", "30s",  "asrc_lib3u" },
        { "asrc_lib4", "20s",  "asrc_lib4u" },
        { "asrc_lib5", "1ms",  "asrc_lib5u" },
        { "asrc_lib6", "1-10s","asrc_lib6u" },
        { "asrc_lib7", "10s",  "asrc_lib7u" },
        { nullptr,     nullptr, nullptr },
    };
    for (int r = 0; r < 8; ++r) {
        const bool last = (r == 7);
        m_libTable->setItem(r, 0, new QTableWidgetItem(kDash));
        m_libTable->setItem(r, 1, new QTableWidgetItem(
            last ? I18n::tr(uw ? "asrc_lib8_uw" : "asrc_lib8_room")
                 : I18n::tr(kLib[r].mat)));
        m_libTable->setItem(r, 2, new QTableWidgetItem(
            last ? QString::fromUtf8(uw ? "5s" : "100ms")
                 : QString::fromUtf8(kLib[r].len)));
        m_libTable->setItem(r, 3, new QTableWidgetItem(
            last ? I18n::tr(uw ? "asrc_lib8u_uw" : "asrc_lib8u_room")
                 : I18n::tr(kLib[r].use)));
    }
    refresh();
}

// ── model ⇔ widgets ────────────────────────────────────────────────────────
void AcousticSourceTab::apply()
{
    if (m_updating) return;
    if (isUnderwater())
        m_p->underwater().sonarSL_dB = m_baseSpl->text().toDouble();
    else
        m_p->acoustic().srcSPL_dB = m_baseSpl->text().toDouble();
    static const int kRates[4] = { 44100, 48000, 96000, 192000 };
    m_p->acoustic().sampleRate =
        kRates[qBound(0, m_renderRate->currentIndex(), 3)];
    m_p->touch();
}

void AcousticSourceTab::refresh()
{
    m_updating = true;
    const double spl = isUnderwater() ? m_p->underwater().sonarSL_dB
                                      : m_p->acoustic().srcSPL_dB;
    m_baseSpl->setText(QString::number(spl, 'g', 6));
    const int sr = m_p->acoustic().sampleRate;
    m_renderRate->setCurrentIndex(
        sr == 44100 ? 0 : sr == 96000 ? 2 : sr == 192000 ? 3 : 1);
    // 入力信号の前処理 (.ofdx acoustic.source_wav) を読み直す
    if (m_wavTrim0) {
        const AcousticOpts &a = m_p->acoustic();
        m_wavTrim0->setText(QString::number(a.wavTrimStart_s, 'g', 6));
        m_wavTrim1->setText(QString::number(a.wavTrimEnd_s, 'g', 6));
        m_wavGain->setText(QString::number(a.wavGain_dB, 'g', 6));
        m_wavHpf->setChecked(a.wavHighPass);
        m_wavHpfHz->setText(QString::number(a.wavHighPassHz, 'g', 6));
    }
    // 音源リスト (.ofdx) を読み直す
    refreshSourceTable();
    // 別プロジェクトが読み込まれたら、前の RIR の分析結果は無効 — 未算出へ
    if (!m_qualBusy) clearAuralQuality();
    m_updating = false;
}
