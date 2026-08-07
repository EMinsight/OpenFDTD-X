// EvReader.cpp
#include "EvReader.h"

#include <QFile>
#include <QFont>
#include <QPainter>
#include <QPolygonF>
#include <QStringList>
#include <QTextStream>

#include <cmath>

namespace ofd {
namespace EvReader {

namespace {

// "%g" で書かれた数値列。要素数が足りない行は不正としてページに入れない。
bool takeDoubles(const QStringList &tok, int first, int n, QVector<double> &out)
{
    if (tok.size() < first + n) return false;
    out.clear();
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        bool ok = false;
        const double v = tok[first + i].toDouble(&ok);
        if (!ok) return false;
        out.push_back(v);
    }
    return true;
}

void pushPts(EvCommand &c, const QVector<double> &v)
{
    for (int i = 0; i + 1 < v.size(); i += 2)
        c.pts.push_back(QPointF(v[i], v[i + 1]));
}

} // namespace

bool parse(const QString &text, EvDocument &doc, QString *err)
{
    doc.pages.clear();
    const QStringList lines = text.split(QLatin1Char('\n'));
    QColor color = Qt::black;
    QVector<double> v;

    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (line.isEmpty()) continue;
        const QStringList tok = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tok.isEmpty()) continue;
        bool ok = false;
        const int idx = tok[0].toInt(&ok);
        if (!ok) continue;   // 読めない行は飛ばす (前方互換)

        if (idx == -1) {                       // 新規ページ
            if (!takeDoubles(tok, 1, 2, v)) continue;
            EvPage page;
            page.width = v[0];
            page.height = v[1];
            doc.pages.push_back(page);
            color = Qt::black;                 // 書き出し側もページ頭で黒に戻す
            continue;
        }
        if (doc.pages.isEmpty()) continue;     // ページ外のコマンドは捨てる
        EvPage &page = doc.pages.last();

        if (idx == -2) {                       // 色
            if (!takeDoubles(tok, 1, 3, v)) continue;
            color = QColor(int(v[0]), int(v[1]), int(v[2]));
            continue;
        }
        if (idx == -3) {                       // 文字列 — 本文は次の行
            if (!takeDoubles(tok, 1, 3, v)) continue;
            if (i + 1 >= lines.size()) continue;
            EvCommand c;
            c.kind = EvCommand::Text;
            c.color = color;
            c.pts.push_back(QPointF(v[0], v[1]));
            c.height = v[2];
            // 本文は空白も含めてそのまま (trimmed しない — 字下げが意味を持つ)
            c.text = lines[++i];
            if (c.text.endsWith(QLatin1Char('\r'))) c.text.chop(1);
            page.commands.push_back(c);
            continue;
        }

        EvCommand c;
        c.color = color;
        int nPts = 0;
        switch (idx) {
            case 2:  c.kind = EvCommand::Line;         nPts = 2; break;
            case 3:  c.kind = EvCommand::FillTriangle; nPts = 3; break;
            case 4:  c.kind = EvCommand::FillQuad;     nPts = 4; break;
            case 21: c.kind = EvCommand::Ellipse;      nPts = 2; break;
            case 22: c.kind = EvCommand::FillEllipse;  nPts = 2; break;
            default: continue;                 // 未知の idx は飛ばす
        }
        if (!takeDoubles(tok, 1, 2 * nPts, v)) continue;
        pushPts(c, v);
        page.commands.push_back(c);
    }

    // 図形が 1 つも無いページしか無いなら「読めなかった」とみなす
    int total = 0;
    for (const EvPage &p : doc.pages) total += p.commands.size();
    if (total == 0) {
        if (err) *err = QStringLiteral("no drawing commands found");
        doc.pages.clear();
        return false;
    }
    return true;
}

bool load(const QString &path, EvDocument &doc, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("cannot open %1: %2")
                            .arg(path, f.errorString());
        doc.pages.clear();
        return false;
    }
    return parse(QString::fromUtf8(f.readAll()), doc, err);
}

void render(QPainter &p, const QRectF &rect, const EvPage &page)
{
    if (!(page.width > 0.0) || !(page.height > 0.0)) return;
    if (!(rect.width() > 0.0) || !(rect.height() > 0.0)) return;

    // 縦横比を保って rect へ最大化し、中央へ寄せる
    const double s = std::min(rect.width() / page.width,
                              rect.height() / page.height);
    const double ox = rect.left() + 0.5 * (rect.width() - page.width * s);
    const double oy = rect.top() + 0.5 * (rect.height() - page.height * s);
    // ev の原点は左下 — y を反転する (post/ev2d.c の HTML 出力と同じ)
    const auto map = [&](const QPointF &q) {
        return QPointF(ox + q.x() * s, oy + (page.height - q.y()) * s);
    };

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    for (const EvCommand &c : page.commands) {
        switch (c.kind) {
        case EvCommand::Line:
            if (c.pts.size() < 2) break;
            p.setPen(QPen(c.color, 1.0));
            p.drawLine(map(c.pts[0]), map(c.pts[1]));
            break;
        case EvCommand::FillTriangle:
        case EvCommand::FillQuad: {
            QPolygonF poly;
            for (const QPointF &q : c.pts) poly << map(q);
            // 書き出し側は塗りと同色で縁もなぞる (HTML の fill+stroke)
            p.setPen(QPen(c.color, 1.0));
            p.setBrush(c.color);
            p.drawPolygon(poly);
            p.setBrush(Qt::NoBrush);
            break;
        }
        case EvCommand::Ellipse:
        case EvCommand::FillEllipse: {
            if (c.pts.size() < 2) break;
            // 2 点は外接矩形の対角。反転後も矩形として正しくなるよう正規化する
            const QRectF r =
                QRectF(map(c.pts[0]), map(c.pts[1])).normalized();
            p.setPen(QPen(c.color, 1.0));
            p.setBrush(c.kind == EvCommand::FillEllipse ? QBrush(c.color)
                                                        : Qt::NoBrush);
            p.drawEllipse(r);
            p.setBrush(Qt::NoBrush);
            break;
        }
        case EvCommand::Text: {
            if (c.pts.isEmpty()) break;
            const double px = c.height * s;
            if (!(px > 0.5)) break;    // 潰れる文字は描かない
            QFont f = p.font();
            f.setFamily(QStringLiteral("monospace"));   // 書き出し側と同じ
            // setPixelSize が pointSize を打ち消す (先に -1 を入れると Qt が警告)
            f.setPixelSize(qMax(1, int(std::lround(px))));
            p.setFont(f);
            p.setPen(QPen(c.color, 1.0));
            // HTML 側は fillText(str, x, Height - y) — ベースライン基準
            p.drawText(map(c.pts[0]), c.text);
            break;
        }
        }
    }
    p.restore();
}

} // namespace EvReader
} // namespace ofd
