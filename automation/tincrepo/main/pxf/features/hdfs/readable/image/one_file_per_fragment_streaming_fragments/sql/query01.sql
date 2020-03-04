-- @description query01 for PXF HDFS Readable images

SELECT image_test_one_file_per_fragment_streaming_fragments.names, image_test_one_file_per_fragment_streaming_fragments.directories FROM image_test_one_file_per_fragment_streaming_fragments, compare_table_one_file_per_fragment_streaming_fragments
    WHERE image_test_one_file_per_fragment_streaming_fragments.fullpaths       = compare_table_one_file_per_fragment_streaming_fragments.fullpaths
    AND image_test_one_file_per_fragment_streaming_fragments.directories       = compare_table_one_file_per_fragment_streaming_fragments.directories
    AND image_test_one_file_per_fragment_streaming_fragments.names             = compare_table_one_file_per_fragment_streaming_fragments.names
    AND image_test_one_file_per_fragment_streaming_fragments.dimensions        = compare_table_one_file_per_fragment_streaming_fragments.dimensions
    AND image_test_one_file_per_fragment_streaming_fragments.images            = compare_table_one_file_per_fragment_streaming_fragments.images
    ORDER BY names, directories;
SELECT image_test_one_file_per_fragment_streaming_fragments_bytea.names, image_test_one_file_per_fragment_streaming_fragments_bytea.directories FROM image_test_one_file_per_fragment_streaming_fragments_bytea, compare_table_one_file_per_fragment_streaming_fragments_bytea
    WHERE image_test_one_file_per_fragment_streaming_fragments_bytea.fullpaths = compare_table_one_file_per_fragment_streaming_fragments_bytea.fullpaths
    AND image_test_one_file_per_fragment_streaming_fragments_bytea.directories = compare_table_one_file_per_fragment_streaming_fragments_bytea.directories
    AND image_test_one_file_per_fragment_streaming_fragments_bytea.names       = compare_table_one_file_per_fragment_streaming_fragments_bytea.names
    AND image_test_one_file_per_fragment_streaming_fragments_bytea.dimensions  = compare_table_one_file_per_fragment_streaming_fragments_bytea.dimensions
    AND image_test_one_file_per_fragment_streaming_fragments_bytea.images      = compare_table_one_file_per_fragment_streaming_fragments_bytea.images
    ORDER BY names, directories;
