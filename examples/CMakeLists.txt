add_executable(hello_world hello_world.cc)
add_executable(phil_test phil_test.cc)
target_link_libraries(hello_world PRIVATE DataFrame)
target_link_libraries(phil_test PRIVATE DataFrame)
hmdf_target_data_files(hello_world
    DATA_FILES
        ${PROJECT_SOURCE_DIR}/data/IBM.csv
        ${PROJECT_SOURCE_DIR}/data/DT_IBM.csv
        ${PROJECT_SOURCE_DIR}/data/DT_AAPL.csv
)
## add_test(NAME hello_world COMMAND hello_world)
