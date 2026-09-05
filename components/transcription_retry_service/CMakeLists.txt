idf_component_register(
    SRCS "transcription_retry_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer gemini_service recording_archive_service recording_service transcription_service
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
