cmake_minimum_required(VERSION 3.20)

project(AsterOS C ASM_NASM)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(KERNEL_NAME asteros_kernel)
set(KERNEL_ELF kernel.elf)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

set(LIMINE_DIR ${CMAKE_SOURCE_DIR}/limine)
set(FLANTERM_DIR ${CMAKE_SOURCE_DIR}/Flanterm)

# Keep a lowercase compatibility path for older generated build rules.
# Some cached Makefile artifacts still reference ${CMAKE_SOURCE_DIR}/flanterm.
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/flanterm AND EXISTS ${FLANTERM_DIR})
  execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${FLANTERM_DIR} ${CMAKE_SOURCE_DIR}/flanterm
    )
endif()

set(ISO_DIR ${CMAKE_BINARY_DIR}/iso_root)
set(ISO_PATH ${CMAKE_BINARY_DIR}/asteros.iso)

set(DISK_IMG ${CMAKE_SOURCE_DIR}/disk.img)
set(DISK_SIZE 64M)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

set(KERNEL_C_FLAGS
    -m64
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-pic
    -fno-pie
    -fno-omit-frame-pointer
    -mno-red-zone
    -mcmodel=kernel
    -mgeneral-regs-only
    -Wall
    -Wextra
    -Wpedantic
    -O2
)

set(KERNEL_LINK_FLAGS
    -m64
    -nostdlib
    -static
    -no-pie
    -z max-page-size=0x1000
    -T ${CMAKE_SOURCE_DIR}/linker.ld
)

# Optional PSF font object.
set(FONT_SRC ${CMAKE_SOURCE_DIR}/assets/font.psf)
set(FONT_BUILD ${CMAKE_BINARY_DIR}/font.psf)
set(FONT_OBJ ${CMAKE_BINARY_DIR}/font.o)

add_custom_command(
    OUTPUT ${FONT_OBJ}
    COMMAND ${CMAKE_COMMAND} -E copy ${FONT_SRC} ${FONT_BUILD}
    COMMAND objcopy
        -I binary
        -O elf64-x86-64
        -B i386:x86-64
        font.psf
        font.o
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS ${FONT_SRC}
    COMMENT "Embedding PSF font"
)

file(GLOB_RECURSE KERNEL_C_SOURCES CONFIGURE_DEPENDS
    ${SRC_DIR}/*.c
)

list(APPEND KERNEL_C_SOURCES
    ${FLANTERM_DIR}/src/flanterm.c
    ${FLANTERM_DIR}/src/flanterm_backends/fb.c
)

list(REMOVE_DUPLICATES KERNEL_C_SOURCES)

file(GLOB_RECURSE KERNEL_NASM_SOURCES CONFIGURE_DEPENDS
    ${SRC_DIR}/boot/*.asm
    ${SRC_DIR}/arch/x86_64/*.asm
    ${SRC_DIR}/arch/x86_64/**/*.asm
)

list(REMOVE_DUPLICATES KERNEL_NASM_SOURCES)

set(NASM_OBJECT_FILES)

foreach(asm_file ${KERNEL_NASM_SOURCES})
  file(RELATIVE_PATH asm_rel ${CMAKE_SOURCE_DIR} ${asm_file})
  string(REPLACE "/" "_" obj_name ${asm_rel})
  string(REPLACE ".asm" ".o" obj_name ${obj_name})

  set(obj_file "${CMAKE_BINARY_DIR}/asm/${obj_name}")

  add_custom_command(
        OUTPUT ${obj_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/asm
        COMMAND nasm -f elf64 -o ${obj_file} ${asm_file}
        DEPENDS ${asm_file}
        COMMENT "Building NASM object ${obj_file}"
    )

  list(APPEND NASM_OBJECT_FILES ${obj_file})
endforeach()

add_custom_target(version_header
    COMMAND ${CMAKE_SOURCE_DIR}/makever.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating version.h"
)

add_custom_command(
    OUTPUT ${DISK_IMG}
    COMMAND qemu-img create -f raw ${DISK_IMG} ${DISK_SIZE}
    COMMENT "Creating ATA disk image: ${DISK_IMG}"
)

add_custom_target(disk-img
    DEPENDS ${DISK_IMG}
)

add_executable(${KERNEL_NAME}
    ${KERNEL_C_SOURCES}
    ${NASM_OBJECT_FILES}
    ${FONT_OBJ}
)

add_dependencies(${KERNEL_NAME} version_header)

target_include_directories(${KERNEL_NAME} PRIVATE
    ${INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/aster
    ${LIMINE_DIR}
    ${FLANTERM_DIR}/src
)

target_compile_options(${KERNEL_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:${KERNEL_C_FLAGS}>
)

target_link_options(${KERNEL_NAME} PRIVATE
    ${KERNEL_LINK_FLAGS}
)

set_target_properties(${KERNEL_NAME} PROPERTIES
    OUTPUT_NAME ${KERNEL_ELF}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Safe source listing.
string(REPLACE ";" "\n" KERNEL_C_SOURCES_TEXT "${KERNEL_C_SOURCES}")
string(REPLACE ";" "\n" KERNEL_NASM_SOURCES_TEXT "${KERNEL_NASM_SOURCES}")

file(WRITE ${CMAKE_BINARY_DIR}/sources.txt
"C sources:
${KERNEL_C_SOURCES_TEXT}

NASM sources:
${KERNEL_NASM_SOURCES_TEXT}
"
)

add_custom_target(print-sources
    COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_BINARY_DIR}/sources.txt
)

add_custom_target(limine
    COMMAND make -C ${LIMINE_DIR}
    COMMENT "Building Limine"
)

add_custom_target(iso
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${ISO_DIR}

    COMMAND ${CMAKE_COMMAND} -E make_directory ${ISO_DIR}/boot
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ISO_DIR}/boot/limine
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ISO_DIR}/EFI/BOOT

    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${KERNEL_NAME}>
        ${ISO_DIR}/boot/${KERNEL_ELF}

    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/limine.conf
        ${ISO_DIR}/boot/limine/limine.conf

    COMMAND ${CMAKE_COMMAND} -E copy
        ${LIMINE_DIR}/limine-bios.sys
        ${ISO_DIR}/boot/limine/limine-bios.sys

    COMMAND ${CMAKE_COMMAND} -E copy
        ${LIMINE_DIR}/limine-bios-cd.bin
        ${ISO_DIR}/boot/limine/limine-bios-cd.bin

    COMMAND ${CMAKE_COMMAND} -E copy
        ${LIMINE_DIR}/limine-uefi-cd.bin
        ${ISO_DIR}/boot/limine/limine-uefi-cd.bin

    COMMAND ${CMAKE_COMMAND} -E copy
        ${LIMINE_DIR}/BOOTX64.EFI
        ${ISO_DIR}/EFI/BOOT/BOOTX64.EFI

    COMMAND xorriso -as mkisofs
        -b boot/limine/limine-bios-cd.bin
        -no-emul-boot
        -boot-load-size 4
        -boot-info-table
        --efi-boot boot/limine/limine-uefi-cd.bin
        -efi-boot-part
        --efi-boot-image
        --protective-msdos-label
        ${ISO_DIR}
        -o ${ISO_PATH}

    COMMAND ${LIMINE_DIR}/limine bios-install ${ISO_PATH}

    DEPENDS ${KERNEL_NAME} limine
    COMMENT "Building Limine ISO: ${ISO_PATH}"
)

add_custom_target(run
    COMMAND qemu-system-x86_64
        -cdrom ${ISO_PATH}
        -drive file=${DISK_IMG},format=raw,if=ide,index=0,media=disk
        -m 256M
        -serial stdio
        -vga std
        -d int,cpu_reset
        -D ${CMAKE_BINARY_DIR}/qemu.log
        -no-reboot
        -no-shutdown
    DEPENDS iso disk-img
    COMMENT "Running AsterOS x86_64"
)

add_custom_target(run-uefi
    COMMAND qemu-system-x86_64
        -bios /usr/share/ovmf/x64/OVMF.fd
        -cdrom ${ISO_PATH}
        -drive file=${DISK_IMG},format=raw,if=ide,index=0,media=disk
        -m 256M
        -serial stdio
        -vga std
        -no-reboot
        -no-shutdown
    DEPENDS iso disk-img
    COMMENT "Running AsterOS with Limine UEFI"
)

add_custom_target(debug
    COMMAND qemu-system-x86_64
        -cdrom ${ISO_PATH}
        -drive file=${DISK_IMG},format=raw,if=ide,index=0,media=disk
        -m 256M
        -serial stdio
        -vga std
        -s
        -S
        -no-reboot
        -no-shutdown
    DEPENDS iso disk-img
    COMMENT "Running AsterOS debug mode"
)

add_custom_target(clean-iso
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${ISO_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove -f ${ISO_PATH}
    COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/qemu.log
    COMMENT "Cleaning ISO files"
)

add_custom_target(clean-disk
    COMMAND ${CMAKE_COMMAND} -E remove -f ${DISK_IMG}
    COMMENT "Removing disk image"
)

add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${ISO_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove -f ${ISO_PATH}
    COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/qemu.log
    COMMAND ${CMAKE_COMMAND} -E remove -f ${DISK_IMG}
    COMMENT "Cleaning ISO, QEMU log and disk image"
)
