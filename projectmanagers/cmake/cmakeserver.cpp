/* KDevelop CMake Support
 *
 * Copyright 2017 Aleix Pol <aleixpol@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "cmakeserver.h"
#include "cmakeprojectdata.h"
#include "cmakeutils.h"

#include <interfaces/iruntime.h>
#include <interfaces/iruntimecontroller.h>
#include <interfaces/icore.h>

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QTemporaryFile>
#include "debug.h"

CMakeServer::CMakeServer(QObject* parent)
    : QObject(parent)
    , m_localSocket(new QLocalSocket(this))
{
    QString path;
    {
        const auto cacheLocation = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir::temp().mkpath(cacheLocation);

        QTemporaryFile file(cacheLocation + "/kdevelopcmake");
        file.open();
        file.close();
        path = file.fileName();
        Q_ASSERT(!path.isEmpty());
    }

    m_process.setProcessChannelMode(QProcess::ForwardedChannels);

#if QT_VERSION < 0x050600
    connect(&m_process, static_cast<void(QProcess::*)(QProcess::ProcessError)>(&QProcess::error),
#else
    connect(&m_process, &QProcess::errorOccurred,
#endif
            this, [this, path](QProcess::ProcessError error) {
        qCWarning(CMAKE) << "cmake server error:" << error << path << m_process.readAllStandardError() << m_process.readAllStandardOutput();
    });
    connect(&m_process, static_cast<void(QProcess::*)(int)>(&QProcess::finished), this, [](int code){
        qCDebug(CMAKE) << "cmake server finished with code" << code;
    });
    connect(&m_process, static_cast<void(QProcess::*)(int)>(&QProcess::finished), this, &CMakeServer::finished);

    connect(m_localSocket, &QIODevice::readyRead, this, &CMakeServer::processOutput);
    connect(m_localSocket, static_cast<void(QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error), this, [this, path](QLocalSocket::LocalSocketError socketError) {
        qCWarning(CMAKE) << "cmake server socket error:" << socketError << path;
        setConnected(false);
    });
    connect(m_localSocket, &QLocalSocket::connected, this, [this]() { setConnected(true); });

    connect(&m_process, &QProcess::started, this, [this, path](){
        //Once the process has started, wait for the file to be created, then connect to it
        QTimer::singleShot(1000, this, [this, path]() {
            m_localSocket->connectToServer(path, QIODevice::ReadWrite);
        });
    });
    m_process.setProgram(CMake::findExecutable());
    m_process.setArguments({"-E", "server", "--experimental", "--pipe=" + path});
    KDevelop::ICore::self()->runtimeController()->currentRuntime()->startProcess(&m_process);
}

CMakeServer::~CMakeServer()
{
    m_process.disconnect();
    m_process.kill();
    m_process.waitForFinished();
}

void CMakeServer::setConnected(bool conn)
{
    if (conn == m_connected)
        return;

    m_connected = conn;
    if (m_connected)
        Q_EMIT connected();
    else
        Q_EMIT disconnected();
}

bool CMakeServer::isServerAvailable()
{
    return m_localSocket->isOpen();
}

static QByteArray openTag() { return QByteArrayLiteral("\n[== \"CMake Server\" ==[\n"); }
static QByteArray closeTag() { return QByteArrayLiteral("\n]== \"CMake Server\" ==]\n"); }

void CMakeServer::sendCommand(const QJsonObject& object)
{
    Q_ASSERT(isServerAvailable());

    const QByteArray data = openTag() + QJsonDocument(object).toJson(QJsonDocument::Compact) + closeTag();
    auto len = m_localSocket->write(data);
//     qCDebug(CMAKE) << "writing...\n" << QJsonDocument(object).toJson();
    Q_ASSERT(len > 0);
}

void CMakeServer::processOutput()
{
    Q_ASSERT(m_localSocket);

    const auto openTag = ::openTag();
    const auto closeTag = ::closeTag();

    m_buffer += m_localSocket->readAll();
    for(; m_buffer.size() > openTag.size(); ) {

        Q_ASSERT(m_buffer.startsWith(openTag));
        const int idx = m_buffer.indexOf(closeTag, openTag.size());
        if (idx >= 0) {
            emitResponse(m_buffer.mid(openTag.size(), idx - openTag.size()));
            m_buffer = m_buffer.mid(idx + closeTag.size());
        } else {
            break;
        }
    }
}

void CMakeServer::emitResponse(const QByteArray& data)
{
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(data, &error);
    if (error.error) {
        qCWarning(CMAKE) << "error processing" << error.errorString() << data;
    }
    Q_ASSERT(doc.isObject());
    Q_EMIT response(doc.object());
}

void CMakeServer::handshake(const KDevelop::Path& source, const KDevelop::Path& build)
{
    Q_ASSERT(!source.isEmpty());

    const QString generatorVariable = QStringLiteral("CMAKE_GENERATOR");
    const QString homeDirectoryVariable = QStringLiteral("CMAKE_HOME_DIRECTORY");
    const auto cacheValues = CMake::readCacheValues(KDevelop::Path(build, QStringLiteral("CMakeCache.txt")),
                                                    {generatorVariable, homeDirectoryVariable});

    QString generator = cacheValues.value(generatorVariable);
    if (generator.isEmpty()) {
        generator = CMake::defaultGenerator();
    }

    // prefer pre-existing source directory, see also: https://gitlab.kitware.com/cmake/cmake/issues/16736
    QString sourceDirectory = cacheValues.value(homeDirectoryVariable);
    if (sourceDirectory.isEmpty()) {
        sourceDirectory = source.toLocalFile();
    } else if (QFileInfo(sourceDirectory).canonicalFilePath() != QFileInfo(source.toLocalFile()).canonicalFilePath()) {
        qCWarning(CMAKE) << "Build directory is configured for another source directory:"
                   << homeDirectoryVariable << sourceDirectory
                   << "wanted to open" << source << "in" << build;
    }

    qCDebug(CMAKE) << "Using generator" << generator << "for project" << source << "in" << build;

    sendCommand({
        {"cookie", {}},
        {"type", "handshake"},
        {"major", 1},
        {"protocolVersion", QJsonObject{{"major", 1}} },
        {"sourceDirectory", sourceDirectory},
        {"buildDirectory", build.toLocalFile()},
        {"generator", generator} //TODO: make it possible to keep whatever they have ATM
    });
}

void CMakeServer::configure(const QStringList& args)
{
    sendCommand({
        {"type", "configure"},
        {"cacheArguments", QJsonArray::fromStringList(args)}
    });
}

void CMakeServer::compute()
{
    sendCommand({ {"type", "compute"} });
}

void CMakeServer::codemodel()
{
    sendCommand({ {"type", "codemodel"} });
}
