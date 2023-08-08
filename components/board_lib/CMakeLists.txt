idf_component_register(SRCS "board_lib.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver log freertos newlib esp_adc_cal esp_rom)
