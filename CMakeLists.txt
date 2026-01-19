cmake_minimum_required(VERSION 3.16)
project(ssh VERSION 10.1.2)

find_package(Albert REQUIRED)

albert_plugin(
    INCLUDE PRIVATE $<TARGET_PROPERTY:albert::applications,INTERFACE_INCLUDE_DIRECTORIES>
    QT Widgets
)
