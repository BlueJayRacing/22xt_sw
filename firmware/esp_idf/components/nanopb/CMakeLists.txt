
set(NANOPB_SRC "${COMPONENT_DIR}/../../../nanopb/src")
set(NANOPB_INCLUDE "${COMPONENT_DIR}/../../../nanopb/include")

file(COPY ${NANOPB_SRC} DESTINATION ${COMPONENT_DIR})
file(COPY ${NANOPB_INCLUDE} DESTINATION ${COMPONENT_DIR})


idf_component_register( SRCS "nanopb.cpp"
                        INCLUDE_DIRS "include"
                        REQUIRES driver
                            )
