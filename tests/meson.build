libfst_tests = [
    'empty_file',
    'mask_no_variables',
    'write_and_read',
]

foreach test : libfst_tests
    exe = executable(
        'test-' + test,
        test + '.c',
        dependencies: [libfst_dep, glib_dep],
    )

    test(test, exe)
endforeach
