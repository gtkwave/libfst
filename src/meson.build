libfst_sources = [
    'fastlz.c',
    'fstapi.c',
    'lz4.c',
]

libfst_headers = [
    'fstapi.h',
]

libfst_dependencies = [
    zlib_dep,
    bzip2_dep,
    # thread_dep,
]

libfst = library(
    'fst',
    sources: libfst_sources,
    dependencies: libfst_dependencies,
    version: meson.project_version(),
    install: true,
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