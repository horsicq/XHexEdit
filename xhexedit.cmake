include_directories(${CMAKE_CURRENT_LIST_DIR})

include(${CMAKE_CURRENT_LIST_DIR}/../Controls/xabstracttableview.cmake)

set(XHEXEDIT_SOURCES
    ${XABSTRACTTABLEVIEW_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/dialoghexedit.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dialoghexedit.h
    ${CMAKE_CURRENT_LIST_DIR}/dialoghexedit.ui
    ${CMAKE_CURRENT_LIST_DIR}/xhexedit.cpp
    ${CMAKE_CURRENT_LIST_DIR}/xhexedit.h
)
