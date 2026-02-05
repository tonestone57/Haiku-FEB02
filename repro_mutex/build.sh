#!/bin/bash
g++ -c test_mutex.cpp \
    -iquote ../src/add-ons/kernel/drivers/disk/mmc \
    -iquote ../src/system/kernel/device_manager \
    -I ../headers/private/. \
    -I ../headers/private/kernel \
    -I ../headers/private/libroot     -I ../headers/private/shared     -I ../headers/private/kernel/boot/platform/bios_ia32     -I ../headers/private/kernel/arch/x86     -I ../headers/private/system     -I ../headers/private/system/arch/x86_64     -I ../headers/private/drivers     -I ../headers/glibc     -I ../headers/posix     -I ../headers     -I ../headers/os     -I ../headers/os/add-ons     -I ../headers/os/add-ons/file_system     -I ../headers/os/add-ons/graphics     -I ../headers/os/add-ons/input_server     -I ../headers/os/add-ons/registrar     -I ../headers/os/add-ons/screen_saver     -I ../headers/os/add-ons/tracker     -I ../headers/os/app     -I ../headers/os/device     -I ../headers/os/drivers     -I ../headers/os/game     -I ../headers/os/interface     -I ../headers/os/kernel     -I ../headers/os/locale     -I ../headers/os/media     -I ../headers/os/mail     -I ../headers/os/midi     -I ../headers/os/midi2     -I ../headers/os/net     -I ../headers/os/storage     -I ../headers/os/support     -I ../headers/os/translation     -D_KERNEL_MODE     -fsyntax-only
