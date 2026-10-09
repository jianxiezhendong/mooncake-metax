if (CUDAToolkit_FOUND)
    file(GLOB TENT_PLATFORM_CUDA_SOURCES "*.cpp")
    add_library(platform_cuda STATIC ${TENT_PLATFORM_CUDA_SOURCES})
    target_link_libraries(platform_cuda PUBLIC tent_common
        CUDA::cudart CUDA::cuda_driver)
elseif(USE_MACA)
    file(GLOB TENT_PLATFORM_CUDA_SOURCES "*.cpp")
    add_library(platform_cuda STATIC ${TENT_PLATFORM_CUDA_SOURCES})
    if(NOT DEFINED MACA_RUNTIME_LIBS)
        set(MACA_RUNTIME_LIBS mcruntime mxc-runtime64 rt)
    endif()
    target_include_directories(platform_cuda PUBLIC ${MACA_INCLUDE_DIR})
    # PRIVATE: PUBLIC USE_CUDA leaks into transport_loader and tries to
    # construct NVLink/MNNVL, which are not built without CUDAToolkit.
    target_compile_definitions(platform_cuda PRIVATE USE_CUDA USE_MACA)
    target_link_libraries(platform_cuda PUBLIC tent_common ${MACA_RUNTIME_LIBS})
endif()
