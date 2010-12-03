# Backends
if(BACKEND_ALPM)
    add_subdirectory(alpm)
endif(BACKEND_ALPM)

if(BACKEND_APT)
    add_subdirectory(apt)
endif(BACKEND_APT)

if(BACKEND_APTCC)
    message(STATUS " APTCC ------")
    add_subdirectory(aptcc)
endif(BACKEND_APTCC)

if(BACKEND_BOX)
    add_subdirectory(box)
endif(BACKEND_BOX)

if(BACKEND_CONARY)
    add_subdirectory(conary)
endif(BACKEND_CONARY)

if(BACKEND_DUMMY)
    add_subdirectory(dummy)
endif(BACKEND_DUMMY)

if(BACKEND_ENTROPY)
    add_subdirectory(entropy)
endif(BACKEND_ENTROPY)

if(BACKEND_OPKG)
    add_subdirectory(opkg)
endif(BACKEND_OPKG)

if(BACKEND_RAZOR)
    add_subdirectory(razor)
endif(BACKEND_RAZOR)

if(BACKEND_PACMAN)
    add_subdirectory(pacman)
endif(BACKEND_PACMAN)

if(BACKEND_PISI)
    add_subdirectory(pisi)
endif(BACKEND_PISI)

if(BACKEND_POLDEK)
    add_subdirectory(poldek)
endif(BACKEND_POLDEK)

if(BACKEND_PORTAGE)
    add_subdirectory(portage)
endif(BACKEND_PORTAGE)

if(BACKEND_PORTS)
    add_subdirectory(ports)
endif(BACKEND_PORTS)

if(BACKEND_SLAPT)
    add_subdirectory(slapt)
endif(BACKEND_SLAPT)

if(BACKEND_SMART)
    add_subdirectory(smart)
endif(BACKEND_SMART)

if(BACKEND_URPMI)
    add_subdirectory(urpmi)
endif(BACKEND_URPMI)

if(BACKEND_YUM)
    add_subdirectory(yum)
endif(BACKEND_YUM)

if(BACKEND_ZYPP)
    add_subdirectory(zypp)
endif(BACKEND_ZYPP)
