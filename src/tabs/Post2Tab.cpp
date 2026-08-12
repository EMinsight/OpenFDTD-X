// Post2Tab.cpp
#include "Post2Tab.h"
#include "TabHelpers.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ固有の翻訳キー (p2x_) — file-local 登録 (既存 p2_ / pp_ は I18n.cpp) ──
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // 遠方界の成分 (far2dcomponent) — mock: pp_main_axis / pp_sub_axis / pp_cp_*
    I18n::reg("p2x_main_axis", "主軸", "Main axis");
    I18n::reg("p2x_sub_axis", "副軸", "Cross axis");
    I18n::reg("p2x_cp_l", "左旋円偏波", "LHCP");
    I18n::reg("p2x_cp_r", "右旋円偏波", "RHCP");
    I18n::reg("p2x_scale", "スケール", "Scale");
    // 遠方界面上(2D) の表 (mock: 面の向き列 / 形式列) と正規化。
    // .ofd の plotfar1d 第1引数 X/Y/Z/V/H に対応する表示名。
    // 近傍界面上の「面」列でも X面/Y面/Z面 を使う。
    I18n::reg("p2x_face_x", "X面", "X-plane");
    I18n::reg("p2x_face_y", "Y面", "Y-plane");
    I18n::reg("p2x_face_z", "Z面", "Z-plane");
    I18n::reg("p2x_face_phi", "φ一定面", "φ const");
    I18n::reg("p2x_face_theta", "θ一定面", "θ const");
    I18n::reg("p2x_circle", "円プロット", "Polar");
    I18n::reg("p2x_xy", "XYプロット", "Linear");
    I18n::reg("p2x_normalize", "最大値で正規化", "Normalize to max");
    // 遠方界全方向(3D) (mock: pp_far_3d) と θ/φ 分割数のまとめラベル
    I18n::reg("p2x_far_3d", "遠方界全方向(3D)", "Far-field full (3D)");
    I18n::reg("p2x_angle_div", "角度分割数", "Angle div");
    // 近傍界面上 (mock: pp_draw_body / pp_zoom / pp_animation + pp_frames)
    I18n::reg("p2x_draw_body", "物体を描く", "Draw objects");
    I18n::reg("p2x_zoom", "一部拡大", "Zoom");
    I18n::reg("p2x_frames", "動画フレーム数", "Animation frames");
    I18n::reg("p2x_frames_hint",
              "フレーム数は .ofd に無いためローカル設定です。",
              "Frame count has no .ofd key, so it is a local setting.");
    I18n::reg("p2x_anim", "動画出力 (near2dframe)",
              "Animation output (near2dframe)");
    // 描画方法 (mock: pp_draw_method の select — 3 択)
    I18n::reg("p2x_draw_method", "描画方法", "Render mode");
    I18n::reg("p2x_draw_fill", "カラー塗りつぶし", "Color fill");
    I18n::reg("p2x_draw_contour", "等高線", "Contour");
    I18n::reg("p2x_draw_vector", "ベクトル", "Vector");
    I18n::reg("p2x_draw_hint",
              "等高線のみ .ofd (near2dcontour) に対応キーがあります。"
              "ベクトル表示はローカル設定です。",
              "Only 'contour' has an .ofd counterpart (near2dcontour); "
              "vector rendering is a local setting.");
    // near1d/near2d の「成分」列の候補ヒント — ドメインで文言を切り替える
    // (機能は全ドメイン有効。列そのものは隠さない)
    I18n::reg("p2x_cmp_hint_em",
              "成分の候補: E / Ex / Ey / Ez / H / Hx / Hy / Hz",
              "Component candidates: E / Ex / Ey / Ez / H / Hx / Hy / Hz");
    I18n::reg("p2x_cmp_hint_ac",
              "成分の候補: p (音圧) など",
              "Component candidates: p (sound pressure), etc.");
    // エクスポート (mock: エクスポート / Export)
    I18n::reg("p2x_export", "エクスポート", "Export");
    I18n::reg("p2x_export_hint",
              "時系列データ・場分布を .h5 で保存 "
              "(書出しは未実装 — 設定の記録のみ)",
              "Saves time-series data and field distributions as .h5 "
              "(export is not implemented — settings are recorded only)");
    return true;
}();

// mock の <span className="badge acc"> 相当 (スタイルは最小限)
QLabel *makeBadge(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet("QLabel { border: 1px solid palette(mid); border-radius: 3px;"
                     " padding: 1px 6px; color: #0078D4; }");
    return l;
}
} // namespace

Post2Tab::Post2Tab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    auto applyCb = [this] { apply(); };

    // 遠方界・近傍界は frequency2 が無いと ofd_post が 1 枚も出さない
    // (post/post.c:37)。チェックを受け付けて黙っていると「反映されない」に
    // 見えるので、出ない項目を名指しで出す。
    m_prereq = new QLabel(body);
    m_prereq->setWordWrap(true);
    m_prereq->setStyleSheet("color:#B8860B; font-size:11px;");
    m_prereq->setVisible(false);
    v->addWidget(m_prereq);

    // far0d
    auto *s0 = new SectionBox(I18n::tr("p2_far0d"), body);
    m_far0dSection = s0;                // 音響/水中では丸ごと隠す (下記参照)
    m_far0d = new QCheckBox(I18n::tr("p2_far0d"), s0);
    m_far0dTheta = new QLineEdit(s0); m_far0dTheta->setMaximumWidth(80);
    m_far0dPhi   = new QLineEdit(s0); m_far0dPhi->setMaximumWidth(80);
    auto *r0 = new QHBoxLayout();
    r0->addWidget(m_far0d, 1);
    r0->addWidget(new QLabel("θ", s0));
    r0->addWidget(m_far0dTheta);
    r0->addWidget(new QLabel("φ", s0));
    r0->addWidget(m_far0dPhi);
    s0->vbox()->addLayout(r0);
    v->addWidget(s0);

    // far1d
    auto *s1 = new SectionBox(I18n::tr("p2_far1d"), body);
    m_far1dSection = s1;                // 音響/水中では丸ごと隠す (下記参照)
    m_far1d = new QTableWidget(0, 3, s1);
    m_far1d->setHorizontalHeaderLabels({
        I18n::tr("p2_dir"), I18n::tr("p2_division"), I18n::tr("p2_angle") });
    m_far1d->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_far1d->verticalHeader()->setDefaultSectionSize(24);
    m_far1d->setMinimumHeight(90);
    s1->vbox()->addWidget(m_far1d);
    auto *r1 = new QHBoxLayout();
    auto *add1 = new QPushButton(I18n::tr("p2_add"), s1);
    auto *del1 = new QPushButton(I18n::tr("p2_del"), s1);
    r1->addWidget(add1); r1->addWidget(del1); r1->addStretch(1);
    s1->vbox()->addLayout(r1);

    auto *r1b = new QHBoxLayout();
    r1b->addWidget(new QLabel(I18n::tr("p2_style"), s1));
    m_far1dStyle = new QComboBox(s1);
    // mock の「形式」列 = 円プロット / XYプロット (far1dstyle 0 / 1)。
    // 既存ファイルが持ちうる 2 は値を落とさないため選択肢として残す
    // (ラベルは .ofd の生値のまま)。
    m_far1dStyle->addItems({ I18n::tr("p2x_circle"), I18n::tr("p2x_xy"),
                             QStringLiteral("2") });
    r1b->addWidget(m_far1dStyle);
    m_far1dDb = new QCheckBox(I18n::tr("p2_db"), s1);
    r1b->addWidget(m_far1dDb);
    // mock: pp_normalize (最大値で正規化) = far1dnorm
    m_far1dNorm = new QCheckBox(I18n::tr("p2x_normalize"), s1);
    m_far1dNorm->setToolTip("far1dnorm");
    r1b->addWidget(m_far1dNorm);
    r1b->addStretch(1);
    s1->vbox()->addLayout(r1b);

    auto *r1c = new QHBoxLayout();
    r1c->addWidget(new QLabel(I18n::tr("p2_component"), s1));
    m_far1dCompE     = new QCheckBox("E", s1);
    m_far1dCompTheta = new QCheckBox("Eθ", s1);
    m_far1dCompPhi   = new QCheckBox("Eφ", s1);
    r1c->addWidget(m_far1dCompE);
    r1c->addWidget(m_far1dCompTheta);
    r1c->addWidget(m_far1dCompPhi);
    r1c->addStretch(1);
    s1->vbox()->addLayout(r1c);
    v->addWidget(s1);

    // far2d (mock: <Section title={t("pp_far_3d")}> = 遠方界全方向(3D))
    auto *s2 = new SectionBox(I18n::tr("p2x_far_3d"), body);
    m_far2dSection = s2;                // 音響/水中では丸ごと隠す (下記参照)
    m_far2d = new QCheckBox(I18n::tr("p2_far2d"), s2);
    m_far2dTheta = new QSpinBox(s2); m_far2dTheta->setRange(1, 3600);
    m_far2dPhi   = new QSpinBox(s2); m_far2dPhi->setRange(1, 3600);
    m_far2dDb = new QCheckBox(I18n::tr("p2_db"), s2);
    m_far2dObj = new QLineEdit(s2); m_far2dObj->setMaximumWidth(80);
    auto *r2 = new QHBoxLayout();
    r2->addWidget(m_far2d, 1);
    // mock: <label>{t("pp_angle_div")}</label> + muted "θ:" / "φ:" の 2 入力
    r2->addWidget(new QLabel(I18n::tr("p2x_angle_div"), s2));
    auto *thLab = new QLabel("θ:", s2);
    auto *phLab = new QLabel("φ:", s2);
    for (auto *l : { thLab, phLab })
        l->setStyleSheet("color:#888888;");          // mock: muted
    r2->addWidget(thLab);
    r2->addWidget(m_far2dTheta);
    r2->addWidget(phLab);
    r2->addWidget(m_far2dPhi);
    r2->addWidget(m_far2dDb);
    r2->addWidget(new QLabel(I18n::tr("p2_obj"), s2));
    r2->addWidget(m_far2dObj);
    s2->vbox()->addLayout(r2);

    // 成分 (mock: θ成分 / φ成分 / 主軸 / 副軸 / 左旋円偏波 / 右旋円偏波)
    // far2dcomponent = E Eθ Eφ Emajor Eminor Elhcp Erhcp の 7 フラグ。
    // 先頭 3 成分は記号そのまま、残り 4 成分は翻訳キー。
    static const char *kComp[7] = { "E", "Eθ", "Eφ", "p2x_main_axis",
                                    "p2x_sub_axis", "p2x_cp_l", "p2x_cp_r" };
    auto *r2c = new QHBoxLayout();
    r2c->addWidget(new QLabel(I18n::tr("p2_component"), s2));
    for (int i = 0; i < 7; ++i) {
        const QString label = (i < 3) ? QString::fromUtf8(kComp[i])
                                      : I18n::tr(kComp[i]);
        m_far2dComp[i] = new QCheckBox(label, s2);
        r2c->addWidget(m_far2dComp[i]);
    }
    r2c->addStretch(1);
    s2->vbox()->addLayout(r2c);

    // スケール指定 (mock: pp_scale の dBi 範囲) — far2dscale = min max
    auto *r2s = new QHBoxLayout();
    r2s->addWidget(new QLabel(I18n::tr("p2x_scale"), s2));
    m_far2dUserScale = new QCheckBox(I18n::tr("p1_user_scale"), s2);
    m_far2dMin = new QLineEdit(s2); m_far2dMin->setMaximumWidth(80);
    m_far2dMax = new QLineEdit(s2); m_far2dMax->setMaximumWidth(80);
    r2s->addWidget(m_far2dUserScale);
    r2s->addWidget(new QLabel(I18n::tr("p1_min"), s2));
    r2s->addWidget(m_far2dMin);
    r2s->addWidget(new QLabel(I18n::tr("p1_max"), s2));
    r2s->addWidget(m_far2dMax);
    r2s->addStretch(1);
    s2->vbox()->addLayout(r2s);
    v->addWidget(s2);

    // near1d
    auto *s3 = new SectionBox(I18n::tr("p2_near1d"), body);
    m_near1d = new QTableWidget(0, 4, s3);
    m_near1d->setHorizontalHeaderLabels({
        I18n::tr("p2_cmp"), I18n::tr("p2_dir"),
        I18n::tr("p2_pos") + " 1", I18n::tr("p2_pos") + " 2" });
    m_near1d->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_near1d->verticalHeader()->setDefaultSectionSize(24);
    m_near1d->setMinimumHeight(90);
    s3->vbox()->addWidget(m_near1d);
    auto *r3 = new QHBoxLayout();
    auto *add3 = new QPushButton(I18n::tr("p2_add"), s3);
    auto *del3 = new QPushButton(I18n::tr("p2_del"), s3);
    m_near1dDb = new QCheckBox(I18n::tr("p2_db"), s3);
    m_near1dNoinc = new QCheckBox(I18n::tr("p2_noinc"), s3);
    r3->addWidget(add3); r3->addWidget(del3);
    r3->addWidget(m_near1dDb); r3->addWidget(m_near1dNoinc);
    r3->addStretch(1);
    s3->vbox()->addLayout(r3);
    // 「成分」列の候補ヒント (文言はドメイン別 — updateDomainVisibility)
    m_near1dCmpHint = new QLabel(s3);
    m_near1dCmpHint->setWordWrap(true);
    m_near1dCmpHint->setStyleSheet("color:#888888; font-size:11px;");  // muted
    s3->vbox()->addWidget(m_near1dCmpHint);
    v->addWidget(s3);

    // near2d
    auto *s4 = new SectionBox(I18n::tr("p2_near2d"), body);
    m_near2d = new QTableWidget(0, 3, s4);
    m_near2d->setHorizontalHeaderLabels({
        I18n::tr("p2_cmp"), I18n::tr("p2_dir"), I18n::tr("p2_pos") });
    m_near2d->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_near2d->verticalHeader()->setDefaultSectionSize(24);
    m_near2d->setMinimumHeight(90);
    s4->vbox()->addWidget(m_near2d);
    auto *r4 = new QHBoxLayout();
    auto *add4 = new QPushButton(I18n::tr("p2_add"), s4);
    auto *del4 = new QPushButton(I18n::tr("p2_del"), s4);
    r4->addWidget(add4); r4->addWidget(del4); r4->addStretch(1);
    s4->vbox()->addLayout(r4);
    // 「成分」列の候補ヒント (文言はドメイン別 — updateDomainVisibility)
    m_near2dCmpHint = new QLabel(s4);
    m_near2dCmpHint->setWordWrap(true);
    m_near2dCmpHint->setStyleSheet("color:#888888; font-size:11px;");  // muted
    s4->vbox()->addWidget(m_near2dCmpHint);
    auto *r4b = new QHBoxLayout();
    r4b->addWidget(new QLabel("dim", s4));
    m_near2dDim0 = new QSpinBox(s4); m_near2dDim0->setRange(0, 1);
    m_near2dDim1 = new QSpinBox(s4); m_near2dDim1->setRange(0, 1);
    m_near2dDb = new QCheckBox(I18n::tr("p2_db"), s4);
    m_near2dContour = new QCheckBox(I18n::tr("p2_contour"), s4);
    m_near2dNoinc = new QCheckBox(I18n::tr("p2_noinc"), s4);
    r4b->addWidget(m_near2dDim0);
    r4b->addWidget(m_near2dDim1);
    r4b->addWidget(m_near2dDb);
    r4b->addWidget(m_near2dContour);
    r4b->addWidget(m_near2dNoinc);
    r4b->addStretch(1);
    s4->vbox()->addLayout(r4b);

    // 物体を描く (near2dobj) / 一部拡大 (near2dzoom) — mock の 2 行目のチェック
    auto *r4c = new QHBoxLayout();
    m_near2dDrawObj = new QCheckBox(I18n::tr("p2x_draw_body"), s4);
    m_near2dZoom    = new QCheckBox(I18n::tr("p2x_zoom"), s4);
    r4c->addWidget(m_near2dDrawObj);
    r4c->addWidget(m_near2dZoom);
    r4c->addStretch(1);
    s4->vbox()->addLayout(r4c);

    // 動画 (mock: 表の「動画」列 = 100)。ON/OFF は near2dframe に永続化し、
    // フレーム数だけモデルに無いのでローカル。
    auto *r4d = new QHBoxLayout();
    m_near2dAnim = new QCheckBox(I18n::tr("p2x_anim"), s4);
    r4d->addWidget(m_near2dAnim);
    r4d->addWidget(new QLabel(I18n::tr("p2x_frames"), s4));
    m_near2dFrames = new QLineEdit("100", s4);
    m_near2dFrames->setMaximumWidth(80);
    r4d->addWidget(m_near2dFrames);
    auto *framesHint = new QLabel(I18n::tr("p2x_frames_hint"), s4);
    framesHint->setWordWrap(true);
    framesHint->setStyleSheet("color:#888888; font-size:11px;");  // mock: muted
    r4d->addWidget(framesHint, 1);
    s4->vbox()->addLayout(r4d);

    // 描画方法 (mock: <Row label={t("pp_draw_method")}><select>…) — 3 択。
    // 「等高線」は上の等高線チェック (near2dcontour) と同じ設定を指すので、
    // 双方向に同期させて表示のずれが出ないようにする。
    auto *r4e = new QHBoxLayout();
    r4e->addWidget(new QLabel(I18n::tr("p2x_draw_method"), s4));
    m_near2dDrawMethod = new QComboBox(s4);
    m_near2dDrawMethod->addItems({ I18n::tr("p2x_draw_fill"),
                                   I18n::tr("p2x_draw_contour"),
                                   I18n::tr("p2x_draw_vector") });
    r4e->addWidget(m_near2dDrawMethod);
    auto *drawHint = new QLabel(I18n::tr("p2x_draw_hint"), s4);
    drawHint->setWordWrap(true);
    drawHint->setStyleSheet("color:#888888; font-size:11px;");  // mock: muted
    r4e->addWidget(drawHint, 1);
    s4->vbox()->addLayout(r4e);
    v->addWidget(s4);

    // ── エクスポート / Export ────────────────────────────────────────────────
    // mock どおりのボタン列。実際の出力はソルバ/ポスト実行時 (Runner) が行う
    // ので、ここは要求の入口だけを持つ。
    auto *s5 = new SectionBox(I18n::tr("p2x_export"), body);
    auto *r5 = new QHBoxLayout();
    auto *csvBtn = new QPushButton(QString::fromUtf8("📄 ")
                                   + I18n::tr("pp_export_csv"), s5);
    auto *h5Btn  = new QPushButton(QString::fromUtf8("💾 ")
                                   + I18n::tr("pp_export_h5"), s5);
    // 書出しは未配線 (Runner 側の実行時出力のみ) — 押せる形で放置しない
    tabhelp::markNotImplemented(csvBtn, I18n::tr(tabhelp::notimpl::kKernel));
    tabhelp::markNotImplemented(h5Btn, I18n::tr(tabhelp::notimpl::kKernel));
    r5->addWidget(csvBtn);
    r5->addWidget(h5Btn);
    r5->addWidget(makeBadge("HDF5", s5));
    auto *exportHint = new QLabel(I18n::tr("p2x_export_hint"), s5);
    exportHint->setWordWrap(true);
    exportHint->setStyleSheet("color:#888888; font-size:11px;");  // mock: muted
    r5->addWidget(exportHint, 1);
    s5->vbox()->addLayout(r5);
    v->addWidget(s5);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // wiring
    for (auto *c : { m_far0d, m_far1dDb, m_far1dNorm, m_far1dCompE,
                     m_far1dCompTheta, m_far1dCompPhi, m_far2d, m_far2dDb,
                     m_far2dUserScale, m_near1dDb, m_near1dNoinc, m_near2dDb,
                     m_near2dContour, m_near2dNoinc, m_near2dDrawObj,
                     m_near2dZoom, m_near2dAnim })
        connect(c, &QCheckBox::toggled, this, applyCb);
    for (auto *c : m_far2dComp)
        connect(c, &QCheckBox::toggled, this, applyCb);
    for (auto *e : { m_far0dTheta, m_far0dPhi, m_far2dObj,
                     m_far2dMin, m_far2dMax })
        connect(e, &QLineEdit::editingFinished, this, applyCb);
    for (auto *sp : { m_far2dTheta, m_far2dPhi, m_near2dDim0, m_near2dDim1 })
        connect(sp, &QSpinBox::valueChanged, this, applyCb);
    connect(m_far1dStyle, &QComboBox::currentIndexChanged, this, applyCb);

    // 描画方法 → 等高線チェック (near2dcontour) を追従させる。
    // 「ベクトル」は .ofd に無いので等高線 OFF + ローカル状態のみ。
    connect(m_near2dDrawMethod, &QComboBox::currentIndexChanged,
            this, [this](int idx) {
        if (m_updating) return;
        m_drawMethod = idx;
        m_updating = true;                       // 等高線側の apply を空回りさせる
        m_near2dContour->setChecked(idx == 1);
        m_updating = false;
        apply();
    });

    connect(add1, &QPushButton::clicked, this, [this] {
        m_p->post().far1d.push_back(Far1d{});
        refresh(); m_p->touch();
    });
    connect(del1, &QPushButton::clicked, this, [this] {
        auto &v = m_p->post().far1d;
        const int r = m_far1d->currentRow();
        if (r >= 0 && r < v.size()) { v.removeAt(r); refresh(); m_p->touch(); }
    });
    connect(m_far1d, &QTableWidget::cellChanged, this, [this] {
        if (!m_updating) { applyFar1dTable(); m_p->touch(); }
    });

    connect(add3, &QPushButton::clicked, this, [this] {
        m_p->post().near1d.push_back(Near1d{});
        refresh(); m_p->touch();
    });
    connect(del3, &QPushButton::clicked, this, [this] {
        auto &v = m_p->post().near1d;
        const int r = m_near1d->currentRow();
        if (r >= 0 && r < v.size()) { v.removeAt(r); refresh(); m_p->touch(); }
    });
    connect(m_near1d, &QTableWidget::cellChanged, this, [this] {
        if (!m_updating) { applyNear1dTable(); m_p->touch(); }
    });

    connect(add4, &QPushButton::clicked, this, [this] {
        m_p->post().near2d.push_back(Near2d{});
        refresh(); m_p->touch();
    });
    connect(del4, &QPushButton::clicked, this, [this] {
        auto &v = m_p->post().near2d;
        const int r = m_near2d->currentRow();
        if (r >= 0 && r < v.size()) { v.removeAt(r); refresh(); m_p->touch(); }
    });
    connect(m_near2d, &QTableWidget::cellChanged, this, [this] {
        if (!m_updating) { applyNear2dTable(); m_p->touch(); }
    });

    // ドメイン切替 → 遠方界セクション等の表示切替と成分ヒントの文言切替
    connect(project, &Project::domainChanged, this,
            [this] { updateDomainVisibility(); });

    connect(project, &Project::loaded, this, &Post2Tab::refresh);
    // frequency2 の有無で前提が変わるので、モデル変更でも出し直す
    connect(project, &Project::changed, this, &Post2Tab::updatePrereq);
    refresh();
    updateDomainVisibility();
}

// ドメインに関係のない UI 項目を隠す (ドメイン監査の結果)。
// - 遠方界 far0d/far1d/far2d (Eθ/Eφ・LHCP/RHCP・dBi) は電磁界/光の概念で、
//   音響 (RIR 解析) / 水中音響 (BELLHOP レイトレース) には無い → 非表示。
// - 「入射波を除く (noinc)」は平面波波源が前提。音響/水中に平面波波源は
//   無い → 非表示。
// - 近傍界の「成分」列は全ドメインで有効。候補のヒントだけを
//   ドメイン別の文言 (音響/水中は音圧 p など) に切り替える。
// 表示のみの切替で、apply() は隠れていても従来どおり全値を書く
// (シリアライズ出力は不変)。
void Post2Tab::updateDomainVisibility()
{
    const Domain d = m_p->activeDomain();
    const bool ac = (d == Domain::Acoustic || d == Domain::Underwater);

    m_far0dSection->setVisible(!ac);
    m_far1dSection->setVisible(!ac);
    m_far2dSection->setVisible(!ac);
    m_near1dNoinc->setVisible(!ac);
    m_near2dNoinc->setVisible(!ac);

    const char *hint = ac ? "p2x_cmp_hint_ac" : "p2x_cmp_hint_em";
    m_near1dCmpHint->setText(I18n::tr(hint));
    m_near2dCmpHint->setText(I18n::tr(hint));
}

void Post2Tab::apply()
{
    if (m_updating) return;
    PostOpts &po = m_p->post();
    po.far0d = m_far0d->isChecked();
    po.far0dTheta = m_far0dTheta->text().toDouble();
    po.far0dPhi   = m_far0dPhi->text().toDouble();
    po.far1dStyle = m_far1dStyle->currentIndex();
    po.far1dDb = m_far1dDb->isChecked();
    po.far1dNorm = m_far1dNorm->isChecked();
    po.far1dComp[0] = m_far1dCompE->isChecked();
    po.far1dComp[1] = m_far1dCompTheta->isChecked();
    po.far1dComp[2] = m_far1dCompPhi->isChecked();
    po.far2d = m_far2d->isChecked();
    po.far2dDivTheta = m_far2dTheta->value();
    po.far2dDivPhi   = m_far2dPhi->value();
    po.far2dDb = m_far2dDb->isChecked();
    po.far2dObj = m_far2dObj->text().toDouble();
    for (int i = 0; i < 7; ++i)
        po.far2dComp[i] = m_far2dComp[i]->isChecked() ? 1 : 0;
    po.far2dUserScale = m_far2dUserScale->isChecked();
    po.far2dMin = m_far2dMin->text().toDouble();
    po.far2dMax = m_far2dMax->text().toDouble();
    po.near1dDb = m_near1dDb->isChecked();
    po.near1dNoinc = m_near1dNoinc->isChecked();
    po.near2dDim[0] = m_near2dDim0->value();
    po.near2dDim[1] = m_near2dDim1->value();
    po.near2dDb = m_near2dDb->isChecked();
    po.near2dContour = m_near2dContour->isChecked();
    po.near2dNoinc = m_near2dNoinc->isChecked();
    // near2dobj は 0/1/2。チェック時は読み込んだ値 (既定 1) をそのまま戻す。
    po.near2dObj = m_near2dDrawObj->isChecked() ? m_near2dObjValue : 0;
    po.near2dZoom = m_near2dZoom->isChecked();
    po.near2dFrame = m_near2dAnim->isChecked();
    syncDrawMethod();
    m_p->touch();
}

// 等高線チェックを直接触られたときも描画方法コンボを合わせる。
// 「ベクトル」は .ofd に無い選択なのでローカル状態から復元する。
void Post2Tab::syncDrawMethod()
{
    if (!m_near2dDrawMethod) return;
    const int idx = m_near2dContour->isChecked()
                        ? 1
                        : (m_drawMethod == 2 ? 2 : 0);
    const bool guard = m_updating;
    m_updating = true;                 // コンボの currentIndexChanged を空回りさせる
    m_near2dDrawMethod->setCurrentIndex(idx);
    m_updating = guard;
    m_drawMethod = idx;
}

// 前提条件の警告を出し直す (core/PostPrereq)
void Post2Tab::updatePrereq()
{
    if (!m_prereq) return;
    const QString w = tabhelp::postPrereqWarning(*m_p, 1);
    m_prereq->setText(w);
    m_prereq->setVisible(!w.isEmpty());
}

void Post2Tab::applyFar1dTable()
{
    auto &v = m_p->post().far1d;
    for (int r = 0; r < m_far1d->rowCount() && r < v.size(); ++r) {
        if (auto *cb = qobject_cast<QComboBox *>(m_far1d->cellWidget(r, 0)))
            v[r].dir = "XYZVH"[cb->currentIndex()];
        if (auto *it = m_far1d->item(r, 1)) v[r].div = it->text().toInt();
        if (auto *it = m_far1d->item(r, 2)) v[r].angle = it->text().toDouble();
    }
}

void Post2Tab::applyNear1dTable()
{
    auto &v = m_p->post().near1d;
    for (int r = 0; r < m_near1d->rowCount() && r < v.size(); ++r) {
        if (auto *it = m_near1d->item(r, 0)) v[r].cmp = it->text();
        if (auto *cb = qobject_cast<QComboBox *>(m_near1d->cellWidget(r, 1)))
            v[r].dir = "XYZ"[cb->currentIndex()];
        if (auto *it = m_near1d->item(r, 2)) v[r].pos1 = it->text().toDouble();
        if (auto *it = m_near1d->item(r, 3)) v[r].pos2 = it->text().toDouble();
    }
}

void Post2Tab::applyNear2dTable()
{
    auto &v = m_p->post().near2d;
    for (int r = 0; r < m_near2d->rowCount() && r < v.size(); ++r) {
        if (auto *it = m_near2d->item(r, 0)) v[r].cmp = it->text();
        if (auto *cb = qobject_cast<QComboBox *>(m_near2d->cellWidget(r, 1)))
            v[r].dir = "XYZ"[cb->currentIndex()];
        if (auto *it = m_near2d->item(r, 2)) v[r].pos = it->text().toDouble();
    }
}

void Post2Tab::refresh()
{
    m_updating = true;
    const PostOpts &po = m_p->post();

    m_far0d->setChecked(po.far0d);
    m_far0dTheta->setText(QString::number(po.far0dTheta, 'g', 6));
    m_far0dPhi->setText(QString::number(po.far0dPhi, 'g', 6));

    m_far1d->setRowCount(po.far1d.size());
    for (int r = 0; r < po.far1d.size(); ++r) {
        const Far1d &f = po.far1d[r];
        auto *dir = new QComboBox(m_far1d);
        // mock「面の向き」列: X面/Y面/Z面/φ一定面/θ一定面。
        // 並びは .ofd の X/Y/Z/V/H と 1:1 (V=φ一定, H=θ一定) なので
        // 保存値は下の "XYZVH" の添字経由で変わらない。
        dir->addItems({ I18n::tr("p2x_face_x"), I18n::tr("p2x_face_y"),
                        I18n::tr("p2x_face_z"), I18n::tr("p2x_face_phi"),
                        I18n::tr("p2x_face_theta") });
        const int di = QString("XYZVH").indexOf(f.dir);
        dir->setCurrentIndex(qMax(0, di));
        connect(dir, &QComboBox::currentIndexChanged, this, [this] {
            if (!m_updating) { applyFar1dTable(); m_p->touch(); }
        });
        m_far1d->setCellWidget(r, 0, dir);
        m_far1d->setItem(r, 1, new QTableWidgetItem(QString::number(f.div)));
        m_far1d->setItem(r, 2, new QTableWidgetItem(
            QString::number(f.angle, 'g', 6)));
    }
    m_far1dStyle->setCurrentIndex(qBound(0, po.far1dStyle, 2));
    m_far1dDb->setChecked(po.far1dDb);
    m_far1dNorm->setChecked(po.far1dNorm);
    m_far1dCompE->setChecked(po.far1dComp[0]);
    m_far1dCompTheta->setChecked(po.far1dComp[1]);
    m_far1dCompPhi->setChecked(po.far1dComp[2]);

    m_far2d->setChecked(po.far2d);
    m_far2dTheta->setValue(po.far2dDivTheta);
    m_far2dPhi->setValue(po.far2dDivPhi);
    m_far2dDb->setChecked(po.far2dDb);
    m_far2dObj->setText(QString::number(po.far2dObj, 'g', 6));
    for (int i = 0; i < 7; ++i)
        m_far2dComp[i]->setChecked(po.far2dComp[i] != 0);
    m_far2dUserScale->setChecked(po.far2dUserScale);
    m_far2dMin->setText(QString::number(po.far2dMin, 'g', 6));
    m_far2dMax->setText(QString::number(po.far2dMax, 'g', 6));

    m_near1d->setRowCount(po.near1d.size());
    for (int r = 0; r < po.near1d.size(); ++r) {
        const Near1d &n = po.near1d[r];
        m_near1d->setItem(r, 0, new QTableWidgetItem(n.cmp));
        auto *dir = new QComboBox(m_near1d);
        dir->addItems({ "X", "Y", "Z" });
        dir->setCurrentIndex(n.dir == 'X' ? 0 : n.dir == 'Y' ? 1 : 2);
        connect(dir, &QComboBox::currentIndexChanged, this, [this] {
            if (!m_updating) { applyNear1dTable(); m_p->touch(); }
        });
        m_near1d->setCellWidget(r, 1, dir);
        m_near1d->setItem(r, 2, new QTableWidgetItem(QString::number(n.pos1, 'g', 6)));
        m_near1d->setItem(r, 3, new QTableWidgetItem(QString::number(n.pos2, 'g', 6)));
    }
    m_near1dDb->setChecked(po.near1dDb);
    m_near1dNoinc->setChecked(po.near1dNoinc);

    m_near2d->setRowCount(po.near2d.size());
    for (int r = 0; r < po.near2d.size(); ++r) {
        const Near2d &n = po.near2d[r];
        m_near2d->setItem(r, 0, new QTableWidgetItem(n.cmp));
        auto *dir = new QComboBox(m_near2d);
        // mock の「面」列は X面/Y面/Z面 (near1d の「線の向き」は X/Y/Z のまま)。
        // 保存値は下の index → 'X'/'Y'/'Z' 変換なので表示名の変更で変わらない。
        dir->addItems({ I18n::tr("p2x_face_x"), I18n::tr("p2x_face_y"),
                        I18n::tr("p2x_face_z") });
        dir->setCurrentIndex(n.dir == 'X' ? 0 : n.dir == 'Y' ? 1 : 2);
        connect(dir, &QComboBox::currentIndexChanged, this, [this] {
            if (!m_updating) { applyNear2dTable(); m_p->touch(); }
        });
        m_near2d->setCellWidget(r, 1, dir);
        m_near2d->setItem(r, 2, new QTableWidgetItem(QString::number(n.pos, 'g', 6)));
    }
    m_near2dDim0->setValue(po.near2dDim[0]);
    m_near2dDim1->setValue(po.near2dDim[1]);
    m_near2dDb->setChecked(po.near2dDb);
    m_near2dContour->setChecked(po.near2dContour);
    m_near2dNoinc->setChecked(po.near2dNoinc);
    if (po.near2dObj > 0) m_near2dObjValue = po.near2dObj;
    m_near2dDrawObj->setChecked(po.near2dObj != 0);
    m_near2dZoom->setChecked(po.near2dZoom);
    m_near2dAnim->setChecked(po.near2dFrame);
    syncDrawMethod();

    m_updating = false;
    updatePrereq();
}
