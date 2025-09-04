# ESP-IDF Project Extensions for OTA Deployment
# This file registers custom idf.py commands

# Define the OTA deployment command
function(idf_ota_flash)
    add_custom_target(ota_flash
        COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/tools/ota_deploy.py"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Deploying firmware via OTA"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_PROJECT_NAME}.bin"
    )
endfunction()

function(idf_ota_check)
    add_custom_target(ota_check
        COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/tools/ota_deploy.py" --check-only
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Checking for OTA updates"
    )
endfunction()

# Register the functions
idf_ota_flash()
idf_ota_check()