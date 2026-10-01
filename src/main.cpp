#include "backend.h"
#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/Page>
#include <QTextCodec>
#include <QtDeclarative/QDeclarativeError>
#include <cstdio>

int main(int argc, char **argv) {
    std::fprintf(stderr, "BBIME: starting native test 0.1.0.15\n");
    bb::cascades::Application app(argc, argv);
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
    Backend backend;
    bb::cascades::QmlDocument *qml =
        bb::cascades::QmlDocument::create("asset:///main.qml").parent(&app);
    qml->setContextProperty("backend", &backend);
    bb::cascades::Page *page = qml->createRootObject<bb::cascades::Page>();
    if (!page) {
        foreach (QDeclarativeError error, qml->errors())
            std::fprintf(stderr, "BBIME QML: %s\n", error.toString().toUtf8().constData());
        return 2;
    }
    app.setScene(page);
    std::fprintf(stderr, "BBIME: scene installed\n");
    return app.exec();
}
