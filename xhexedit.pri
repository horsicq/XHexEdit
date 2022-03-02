INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

HEADERS += \
    $$PWD/dialoghexedit.h \
    $$PWD/xhexedit.h

SOURCES += \
    $$PWD/dialoghexedit.cpp \
    $$PWD/xhexedit.cpp
    
!contains(XCONFIG, xabstracttableview) {
    XCONFIG += xabstracttableview
    include($$PWD/../Controls/xabstracttableview.pri)
}

!contains(XCONFIG, xbinary) {
    XCONFIG += xbinary
    include($$PWD/../Formats/xbinary.pri)
}

DISTFILES += \
    $$PWD/LICENSE \
    $$PWD/README.md \
    $$PWD/xhexedit.cmake

FORMS += \
    $$PWD/dialoghexedit.ui
