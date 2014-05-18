// acpi.h

#pragma once
#include "includes.h"
extern "C"{
    #include "acpica/platform/acenv.h"
    #include "acpica/actypes.h"
    #include "acpica/accommon.h"
    #include "acpica/acpixf.h"
    #include "acpica/acexcep.h"
}


extern void initialize_acpi();