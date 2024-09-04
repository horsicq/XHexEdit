INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

HEADERS += \
    $$PWD/dialoghexedit.h \
    $$PWD/xhexedit.h

SOURCES += \
    $$PWD/dialoghexedit.cpp \
    $$PWD/xhexedit.cpp

FORMS += \
    $$PWD/dialoghexedit.ui
    
!contains(XCONFIG, xabstracttableview) {
    XCONFIG += xabstracttableview
    include($$PWD/../Controls/xabstracttableview.pri)
}

DISTFILES += \
    $$PWD/LICENSE \
    $$PWD/README.md \
    $$PWD/xhexedit.cmake
