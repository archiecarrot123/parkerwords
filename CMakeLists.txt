cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)


set(CMAKE_CXX_FLAGS "-Wall -Wextra -O3")

project(parkerwords)

add_executable(parkerwords parkerwords.cpp)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/words_alpha.txt
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
