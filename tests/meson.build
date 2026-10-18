libfst_tests = [
    'attr_enum_value_type',
    'chain_table_across_sections',
    'empty_file',
    'mask_no_variables',
    'packed_extent_wraps',
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

# The malformed file corpus is read from the source tree rather than written by
# the test, so the case file is handed to it as an argument.  One test per case,
# so that a file the reader does not survive names itself and stops nothing else.
malformed_cases = [
    'chain_shorter_than_its_length',
    'chain_table_overrun',
    'chain_table_pair_unterminated',
    'chain_table_unterminated',
    'chain_uncompresses_past_traversal',
    'chain_varint_pair_past_traversal',
    'geometry_short',
    'geometry_varint_unterminated',
    'frame_maxhandle_too_big',
    'frame_real_narrower_than_eight',
    'rvat_chain_table_overrun',
    'time_chain_empty',
    'time_chain_short',
    'traversal_buffer_smaller_than_chain',
    'traversal_buffer_smaller_than_chain_fastlz',
    'uncompressed_chain_past_traversal',
    'uncompressed_chain_shorter_than_its_varint',
    'value_change_past_traversal',
]

malformed = executable(
    'test-reader_malformed_files',
    'reader_malformed_files.c',
    dependencies: [libfst_dep, glib_dep],
)

foreach case : malformed_cases
    test(
        'malformed_' + case,
        malformed,
        args: [meson.current_source_dir() / 'data' / 'malformed' / (case + '.fst')],
        suite: 'malformed',
    )
endforeach
