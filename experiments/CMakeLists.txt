cmake_minimum_required(VERSION 3.2)

project(experiments)

add_exe(toy_experiment ${CMAKE_CURRENT_SOURCE_DIR}/toy_experiment.cpp)
add_exe(ddgi_experiment ${CMAKE_CURRENT_SOURCE_DIR}/ddgi_experiment.cpp)
add_exe(ao_experiment ${CMAKE_CURRENT_SOURCE_DIR}/ao_experiment.cpp)
add_exe(specular_experiment ${CMAKE_CURRENT_SOURCE_DIR}/specular_experiment.cpp)