#   Copyright 2016 Comcast Cable Communications Management, LLC
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

add_test(NAME TestSvcAgt 
 COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_svcagt
)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -W -g -fprofile-arcs -ftest-coverage -O0")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -W  -g -fprofile-arcs -ftest-coverage -O0")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage -O0")

add_executable(test_svcagt test_svcagt.c
 ../src/svc_agt.c
 ../src/svcagt_db.c
 ../src/svcagt_files.c
 ../src/svcagt_log.c
 ../src/svcagt_time.c
 ../src/svcagt_systemctl.c
 ../src/svcagt_startup_states.c
)

add_executable(mock_systemctl mock_systemctl.c)

target_link_libraries (mock_systemctl -lm)

add_executable(check_goal_states_file check_goal_states_file.c)

target_link_libraries (check_goal_states_file -lm)

target_link_libraries (test_svcagt gcov)
target_link_libraries (test_svcagt cunit)
target_link_libraries (test_svcagt -lm)

add_custom_target(coverage
                  COMMAND lcov -q --capture --directory ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_svcagt.dir/__/src --output-file coverage.info
                  COMMAND genhtml coverage.info
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
