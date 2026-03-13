# Auto-increment build number after successful build
# Reads build_number.txt, increments, writes back

set(BUILD_FILE "${CMAKE_CURRENT_LIST_DIR}/../build_number.txt")

if(EXISTS "${BUILD_FILE}")
    file(READ "${BUILD_FILE}" BUILD_NUM)
    string(STRIP "${BUILD_NUM}" BUILD_NUM)
    math(EXPR BUILD_NUM "${BUILD_NUM} + 1")
else()
    set(BUILD_NUM 1)
endif()

file(WRITE "${BUILD_FILE}" "${BUILD_NUM}\n")
