#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
    // The rest of the Omarchy palette. Each falls back to Yeptes's own
    // neutral greys, so the app looks the same without Omarchy.
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QColor base READ base NOTIFY changed)              // menus and dialogs
    Q_PROPERTY(QColor chrome READ chrome NOTIFY changed)          // status bar and notes panel
    Q_PROPERTY(QColor scrim READ scrim NOTIFY changed)            // behind overlays
    Q_PROPERTY(QColor raised READ raised NOTIFY changed)          // hovered buttons and rows
    Q_PROPERTY(QColor line READ line NOTIFY changed)              // borders and dividers
    Q_PROPERTY(QColor selection READ selection NOTIFY changed)
    Q_PROPERTY(QColor text READ text NOTIFY changed)              // titles and values
    Q_PROPERTY(QColor subtext READ subtext NOTIFY changed)        // body text
    Q_PROPERTY(QColor dim READ dim NOTIFY changed)                // labels and hints
    Q_PROPERTY(QColor faint READ faint NOTIFY changed)            // line numbers, punctuation
    Q_PROPERTY(QColor editorBackground READ editorBackground NOTIFY changed)
    Q_PROPERTY(QColor editorText READ editorText NOTIFY changed)
    Q_PROPERTY(QColor red READ red NOTIFY changed)
    Q_PROPERTY(QColor green READ green NOTIFY changed)
    Q_PROPERTY(QColor orange READ orange NOTIFY changed)
    Q_PROPERTY(QColor magenta READ magenta NOTIFY changed)
    Q_PROPERTY(QColor cyan READ cyan NOTIFY changed)
    Q_PROPERTY(QColor brightRed READ brightRed NOTIFY changed)
    Q_PROPERTY(QColor brightGreen READ brightGreen NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    int radius() const { return m_radius; }

    const QHash<QString, QString> &colors() const { return m_colors; }
    bool dark() const { return m_colors.value("mode", "dark") != "light"; }
    QColor base() const { return pick("background", "#17171b"); }
    QColor chrome() const { return pick("dark_background", "#141417"); }
    QColor scrim() const { return pick("darker_background", "#0e0e10"); }
    QColor raised() const { return pick("lighter_background", "#26262d"); }
    QColor line() const { return pick("muted", "#2a2a32"); }
    QColor selection() const { return pick("selection", "#2a2a32"); }
    QColor text() const { return pick("bright_foreground", "#e6e6ea"); }
    QColor subtext() const { return pick("foreground", "#c9c9d1"); }
    QColor dim() const;
    QColor faint() const { return pick("dark_foreground", "#5f5f6a"); }
    QColor editorBackground() const { return pick("background", "#1a1b26"); }
    QColor editorText() const { return pick("foreground", "#c0caf5"); }
    QColor red() const { return pick("red", "#f7768e"); }
    QColor green() const { return pick("green", "#9ece6a"); }
    QColor orange() const { return pick("orange", "#ff9e64"); }
    QColor magenta() const { return pick("bright_magenta", "#bb9af7"); }
    QColor cyan() const { return pick("cyan", "#7dcfff"); }
    QColor brightRed() const { return pick("bright_red", "#ff9aa9"); }
    QColor brightGreen() const { return pick("bright_green", "#b9f27c"); }

    // Every `name = "#rrggbb"` (and `mode`) in an Omarchy colors.toml.
    static QHash<QString, QString> readColors(const QString &path);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();
    QColor pick(const QString &key, const char *fallback) const;

    QString m_directory;
    QString m_accent = "#FFD60A";
    QHash<QString, QString> m_colors;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
