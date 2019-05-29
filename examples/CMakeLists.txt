
function(writer_exe programfile)
	add_executable(${programfile}.exe ${programfile}.c)
	target_link_libraries(${programfile}.exe adios2::adios2 
	                                       MPI::MPI_C)
    set_property(TARGET ${programfile}.exe PROPERTY LINKER_LANGUAGE CXX)
endfunction()

writer_exe(writer_weak_scaling)

