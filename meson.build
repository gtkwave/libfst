project(
    'libfst',
    'c',
    version: '1.0.0',
)

pkg = import('pkgconfig')
cc = meson.get_compiler('c')

add_project_arguments('-D_GNU_SOURCE', language: 'c')

# Dependencies

zlib_req = '>=1.2.0'
bzip2_req = '>=1.0.0'
glib_req = '>=2.64.0' # only used for tests

zlib_dep = dependency('zlib', version: zlib_req)

bzip2_dep = dependency('bzip2', version: bzip2_req, required: false)
if not bzip2_dep.found()
    # pkg-config files for bzip2 aren't installed on all distributions,
    # try to manually find the library as a fallback
    bzip2_dep = cc.find_library('bz2', has_headers: 'bzlib.h')
endif

glib_dep = dependency('glib-2.0', version: glib_req, required: false)

subdir('src')
subdir('tests')