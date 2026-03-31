# libzypp/external/CMakeLists.txt
# -----------------------------------------------------------------------------
# Polyfill Targets (Offline-Safe)
# -----------------------------------------------------------------------------
# These targets expose the vendored headers to the rest of the libzypp build.
# They do NOT attempt to download anything during a normal build.

add_library(zypp_ranges_polyfill INTERFACE)
target_include_directories(zypp_ranges_polyfill INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/nanorange/include>)

add_library(zypp_span_polyfill INTERFACE)
target_include_directories(zypp_span_polyfill INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tcb-span/include>)
# -----------------------------------------------------------------------------
# Developer Maintenance Targets (Run Manually)
# -----------------------------------------------------------------------------
# These targets are strictly for developers to update the vendored polyfills
# prior to committing them to the repository. They require network access.
set(RANGE_VERSION "bf32251d65673fe170d602777c087786c529ead8")
set(RANGE_URL "https://raw.githubusercontent.com/tcbrindle/NanoRange/${RANGE_VERSION}")
add_custom_target(update-ranges
    COMMAND ${CMAKE_COMMAND} -E echo "Fetching ranges polyfill v${RANGE_VERSION}..."
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_SOURCE_DIR}/nanorange
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/nanorange/include/nanorange
    COMMAND curl -sSL ${RANGE_URL}/single_include/nanorange.hpp -o ${CMAKE_CURRENT_SOURCE_DIR}/nanorange/include/nanorange/nanorange.hpp
    COMMAND curl -sSL ${RANGE_URL}/LICENSE_1_0.txt -o ${CMAKE_CURRENT_SOURCE_DIR}/nanorange/LICENSE_1_0.txt
    COMMAND ${CMAKE_COMMAND} -E echo "range polyfill successfully updated."
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Updating vendored range polyfill headers"
)
set(TCB_SPAN_VERSION "836dc6a0efd9849cb194e88e4aa2387436bb079b") # Provide a specific tag or commit hash here
set(TCB_SPAN_URL "https://raw.githubusercontent.com/tcbrindle/span/${TCB_SPAN_VERSION}")
add_custom_target(update-span
    COMMAND ${CMAKE_COMMAND} -E echo "Fetching tcb-span ${TCB_SPAN_VERSION}..."
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_SOURCE_DIR}/tcb-span
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/tcb-span/include/tcb
    COMMAND curl -sSL ${TCB_SPAN_URL}/include/tcb/span.hpp -o ${CMAKE_CURRENT_SOURCE_DIR}/tcb-span/include/tcb/span.hpp
    COMMAND curl -sSL ${TCB_SPAN_URL}/LICENSE_1_0.txt -o ${CMAKE_CURRENT_SOURCE_DIR}/tcb-span/LICENSE_1_0.txt
    COMMAND ${CMAKE_COMMAND} -E echo "tcb-span successfully updated."
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Updating vendored tcb-span header"
)
