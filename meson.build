project('AsterOS', ['c'],
  version: '0.1.0',
  default_options: [
    'c_std=c23',
    'warning_level=3',
    'b_lundef=false',
    'b_pie=false',
  ]
)

kernel_name = 'asteros_kernel'
kernel_elf = 'kernel.elf'

src_dir = meson.project_source_root() / 'src'
include_dir = meson.project_source_root() / 'include'

limine_dir = meson.project_source_root() / 'limine'
flanterm_dir = meson.project_source_root() / 'Flanterm'

iso_dir = meson.project_build_root() / 'iso_root'
iso_path = meson.project_build_root() / 'asteros.iso'

disk_img = meson.project_source_root() / 'disk.img'
disk_size = '64M'

kernel_c_flags = [
  '-m64',
  '-ffreestanding',
  '-fno-builtin',
  '-fno-stack-protector',
  '-fno-pic',
  '-fno-pie',
  '-fno-omit-frame-pointer',
  '-mno-red-zone',
  '-mcmodel=kernel',
  '-mgeneral-regs-only',
  '-Wall',
  '-Wextra',
  '-Wpedantic',
  '-O2',
]

kernel_link_flags = [
  '-m64',
  '-nostdlib',
  '-static',
  '-no-pie',
  '-z', 'max-page-size=0x1000',
  '-T', meson.project_source_root() / 'linker.ld',
]

kernel_inc = include_directories(
  'include',
  'include/aster',
  'limine',
  'Flanterm/src',
)

#
# version.h
#

version_header = custom_target('version_header',
  output: 'version.stamp',
  command: [
    'sh', '-c',
    'cd @0@ && ./makever.sh && touch @OUTPUT@'.format(meson.project_source_root())
  ],
  build_by_default: true,
)

#
# C sources
#

kernel_c_sources = []

find_c_sources = run_command(
  'sh', '-c',
  'find src -name "*.c" ! -path "src/user/shell_user.c" | sort',
  check: true,
)

foreach f : find_c_sources.stdout().strip().split('\n')
  if f != ''
    kernel_c_sources += files(f)
  endif
endforeach

kernel_c_sources += files(
  'Flanterm/src/flanterm.c',
  'Flanterm/src/flanterm_backends/fb.c',
)

#
# font.psf -> font.o
#

font_obj = custom_target('font_o',
  input: 'assets/font.psf',
  output: 'font.o',
  command: [
    'sh', '-c',
    'rm -f font.psf && ' +
    'cp @INPUT@ font.psf && ' +
    'objcopy -I binary -O elf64-x86-64 -B i386:x86-64 font.psf @OUTPUT@'
  ],
)

#
# usershell.elf
#

usershell_bin = executable('usershell_bin',
  files('src/user/shell_user.c'),
  include_directories: include_directories(
    'include',
    'include/aster',
  ),
  c_args: [
    '-m64',
    '-ffreestanding',
    '-fno-builtin',
    '-fno-stack-protector',
    '-fno-pic',
    '-fno-pie',
    '-fno-omit-frame-pointer',
    '-mno-red-zone',
    '-mcmodel=large',
    '-mgeneral-regs-only',
    '-Wall',
    '-Wextra',
    '-Wpedantic',
    '-O2',
    '-fno-asynchronous-unwind-tables',
    '-fno-unwind-tables',
  ],
  link_args: [
    '-m64',
    '-nostdlib',
    '-static',
    '-no-pie',
    '-z', 'max-page-size=0x1000',
    '-T', meson.project_source_root() / 'usershell.ld',
  ],
  name_suffix: 'elf',
  install: false,
)

usershell_blob = custom_target('usershell_blob_o',
  input: usershell_bin,
  output: 'usershell_blob.o',
  command: [
    'sh', '-c',
    'rm -f usershell.bin && ' +
    'cp @INPUT@ usershell.bin && ' +
    'objcopy -I binary -O elf64-x86-64 -B i386:x86-64 usershell.bin @OUTPUT@'
  ],
)

#
# NASM sources -> .o
#

nasm_objects = []

find_asm_sources = run_command(
  'sh', '-c',
  'find src/boot src/arch/x86_64 -name "*.asm" 2>/dev/null | sort',
  check: true,
)

foreach asm_path : find_asm_sources.stdout().strip().split('\n')
  if asm_path != ''
    obj_name = asm_path.replace('/', '_').replace('.asm', '.o')

    nasm_obj = custom_target('nasm_' + obj_name,
      input: asm_path,
      output: obj_name,
      command: [
        'nasm',
        '-f', 'elf64',
        '-o', '@OUTPUT@',
        '@INPUT@',
      ],
    )

    nasm_objects += nasm_obj
  endif
endforeach

#
# kernel.elf
#

kernel = executable(kernel_name,
  kernel_c_sources,
  nasm_objects,
  font_obj,
  usershell_blob,
  include_directories: kernel_inc,
  c_args: kernel_c_flags,
  link_args: kernel_link_flags,
  name_suffix: 'elf',
  install: false,
)

#
# disk.img
#

run_target('disk-img',
  command: [
    'sh', '-c',
    '''
set -e

if [ ! -f "@0@" ]; then
  qemu-img create -f raw "@0@" "@1@"
else
  echo "disk image already exists: @0@"
fi
'''.format(disk_img, disk_size)
  ],
)

#
# limine
#

run_target('limine',
  command: [
    'make',
    '-C',
    limine_dir,
  ],
)

#
# ISO
#

iso_target = custom_target('iso',
  output: 'asteros.iso',
  input: kernel,
  command: [
    'sh', '-c',
    '''
set -e

ISO_DIR="@0@"
SRC_ROOT="@1@"
LIMINE_DIR="@2@"
KERNEL="@INPUT@"
ISO_PATH="@OUTPUT@"

make -C "$LIMINE_DIR"

rm -rf "$ISO_DIR"

mkdir -p "$ISO_DIR/boot/limine"
mkdir -p "$ISO_DIR/EFI/BOOT"

cp "$KERNEL" "$ISO_DIR/boot/kernel.elf"
cp "$SRC_ROOT/limine.conf" "$ISO_DIR/boot/limine/limine.conf"

cp "$LIMINE_DIR/limine-bios.sys" "$ISO_DIR/boot/limine/limine-bios.sys"
cp "$LIMINE_DIR/limine-bios-cd.bin" "$ISO_DIR/boot/limine/limine-bios-cd.bin"
cp "$LIMINE_DIR/limine-uefi-cd.bin" "$ISO_DIR/boot/limine/limine-uefi-cd.bin"
cp "$LIMINE_DIR/BOOTX64.EFI" "$ISO_DIR/EFI/BOOT/BOOTX64.EFI"

xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label "$ISO_DIR" -o "$ISO_PATH"

"$LIMINE_DIR/limine" bios-install "$ISO_PATH"
'''.format(
      iso_dir,
      meson.project_source_root(),
      limine_dir,
    )
  ],
  depends: [
    version_header,
  ],
  build_by_default: true,
)

#
# run
#

run_target('run',
  command: [
    'sh', '-c',
    '''
set -e

ninja -C "@0@"

if [ ! -f "@1@" ]; then
  qemu-img create -f raw "@1@" "@2@"
fi

qemu-system-x86_64 -cdrom "@0@/asteros.iso" -drive file="@1@",format=raw,if=ide,index=0,media=disk -m 256M -serial stdio -vga std -d int,cpu_reset -D "@0@/qemu.log" -no-reboot -no-shutdown
'''.format(meson.project_build_root(), disk_img, disk_size)
  ],
)

run_target('run-uefi',
  command: [
    'sh', '-c',
    '''
set -e

ninja -C "@0@"

if [ ! -f "@1@" ]; then
  qemu-img create -f raw "@1@" "@2@"
fi

qemu-system-x86_64 -bios /usr/share/ovmf/x64/OVMF.fd -cdrom "@0@/asteros.iso" -drive file="@1@",format=raw,if=ide,index=0,media=disk -m 256M -serial stdio -vga std -no-reboot -no-shutdown
'''.format(meson.project_build_root(), disk_img, disk_size)
  ],
)

run_target('debug',
  command: [
    'sh', '-c',
    '''
set -e

ninja -C "@0@"

if [ ! -f "@1@" ]; then
  qemu-img create -f raw "@1@" "@2@"
fi

qemu-system-x86_64 -cdrom "@0@/asteros.iso" -drive file="@1@",format=raw,if=ide,index=0,media=disk -m 256M -serial stdio -vga std -s -S -no-reboot -no-shutdown
'''.format(meson.project_build_root(), disk_img, disk_size)
  ],
)

#
# clean helpers
#

run_target('clean-iso',
  command: [
    'sh', '-c',
    'rm -rf "@0@" "@1@" "@2@/qemu.log"'.format(
      iso_dir,
      iso_path,
      meson.project_build_root(),
    )
  ],
)

run_target('clean-disk',
  command: [
    'sh', '-c',
    'rm -f "@0@"'.format(disk_img)
  ],
)

run_target('clean-all',
  command: [
    'sh', '-c',
    'rm -rf "@0@" "@1@" "@2@/qemu.log" "@3@"'.format(
      iso_dir,
      iso_path,
      meson.project_build_root(),
      disk_img,
    )
  ],
)
