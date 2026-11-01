# PPM 作为 ESP-IDF component
#
# 不复制源码。SRCS 用相对路径指回旁边的 PPM 仓库，
# 这样板上编的和 PC 上测过的是同一份文件。

set(PPM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../PPM")

idf_component_register(
    SRCS
        "${PPM_ROOT}/src/core/ppm_json.cpp"
        "${PPM_ROOT}/src/core/ppm_config.cpp"
        "${PPM_ROOT}/src/core/ppm_ctx.cpp"
        "${PPM_ROOT}/src/core/ppm_abi_check.cpp"
        "${PPM_ROOT}/src/pipeline/ppm_module.cpp"
        "${PPM_ROOT}/src/pipeline/ppm_sched.cpp"
        "${PPM_ROOT}/src/backend/ppm_mock.cpp"
        "${PPM_ROOT}/src/decision/ppm_decision.cpp"
        "${PPM_ROOT}/src/decision/ppm_dec_drowsy.cpp"
        "${PPM_ROOT}/src/decision/ppm_dec_attention.cpp"
        "${PPM_ROOT}/src/decision/ppm_dec_presence.cpp"
        "${PPM_ROOT}/src/decision/ppm_dec_gesture.cpp"
    INCLUDE_DIRS "${PPM_ROOT}/include"
)

target_include_directories(${COMPONENT_LIB} PRIVATE
    "${PPM_ROOT}/src/core"
    "${PPM_ROOT}/src/pipeline"
    "${PPM_ROOT}/src/backend"
    "${PPM_ROOT}/src/decision"
)
