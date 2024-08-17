cmake_minimum_required(VERSION 3.10)
project(your_project_name)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g")

# Add your source files here
set(SOURCE_FILES server.c request.c segel.c client.c request_queue.c output.c)

# Create executables
add_executable(server server.c request.c segel.c request_queue.c)
target_link_libraries(server pthread)

add_executable(client client.c segel.c)

add_executable(output.cgi output.c)
