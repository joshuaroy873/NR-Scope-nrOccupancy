if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost required to compile srsUE and ")
endif()

########################################################################
# Setup the include and linker paths
########################################################################
include_directories(
    ${Boost_INCLUDE_DIRS}
    ${SEC_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}
    ${YAML_CPP_INCLUDE_DIR}
)

if(PUSH_TO_GOOGLE)
    include_directories(${PYTHON_INCLUDE_DIR})
    include_directories(${GOOGLE_CLOUD_CPP_INCLUDE_DIRS})
endif()

link_directories(
    ${Boost_LIBRARY_DIRS}
    ${SEC_LIBRARY_DIRS}
)

########################################################################
# Add subdirectories
########################################################################
add_subdirectory(src)