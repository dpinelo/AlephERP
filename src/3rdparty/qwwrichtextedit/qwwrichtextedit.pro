message( "---------------------------------" )
message( "CONSTRUYENDO QWWRICHTEXTEDIT..." )
message( "---------------------------------" )

include (../../../config.pri)

win32 {
    DEFINES += WW_BUILD_WWWIDGETS WINDOWS
}

contains(QT_VERSION, ^4\\.[0-8]\\..*) {
    QT += gui
}
!contains(QT_VERSION, ^4\\.[0-8]\\..*) {
    QT += widgets
}

TARGET = qwwrichtextedit
TEMPLATE = lib

INCLUDEPATH += .

HEADERS = qwwrichtextedit.h \
          qwwcolorbutton.h \
          wwglobal.h \
          wwglobal_p.h \
          colormodel.h

SOURCES = qwwrichtextedit.cpp \
          qwwcolorbutton.cpp \
          wwglobal_p.cpp \
          colormodel.cpp

RESOURCES += wwresources.qrc

# Instalamos los archivos include
includesTarget.path = $$BUILDPATH/include
includesTarget.files = $$HEADERS
#INSTALLS += includesTarget
