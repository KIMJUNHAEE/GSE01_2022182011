#include "stdafx.h"

// cgltf uses portable C runtime APIs. Keep this exception local to the vendor code.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

#define CGLTF_IMPLEMENTATION
#include "Dependencies/cgltf.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif
