#include <QFile>
#include <QRegularExpression>
#include <QTest>

class TestArchitecture : public QObject {
    Q_OBJECT

private slots:
    void qslRenderersStayOutOfCoreSourceBlock()
    {
        QFile file(QStringLiteral(CMAKE_SOURCE_DIR "/CMakeLists.txt"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString cmake = QString::fromUtf8(file.readAll());
        const qsizetype coreStart = cmake.indexOf(QStringLiteral("add_library(decolog_core STATIC"));
        const qsizetype coreEnd = cmake.indexOf(QStringLiteral("qt_add_resources(decolog_core"), coreStart);
        QVERIFY(coreStart >= 0);
        QVERIFY(coreEnd > coreStart);
        const QString coreBlock = cmake.mid(coreStart, coreEnd - coreStart);
        QVERIFY(!coreBlock.contains(QStringLiteral("src/core/QslCards.cpp")));
        QVERIFY(!coreBlock.contains(QStringLiteral("src/core/QslDesign.cpp")));

        const qsizetype appStart = cmake.indexOf(QStringLiteral("qt_add_executable(decolog"));
        QVERIFY(appStart >= 0);
        const QString appBlock = cmake.mid(appStart, cmake.indexOf(QStringLiteral("set(DECODXLOG_QML_FILES"), appStart) - appStart);
        QVERIFY(appBlock.contains(QStringLiteral("src/core/QslCards.cpp")));
        QVERIFY(appBlock.contains(QStringLiteral("src/core/QslDesign.cpp")));
    }
};

QTEST_GUILESS_MAIN(TestArchitecture)
#include "tst_architecture.moc"
