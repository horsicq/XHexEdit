include_directories(${CMAKE_CURRENT_LIST_DIR})

if (NOT DEFINED XABSTRACTTABLEVIEW_SOURCES)
    include(${CMAKE_CURRENT_LIST_DIR}/../Controls/xabstracttableview.cmake)
    set(XHEXVIEW_SOURCES ${XHEXVIEW_SOURCES} ${XABSTRACTTABLEVIEW_SOURCES})
endif()

set(XHEXEDIT_SOURCES
    ${XHEXVIEW_SOURCES}
    ${XABSTRACTTABLEVIEW_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/dialoghexedit.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dialoghexedit.h
    ${CMAKE_CURRENT_LIST_DIR}/dialoghexedit.ui
    ${CMAKE_CURRENT_LIST_DIR}/xhexedit.cpp
    ${CMAKE_CURRENT_LIST_DIR}/xhexedit.h
)
