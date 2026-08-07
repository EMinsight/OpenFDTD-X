// EvCanvas.cpp
#include "EvCanvas.h"
#include "../I18n.h"

#include <QFileInfo>
#include <QPainter>

using namespace ofd;

namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("evc_empty",
              "作図出力 (ev.ev2) がまだありません。"
              "「一括 (計算+ポスト)」または「ポスト処理」を実行してください。",
              "No figure output (ev.ev2) yet — run \"Solver + post\" or "
              "\"Post\".");
    return true;
}();
} // namespace

EvCanvas::EvCanvas(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(240);
    // 図は白地に描かれる前提 (カーネルは黒を既定色にする)
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::white);
    setPalette(pal);
    m_placeholder = I18n::tr("evc_empty");
}

bool EvCanvas::load(const QString &path, QString *err)
{
    QString e;
    EvDocument doc;
    if (!EvReader::load(path, doc, &e)) {
        m_doc.pages.clear();
        m_path.clear();
        m_page = 0;
        m_placeholder = e;
        if (err) *err = e;
        update();
        emit pageChanged(0, 0);
        return false;
    }
    m_doc = doc;
    m_path = path;
    m_page = 0;
    update();
    emit pageChanged(m_page, m_doc.pages.size());
    return true;
}

void EvCanvas::clear()
{
    m_doc.pages.clear();
    m_path.clear();
    m_page = 0;
    m_placeholder = I18n::tr("evc_empty");
    update();
    emit pageChanged(0, 0);
}

void EvCanvas::setPage(int i)
{
    if (m_doc.pages.isEmpty()) return;
    const int n = qBound(0, i, m_doc.pages.size() - 1);
    if (n == m_page) return;
    m_page = n;
    update();
    emit pageChanged(m_page, m_doc.pages.size());
}

void EvCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (m_doc.pages.isEmpty()) {
        p.setPen(QColor(0x88, 0x88, 0x88));
        p.drawText(rect().adjusted(12, 12, -12, -12),
                   Qt::AlignCenter | Qt::TextWordWrap, m_placeholder);
        return;
    }
    EvReader::render(p, QRectF(rect()).adjusted(6, 6, -6, -6),
                     m_doc.pages[m_page]);
}
