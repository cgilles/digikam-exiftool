#
# Copyright (c) 2010-2020 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

project(digikam-exiftool)

cmake_minimum_required(VERSION "3.3.2")

set(CMAKE_AUTOMOC ON)

find_package(Qt5 5.6.0
             NO_MODULE COMPONENTS
             Core
             Gui
)

add_executable(exiftooloutput_cli
               exiftooloutput_cli.cpp
               exiftoolparser.cpp
               exiftoolprocess.cpp
)

target_link_libraries(exiftooloutput_cli Qt5::Core Qt5::Gui)
