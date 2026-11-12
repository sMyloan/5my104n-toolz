#include "FolderScanner.h"
#include <QDir>

FolderScanner::FolderScanner(const QString& folder, QObject* parent)
: QThread(parent), m_folder(folder) {}

void FolderScanner::run()
{
    QDir dir(m_folder);
    if (!dir.exists()) {
        emit errorOccurred("Not a directory: " + m_folder);
        return;
    }
    QStringList paths;
    //поиск только файлов с расширением smy сортированных по имени
    const auto names = dir.entryList(QStringList() << "*.smy",
                                     QDir::Files | QDir::Readable,
                                     QDir::Name);
    for (const QString& name : names)
        paths << dir.absoluteFilePath(name);
    emit finishedScan(paths);
}
