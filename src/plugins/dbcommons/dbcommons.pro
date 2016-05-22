message( "---------------------------------" )
message( "CONSTRUYENDO DBCOMMONS..." )
message( "---------------------------------" )

include (../../../config.pri)

CONFIG += plugin shared

contains (ADVANCEDEDIT, Y) {
    LIBS += -lqwwrichtextedit
}

CONFIG(release, debug|release) {
    DESTDIR = $$BUILDPATH/release/plugins/designer/
}
CONFIG(debug, debug|release) {
    DESTDIR = $$BUILDPATH/debug/plugins/designer/
}

TARGET = dbcommonsplugin
TEMPLATE = lib

QT += widgets uitools designer webenginewidgets

QT += sql script network

LIBS += -ldaobusiness

HEADERS = dbcomboboxplugin.h \
    dbdetailviewplugin.h \
    dbfiltertableviewplugin.h \
    dbtableviewplugin.h \
    dbcommonsplugin.h \
    aerpscriptwidgetplugin.h \
    dblineeditplugin.h \
    dblabelplugin.h \
    dbcheckboxplugin.h \
    dbtexteditplugin.h \
    dbdatetimeeditplugin.h \
    dbnumbereditplugin.h \
    dbtabwidgetplugin.h \
    dbframebuttonsplugin.h \
    dbtreeviewplugin.h \
    dblistviewplugin.h \
    graphicsstackedwidgetplugin.h \
    menutreewidgetplugin.h \
    dbchooserecordbuttonplugin.h \
    dbfileuploadplugin.h \
    mainwindowplugin.h \
    dbfiltertreeviewplugin.h \
    aerpbasedialogplugin.h \
    dbbaseplugin.h \
    dbcommontaskmenufactory.h \
    alepherpdbcommons.h \
    dbrelatedelementsviewplugin.h \
    dbchooserelatedrecordbuttonplugin.h \
    dbscheduleviewplugin.h \
    dbfullscheduleviewplugin.h \
    dbchoosefieldnametaskmenu.h \
    dbchoosefieldnamedlg.h \
    dbchooserecordbuttontaskmenu.h \
    dbchooserecordbuttonassignfieldsdlg.h \
    dbmappositionplugin.h \
    dbgrouprelationmmhelperplugin.h

SOURCES = dbcomboboxplugin.cpp \
    dbdetailviewplugin.cpp \
    dbfiltertableviewplugin.cpp \
    dbtableviewplugin.cpp \
    dbcommonsplugin.cpp \
    aerpscriptwidgetplugin.cpp \
    dblineeditplugin.cpp \
    dblabelplugin.cpp \
    dbcheckboxplugin.cpp \
    dbtexteditplugin.cpp \
    dbdatetimeeditplugin.cpp \
    dbnumbereditplugin.cpp \
    dbtabwidgetplugin.cpp \
    dbframebuttonsplugin.cpp \
    dbtreeviewplugin.cpp \
    dblistviewplugin.cpp \
    graphicsstackedwidgetplugin.cpp \
    menutreewidgetplugin.cpp \
    dbchooserecordbuttonplugin.cpp \
    dbfileuploadplugin.cpp \
    mainwindowplugin.cpp \
    dbfiltertreeviewplugin.cpp \
    aerpbasedialogplugin.cpp \
    dbbaseplugin.cpp \
    dbcommontaskmenufactory.cpp \
    dbrelatedelementsviewplugin.cpp \
    dbchooserelatedrecordbuttonplugin.cpp \
    dbscheduleviewplugin.cpp \
    dbfullscheduleviewplugin.cpp \
    dbchoosefieldnametaskmenu.cpp \
    dbchoosefieldnamedlg.cpp \
    dbchooserecordbuttontaskmenu.cpp \
    dbchooserecordbuttonassignfieldsdlg.cpp \
    dbmappositionplugin.cpp \
    dbgrouprelationmmhelperplugin.cpp

FORMS += \
    $$LIB_DAOBUSINESS/widgets/dbdetailview.ui \
    $$LIB_DAOBUSINESS/widgets/dbabstractfilterview.ui \
    $$LIB_DAOBUSINESS/widgets/dbfileupload.ui \
    $$LIB_DAOBUSINESS/widgets/dbrelatedelementsview.ui \
    aerpscriptwidget.ui \
    dbnumberedit.ui \
    dbchooserecordbuttonassignfieldsdlg.ui \
    dbchoosefieldnamedlg.ui
RESOURCES = dbcommons.qrc

OTHER_FILES += \
    dbcommonsplugin.json

contains(BARCODESUPPORT, Y) {
    HEADERS += dbbarcodeplugin.h
    SOURCES += dbbarcodeplugin.cpp
}

contains(AERPADVANCEDEDIT, Y) {
    LIBS += -lqwwrichtextedit

    HEADERS += qwwrichtexteditplugin.h \
               dbrichtexteditplugin.h \
               dbcodeeditplugin.h

    SOURCES += qwwrichtexteditplugin.cpp \
               dbrichtexteditplugin.cpp \
               dbcodeeditplugin.cpp

    !contains(USEQSCINTILLA, Y) {
        LIBS += -lqcodeedit
    }

    contains(DEVTOOLS, Y) {
        FORMS   += $$LIB_DAOBUSINESS/widgets/aerpsystemobjecteditorwidget.ui
        HEADERS += aerpsystemobjecteditorplugin.h
        SOURCES += aerpsystemobjecteditorplugin.cpp
    }
}

contains(AERPDOCMNGSUPPORT, Y) {
    HEADERS += dbrecorddocumentsplugin.h
    SOURCES += dbrecorddocumentsplugin.cpp
}
