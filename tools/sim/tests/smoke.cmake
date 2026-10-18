# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

# Smoke test for montezuma_sim: determinism (same arguments, same report)
# and exit codes. Run by CTest with -DSIM, -DEXAMPLES and -DWORK.
file(MAKE_DIRECTORY ${WORK})

function(run_sim out_var code_var)
  execute_process(COMMAND ${SIM} ${ARGN} OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code)
  set(${out_var} "${out}" PARENT_SCOPE)
  set(${code_var} "${code}" PARENT_SCOPE)
endfunction()

foreach(fmt table csv json)
  run_sim(first code1 --seeds 4 --format ${fmt} ${EXAMPLES}/e1-l03.json)
  run_sim(second code2 --seeds 4 --format ${fmt} ${EXAMPLES})
  if(NOT code1 EQUAL 0 OR NOT code2 EQUAL 0)
    message(FATAL_ERROR "sim exited ${code1}/${code2} for --format ${fmt}")
  endif()
  if(NOT first STREQUAL second)
    message(FATAL_ERROR "sim reports differ between two identical runs (${fmt})")
  endif()
  if(first STREQUAL "")
    message(FATAL_ERROR "sim printed nothing (${fmt})")
  endif()
endforeach()

# Bot list order does not change a bot's numbers.
run_sim(a code --seeds 3 --bots greedy --difficulty normal --format csv ${EXAMPLES})
run_sim(b code --seeds 3 --bots planner,greedy --difficulty both --format csv ${EXAMPLES})
string(REGEX MATCH "e1-l03,greedy,normal[^\n]*" row_a "${a}")
string(REGEX MATCH "e1-l03,greedy,normal[^\n]*" row_b "${b}")
if(row_a STREQUAL "" OR NOT row_a STREQUAL row_b)
  message(FATAL_ERROR "greedy/normal row depends on the bot or difficulty list:\n${row_a}\n${row_b}")
endif()

# Exit codes: usage and bad input are 2.
run_sim(out code)
if(NOT code EQUAL 2)
  message(FATAL_ERROR "no input: expected exit 2, got ${code}")
endif()
foreach(bad "--seeds;0" "--bots;smart" "--difficulty;easy" "--upgrades;some" "--format;xml")
  run_sim(out code ${bad} ${EXAMPLES})
  if(NOT code EQUAL 2)
    message(FATAL_ERROR "${bad}: expected exit 2, got ${code}")
  endif()
endforeach()
file(WRITE ${WORK}/bad.json "{\"id\": \"e1-l01\"}")
run_sim(out code ${WORK}/bad.json)
if(NOT code EQUAL 2)
  message(FATAL_ERROR "invalid level: expected exit 2, got ${code}")
endif()
