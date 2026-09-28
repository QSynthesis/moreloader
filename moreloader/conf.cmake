include(GNUInstallDirs)

# ----------------------------------
# Project Constants
# ----------------------------------
set(MORELOADER_INCLUDE_DIR "include")

# The sub-libraries are linked into the single executable moreloader and are not distributed on
# their own.
set(MORELOADER_BUILD_STATIC ON)

# Warnings that indicate an ABI mismatch with the guest are errors. A wrapper whose attributes are
# ignored, or whose return is missing, corrupts the state of the guest without an immediate
# failure.
function(_moreloader_common_configure_target _target)
    target_compile_options(${_target} PRIVATE
        -Wall -Wextra -Wno-unused-parameter
        -Werror=return-type -Werror=attributes
    )
endfunction()

set(MORELOADER_POST_CONFIGURE_COMMANDS _moreloader_common_configure_target)

# ----------------------------------
# Include Build Helpers
# ----------------------------------
qm_import(private/BuildSystem)

# Named for the module rather than for PROJECT_NAME, which each sub-library sets to its own target
# name, MoreLoaderSupport and the rest.
qm_setup_build_repo_helpers(moreloader)
