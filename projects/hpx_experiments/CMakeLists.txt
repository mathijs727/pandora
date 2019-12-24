add_executable(hpx_experiments "main.cpp")
target_compile_features(hpx_experiments PUBLIC cxx_std_17)

target_link_libraries(hpx_experiments
	PRIVATE
		GSL
		hpx hpx_init iostreams_component)