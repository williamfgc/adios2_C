
function(make_exe programfile)
	add_executable(${programfile}.exe ${programfile}.c)
	target_link_libraries(${programfile}.exe adios2::adios2 
	                                       MPI::MPI_C)
    set_property(TARGET ${programfile}.exe PROPERTY LINKER_LANGUAGE CXX)
endfunction()

make_exe(writer_weak_scaling)
make_exe(reader_weak_scaling)

