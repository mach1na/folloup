idf_component_register(
    SRCS "topic_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES json storage_service
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
