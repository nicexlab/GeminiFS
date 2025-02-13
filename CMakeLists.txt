cmake_minimum_required (VERSION 3.1)
project (libnvm LANGUAGES CUDA C CXX)

set (CMAKE_C_STANDARD 99)
set (CMAKE_CXX_STANDARD 11)

set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED)
find_package (CUDA 12.4)




set (NVIDIA "" CACHE PATH "Path to Nvidia driver source")
set (KERNEL "/lib/modules/${CMAKE_SYSTEM_VERSION}/build" CACHE PATH "Path to kernel source, or module directory")


set (nvidia_archs "70;80;90" CACHE STRING "NVIDIA compute architectures")
set (no_module false CACHE BOOL "Do not build kernel module")
set (no_cuda false CACHE BOOL "Do not build library with CUDA support")
set (no_cc false CACHE BOOL "Do not assume cache-coherent architecture")
set (no_fio true CACHE BOOL "Do not build FIO plugin")
set (BUILD_SHARED_LIBS true CACHE BOOL "Build shared library, set to false for static")

if (no_smartio)
    set (no_smartio_samples true)
    set (no_smartio_benchmarks true)
endif ()

# Set global compiler flags
set (GPU_ARCHS "" CACHE INTERNAL "" FORCE)
foreach (arch ${nvidia_archs})
    string (CONCAT GPU_ARCHS ${GPU_ARCHS} " -gencode arch=compute_${arch},code=sm_${arch}")
endforeach ()

set (convert_warnings implicit-function-declaration incompatible-pointer-types int-conversion return-type unused-function unused-variable)
set (flags "")
foreach (flag ${convert_warnings})
    string (APPEND flags " -Werror=${flag}")
endforeach()
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra ${flags}" CACHE INTERNAL "" FORCE)
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG} -g -DDEBUG" CACHE INTERNAL "" FORCE)
set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2" CACHE INTERNAL "" FORCE)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++11" CACHE INTERNAL "" FORCE)
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -g -DDEBUG " CACHE INTERNAL "" FORCE)
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -std=c++11" CACHE INTERNAL "" FORCE)
set (CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} -lineinfo -D__CUDA__ -D__CUDACC__ -O3;-std=c++11;${GPU_ARCHS}")
set (CUDA_NVCC_FLAGS_RELEASE "${CMAKE_NVCC_FLAGS_RELEASE} -O2" CACHE INTERNAL "" FORCE)

if (no_cc)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__NO_COHERENCE__")
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -D__NO_COHERENCE__")
    set(CCUDA_NVCC_FLAGS "${CMAKE_C_FLAGS} -D__NO_COHERENCE__")
endif ()


# Try to find Nvidia driver
if (CUDA_FOUND AND NOT no_cuda)

    # Try to find Nvidia headers (will choose the newest version)
    file (GLOB_RECURSE driver_paths "/usr/src/nvidia-*/nv-p2p.h")
    string (REPLACE "nv-p2p.h" "" driver_paths "${driver_paths}")
    if (driver_paths)
        list (REVERSE driver_paths)
    endif ()

    find_path (driver_include NAMES "nv-p2p.h" PATHS "${NVIDIA}" ${driver_paths})
    set (driver_dir "${driver_include}")

    # Find Nvidia driver symbols
    if (NOT EXISTS "${driver_dir}/Module.symvers")
        get_filename_component (driver_dir "${driver_dir}" DIRECTORY)
    endif ()

    if ("${driver_dir}" STREQUAL "")
        message(STATUS "No NVIDIA driver symbols found")
    else ()
        message(STATUS "Using NVIDIA driver found in ${driver_dir}")
    endif ()
endif ()




# Library source files
set (libnvm_root "${PROJECT_SOURCE_DIR}/src")
file (GLOB libnvm_source "${libnvm_root}/*.cpp")
file (GLOB libnvm_linux_source "${libnvm_root}/linux/*.cpp")
file (GLOB libnvm_include "${PROJECT_BINARY_DIR}/include/*.h"  "${libnvm_root}/include/*.h" )

# # Samples shared files
set (samples_root "${CMAKE_SOURCE_DIR}/examples/")



# Specify where stuff should be placed
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
# set (module_output "${CMAKE_BINARY_DIR}/module")
# set (fio_output "${CMAKE_BINARY_DIR}/fio")



# Set include directories
include_directories ("${PROJECT_BINARY_DIR}/include" "${PROJECT_SOURCE_DIR}/src/include" "${PROJECT_SOURCE_DIR}/src/freestanding/include")



# Create NVME library target
CUDA_ADD_LIBRARY (libnvm ${libnvm_source} ${libnvm_linux_source}) # TODO: add check for OS
target_include_directories (libnvm PRIVATE "${libnvm_root}")
target_link_libraries(libnvm ${CUDA_LIBRARIES})


target_link_libraries (libnvm Threads::Threads)
message (STATUS "Configuring libnvm without SmartIO")




if (CUDA_FOUND AND NOT no_cuda)
    target_compile_definitions(libnvm PRIVATE _CUDA __CUDA__)
endif ()

# Set name of library
#set_target_properties (libnvm PROPERTIES OUTPUT_NAME "${CMAKE_PROJECT_NAME}")
set_target_properties (libnvm PROPERTIES OUTPUT_NAME "nvm")

# Set install locations
install (FILES ${libnvm_include} DESTINATION "include")
install (TARGETS libnvm DESTINATION "lib")



# Create kernel module makefile
if (NOT no_module)
    if (CUDA_FOUND AND NOT no_cuda AND EXISTS "${driver_dir}/Module.symvers")
        set (module_ccflags "-D_CUDA -I${driver_include} -I${libnvm_root} -I${libnvm_root}/linux")
        set (module_symbols "${driver_dir}/Module.symvers")
        message(STATUS "Configuring kernel module with CUDA")
    else ()
        set (module_ccflags "-I${libnvm_root} -I${libnvm_root}/linux")
        message(STATUS "Configuring kernel module without CUDA")
    endif ()

    # configure_file ("${module_root}/Makefile.in" "${module_output}/Makefile" @ONLY)

    add_custom_target (kernel_module "${CMAKE_MAKE_PROGRAM}" WORKING_DIRECTORY "${module_output}")
endif ()





# Macro for creating sample target
macro (make_sample target binary_name files)
    add_executable (${target} EXCLUDE_FROM_ALL ${files})

    add_dependencies (${target} libnvm)
    target_link_libraries (${target} libnvm)

    # TODO: Do we want to install samples or not?
    #set_target_properties (${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/samples")
    set_target_properties (${target} PROPERTIES OUTPUT_NAME "nvm-${binary_name}")

    list (APPEND sample_targets "${target}")
    set (sample_targets "${sample_targets}" PARENT_SCOPE)

    #install (TARGETS ${target} DESTINATION "bin")
endmacro ()





# Make target depend on pthreads
macro (set_multithread target)
    target_link_libraries (${target} Threads::Threads)
endmacro ()


# Set preferred ordering for sample targets

# Make benchmark target
# We assume all benchmarks are CUDA programs
macro (make_benchmark target binary_name files)
    if (CUDA_FOUND AND NOT no_cuda)
        cuda_add_executable (${target} EXCLUDE_FROM_ALL ${files} OPTIONS ${GPU_ARCHS} -D__CUDA__) # Ugly bugly
        target_compile_definitions (${target} PRIVATE __CUDA__)

        add_dependencies (${target} libnvm)
        target_link_libraries (${target} libnvm)
        target_link_libraries (${target} ${CUDA_LIBRARIES})
        set_target_properties (${target} PROPERTIES OUTPUT_NAME "nvm-${binary_name}")

        list (APPEND benchmark_targets "${target}")
        set (benchmark_targets "${benchmark_targets}" PARENT_SCOPE)
    endif ()
endmacro ()


macro (make_samples_choice ctarget sisci_target other_target)
    if (NOT no_smartio_samples AND sisci_include AND sisci_lib)
        add_custom_target (${ctarget} DEPENDS ${sisci_target})
        list (REMOVE_ITEM sample_targets "${other_target}")
        set (sample_targets "${sample_targets}" PARENT_SCOPE)
    else ()
        add_custom_target (${ctarget} DEPENDS ${other_target})
        list (REMOVE_ITEM sample_targets "${sisci_target}")
        set (sample_targets "${sample_targets}" PARENT_SCOPE)
    endif ()
endmacro ()

add_subdirectory ("${samples_root}/integrity")

# Build all samples
add_custom_target (samples DEPENDS ${sample_targets})
add_custom_target (examples DEPENDS samples)


