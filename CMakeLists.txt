cmake_minimum_required(VERSION 3.8)

project("fitsrt")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(FITSDK_SRC
	"fitsdk/fit_convert.h"
	"fitsdk/fit_crc.c"
	"fitsdk/fit_crc.h"
	"fitsdk/fit_example.c"
	"fitsdk/fit_example.h"
	"fitsdk/fit_product.h"
	"fitsdk/fit_ram.c"
	"fitsdk/fit_ram.h"
	"fitsdk/fit.c"
	"fitsdk/fit.h"
	"fitsdk/fit_config.h"
	"fitsdk/fit_convert.c"
	)

set(TARGET_SRC
	"fitsrt.cpp"
	)

execute_process(COMMAND echo "Run conan install...")
execute_process(COMMAND conan install . --build missing WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

include(${PROJECT_SOURCE_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable(${PROJECT_NAME} ${TARGET_SRC} ${FITSDK_SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE ${CONAN_LIBS})
