# ESP-IDF component manifest for M5Unit-FINGER.
# Upstream: https://github.com/m5stack/M5Unit-FINGER
#
# UnitFPC1020A (UnitFinger / HatFinger / FacesFinger) and UnitFinger2 over UART.
# FacesFinger drives the M-Bus panel/touch power pins via the ESP-IDF native driver/gpio API.
file(GLOB_RECURSE SRCS "src/*.cpp")
idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "src"
    REQUIRES M5UnitUnified
    PRIV_REQUIRES M5Utility driver
)
