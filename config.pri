#-----------------------------------------------------------------------------------------------------
# Esta es la sección que el usuario debe editar
#-----------------------------------------------------------------------------------------------------
ALEPHERPPATH=$$PWD
APPNAME=alepherp

# Paths importantes
win32 {
    win32-g++ {
        #FIREBIRDPATH=/usr
        #ADDITIONALLIBS=/home/david/src/alepherp/build.win32
        #FIREBIRDPATH=C:/Users/David/programacion/Firebird-2.5.2.26540
        ADDITIONALLIBS=C:/Users/david/src/alepherp/libraries/win32/gcc
        #FIREBIRDPATH=C:/Users/David/programacion/Firebird-2.5.2.26540
        #ADDITIONALLIBS=C:/Users/david/src/alepherp/libraries/win32/gcc
    }
    win64-g++ {
        FIREBIRDPATH=/usr
        ADDITIONALLIBS=/home/david/src/alepherp/build.win64
    }
    win32-msvc* {
        #FIREBIRDPATH=C:/Users/David/programacion/Firebird-2.5.2.26540
        #ADDITIONALLIBS=C:/Users/David/programacion/libraries/win32/msvc
        FIREBIRDPATH=C:/Users/David/src/Firebird-2.5.2.26540
        ADDITIONALLIBS=C:/Users/David/src/alepherp/libraries/win32/msvc
        INCLUDEPATH += $$PWD/projects/msvc/src/3rdparty/zlib-1.2.8 \
                       $$PWD/projects/msvc/src/3rdparty/libpng-1.6.15 \
                       $$PWD/projects/msvc/src/3rdparty/jpeg-9a
    }
    INCLUDEPATH += $$ADDITIONALLIBS/include
    DEFINES += ALEPHERP_BUILD_DLL
}

# Módulos
ADVANCEDEDIT=Y
USEQSCINTILLA=Y
OPENRPTSUPPORT=Y
DEVTOOLS=Y
SMTPBUILTIN=N
PRINTINGERPSUPPORT=N
CLOUDSUPPORT=N
LOCALMODE=N
FIREBIRDSUPPORT=N
DOCMNGSUPPORT=N
USELIBMAGIC=N
STANDALONE=N
USEWEBSOCKETS=N
FORCETOUSECLOUD=N
WIASUPPORT=N
JASPERSERVERSUPPORT=N
BARCODESUPPORT=N
QTDESIGNERBUILTIN=N
QTSCRIPTBINDING=N
TESTPARTS=N
# Puede ser, xbase64, xbase2 o dbase-code
DBASELIBRARY=dbase-code
LINUX32BITS=N
XLSSUPPORT=N

VMIMESMTPSUPPORT=N
POCOSMTPSUPPORT=N
QXTSMTPSUPPORT=N
CSSMTPSUPPORT=N

CONFIG += debug_and_release

#-----------------------------------------------------------------------------------------------------
# Hasta aquí
#-----------------------------------------------------------------------------------------------------

SMTPSUPPORT=N

CONFIG += c++11

android {
    BUILDTYPE=android
}

macx {
    BUILDTYPE=macos
}

win32-g++ {
    BUILDTYPE=win32/gcc
}
win64-g++ {
    BUILDTYPE=win64/gcc
}
win32-msvc* {
    BUILDTYPE=win32/msvc
}

unix {
    contains (LINUX32BITS, Y) {
        BUILDTYPE=unix/m32
        QMAKE_LFLAGS = -m32
        QMAKE_CXXFLAGS = -m32
    }
    !contains (LINUX32BITS, Y) {
        BUILDTYPE=unix/m64
        QMAKE_LFLAGS = -m64
        QMAKE_CXXFLAGS = -m64
    }
}

macx {
    CONFIG-=app_bundle
}

BUILDPATH=$$ALEPHERPPATH/build/$$QT_VERSION/$$BUILDTYPE
message( "CONFIG: Construyendo en:"  $$BUILDPATH " para arquitectura " $$QMAKE_HOST.arch )

# Para Qt 4.8.3
#CONFIG += rtti

#Compilar una versión estática de libpq
# http://pgolub.wordpress.com/2008/12/15/building-postgresql-client-library-using-mingw-under-winxp-sp3/
# Para compilar las Qt sin depender de los archivos de MingW solo hay que añadir, en
# qt\4.8.4\mkspecs\win32-g++\qmake.conf
# QMAKE_LFLAGS = -static-libgcc -static-libstdc++
# https://blog.qt.digia.com/blog/2012/05/08/qt-commercial-support-weekly-19-how-to-write-your-own-static-library-with-qt-2/

CONFIG(release, debug|release) {
    #MOC_DIR = $$ALEPHERPPATH/build/tmp/$$QT_VERSION/$$BUILDTYPE/$$APPNAME/release/moc
    #OBJECTS_DIR = $$ALEPHERPPATH/build/tmp/$$QT_VERSION/$$BUILDTYPE/$$APPNAME/release/objetos
    #UI_DIR = $$ALEPHERPPATH/build/tmp/$$QT_VERSION/$$BUILDTYPE/$$APPNAME/release/ui
    #RCC_DIR = $$ALEPHERPPATH/build/tmp/$$QT_VERSION/$$BUILDTYPE/$$APPNAME/release/rcc
    TESTPARTS=N
    DESTDIR = $$BUILDPATH/release
    LIBS += -L$$BUILDPATH/release \
            -L$$BUILDPATH/release/plugins/designer
    unix {
        !macx {
            contains(QMAKE_HOST.arch,x86_64) {
                LIBS += -L/usr/lib/x86_64-linux-gnu
            }
        }
    }
    DEFINES += ALEPHERP_RELEASE
}

CONFIG(debug, debug|release) {
    #CONFIG += console
    #MOC_DIR = $$ALEPHERPPATH/build/tmp/$$QT_VERSION/$$BUILDTYPE/$$APPNAME/debug/moc
    #OBJECTS_DIR = $$ALEPHERPPATH/build/tmp/$$QT_VERSION/$$BUILDTYPE/$$APPNAME/debug/objetos
    #UI_DIR = $$ALEPHERPPATH/build/tmp/$$QT_VERSION/$$BUILDTYPE/$$APPNAME/debug/ui
    #RCC_DIR = $$ALEPHERPPATH/build/tmp/$$QT_VERSION/$$BUILDTYPE/$$APPNAME/debug/rcc
    DESTDIR = $$BUILDPATH/debug
    LIBS += -L$$BUILDPATH/debug \
            -L$$BUILDPATH/debug/plugins/designer
    unix {
        !macx {
            contains(QMAKE_HOST.arch,x86_64) {
                LIBS += -L/usr/lib/x86_64-linux-gnu
            }
        }
    }
    DEFINES += ALEPHERP_DEBUG
}

win32 {
    # Con esto evitamos usar las librerias libstdc++-6.dll and libgcc_s_sjlj-1.dll de minggw
    #QMAKE_LFLAGS = -static-libgcc -static-libstdc++
    #LFLAGS = -static-libgcc -static-libstdc++
    # Esta opción se incluye para el profiling con Very Sleepy. Pero no funciona con GDB.
    # QMAKE_CXXFLAGS_DEBUG = -fno-omit-frame-pointer
}

win32-msvc* {
    CONFIG(release, debug|release) {
        INCLUDEPATH += $$ALEPHERPPATH/build/tmp/$$QT_VERSION/$$BUILDTYPE/$$APPNAME/release/moc
    }
    CONFIG(debug, debug|release) {
        INCLUDEPATH += $$ALEPHERPPATH/build/tmp/$$QT_VERSION/$$BUILDTYPE/$$APPNAME/debug/moc
    }
}

unix {
    INCLUDEPATH += -I/usr/include/qt$$QT_MAJOR_VERSION -I/usr/include/qt5
    LIBS += -L/usr/lib -L$$BUILDPATH/plugins/designer
}

contains (QTDESIGNERBUILTIN, Y) {
    DEFINES += ALEPHERP_QTDESIGNERBUILTIN
}

contains (TESTPARTS, Y) {
    DEFINES += ALEPHERP_TEST
}

contains (DBASELIBRARY, xbase64) {
    DEFINES += ALEPHERP_XBASE64
}

contains (DBASELIBRARY, xbase2) {
    DEFINES += ALEPHERP_XBASE2
}

contains (DBASELIBRARY, dbase-code) {
    DEFINES += ALEPHERP_DBASE_CODE
}

contains (SMTPBUILTIN, Y) {
    message("CONFIG: Configurando sistema con soporte para SMTP...")
    SMTPSUPPORT=Y
    DEFINES += ALEPHERP_SMTP_SUPPORT ALEPHERP_BUILT_IN_SMTP_SUPPORT
}

contains (VMIMESMTPSUPPORT, Y) {
    message( "CONFIG: Configurando sistema con soporte para SMTP..." )
    SMTPSUPPORT=Y
}

contains (POCOSMTPSUPPORT, Y) {
    message( "CONFIG: Configurando sistema con soporte para SMTP..." )
    SMTPSUPPORT=Y
}

contains (QXTSMTPSUPPORT, Y) {
    message( "CONFIG: Configurando sistema con soporte para SMTP..." )
    SMTPSUPPORT=Y
}

contains (CSSMTPSUPPORT, Y) {
    message( "CONFIG: Configurando sistema con soporte para SMTP..." )
    SMTPSUPPORT=Y
}

android|ios {
    android {
        message( "CONFIG: Configurando sistema para Android..." )
        DEFINES += ALEPHERP_ANDROID
    }
    ios {
        message( "CONFIG: Configurando sistema para iOS..." )
        DEFINES += ALEPHERP_IOS
    }
    AERPSMTPSUPPORT=N
    AERPFIREBIRDSUPPORT=N
    AERPLOCALMODE=N
    AERPDOCMNGSUPPORT=N
    AERPADVANCEDEDIT=N
    USEQSCINTILLA=N
    VMIMESMTPSUPPORT=N
    PRINTINGERPSUPPORT=N
} else {
    AERPSMTPSUPPORT=$$SMTPSUPPORT
    AERPFIREBIRDSUPPORT=$$FIREBIRDSUPPORT
    AERPLOCALMODE=$$LOCALMODE
    AERPDOCMNGSUPPORT=$$DOCMNGSUPPORT
    AERPADVANCEDEDIT=$$ADVANCEDEDIT
}

contains(AERPLOCALMODE, Y) {
    contains(FIREBIRDSUPPORT, Y) {
        DEFINES += ALEPHERP_LOCALMODE
    }
}

contains (AERPADVANCEDEDIT, Y) {
    DEFINES += ALEPHERP_ADVANCED_EDIT
}

contains (AERPDOCMNGSUPPORT, Y) {
    DEFINES += ALEPHERP_DOC_MANAGEMENT
}

contains (AERPSMTPSUPPORT, Y) {
    DEFINES += ALEPHERP_SMTP_SUPPORT
}

contains (AERPFIREBIRDSUPPORT, Y) {
    DEFINES += ALEPHERP_FIREBIRD_SUPPORT
}

contains(USEQSCINTILLA, Y) {
    DEFINES += ALEPHERP_QSCISCINTILLA
}

contains (CLOUDSUPPORT, Y) {
    DEFINES += ALEPHERP_CLOUD_SUPPORT
}

contains(FORCETOUSECLOUD, Y) {
    DEFINES += ALEPHERP_FORCE_TO_USE_CLOUD
    STANDALONE = N
}

contains(STANDALONE, Y) {
    DEFINES += ALEPHERP_STANDALONE
}

contains(DEVTOOLS, Y) {
    DEFINES += ALEPHERP_DEVTOOLS
}

contains(BARCODESUPPORT, Y) {
    DEFINES += ALEPHERP_BARCODE
}

# --------------------------------------
# A partir de aquí includes y otras cosas necesarias para compilar...
# --------------------------------------
# Algunas definiciones comunes a todo el sistema
LIB_DAOBUSINESS = $$ALEPHERPPATH/src/lib/daobusiness

INCLUDEPATH += $$ALEPHERPPATH/src \
               $$BUILDPATH/include \
               $$ADDITIONALLIBS/include \
               $$ALEPHERPPATH/src/3rdparty
LIBS += -L$$BUILDPATH/lib -L$$ADDITIONALLIBS/lib

contains (DEVTOOLS, Y) {
    INCLUDEPATH += $$ALEPHERPPATH/src/3rdparty/diff
}

contains (AERPADVANCEDEDIT, Y) {
    INCLUDEPATH += $$ALEPHERPPATH/src/3rdparty/htmleditor \
               $$ALEPHERPPATH/src/3rdparty/qwwrichtextedit
    !contains(USEQSCINTILLA, Y) {
        INCLUDEPATH += $$ALEPHERPPATH/src/plugins/qcodeedit
        INCLUDEPATH += $$ALEPHERPPATH/src/plugins/qcodeedit/designer-plugin
    }
    contains(USEQSCINTILLA, Y) {
        INCLUDEPATH += $$ALEPHERPPATH/src/3rdparty/qscintilla/Qt4Qt5
    }
}

macx {
    INCLUDEPATH += $$ALEPHERPPATH/src/3rdparty/muparser/include
}

android|ios {
    INCLUDEPATH += $$ALEPHERPPATH/src/3rdparty/muparser/include
}
android {
    INCLUDEPATH += $$ALEPHERPPATH/src/android/aerpandroid
}

INCLUDEPATH += $$ALEPHERPPATH/src/lib/daobusiness

!android:!ios {
    contains (JASPERSERVERSUPPORT, Y) {
        INCLUDEPATH += $$ALEPHERPPATH/src/lib/jasperserver
    }
    INCLUDEPATH += $$ALEPHERPPATH/src/plugins/dbcommons
    INCLUDEPATH += $$ALEPHERPPATH/src/plugins/htmleditorplugin
    INCLUDEPATH += $$ALEPHERPPATH/src/plugins/scriptjasperserverplugin
    INCLUDEPATH += $$ALEPHERPPATH/src/plugins/jasperserverwebviewplugin
}

contains (VMIMESMTPSUPPORT, Y) {
    INCLUDEPATH += $$ALEPHERPPATH/src/plugins/vmimesmtp
}

contains (POCOSMTPSUPPORT, Y) {
    INCLUDEPATH += $$ALEPHERPPATH/src/plugins/pocosmtp
}

contains (QXTSMTPSUPPORT, Y) {
    INCLUDEPATH += $$ALEPHERPPATH/src/plugins/qxtsmtp
}

contains (CSSMTPSUPPORT, Y) {
    INCLUDEPATH += $$ALEPHERPPATH/src/plugins/csmailsmtp
}

contains (PRINTINGERPSUPPORT, Y) {
    INCLUDEPATH += $$ALEPHERPPATH/src/lib/printing \
                   $$ALEPHERPPATH/src/plugins/perpscriptextensionplugin \
                   $$ALEPHERPPATH/src/plugins/sheetimposition
}

contains (OPENRPTSUPPORT, Y) {
    INCLUDEPATH += $$ALEPHERPPATH/src/plugins/openrptplugin
}

contains (CLOUDSUPPORT, Y) {
    INCLUDEPATH += $$ALEPHERPPATH/src/plugins/aerphttpsqldriver
    contains (USEWEBSOCKETS, Y) {
        INCLUDEPATH += $$ALEPHERPPATH/src/plugins/aerphttpsqldriver/qtwebsockets
    }
}

contains (AERPDOCMNGSUPPORT, Y) {
    INCLUDEPATH += $$ALEPHERPPATH/src/plugins/dbdocumentmngmnt \
                   $$ALEPHERPPATH/src/plugins/plaincontentplugin
    win32 {
        INCLUDEPATH += $$ALEPHERPPATH/src/plugins/qtwain
        contains (WIASUPPORT, Y) {
            INCLUDEPATH += $$ALEPHERPPATH/src/plugins/qwiascan
        }
    }
    unix {
        INCLUDEPATH += $$ALEPHERPPATH/src/plugins/qsane
    }
}
