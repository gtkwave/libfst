libfst_sources = [
    'fstapi.c',
    'lz4.c',
]

libfst_headers = [
    'fstapi.h',
]

libfst_dependencies = [
    zlib_dep,
    bzip2_dep,
    thread_dep,
]

# fastlz.c intentionally do unaligned writes, which trigger sanitizers
libfastlz = static_library(
    'fastlz',
    'fastlz.c',
    c_args: cc.get_supported_arguments('-fno-sanitize=alignment'),
    pic: true,
)

libfst = library(
    'fst',
    sources: libfst_sources,
    dependencies: libfst_dependencies,
    link_whole: libfastlz,
    version: meson.project_version(),
    install: true,
    include_directories: config_inc,
)

libfst_dep = declare_dependency(
    link_with: libfst,
    include_directories: '.',
)

meson.override_dependency('libfst', libfst_dep)

install_headers(libfst_headers, subdir: 'libfst')

pkg.generate(
    name: 'libfst',
    libraries: libfst,
    description: 'FST reader and writer library',
    subdirs: 'libfst',
)
