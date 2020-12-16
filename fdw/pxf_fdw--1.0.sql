/* fdw/pxf_fdw--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pxf_fdw" to load this file. \quit

CREATE FUNCTION pxf_fdw_handler()
RETURNS fdw_handler
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION pxf_fdw_validator(text[], oid)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION pxf_fdw_version()
RETURNS pg_catalog.text STRICT
AS 'MODULE_PATHNAME'
LANGUAGE C;

CREATE FOREIGN DATA WRAPPER jdbc_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'jdbc', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER hdfs_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'hdfs', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER hive_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'hive', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER hbase_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'hbase', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER s3_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 's3', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER gs_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'gs', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER adl_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'adl', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER wasbs_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'wasbs', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER file_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'file', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER demo_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'demo', mpp_execute 'all segments' );

COMMENT ON FOREIGN DATA WRAPPER jdbc_pxf_fdw
    IS 'PXF JDBC foreign data wrapper';

COMMENT ON FOREIGN DATA WRAPPER hdfs_pxf_fdw
    IS 'PXF HDFS foreign data wrapper';

COMMENT ON FOREIGN DATA WRAPPER hive_pxf_fdw
    IS 'PXF Hive foreign data wrapper';

COMMENT ON FOREIGN DATA WRAPPER hbase_pxf_fdw
    IS 'PXF HBase foreign data wrapper';

COMMENT ON FOREIGN DATA WRAPPER s3_pxf_fdw
    IS 'PXF AWS S3 foreign data wrapper';

COMMENT ON FOREIGN DATA WRAPPER gs_pxf_fdw
    IS 'PXF Google Cloud Storage (GS) foreign data wrapper';

COMMENT ON FOREIGN DATA WRAPPER adl_pxf_fdw
    IS 'PXF Azure Data Lake (ADL) foreign data wrapper';

COMMENT ON FOREIGN DATA WRAPPER wasbs_pxf_fdw
    IS 'PXF Windows Azure Storage Blob (WASB) foreign data wrapper';

COMMENT ON FOREIGN DATA WRAPPER file_pxf_fdw
    IS 'PXF File foreign data wrapper';

COMMENT ON FOREIGN DATA WRAPPER demo_pxf_fdw
    IS 'PXF Demo foreign data wrapper. Quickly test PXF without configuring any external access';

COMMENT ON FUNCTION pxf_fdw_version()
    IS 'Displays the version of pxf_fdw';
