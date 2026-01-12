cmake_minimum_required(VERSION 3.10)
project(identify-export)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set MUD source directory (parent of tools/)
set(MUD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# Include MUD headers
include_directories(${MUD_SOURCE_DIR}/src)

# Find required libraries
# Try to use pkg-config for SQLite3
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(SQLITE3 REQUIRED sqlite3)
else()
	find_package(SQLite3 REQUIRED)
endif()

# Iconv is usually built-in on Linux
find_library(ICONV_LIBRARY NAMES iconv)
if (NOT ICONV_LIBRARY)
	# If not found, it might be in libc
	set(ICONV_LIBRARY "")
endif()

# Link with circle.library for full MUD functionality
# This gives us access to CharData, ObjData, boot system, and mort_show_obj_values
# Note: This approach is simpler than trying to cherry-pick individual source files

set(MUD_SOURCES
	# No sources needed - we'll link with circle.library instead
)

# Identify-export sources
set(IDENTIFY_EXPORT_SOURCES
    src/main.cpp
    src/exporter.cpp
    src/mock_char.cpp
    src/db_writer.cpp
    src/encoding.cpp
    src/mud_stubs.cpp
    src/identify_simulation.cpp
)

# Create executable
add_executable(identify-export
    ${IDENTIFY_EXPORT_SOURCES}
    ${MUD_SOURCES}
)

# Link with circle.library from parent project
# This gives us full MUD functionality (CharData, ObjData, spells, etc.)
target_link_libraries(identify-export circle.library)

# Link SQLite3
if (PKG_CONFIG_FOUND AND SQLITE3_FOUND)
	target_include_directories(identify-export PRIVATE ${SQLITE3_INCLUDE_DIRS})
	target_link_libraries(identify-export ${SQLITE3_LIBRARIES})
else()
	target_link_libraries(identify-export SQLite::SQLite3)
endif()

# Link iconv if found
if (ICONV_LIBRARY)
	target_link_libraries(identify-export ${ICONV_LIBRARY})
endif()

# Add pthread (needed by circle.library on Linux)
if (UNIX)
	target_link_libraries(identify-export pthread)
endif()

# Compiler flags
target_compile_options(identify-export PRIVATE
    -Wall
    -Wextra
    -DTEST_BUILD
    -DNOCRYPT
)

# vim: ts=4 sw=4 tw=0 noet syntax=cmake :
