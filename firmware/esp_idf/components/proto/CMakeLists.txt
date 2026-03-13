find_program(PROTOC_PATH protoc REQUIRED)
# gets the path for protoc
if(NOT PROTOC_PATH)
    message(FATAL_ERROR "protoc not found! Install protobuf compiler.")
endif()

message(STATUS "Using protoc at: ${PROTOC_PATH}")

set(PROTO_PATH "${COMPONENT_DIR}/../../../../proto")
set(NANOPB_PATH "${COMPONENT_DIR}/../../../nanopb")

message(STATUS "Resolved NANOPB_PATH: ${NANOPB_PATH}")
file(COPY ${NANOPB_PATH}/generator/proto/nanopb.proto DESTINATION ${PROTO_PATH}/)
message(STATUS "Proto path at: ${PROTO_PATH}")
# this is the path to the proto folder in the 22xt_sw folder not the component
set(PROTO_OUT_PATH "${COMPONENT_DIR}/src")
message(STATUS "Proto out path at: ${PROTO_OUT_PATH}")
# this is the component

list(APPEND CMAKE_MODULE_PATH "${COMPONENT_DIR}/cmake")
include(Functions)
# gets the functions.cmake file

get_directory_files(
    DIRECTORY ${PROTO_PATH}
    OUTPUT_VARIABLE PROTO_FILES
    PATTERN "*.proto"
)
#finds all the files in PROTO_PATH with .proto and puts that into PROTO_FILEs

message(STATUS "Proto files at: ${PROTO_FILES}")
add_prefix_postfix(FULL_PATH_PROTO_FILES "${PROTO_FILES}"
    PREFIX  "${PROTO_PATH}/"
)
# 
message(STATUS "Proto files at: ${FULL_PATH_PROTO_FILES}")


remove_prefix_postfix("${PROTO_FILES}" "" ".proto" PROTO_NAMES)
message(STATUS "Proto files at: ${PROTO_FILES}")


add_prefix_postfix(C_PROTO_FILES "${PROTO_NAMES}"
    PREFIX  "${PROTO_OUT_PATH}/"
    POSTFIX ".pb.cc"
)
message(STATUS "C Proto files at: ${C_PROTO_FILES}")

message(STATUS "Component dir: ${COMPONENT_DIR}")

clean_directory_preserve("${COMPONENT_DIR}/src")
write_files("${C_PROTO_FILES}" "${PROTO_FILES}" "${PROTO_OUT_PATH}")

idf_component_register(SRCS ${C_PROTO_FILES}
                        INCLUDE_DIRS "src"
                        REQUIRES esp_system nanopb
                        )


# Define the path to your nanopb generator folder
set(NANOPB_GENERATOR_PATH "${NANOPB_PATH}/generator")

add_custom_command(OUTPUT ${C_PROTO_FILES}
     COMMAND ${PROTOC_PATH} 
             --proto_path=${PROTO_PATH} 
             --cpp_out=${PROTO_OUT_PATH} 
             -I/opt/homebrew/bin/protoc/
             ${FULL_PATH_PROTO_FILES}
     DEPENDS ${FULL_PATH_PROTO_FILES}
     VERBATIM)

    #  add_custom_command(OUTPUT ${C_PROTO_FILES}
    #  COMMAND ${PROTOC_PATH} 
    #          --proto_path=${PROTO_PATH} 
    #          --cpp_out=${PROTO_OUT_PATH} 
    #          ${FULL_PATH_PROTO_FILES}
    #  DEPENDS ${FULL_PATH_PROTO_FILES}
    #  VERBATIM)

add_custom_target(protos DEPENDS ${C_PROTO_FILES})
add_dependencies(${COMPONENT_LIB} protos)

set_property(DIRECTORY "${COMPONENT_DIR}" APPEND PROPERTY
          ADDITIONAL_CLEAN_FILES ${C_PROTO_FILES})

message(STATUS "Finished with cmake?")
