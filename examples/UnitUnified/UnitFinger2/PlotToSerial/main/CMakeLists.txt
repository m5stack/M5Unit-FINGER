# Dependencies (M5Unit-FINGER, M5UnitUnified, M5Unified, ...) are pulled in via
# main/idf_component.yml and auto-added as requirements by the component manager.
# UnitFinger2 has a single unit/wiring variant, so no common/variant.cmake is included.
idf_component_register(
    SRCS "PlotToSerial.cpp"
    INCLUDE_DIRS ".")
