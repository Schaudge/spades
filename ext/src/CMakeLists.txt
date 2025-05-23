# -*- cmake -*-

include(FetchContent)

if (SPADES_USE_JEMALLOC)
  add_subdirectory(jemalloc EXCLUDE_FROM_ALL)
endif()
if (SPADES_USE_MIMALLOC)
  add_subdirectory(mimalloc EXCLUDE_FROM_ALL)
endif()

# Need to put it first as e.g. bwa depends on zlib
FetchContent_Declare(zlib_ng
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/zlib-ng")
FetchContent_GetProperties(zlib_ng)
if (NOT zlib_ng_POPULATED)
  set(SKIP_INSTALL_ALL ON)
  set(ZLIB_COMPAT ON)
  set(WITH_GTEST OFF)
  FetchContent_Populate(zlib_ng)
  add_subdirectory(${zlib_ng_SOURCE_DIR} ${zlib_ng_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_subdirectory(nlopt)
add_subdirectory(python_libs/joblib3)
add_subdirectory(python_libs/pyyaml3)
add_subdirectory(bamtools)
add_subdirectory(samtools)
add_subdirectory(cppformat)
add_subdirectory(ssw)
add_subdirectory(llvm)
add_subdirectory(bwa)
add_subdirectory(gqf)
add_subdirectory(edlib)
add_subdirectory(lexy)
add_subdirectory(easel)
add_subdirectory(hmmer)
add_subdirectory(googletest)
add_subdirectory(teamcity_gtest)
add_subdirectory(spoa)
if (SPADES_USE_NCBISDK)
  add_subdirectory(ncbi EXCLUDE_FROM_ALL)
endif()
