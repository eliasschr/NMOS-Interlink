cmake_minimum_required(VERSION 3.20)

project(NMOSInterlink VERSION 0.0.0 LANGUAGES CXX)

add_executable(nmos-interlink
    src/main.cpp
)

target_compile_features(nmos-interlink PRIVATE cxx_std_17)
target_compile_definitions(nmos-interlink PRIVATE
    NMOS_INTERLINK_VERSION="${PROJECT_VERSION}"
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
