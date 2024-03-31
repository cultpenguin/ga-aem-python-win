#include(cmake/Message-Functions.cmake)
set(PETSC_DIR $ENV{PETSC_DIR})
set(PETSC_LIBRARY_DIR $ENV{PETSC_LIBRARY_DIR})
set(PETSC_FOUND FALSE)
message(STATUS "\nChecking for PETSc")
find_package(PETSC QUIET)
if(NOT PETSC_FOUND AND PkgConfig_FOUND)
	pkg_search_module(PETSC IMPORTED_TARGET PETSc)
	if(PETSC_FOUND)
		message(STATUS "PETSC was found by pkg_search_module()")
		#reportallvars()
	endif()
endif()
if(NOT PETSC_FOUND)
	message(STATUS "PETSc was NOT found by find_package() or pkg_search_module() --- resorting to setup via environment variables instead")
	include(cmake/Configure-PETSc-Manual-Setup.cmake)
endif()

#if(PETSC_FOUND)
#	reportvar(PETSC_DIR)
#	reportvar(PETSC_INSTALL_PREFIX)
#	reportvar(PETSC_VERSION)
#	reportvar(PETSC_INCLUDE_DIRS)
#	reportvar(PETSC_LIB_DIR)
#	reportvar(PETSC_EXTRA_LIB_DIR)
#	reportvar(PETSC_LIBRARIES)
#	reportvar(PETSC_LINK_LIBRARIES)
#	reportvar(PETSC_LDFLAGS)
#	endif()

