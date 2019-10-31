function(enable_native_isa target_name)
	get_target_property(target_type ${target_name} TYPE)
	
	if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
		# MSVC does not give us an /march=native option so just enable the highest possible
		if (target_type MATCHES "INTERFACE_LIBRARY")
			target_compile_options(${target_name} INTERFACE "/arch:AVX2")
		else()
			target_compile_options(${target_name} PUBLIC "/arch:AVX2")
		endif()
	elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		message("target_type: ${target_type}")
		if (target_type MATCHES "INTERFACE_LIBRARY")
			target_compile_options(${target_name} INTERFACE -march=native)
		else()
			target_compile_options(${target_name} PUBLIC -march=native)
		endif()
	else()
		message(FATAL "Unknown C++ Compiler ID")
	endif()
endfunction(enable_native_isa)
