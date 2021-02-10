INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
    
!contains(XCONFIG, xabstracttableview) {
    XCONFIG += xabstracttableview
    include($$PWD/../Controls/xabstracttableview.pri)
}

!contains(XCONFIG, xbinary) {
    XCONFIG += xbinary
    include($$PWD/../Formats/xbinary.pri)
}

HEADERS += \
    $$PWD/xhexedit.h

SOURCES += \
    $$PWD/xhexedit.cpp
