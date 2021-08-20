set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -Os -s")
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Boost 1.71 REQUIRED COMPONENTS program_options)
# Search OpenSSL
find_package(PkgConfig REQUIRED)
pkg_search_module(OPENSSL REQUIRED openssl)

if( OPENSSL_FOUND )
    include_directories(${OPENSSL_INCLUDE_DIRS})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
else()
    # Error; with REQUIRED, pkg_search_module() will throw an error by it's own
endif()

# target_link_libraries(${YOUR_TARGET_HERE} )
# find_package(OpenSSL 0.10.23 REQUIRED  COMPONENTS SSL Crypto)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
include_directories(${Boost_INCLUDE_DIR} ${OpenSSL_INCLUDE_DIR})

add_executable(dnahide main.cc genbank.cc crypto/kdf/fastbpkdf2.c)
target_link_libraries(dnahide PRIVATE Threads::Threads ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ZLIB::ZLIB)

install(TARGETS dnahide RUNTIME DESTINATION bin)
