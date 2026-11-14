#pragma once

#include <QObject>
#include <QUrl>

// The open and save dialogs, behind an interface so tests can inject a fake
// picker instead of talking to the desktop portal. Follows Omacut (MIT, David
// Heinemeier Hansson).
class FilePicker : public QObject {
    Q_OBJECT

public:
    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~FilePicker() = default;

    // folder is where the dialog starts; name is the suggested file name.
    virtual void openFile(const QString &folder) = 0;
    virtual void saveFile(const QString &folder, const QString &name) = 0;
    // A save dialog for a copy; the note itself stays where it is.
    virtual void exportFile(const QString &folder, const QString &name) = 0;

signals:
    void openSelected(const QUrl &url);
    void saveSelected(const QUrl &url);
    void exportSelected(const QUrl &url);
    void failed(const QString &message);
};
