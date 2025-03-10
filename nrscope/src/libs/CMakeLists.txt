file(GLOB_RECURSE SOURCES "*.cc" "*.c")
add_library(nrscope_libs STATIC ${SOURCES})

# message(${PROJECT_SOURCE_DIR})
# target_include_directories(ngscope_dci PUBLIC ${PROJECT_SOURCE_DIR}/ngscope/hdr)

if(ENABLE_GUI AND SRSGUI_FOUND)
  target_link_libraries(nrscope_libs ${SRSGUI_LIBRARIES})

target_link_libraries(nrscope_libs ${SRSRAN_SOURCES}
                                   ${SRSUE_SOURCES})
endif()

# add_executable(to_sunshine to_sunshine.cc)
# target_link_libraries(to_sunshine srsue_phy
#                                 srsran_common
#                                 srsran_phy
#                                 srsran_radio
#                                 rrc_nr_asn1
#                                 nrscope_libs
#                                 ${SRSUE_SOURCES} 
#                                 ${SRSRAN_SOURCES}
#                                 ${CMAKE_THREAD_LIBS_INIT}
#                                 ${Boost_LIBRARIES}
#                                 ${YAML_CPP_LIBRARIES})
# add_executable(to_moonlight to_moonlight.cc)
# target_link_libraries(to_moonlight srsue_phy
#                                 srsran_common
#                                 srsran_phy
#                                 srsran_radio
#                                 rrc_nr_asn1
#                                 nrscope_libs
#                                 ${SRSUE_SOURCES} 
#                                 ${SRSRAN_SOURCES}
#                                 ${CMAKE_THREAD_LIBS_INIT}
#                                 ${Boost_LIBRARIES}
#                                 ${YAML_CPP_LIBRARIES})

