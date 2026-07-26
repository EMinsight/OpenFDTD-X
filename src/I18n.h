// I18n.h — minimal in-memory translation table (ja / en / both).
//
// Matches the i18n.js used in the HTML mock. Use I18n::tr("g_title") to look
// up a key; unknown keys return themselves so missing entries stay visible.
#pragma once
#include <QString>
#include <QHash>

namespace ofd {

class I18n {
public:
    static I18n &instance();
    void    setLanguage(const QString &lang);   // "ja" | "en" | "both"
    QString lang() const { return m_lang; }

    static QString tr(const QString &key);

    // Register a key from outside I18n.cpp. Tabs that own a large private
    // vocabulary (the Ansys/Lumerical-style panels) call this from a file-local
    // static initialiser instead of growing loadTables() into a merge magnet.
    // An existing key is left alone, so loadTables() always wins.
    static void reg(const char *key, const char *ja, const char *en);

private:
    I18n();
    void loadTables();

    QString m_lang = "ja";
    QHash<QString, QString> m_ja;
    QHash<QString, QString> m_en;
};

} // namespace ofd
