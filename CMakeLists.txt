cmake_minimum_required (VERSION 3.5)

list (APPEND CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake
  ${CMAKE_CURRENT_BINARY_DIR})

project (cxlmalloc
  VERSION 1.0
  LANGUAGES CXX)

# WARNING_FLAGS:=-ftrapv -Wreturn-type -W -Wall \
# -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter

option(USE_CXL "option for using cxl devic" OFF)
# if you would like to disbale, use '-D USE_CXL=OFF'

if(USE_CXL)
  message("[USE CXL DEV]")
  add_definitions("-DUSE_CXL_DEV")
endif()

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -pthread -lnuma -lstdc++ -fpermissive -fno-omit-frame-pointer -fPIC")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -pthread -lnuma -lstdc++ -fpermissive -fno-omit-frame-pointer -fPIC")

set (CMAKE_CXX_STANDARD
  "20"
  CACHE
  STRING
  "C++ standard to build with.")

set (CMAKE_BUILD_TYPE
  "${CMAKE_BUILD_TYPE}"
  CACHE
  STRING
  "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel Dev Sanitize."
  FORCE)

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE "Debug")
endif ()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_EXE_LINKER_FLAGS "-latomic")

option(FAULT_INJECTION "fault injection option" OFF)
if(FAULT_INJECTION)
  add_definitions(-DFAULT_INJECTION)  
endif()

option(CXL_BUILD_TESTS       "Build test executables" ON)

set(INCLUDE_DIRECTORIES include)

add_library (cxlmalloc STATIC
  src/alloc.cpp
  src/cxl_ref.cpp
  src/cxl_shm.cpp
  src/page.cpp
  src/segment.cpp
  src/page_queue.cpp
  src/transfer.cpp
  src/hash_index.cpp
  src/recovery.cpp
)
target_include_directories(cxlmalloc PUBLIC ${INCLUDE_DIRECTORIES})

# if(CXL_BUILD_TESTS)
#   list(APPEND cxl_build_targets "tests")
# endif()

if (CXL_BUILD_TESTS)
  enable_testing()

  foreach(TEST_NAME api stress)
    add_executable(cxlmalloc-test-${TEST_NAME} test/test_${TEST_NAME}.cpp)
    target_include_directories(cxlmalloc-test-${TEST_NAME} PRIVATE ${INCLUDE_DIRECTORIES})
    target_link_libraries(cxlmalloc-test-${TEST_NAME} PRIVATE cxlmalloc atomic)

    add_test(NAME test-${TEST_NAME} COMMAND cxlmalloc-test-${TEST_NAME})
  endforeach()
endif()

foreach(TEST_NAME threadtest sh6bench rpc_server rpc_client kv kv_zipf tatp smallbank graph)
  add_executable(cxlmalloc-benchmark-${TEST_NAME} test/benchmark/${TEST_NAME}.cpp)
  target_link_libraries(cxlmalloc-benchmark-${TEST_NAME} PUBLIC cxlmalloc atomic numa)
  target_include_directories(cxlmalloc-benchmark-${TEST_NAME} PUBLIC ${INCLUDE_DIRECTORIES})
endforeach()


foreach(TEST_NAME shm_init recovery_test recovery_check recovery_test_time)
  add_executable(${TEST_NAME} test/${TEST_NAME}.cpp)
  target_link_libraries(${TEST_NAME} PUBLIC cxlmalloc atomic)
  target_include_directories(${TEST_NAME} PUBLIC ${INCLUDE_DIRECTORIES})
endforeach()

add_executable(cxlmalloc-benchmark-wc test/benchmark/mapreduce/wordcount.cpp)
target_include_directories(cxlmalloc-benchmark-wc PRIVATE ${INCLUDE_DIRECTORIES})
target_link_libraries(cxlmalloc-benchmark-wc PRIVATE cxlmalloc atomic numa)

add_executable(cxlmalloc-benchmark-pr test/benchmark/mapreduce/pagerank.cpp)
target_include_directories(cxlmalloc-benchmark-pr PRIVATE ${INCLUDE_DIRECTORIES})
target_link_libraries(cxlmalloc-benchmark-pr PRIVATE cxlmalloc atomic numa)

add_executable(cxlmalloc-benchmark-km test/benchmark/mapreduce/kmeans.cpp)
target_include_directories(cxlmalloc-benchmark-km PRIVATE ${INCLUDE_DIRECTORIES})
target_link_libraries(cxlmalloc-benchmark-km PRIVATE cxlmalloc atomic numa)

find_package(TBB REQUIRED)

add_executable(cxlmalloc-benchmark-kv_baseline test/benchmark/kv_baseline.cpp)
target_link_libraries(cxlmalloc-benchmark-kv_baseline PUBLIC cxlmalloc PUBLIC tbb atomic numa)
target_include_directories(cxlmalloc-benchmark-kv_baseline PUBLIC ${INCLUDE_DIRECTORIES})

add_executable(cxlmalloc-benchmark-smallbank_tbb test/benchmark/smallbank_tbb.cpp)
target_link_libraries(cxlmalloc-benchmark-smallbank_tbb PUBLIC cxlmalloc PUBLIC tbb atomic numa)
target_include_directories(cxlmalloc-benchmark-smallbank_tbb PUBLIC ${INCLUDE_DIRECTORIES})

add_executable(cxlmalloc-benchmark-tatp_tbb test/benchmark/tatp_tbb.cpp)
target_link_libraries(cxlmalloc-benchmark-tatp_tbb PUBLIC cxlmalloc PUBLIC tbb atomic numa)
target_include_directories(cxlmalloc-benchmark-tatp_tbb PUBLIC ${INCLUDE_DIRECTORIES})
