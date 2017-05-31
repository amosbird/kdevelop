/* KDevelop CMake Support
 *
 * Copyright 2017 Aleix Pol Gonzalez <aleixpol@kde.org>
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

#include <cmakeserver.h>
#include <tests/autotestshell.h>
#include <tests/testproject.h>
#include <tests/testcore.h>
#include "testhelpers.h"

#include <QTest>
#include <QJsonObject>
#include <QLoggingCategory>

using namespace KDevelop;

class CMakeServerTest : public QObject
{
    Q_OBJECT
public:
    CMakeServerTest()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false\ndefault.debug=true\nkdevelop.projectmanagers.cmake.debug=true\n"));

        AutoTestShell::init();
        TestCore::initialize();
    }

    bool testAvailable = true;
private slots:
    void disableTest(int code)
    {
        testAvailable = code!=0;
    }

    void testRun()
    {
        CMakeServer server(this);
        connect(&server, &CMakeServer::finished, this, &CMakeServerTest::disableTest);
        QSignalSpy spyConnected(&server, &CMakeServer::connected);
        QVERIFY(server.isServerAvailable() || spyConnected.wait() || !testAvailable);
        if (!testAvailable) {
            QSKIP("need a newer cmake to test the cmakeserver");
            return;
        }

        QSignalSpy spy(&server, &CMakeServer::response);

        QJsonObject codeModel;
        int errors = 0;
        connect(&server, &CMakeServer::response, this, [&errors, &codeModel, &server](const QJsonObject &response) {
            if (response.value(QStringLiteral("type")) == QLatin1String("reply")) {
                if (response.value(QStringLiteral("inReplyTo")) == QLatin1String("configure"))
                    server.compute();
                else if (response.value(QStringLiteral("inReplyTo")) == QLatin1String("compute"))
                    server.codemodel();
                else if(response.value(QStringLiteral("inReplyTo")) == QLatin1String("codemodel"))
                    codeModel = response;
            } else if(response.value(QStringLiteral("type")) == QLatin1String("error")) {
                ++errors;
            }
        });

        const QString name = QStringLiteral("single_subdirectory");
        const auto paths = projectPaths(name);
        const QString builddir = QStringLiteral(CMAKE_TESTS_BINARY_DIR "/cmake-server-test-builddir/") + name;
        QVERIFY(QDir(builddir).removeRecursively());
        QVERIFY(QDir(builddir).mkpath(builddir));

        QVERIFY(spy.wait());
        server.handshake(paths.sourceDir, Path(builddir));
        QVERIFY(spy.wait());
        server.configure({});
        while(codeModel.isEmpty())
            QVERIFY(spy.wait());
        QCOMPARE(errors, 0);
        QVERIFY(!codeModel.isEmpty());
        qDebug() << "codemodel" << codeModel;
    }
};

QTEST_MAIN( CMakeServerTest )

#include "test_cmakeserver.moc"
