# Dependencies (M5Unit-FINGER, M5UnitUnified, M5Unified, ...) are pulled in via
# main/idf_component.yml and auto-added as requirements by the component manager.
# UnitFinger2 has a single unit/wiring variant, so no common/variant.cmake is included.
# template_data.cpp lives at the example root (sibling of main/) and defines the template_data[]
# blob that User.cpp references via extern; compile it here too (PlatformIO/Arduino pick it up by
# building the whole example directory).
idf_component_register(
    SRCS "User.cpp" "../template_data.cpp"
    INCLUDE_DIRS ".")
