cmake_minimum_required(VERSION 3.25)

cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)

project(campus_ticketing LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CORE_SRC
    app/seed.cpp
    domain/datetime.cpp
    domain/enums.cpp
    infra/csv_util.cpp
    infra/data_store.cpp
    infra/security.cpp
    service/activity_service.cpp
    service/audit_service.cpp
    service/auth_service.cpp
    service/background_task_service.cpp
    service/notification_service.cpp
    service/registration_service.cpp
    service/role_service.cpp
    service/service_result.cpp
    service/session_service.cpp
    service/stats_service.cpp
    service/ticket_service.cpp)

add_library(campus_core STATIC ${CORE_SRC})
target_include_directories(campus_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(
  campus_ticketing
  main.cpp
  ui/console_io.cpp
  ui/console_view.cpp
  ui/console_view_admin.cpp
  ui/console_view_organizer.cpp)
target_link_libraries(campus_ticketing PRIVATE campus_core)

if(MSVC)
  target_compile_options(campus_core PRIVATE /utf-8 /W4)
  target_compile_options(campus_ticketing PRIVATE /utf-8 /W4)
else()
  target_compile_options(campus_core PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(campus_ticketing PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

if(BUILD_TESTING)
  add_executable(campus_tests tests/test_main.cpp)
  target_link_libraries(campus_tests PRIVATE campus_core)
  if(MSVC)
    target_compile_options(campus_tests PRIVATE /utf-8 /W4)
  else()
    target_compile_options(campus_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME campus_tests COMMAND campus_tests)
  add_executable(campus_uml_tests tests/test_uml.cpp ui/console_io.cpp
    ui/console_view.cpp ui/console_view_organizer.cpp ui/console_view_admin.cpp)
  target_link_libraries(campus_uml_tests PRIVATE campus_core)
  if(MSVC)
    target_compile_options(campus_uml_tests PRIVATE /utf-8 /W4)
  else()
    target_compile_options(campus_uml_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  foreach(n RANGE 1 35)
    add_test(NAME uml_${n} COMMAND campus_uml_tests ${n})
    set_tests_properties(uml_${n} PROPERTIES TIMEOUT 20)
  endforeach()
endif()

find_program(
  CLANG_FORMAT
  NAMES clang-format clang-format.exe
  HINTS
    "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin"
    "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/Professional/VC/Tools/Llvm/bin"
    "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/Enterprise/VC/Tools/Llvm/bin")
if(CLANG_FORMAT)
  file(
    GLOB_RECURSE FMT_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
  list(FILTER FMT_SRC EXCLUDE REGEX "/build/")
  add_custom_target(
    format
    COMMAND ${CLANG_FORMAT} -i -style=file ${FMT_SRC}
    VERBATIM)
  add_custom_target(
    format-check
    COMMAND ${CLANG_FORMAT} --dry-run --Werror -style=file ${FMT_SRC}
    VERBATIM)
endif()
