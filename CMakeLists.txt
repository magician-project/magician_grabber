cmake_minimum_required(VERSION 3.5)
project(rclcpp_magician_grabber)

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--copy-dt-needed-entries -O2 -fPIC -fPIE -Wall -Wno-unused-function")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--copy-dt-needed-entries -O2 -fPIC -fPIE -Wall -Wno-unused-function")


find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
 
find_package(PkgConfig REQUIRED)
pkg_check_modules(ARAVIS REQUIRED aravis-0.10)


set(SRC 
ros_magician_grabber.cpp 
arduinoSensor.c
atiForceSensor.c
gigeCameraSensor.c
sharedMemoryVideoBuffers.c
imageStreamer.c
tactileFeatures.c
)

set(TACTILE_SRC 
tactile_processor/TactileProcessor.cpp
)

add_executable(magician_grabber ${SRC} ) #${TACTILE_SRC} 
ament_target_dependencies(magician_grabber rclcpp std_msgs)

#target_compile_definitions(magician_grabber PRIVATE TACTILE TACTILE_LIBRARY)
target_include_directories(magician_grabber PRIVATE ${ARAVIS_INCLUDE_DIRS})
target_link_libraries(magician_grabber ${ARAVIS_LIBRARIES} pthread m) #fftw3 

install(TARGETS
  magician_grabber
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
