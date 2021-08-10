set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -g")
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Boost 1.71 COMPONENTS program_options REQUIRED)
find_package(ZLIB)
find_package(Threads REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

add_executable(dnahide main.cc sha256.cc)
target_link_libraries(dnahide PUBLIC ${Boost_LIBRARIES} ZLIB::ZLIB PRIVATE Threads::Threads)

install(TARGETS dnahide RUNTIME DESTINATION bin)
