enable_testing()

set( TEST_NAME TestCommon )

set( ${TEST_NAME}_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/BaseTest.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/Common.hpp
)
set( ${TEST_NAME}_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/BaseTest.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Common.cpp
)

add_library( ${TEST_NAME}
	OBJECT
	${${TEST_NAME}_HEADER_FILES}
	${${TEST_NAME}_SOURCE_FILES}
)
target_include_directories( ${TEST_NAME} PUBLIC
	${IncludeDirs}
)
target_compile_options( ${TEST_NAME} PUBLIC
	${CompileOptions}
)
target_compile_definitions( ${TEST_NAME} PUBLIC
	${CompileDefinitions}
)
set_target_properties( ${TEST_NAME} PROPERTIES
	CXX_STANDARD 17
	FOLDER "Tests"
)

file( GLOB TEST_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/Test*.cpp
)

foreach ( TEST_FILE ${TEST_FILES} )
	get_filename_component( TEST_NAME ${TEST_FILE} NAME_WE )
	add_executable( ${TEST_NAME}
		$<TARGET_OBJECTS:TestCommon>
		${TEST_FILE}
	)
	target_compile_options( ${TEST_NAME} PRIVATE
		${CompileOptions}
	)
	target_compile_definitions( ${TEST_NAME} PRIVATE
		${CompileDefinitions}
	)
	target_include_directories( ${TEST_NAME} PRIVATE
		${IncludeDirs}
	)
	target_link_libraries( ${TEST_NAME} PRIVATE
		${LinkLibraries}
	)
	set_target_properties( ${TEST_NAME} PROPERTIES
		CXX_STANDARD 17
		FOLDER "Tests"
	)
	add_test(
		NAME ${TEST_NAME}
		COMMAND ${TEST_NAME}
	)
	install(
		TARGETS ${TEST_NAME}
		COMPONENT ${TEST_NAME}
		CONFIGURATIONS Release
		EXPORT ${TEST_NAME}
		RUNTIME DESTINATION bin
	)
	install(
		TARGETS ${TEST_NAME}
		COMPONENT ${TEST_NAME}_dev
		CONFIGURATIONS RelWithDebInfo
		EXPORT ${TEST_NAME}
		RUNTIME DESTINATION bin/RelWithDebInfo
	)
	install(
		TARGETS ${TEST_NAME}
		COMPONENT ${TEST_NAME}_dev
		CONFIGURATIONS Debug
		EXPORT ${TEST_NAME}
		RUNTIME DESTINATION bin/Debug
	)
endforeach ()
