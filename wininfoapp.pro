# wininfoapp.pro

QT += widgets network core  # Include core, network, widgets modules

CONFIG += c++17 windows  # Enable C++17 and Windows-specific configurations

SOURCES += wininfoapp.cpp \
           src/main.cpp

HEADERS += include/myheader.h

FORMS += mainwindow.ui

RESOURCES += # Add your .qrc file here if you use Qt resources

# Application icon (Windows)
RC_FILE = appicon.rc

CONFIG += windows


# Uncomment and configure SQLite if needed
# DEFINES += USE_SQLITE
# win32:LIBS += -lsqlite3
# INCLUDEPATH += $$PWD/libs
# win32:LIBS += -L$$PWD/libs -lsqlite3

# Additional include paths
INCLUDEPATH += include

# Output directory
DESTDIR = build

# Target name
TARGET = wininfoapp

RC_FILE = [appicon.rc](http://_vscodecontentref_/1)