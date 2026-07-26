// TransmissionLineTab.h — 伝送線路特性タブ (openfdtd-family.jsx TransmissionLineTab 相当)。
// OpenFDTD ドキュメント §2.14「伝送線路」: マイクロストリップ・同軸・導波管の
//   特性インピーダンス Z₀ / 伝搬定数 γ = α + jβ / Sパラメータ / 不連続部の設定。
// Static prototype: all toggles are local state (no matching Project field).
#pragma once
#include <QScrollArea>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLineEdit;

namespace ofd {

class Project;
class SectionBox;

class TransmissionLineTab : public QScrollArea {
    Q_OBJECT
public:
    explicit TransmissionLineTab(Project *project, QWidget *parent = nullptr);

private:
    // モックの <Row><Check/></Row> 群を1セクションにまとめて生成
    SectionBox *checkSection(QWidget *parent, const char *titleKey,
                             const char *const *keys, const bool *checked, int n,
                             QVector<QCheckBox *> *out);

    Project *m_p;

    QComboBox *m_z0Method;              // V/I 法 / 電力定義 / 静電容量法
    QCheckBox *m_z0FreqDep, *m_z0ReIm;
    QVector<QCheckBox *> m_gamma;       // 伝搬定数 γ
    QLineEdit *m_ports;                 // ポート数
    QVector<QCheckBox *> m_spara;       // Sパラメータ
    QVector<QCheckBox *> m_disc;        // 不連続部・整合
};

} // namespace ofd
