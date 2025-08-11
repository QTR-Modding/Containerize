# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO QTR-Modding/DynamicAnimationFrameworkSE_API
    REF b88420430ba6efb8932a062c674f04fd2647737f
    SHA512 bf2c9dcbcb3e40f5e88b001da9f0b9d2d3e048526a36286aae27274e26ad389ef38e419a7bccbbdf3d3446a870af4142f1bddbb1815ff62ee631a970fbc57e74
    HEAD_REF main
)

# Install codes
set(DAF_API_INCLUDE_DIRS	${SOURCE_PATH}/include/DynamicAnimationFramework)
file(INSTALL ${DAF_API_INCLUDE_DIRS} DESTINATION ${CURRENT_PACKAGES_DIR}/include)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/NOTICE")