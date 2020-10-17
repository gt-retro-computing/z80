add_executable(imsai imsai.cpp 8251Uart.cpp IODevice.cpp IODevice.h TMS5501.cpp TMS5501.h)

set_target_properties(imsai PROPERTIES COMPILE_FLAGS "-O0")
