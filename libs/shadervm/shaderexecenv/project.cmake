set(shaderexecenv_srcs
	shadeops_bake3d.cpp
	shadeops_bake3d_diffuse.cpp
	shadeops_bake3d_nondiffuse.cpp
	shadeops_indirect.cpp
	shadeops_texture3d.cpp
	shadeops_indirectdiffuse.cpp
	shadeops_comp.cpp
	shadeops_deriv.cpp
	shadeops_dso.cpp
	shadeops_illum.cpp
	shadeops_inter.cpp
	shadeops_math.cpp
	shadeops_matrx.cpp
	shadeops_rand.cpp
	shadeops_text.cpp
	shadeops_tmap.cpp
	shaderexecenv.cpp
)
make_absolute(shaderexecenv_srcs ${shaderexecenv_SOURCE_DIR})

set(shaderexecenv_hdrs
	shaderexecenv.h
)
make_absolute(shaderexecenv_hdrs ${shaderexecenv_SOURCE_DIR})

include_directories(${shaderexecenv_SOURCE_DIR})
