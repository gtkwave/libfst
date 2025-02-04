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
    # include_directories: config_inc,
    install: true,
    version: meson.project_version(),
)

libfst_dep = declare_dependency(
    link_with: libfst,
    include_directories: '.',
)

install_headers(libfst_headers, subdir: 'libfst')