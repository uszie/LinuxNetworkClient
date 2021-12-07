PROJECT(lib_gui_plugin_kde4)
FIND_PACKAGE(KDE4 REQUIRED)
include (KDE4Defaults)
INCLUDE_DIRECTORIES( ${CMAKE_SOURCE_DIR} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  ../../libnetwork ../../../ )

set(_gui_plugin_kde4_SRCS 
   kde4.cpp 
 )

kde4_add_library(_gui_plugin_kde4 MODULE ${_gui_plugin_kde4_SRCS})

target_link_libraries(_gui_plugin_kde4  pthread ${KDE4_KDEUI_LIBS} )

install(TARGETS _gui_plugin_kde4 DESTINATION ${LNC_PLUGIN_DIR} )


########### install files ###############

#install( FILES kapp4.desktop  DESTINATION  ${XDG_APPS_DIR} )
#install( FILES kapp4.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
#install( FILES kapp4ui.rc  DESTINATION  ${DATA_INSTALL_DIR}/kapp4 )


