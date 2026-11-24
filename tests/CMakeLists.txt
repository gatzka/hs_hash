option(HS_HASH_SANITIZE "Build the hs_hash tests with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(HS_HASH_WARNINGS_AS_ERRORS "Treat compiler warnings in the hs_hash tests as errors" OFF)

set(HS_HASH_HEADER "${PROJECT_SOURCE_DIR}/hs_hash/hs_hash.h")
set(HS_HASH_README "${PROJECT_SOURCE_DIR}/README.md")
# The examples and the split store header below are generated from these
# files, so CMake must run again when they change.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HS_HASH_HEADER}" "${HS_HASH_README}")

if(MSVC)
    set(HS_HASH_TEST_WARNINGS /W4)
    set(HS_HASH_STRICT_WARNINGS /W4)
    if(HS_HASH_WARNINGS_AS_ERRORS)
        list(APPEND HS_HASH_TEST_WARNINGS /WX)
        list(APPEND HS_HASH_STRICT_WARNINGS /WX)
    endif()
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(HS_HASH_TEST_WARNINGS -Wall -Wextra -Wpedantic -Wcast-qual -Wshadow)
    set(HS_HASH_STRICT_WARNINGS ${HS_HASH_TEST_WARNINGS}
        -Wconversion -Wsign-conversion -Wcast-align -Wstrict-prototypes -Wmissing-prototypes -Wundef
    )
    if(HS_HASH_WARNINGS_AS_ERRORS)
        list(APPEND HS_HASH_TEST_WARNINGS -Werror)
        list(APPEND HS_HASH_STRICT_WARNINGS -Werror)
    endif()
endif()

set(HS_HASH_SANITIZER_FLAGS "")
if(HS_HASH_SANITIZE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set(HS_HASH_SANITIZER_FLAGS -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    else()
        message(WARNING "HS_HASH_SANITIZE is only supported with GCC and Clang")
    endif()
endif()

# Adds a test program built as C99 without extensions, with the test
# warnings and sanitizers. It doesn't link hs_hash, so that the split
# store tests can use their own copy of the header.
function(hs_hash_add_test_program name)
    add_executable(${name} ${ARGN})
    set_target_properties(${name} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    target_compile_options(${name} PRIVATE ${HS_HASH_TEST_WARNINGS} ${HS_HASH_SANITIZER_FLAGS})
    target_link_options(${name} PRIVATE ${HS_HASH_SANITIZER_FLAGS})
endfunction()

# Unit tests and usage tests

hs_hash_add_test_program(unit unit.c)
target_link_libraries(unit PRIVATE hs_hash::hs_hash)
add_test(NAME unit COMMAND unit)

hs_hash_add_test_program(override override.c)
target_link_libraries(override PRIVATE hs_hash::hs_hash)
add_test(NAME override COMMAND override)

hs_hash_add_test_program(shared_table shared_table_a.c shared_table_b.c)
target_link_libraries(shared_table PRIVATE hs_hash::hs_hash)
add_test(NAME shared_table COMMAND shared_table)

# The usage examples of README.md and of the comment in hs_hash.h must
# compile and run.
file(READ "${HS_HASH_README}" readme)
string(FIND "${readme}" "```c\n" start)
if(start EQUAL -1)
    message(FATAL_ERROR "No C code block found in README.md")
endif()
math(EXPR start "${start} + 5")
string(SUBSTRING "${readme}" ${start} -1 readme)
string(FIND "${readme}" "\n```" end)
string(SUBSTRING "${readme}" 0 ${end} readme_example)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/readme_example.c.tmp" "${readme_example}\n")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/readme_example.c.tmp" "${CMAKE_CURRENT_BINARY_DIR}/readme_example.c" COPYONLY)

file(READ "${HS_HASH_HEADER}" header)
string(FIND "${header}" " * Example:\n" start)
string(FIND "${header}" " * The return values" end)
if(start EQUAL -1 OR end EQUAL -1)
    message(FATAL_ERROR "No usage example found in hs_hash.h")
endif()
math(EXPR start "${start} + 11")
math(EXPR length "${end} - ${start}")
string(SUBSTRING "${header}" ${start} ${length} header_example)
string(REGEX REPLACE "\n \\* ?" "\n" header_example "${header_example}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/header_example.c.tmp" "#include <hs_hash/hs_hash.h>\n${header_example}")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/header_example.c.tmp" "${CMAKE_CURRENT_BINARY_DIR}/header_example.c" COPYONLY)

foreach(example readme_example header_example)
    hs_hash_add_test_program(${example} "${CMAKE_CURRENT_BINARY_DIR}/${example}.c" example_main.c)
    target_link_libraries(${example} PRIVATE hs_hash::hs_hash)
    add_test(NAME ${example} COMMAND ${example})
endforeach()

# Compiles all of the header with strict warnings. Nothing is run.
add_library(warnings OBJECT warnings.c)
target_link_libraries(warnings PRIVATE hs_hash::hs_hash)
set_target_properties(warnings PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_options(warnings PRIVATE ${HS_HASH_STRICT_WARNINGS})

# Fuzzers with simulated interrupts

foreach(key UINT32 UINT64 STRING)
    string(TOLOWER ${key} key_lower)
    hs_hash_add_test_program(fuzz_${key_lower} fuzz.c)
    target_link_libraries(fuzz_${key_lower} PRIVATE hs_hash::hs_hash)
    target_compile_definitions(fuzz_${key_lower} PRIVATE FUZZ_KEY_${key})
    add_test(NAME fuzz_${key_lower} COMMAND fuzz_${key_lower} 20000 1)
endforeach()

# Same with a copy of the header in which every hop_info store is split
# into pieces, like on an 8 or 16 bit CPU.
set(split_header "${header}")
string(REGEX REPLACE
    "table\\[([A-Za-z_]+)\\]\\.hop_info = ([^;\n]+);"
    "HS_TEST_STORE_HOP(&table[\\1].hop_info, \\2);"
    split_header "${split_header}"
)
string(REGEX MATCHALL "HS_TEST_STORE_HOP" split_stores "${split_header}")
string(REGEX MATCHALL "\\.hop_info = " remaining_stores "${split_header}")
list(LENGTH split_stores split_store_count)
if(split_store_count EQUAL 0 OR remaining_stores)
    message(FATAL_ERROR "Can't instrument the hop_info stores in hs_hash.h for the split store tests")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/split/hs_hash/hs_hash.h.tmp" "${split_header}")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/split/hs_hash/hs_hash.h.tmp" "${CMAKE_CURRENT_BINARY_DIR}/split/hs_hash/hs_hash.h" COPYONLY)

foreach(variant "8bit;UINT32;1;0" "16bit;UINT64;2;1")
    list(GET variant 0 name)
    list(GET variant 1 key)
    list(GET variant 2 piece)
    list(GET variant 3 reverse)
    hs_hash_add_test_program(fuzz_split_${name} fuzz.c)
    target_include_directories(fuzz_split_${name} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/split")
    target_compile_definitions(fuzz_split_${name} PRIVATE
        FUZZ_KEY_${key} FUZZ_SPLIT_PIECE=${piece} FUZZ_SPLIT_REVERSE=${reverse}
    )
    add_test(NAME fuzz_split_${name} COMMAND fuzz_split_${name} 20000 2)
endforeach()

# Real asynchronous interrupts, POSIX only

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)
if(UNIX AND CMAKE_USE_PTHREADS_INIT)
    hs_hash_add_test_program(irq_signal irq_signal.c)
    target_link_libraries(irq_signal PRIVATE hs_hash::hs_hash Threads::Threads)
    add_test(NAME irq_signal COMMAND irq_signal 1)
endif()

# Declarations that must not compile. Each test builds a target that is
# excluded from the normal build and checks the error message.

get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(multi_config)
    set(config_args --config $<CONFIG>)
else()
    set(config_args "")
endif()

function(hs_hash_add_compile_fail_test name order values regex)
    add_library(compile_fail_${name} OBJECT EXCLUDE_FROM_ALL compile_fail.c)
    target_link_libraries(compile_fail_${name} PRIVATE hs_hash::hs_hash)
    target_compile_definitions(compile_fail_${name} PRIVATE
        COMPILE_FAIL_ORDER=${order} COMPILE_FAIL_VALUES=${values} ${ARGN}
    )
    add_test(NAME compile_fail_${name}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target compile_fail_${name} ${config_args}
    )
    # Several builds in the same build tree at once can conflict.
    set_tests_properties(compile_fail_${name} PROPERTIES
        PASS_REGULAR_EXPRESSION "${regex}"
        RESOURCE_LOCK hs_hash_build_tree
    )
endfunction()

# MSVC only reports error C2118 (negative subscript), without the name.
hs_hash_add_compile_fail_test(order_0 0 1 "hs_hash_order_of_X_must_be_between_1_and_31|C2118")
hs_hash_add_compile_fail_test(order_32 32 1 "hs_hash_order_of_X_must_be_between_1_and_31|C2118")
hs_hash_add_compile_fail_test(add_range_0 8 1 "HS_HASH_ADD_RANGE must be at least 1" HS_HASH_ADD_RANGE=0)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    hs_hash_add_compile_fail_test(too_large 31 1 "hs_hash_table_X_exceeds_max_object_size|C2118")
elseif(NOT MSVC)
    # MSVC rejects arrays above 2 GiB before the check is reached.
    hs_hash_add_compile_fail_test(too_large 31 1073741824u "hs_hash_table_X_exceeds_max_object_size")
endif()

set_tests_properties(fuzz_uint32 fuzz_uint64 fuzz_string fuzz_split_8bit fuzz_split_16bit PROPERTIES TIMEOUT 300)
