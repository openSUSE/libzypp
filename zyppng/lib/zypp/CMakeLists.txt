# zyppng/lib/zypp/CMakeLists.txt
#
# C++20 named module 'zyppng'.
# FILE_SET CXX_MODULES requires CMake >= 3.28.
cmake_minimum_required(VERSION 3.28)

include(${zypp-libs_SOURCE_DIR}/zypp-logic/zypp/zypp.cmake)

# ---------------------------------------------------------------------------
# Module source manifests — shared between the production object library
# (zyppng-objlib) and the test twin (zyppng-objlib-test).
#
# Both object libraries compile the *same* module sources. The test twin
# additionally attaches test impl units (added by zyppng_add_module_test()
# in zyppng/tests/) so that test code lives inside the module wall and has
# full visibility of every symbol — exported or not.
#
# Bottom-up dependency order matters for FILE_SET CXX_MODULES.
# ---------------------------------------------------------------------------
set( ZYPPNG_MODULE_INTERFACE_UNITS

      # ── Bridge wrapper partitions (LEGACY-BRIDGE) ─────────────────────
      # Each wraps a zypp:: type into zyppng:: until the legacy dep is cut.
      ng/arch.cppm
      ng/bit.cppm
      ng/capmatch.cppm
      ng/countrycode.cppm
      ng/cpeid.cppm
      ng/dep.cppm
      ng/edition.cppm
      ng/idstring.cppm
      ng/idstringtype.cppm
      ng/languagecode.cppm
      ng/locale.cppm
      ng/range.cppm
      ng/rel.cppm
      ng/relcompare.cppm
      ng/reskind.cppm
      ng/resolvernamespace.cppm
      ng/restraits.cppm

      # ── base/ leaf partitions ──────────────────────────────────────────
      ng/base/serialnumber.cppm
      ng/base/settracker.cppm
      ng/base/strmatcher.cppm

      # ── sat/ leaf partitions (strict bottom-up dependency order) ───────
      ng/sat/stringpool.cppm        # LEGACY-REQUIRED wrapper (thin GMF include)
      ng/sat/poolconstants.cppm
      ng/sat/solvattr.cppm
      ng/sat/poolmember.cppm
      ng/sat/capability.cppm
      ng/sat/capabilities.cppm
      ng/sat/queue.cppm
      ng/sat/solvable.cppm
      ng/sat/repository.cppm
      ng/sat/lookupattr.cppm
      ng/sat/lookupattrtools.cppm
      ng/sat/preparedpool.cppm
      ng/sat/solvableident.cppm
      ng/sat/solvablespec.cppm
      ng/sat/cap2str.cppm

      # ── sat/namespaces/ leaf partitions ───────────────────────────────
      ng/sat/namespaces/namespaceprovider.cppm
      ng/sat/namespaces/filesystem.cppm
      ng/sat/namespaces/language.cppm
      ng/sat/namespaces/modalias.cppm

      # ── sat/components/ leaf partitions ───────────────────────────────
      ng/sat/components/poolcomponents.cppm
      ng/sat/components/architecturecomponent.cppm
      ng/sat/components/autoinstalledcomponent.cppm
      ng/sat/components/namespacecomponent.cppm
      ng/sat/components/packagepolicycomponent.cppm

      # ── sat_pool (depends on primitives + poolcomponents base) ─────────
      ng/sat/pool.cppm

      # ── log/ leaf partitions ───────────────────────────────────────────
      ng/log/format.cppm
      ng/log/sat/capabilities.cppm
      ng/log/sat/capability.cppm
      ng/log/sat/lookupattr.cppm
      ng/log/sat/queue.cppm
      ng/log/sat/repository.cppm
      ng/log/sat/solvable.cppm
      ng/log/sat/solvablespec.cppm

      # ── Root-level partitions ─────────────────────────────────────────
      ng/context_private.cppm       # internal (non-exported)
      ng/applicationdata.cppm
      ng/context.cppm

      # ── Domain aggregator partitions (re-export their siblings) ───────
      ng/sat.cppm
      ng/log.cppm
      ng/base.cppm

      # ── Primary interface ──────────────────────────────────────────────
      ng/zyppng.cppm
)

set( ZYPPNG_MODULE_IMPL_UNITS
  ng/applicationdata.cc
  ng/context.cc
  ng/sat/cap2str.cc
  ng/sat/capabilities.cc
  ng/sat/capability.cc
  ng/sat/lookupattr.cc
  ng/sat/pool.cc
  ng/sat/preparedpool.cc
  ng/sat/queue.cc
  ng/sat/repository.cc
  ng/sat/solvable.cc
  ng/sat/solvableident.cc
  ng/sat/solvablespec.cc
  ng/sat/namespaces/filesystem.cc
  ng/sat/namespaces/language.cc
  ng/sat/namespaces/modalias.cc
  ng/sat/namespaces/namespaceprovider.cc
  ng/sat/components/architecturecomponent.cc
  ng/sat/components/autoinstalledcomponent.cc
  ng/sat/components/namespacecomponent.cc
  ng/sat/components/packagepolicycomponent.cc
  ng/sat/components/poolcomponents.cc
  ng/log/sat/capabilities.cc
  ng/log/sat/capability.cc
  ng/log/sat/lookupattr.cc
  ng/log/sat/queue.cc
  ng/log/sat/repository.cc
  ng/log/sat/solvable.cc
  ng/log/sat/solvablespec.cc
)

# Expose to the parent scope so zyppng_add_module_test() (defined further up
# in zyppng/tests/) can attach extra impl units to zyppng-objlib-test.
set( ZYPPNG_MODULE_INTERFACE_UNITS ${ZYPPNG_MODULE_INTERFACE_UNITS} PARENT_SCOPE )
set( ZYPPNG_MODULE_IMPL_UNITS      ${ZYPPNG_MODULE_IMPL_UNITS}      PARENT_SCOPE )

# ---------------------------------------------------------------------------
# zyppng-objlib: the production object library that backs the zyppng module.
#
# zypp_add_zypp_target() still creates the target and wires the legacy zypp/
# sources (Arch.cc, Edition.cc, etc.) plus the LEGACY-REQUIRED stringpool.cc.
# The ng/ module interface and implementation units are added below.
# ---------------------------------------------------------------------------
zypp_add_zypp_target( TARGETNAME zyppng-objlib FLAGS zyppng_lib_compiler_flags )

target_sources( zyppng-objlib
  PUBLIC
    FILE_SET CXX_MODULES FILES
      ${ZYPPNG_MODULE_INTERFACE_UNITS}
)

target_sources( zyppng-objlib PRIVATE
  ${ZYPPNG_MODULE_IMPL_UNITS}
)

# ---------------------------------------------------------------------------
# zyppng-objlib-test: the test twin of the module object library.
#
# Same module sources as zyppng-objlib so the BMI graph is identical, plus
# test impl units appended later from zyppng/tests/ via target_sources().
# Test code attached this way sits *inside* the module wall and sees every
# symbol — including detail::* and non-exported types — without re-export
# bookkeeping.
#
# Linked directly by zyppng test executables. Never reaches a production
# binary because libzypp-glib links the wrapping zyppng STATIC target which
# only references zyppng-objlib.
# ---------------------------------------------------------------------------
zypp_add_zypp_target( TARGETNAME zyppng-objlib-test FLAGS zyppng_lib_compiler_flags )

# Flips ZYPPNG_TEST_EXPORT (see ng/zypp_test_export.h) to `export`.
# Production target zyppng-objlib leaves the macro undefined, so the same
# source declarations stay module-internal there.
target_compile_definitions( zyppng-objlib-test PRIVATE ZYPPNG_TEST_BUILD=1 )

target_sources( zyppng-objlib-test
  PUBLIC
    FILE_SET CXX_MODULES FILES
      ${ZYPPNG_MODULE_INTERFACE_UNITS}
)

target_sources( zyppng-objlib-test PRIVATE
  ${ZYPPNG_MODULE_IMPL_UNITS}
)

# ---------------------------------------------------------------------------
# INTERFACE link propagation — shared system & sub-tier deps.
#
# Both object libraries need to propagate the same dependency surface to
# their consumers (the production zyppng wrapper, and test executables).
# Declared once via a helper macro to keep the two lists in lockstep.
#
# CAUTION: order matters at link time. zyppng-curl / -media must precede
# zyppng-core to avoid symbol-lookup errors at executable link time.
# ---------------------------------------------------------------------------
macro( zyppng_propagate_link_deps target )
  target_link_libraries( ${target} INTERFACE zyppng-curl )
  target_link_libraries( ${target} INTERFACE zyppng-media )
  target_link_libraries( ${target} INTERFACE zyppng-core )

  target_link_libraries( ${target} INTERFACE ${LibSolv_LIBRARIES} )
  target_link_libraries( ${target} INTERFACE ${LIBXML2_LIBRARIES} )
  target_link_libraries( ${target} INTERFACE ${RPM_LIBRARY} ${RPMIO_LIBRARY} )
  target_link_libraries( ${target} INTERFACE ${GETTEXT_LIBRARIES} )
  target_link_libraries( ${target} INTERFACE ${Boost_THREAD_LIBRARY} )
  target_link_libraries( ${target} INTERFACE ${YAML_CPP_LIBRARIES} )

  if( UDEV_FOUND )
    target_link_libraries( ${target} INTERFACE ${UDEV_LIBRARY} )
  elseif( HAL_FOUND )
    target_link_libraries( ${target} INTERFACE ${HAL_LIBRARY} ${HAL_STORAGE_LIBRARY} ${DBUS_LIBRARY} )
  endif()
endmacro()

zyppng_propagate_link_deps( zyppng-objlib      )
zyppng_propagate_link_deps( zyppng-objlib-test )
