/*
 * pxf_option.c
 *		  Foreign-data wrapper option handling for PXF (Platform Extension Framework)
 *
 * IDENTIFICATION
 *		  fdw/pxf_option.c
 */

#include "postgres.h"

#include <curl/curl.h>

#include "pxf_fdw.h"
#include "pxf_option.h"

#include "access/reloptions.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_user_mapping.h"
#include "commands/copy.h"
#include "commands/defrem.h"
#include "nodes/makefuncs.h"
#include "foreign/foreign.h"

#define ENV_PXF_HOST "PXF_HOST"
#define ENV_PXF_PORT "PXF_PORT"

#define FDW_OPTION_WIRE_FORMAT_TEXT "text"
#define FDW_OPTION_WIRE_FORMAT_CSV "csv"
#define FDW_OPTION_WIRE_FORMAT_BINARY "binary"

#define FDW_OPTION_REJECT_LIMIT_ROWS "rows"
#define FDW_OPTION_REJECT_LIMIT_PERCENT "percent"

#define FDW_OPTION_CONFIG "config"
#define FDW_OPTION_DISABLE_PPD "disable_ppd"
#define FDW_OPTION_FORMAT "format"
#define FDW_OPTION_LOG_ERRORS "log_errors"
#define FDW_OPTION_MPP_EXECUTE "mpp_execute"
#define FDW_OPTION_PROTOCOL "protocol"
#define FDW_OPTION_PXF_HOST "pxf_host"
#define FDW_OPTION_PXF_PORT "pxf_port"
#define FDW_OPTION_PXF_PROTOCOL "pxf_protocol"
#define FDW_OPTION_REJECT_LIMIT "reject_limit"
#define FDW_OPTION_REJECT_LIMIT_TYPE "reject_limit_type"
#define FDW_OPTION_RESOURCE "resource"

/*
 * SSL Options
 */
#define FDW_OPTION_SSL_VERBOSE "ssl_verbose"
#define FDW_OPTION_SSL_DISABLE_VERIFICATION "ssl_disable_verification"
#define FDW_OPTION_SSL_CLIENT_CERT_PATH "ssl_client_cert_path"
#define FDW_OPTION_SSL_PRIVATE_KEY_PASSWORD "ssl_private_key_password"
#define FDW_OPTION_SSL_PRIVATE_KEY_PATH "ssl_private_key_path"
#define FDW_OPTION_SSL_TRUSTED_CA_PATH "ssl_trusted_ca_path"
#define FDW_OPTION_SSL_VERSION "ssl_version"

#define FDW_OPTION_SSL_VERSION_DEFAULT "default"
#define FDW_OPTION_SSL_VERSION_TLSv1 "TLSv1"
#define FDW_OPTION_SSL_VERSION_SSLv2 "SSLv2"
#define FDW_OPTION_SSL_VERSION_SSLv3 "SSLv3"
#define FDW_OPTION_SSL_VERSION_TLSv1_0 "TLSv1.0"
#define FDW_OPTION_SSL_VERSION_TLSv1_1 "TLSv1.1"
#define FDW_OPTION_SSL_VERSION_TLSv1_2 "TLSv1.2"
#define FDW_OPTION_SSL_VERSION_TLSv1_3 "TLSv1.3"

/*
 * Format Options
 */
#define FDW_COPY_OPTION_FORMAT "format"
#define FDW_COPY_OPTION_HEADER "header"
#define FDW_COPY_OPTION_DELIMITER "delimiter"
#define FDW_COPY_OPTION_QUOTE "quote"
#define FDW_COPY_OPTION_ESCAPE "escape"
#define FDW_COPY_OPTION_NULL "null"
#define FDW_COPY_OPTION_ENCODING "encoding"
#define FDW_COPY_OPTION_NEWLINE "newline"
#define FDW_COPY_OPTION_FILL_MISSING_FIELDS "fill_missing_fields"
#define FDW_COPY_OPTION_FORCE_NOT_NULL "force_not_null"
#define FDW_COPY_OPTION_FORCE_NULL "force_null"

/*
 * Describes the valid copy options for objects that use this wrapper.
 */
struct PxfFdwOption
{
	const char *optname;
	Oid			optcontext;		/* Oid of catalog in which option may appear */
};

static const struct PxfFdwOption valid_options[] = {
	{FDW_OPTION_PROTOCOL, ForeignDataWrapperRelationId},
	{FDW_OPTION_RESOURCE, ForeignTableRelationId},
	{FDW_OPTION_FORMAT, ForeignTableRelationId},
	{FDW_OPTION_CONFIG, ForeignServerRelationId},

	/* Error handling */
	{FDW_OPTION_REJECT_LIMIT, ForeignTableRelationId},
	{FDW_OPTION_REJECT_LIMIT_TYPE, ForeignTableRelationId},
	{FDW_OPTION_LOG_ERRORS, ForeignTableRelationId},

	/* Sentinel */
	{NULL, InvalidOid}
};

struct PxfFdwSslVersionOption
{
	const char *optname;
	int			version;		/* Value for the given version */
};

static const struct PxfFdwSslVersionOption valid_ssl_version_options[] = {
	{FDW_OPTION_SSL_VERSION_DEFAULT, CURL_SSLVERSION_DEFAULT},
	{FDW_OPTION_SSL_VERSION_TLSv1, CURL_SSLVERSION_TLSv1},
	{FDW_OPTION_SSL_VERSION_SSLv2, CURL_SSLVERSION_SSLv2},
	{FDW_OPTION_SSL_VERSION_SSLv3, CURL_SSLVERSION_SSLv3},
	{FDW_OPTION_SSL_VERSION_TLSv1_0, CURL_SSLVERSION_TLSv1_0},
	{FDW_OPTION_SSL_VERSION_TLSv1_1, CURL_SSLVERSION_TLSv1_1},
	{FDW_OPTION_SSL_VERSION_TLSv1_2, CURL_SSLVERSION_TLSv1_2},
	{FDW_OPTION_SSL_VERSION_TLSv1_3, CURL_SSLVERSION_TLSv1_3},

	/* Sentinel */
	{NULL, -1}
};

/*
 * Valid COPY options for *_pxf_fdw.
 * These options are based on the options for the COPY FROM command.
 * But note that force_not_null and force_null are handled as boolean options
 * attached to a column, not as table options.
 *
 * Note: If you are adding new option for user mapping, you need to modify
 * fileGetOptions(), which currently doesn't bother to look at user mappings.
 */
static const struct PxfFdwOption valid_copy_options[] = {
	/* Format options */
	/* oids option is not supported */
	/* freeze option is not supported */
	{FDW_OPTION_FORMAT, ForeignTableRelationId},
	{FDW_COPY_OPTION_HEADER, ForeignTableRelationId},
	{FDW_COPY_OPTION_DELIMITER, ForeignTableRelationId},
	{FDW_COPY_OPTION_QUOTE, ForeignTableRelationId},
	{FDW_COPY_OPTION_ESCAPE, ForeignTableRelationId},
	{FDW_COPY_OPTION_NULL, ForeignTableRelationId},
	{FDW_COPY_OPTION_ENCODING, ForeignTableRelationId},
	{FDW_COPY_OPTION_NEWLINE, ForeignTableRelationId},
	{FDW_COPY_OPTION_FILL_MISSING_FIELDS, ForeignTableRelationId},
	{FDW_COPY_OPTION_FORCE_NOT_NULL, AttributeRelationId},
	{FDW_COPY_OPTION_FORCE_NULL, AttributeRelationId},

	/* Sentinel */
	{NULL, InvalidOid}
};

extern Datum pxf_fdw_validator(PG_FUNCTION_ARGS);

/*
 * Helper functions
 */
static Datum ValidateCopyOptions(List *options_list, Oid catalog);

static bool IsCopyOption(const char *option);

static bool IsValidCopyOption(const char *option, Oid context);

static void ValidateOption(char *, Oid);

/*
 * Validate the generic options given to a FOREIGN DATA WRAPPER, SERVER,
 * USER MAPPING or FOREIGN TABLE that uses file_fdw.
 *
 * Raise an ERROR if the option or its value is considered invalid.
 *
 */
Datum
pxf_fdw_validator(PG_FUNCTION_ARGS)
{
	char		*endptr = NULL;
	char		*port_str = NULL;
	char		*protocol = NULL;
	char		*resource = NULL;
	char		*reject_limit_type = FDW_OPTION_REJECT_LIMIT_ROWS;
	bool		log_errors_set = false;
	List		*options_list = untransformRelOptions(PG_GETARG_DATUM(0));
	Oid			catalog = PG_GETARG_OID(1);
	List		*copy_options = NIL;
	ListCell	*cell;
	int		reject_limit = -1,
				pxf_port;

	// TODO: validate SSL options

	foreach(cell, options_list)
	{
		DefElem    *def = (DefElem *) lfirst(cell);

		/*
		 * check whether option is valid at it's catalog level, if not valid,
		 * error out
		 */
		ValidateOption(def->defname, catalog);

		if (strcmp(def->defname, FDW_OPTION_PROTOCOL) == 0)
			protocol = defGetString(def);
		else if (strcmp(def->defname, FDW_OPTION_RESOURCE) == 0)
			resource = defGetString(def);
		else if (strcmp(def->defname, FDW_OPTION_MPP_EXECUTE) == 0)
		{
			if (catalog == UserMappingRelationId)
				ereport(ERROR,
						(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
						 errmsg("the %s option cannot be defined at the user mapping level",
								FDW_OPTION_MPP_EXECUTE)));
		}
		else if (strcmp(def->defname, FDW_OPTION_FORMAT) == 0)
		{
			/*
			 * Format option in PXF is different from the COPY format option.
			 * In PXF, format refers to the file format on the external
			 * system, for example Parquet, Avro, Text, CSV.
			 *
			 * For COPY, the format can only be text, csv, or binary. pxf_fdw
			 * leverages the csv and text formats in COPY.
			 */
			char	   *value = defGetString(def);

			if (pg_strcasecmp(value, FDW_OPTION_WIRE_FORMAT_TEXT) == 0 ||
				pg_strcasecmp(value, FDW_OPTION_WIRE_FORMAT_CSV) == 0)
				copy_options = lappend(copy_options, def);
		}
		else if (strcmp(def->defname, FDW_OPTION_PXF_PORT) == 0)
		{
			if (catalog == UserMappingRelationId)
				ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
						errmsg("the %s option cannot be defined at the user mapping level",
							FDW_OPTION_PXF_PORT)));

			if (catalog == ForeignTableRelationId)
				ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
						errmsg("the %s option cannot be defined at the foreign table level",
							FDW_OPTION_PXF_PORT)));

			port_str = defGetString(def);

			/*
			 * Validate port number
			 */
			if (port_str)
			{
				pxf_port = (int) strtol(port_str, &endptr, 10);

				if (port_str == endptr)
				{
					ereport(ERROR, (errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
						errmsg("unable to parse pxf_port number '%s'", port_str)));
				}

				if (pxf_port < 0 || pxf_port > 65535)
				{
					ereport(ERROR, (errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
						errmsg("invalid pxf_port number: %d. valid pxf_port numbers are 0 to 65535", pxf_port)));
				}
			}
		}
		else if (strcmp(def->defname, FDW_OPTION_PXF_HOST) == 0)
		{
			if (catalog == UserMappingRelationId)
				ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
						errmsg("the %s option cannot be defined at the user mapping level",
							FDW_OPTION_PXF_HOST)));

			if (catalog == ForeignTableRelationId)
				ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
						errmsg("the %s option cannot be defined at the foreign table level",
							FDW_OPTION_PXF_HOST)));
		}
		else if (strcmp(def->defname, FDW_OPTION_PXF_PROTOCOL) == 0)
		{
			char *pxf_protocol = defGetString(def);

			if (catalog == UserMappingRelationId)
				ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
						errmsg("the %s option cannot be defined at the user mapping level",
							FDW_OPTION_PXF_PROTOCOL)));

			if (catalog == ForeignTableRelationId)
				ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
						errmsg("the %s option cannot be defined at the foreign table level",
							FDW_OPTION_PXF_PROTOCOL)));

			if (!IsValidPxfProtocolValue(&pxf_protocol, NULL, PGC_S_DEFAULT))
				ereport(ERROR, (errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
					errmsg("invalid pxf_protocol value '%s'. valid pxf_protocol values are 'http' or 'https'", pxf_protocol)));
		}
		else if (strcmp(def->defname, FDW_OPTION_REJECT_LIMIT) == 0)
		{
			char	   *pStr = defGetString(def);

			reject_limit = (int) strtol(pStr, &endptr, 10);

			if (pStr == endptr || reject_limit < 1)
				ereport(ERROR,
						(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
						 errmsg("invalid %s value '%s', should be a positive integer", FDW_OPTION_REJECT_LIMIT, pStr)));
		}
		else if (strcmp(def->defname, FDW_OPTION_REJECT_LIMIT_TYPE) == 0)
		{
			reject_limit_type = defGetString(def);
			if (pg_strcasecmp(reject_limit_type,
							  FDW_OPTION_REJECT_LIMIT_ROWS) != 0 &&
				pg_strcasecmp(reject_limit_type,
							  FDW_OPTION_REJECT_LIMIT_PERCENT) != 0)
				ereport(ERROR,
						(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
						 errmsg("invalid %s value, only '%s' and '%s' are supported",
								FDW_OPTION_REJECT_LIMIT_TYPE,
								FDW_OPTION_REJECT_LIMIT_ROWS,
								FDW_OPTION_REJECT_LIMIT_PERCENT)));
		}
		else if (strcmp(def->defname, FDW_OPTION_LOG_ERRORS) == 0)
		{
			(void) defGetBoolean(def); /* call is required for validation */
			log_errors_set = true;
		}
		else if (strcmp(def->defname, FDW_OPTION_DISABLE_PPD) == 0)
		{
			(void) defGetBoolean(def); /* call is required for validation */

			if (catalog == UserMappingRelationId)
				ereport(ERROR,
						(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
								errmsg("the %s option cannot be defined at the user mapping level",
									   FDW_OPTION_DISABLE_PPD)));

			if (catalog == ForeignDataWrapperRelationId)
				ereport(ERROR,
						(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
								errmsg("the %s option cannot be defined at the foreign-data wrapper level",
									   FDW_OPTION_DISABLE_PPD)));
		}
		else if (IsCopyOption(def->defname))
			copy_options = lappend(copy_options, def);
	}

	if (catalog == ForeignDataWrapperRelationId &&
		(protocol == NULL || strcmp(protocol, "") == 0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_DYNAMIC_PARAMETER_VALUE_NEEDED),
				 errmsg("the %s option must be defined for PXF foreign-data wrappers", FDW_OPTION_PROTOCOL)));
	}

	if (catalog == ForeignTableRelationId &&
		(resource == NULL || strcmp(resource, "") == 0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_DYNAMIC_PARAMETER_VALUE_NEEDED),
				 errmsg("the %s option must be defined at the foreign table level", FDW_OPTION_RESOURCE)));
	}

	/* Validate reject limit */
	if (reject_limit != -1)
	{
		if (pg_strcasecmp(reject_limit_type, FDW_OPTION_REJECT_LIMIT_ROWS) == 0)
		{
			if (reject_limit < 2)
				ereport(ERROR,
						(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
						 errmsg("invalid (ROWS) %s value '%d', valid values are 2 or larger",
								FDW_OPTION_REJECT_LIMIT,
								reject_limit)));
		}
		else
		{
			if (reject_limit < 1 || reject_limit > 100)
				ereport(ERROR,
						(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
						 errmsg("invalid (PERCENT) %s value '%d', valid values are 1 to 100",
								FDW_OPTION_REJECT_LIMIT,
								reject_limit)));
		}
	}
	else
	{
		if (log_errors_set)
			ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
					 errmsg("the %s option cannot be set without reject_limit", FDW_OPTION_LOG_ERRORS)));
	}

	/*
	 * Additional validations for Copy options
	 */
	ValidateCopyOptions(copy_options, catalog);

	PG_RETURN_VOID();
}

/*
 * Validate options for the Copy command. Postgresql has its own validation
 * for the copy options. We only do special validation for force_not_null
 * and force_null, because they are set at the attribute level.
 */
Datum
ValidateCopyOptions(List *options_list, Oid catalog)
{
	DefElem    *force_not_null = NULL;
	DefElem    *force_null = NULL;
	List	   *copy_options = NIL;
	ListCell   *cell;

	/*
	 * Check that only options supported by copy, and allowed for the current
	 * object type, are given.
	 */
	foreach(cell, options_list)
	{
		DefElem    *def = (DefElem *) lfirst(cell);

		if (!IsValidCopyOption(def->defname, catalog))
		{
			const struct PxfFdwOption *opt;
			StringInfoData buf;

			/*
			 * Unknown option specified, complain about it. Provide a hint
			 * with list of valid options for the object.
			 */
			initStringInfo(&buf);
			for (opt = valid_copy_options; opt->optname; opt++)
			{
				if (catalog == opt->optcontext)
					appendStringInfo(&buf, "%s%s", (buf.len > 0) ? ", " : "",
									 opt->optname);
			}

			ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
					 errmsg("invalid option \"%s\"", def->defname),
					 buf.len > 0
					 ? errhint("Valid options in this context are: %s", buf.data)
					 : errhint("There are no valid options in this context.")));
		}

		/*
		 * force_not_null is a boolean option; after validation we can discard
		 * it - it will be retrieved later in PxfGetOptions()
		 */
		if (strcmp(def->defname, FDW_COPY_OPTION_FORCE_NOT_NULL) == 0)
		{
			if (force_not_null)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 errhint("option \"%s\" supplied more than once for a column", FDW_COPY_OPTION_FORCE_NOT_NULL)));
			force_not_null = def;
			/* Don't care what the value is, as long as it's a legal boolean */
			(void) defGetBoolean(def);
		}
		/* See comments for force_not_null above */
		else if (strcmp(def->defname, FDW_COPY_OPTION_FORCE_NULL) == 0)
		{
			if (force_null)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 errhint("option \"%s\" supplied more than once for a column", FDW_COPY_OPTION_FORCE_NULL)));
			force_null = def;
			(void) defGetBoolean(def);
		}
		else
			copy_options = lappend(copy_options, def);
	}

	/*
	 * Apply the core COPY code's validation logic for more checks.
	 */
#if PG_VERSION_NUM >= 90600
	ProcessCopyOptions(NULL, NULL, true, copy_options);
#else
	ProcessCopyOptions(NULL, true, copy_options, 0, true);
#endif

	PG_RETURN_VOID();
}

/*
 * Fetch the options for a pxf_fdw foreign table.
 */
PxfOptions *
PxfGetOptions(Oid foreigntableid)
{
	char			*port_str = NULL,
				*host_str = NULL,
				*ssl_version = NULL;
	Node			*wireFormat;
	UserMapping		*user;
	ForeignTable		*table;
	ForeignServer		*server;
	ForeignDataWrapper	*wrapper;
	List			*options;
	PxfOptions		*opt;
	ListCell		*lc;
	List			*copy_options,
				*other_options,
				*other_option_name_strings = NULL;

	opt = (PxfOptions *) palloc(sizeof(PxfOptions));
	memset(opt, 0, sizeof(PxfOptions));

	copy_options = NIL;
	other_options = NIL;

	opt->reject_limit = -1;
	opt->is_reject_limit_rows = true;
	opt->log_errors = false;

	opt->ssl_options = (PxfSSLOptions *) palloc(sizeof(PxfSSLOptions));
	memset(opt->ssl_options, 0, sizeof(PxfSSLOptions));

	/*
	 * Get the port and host strings from the environment variables
	 * This is for backwards compatibility to support migrations from the PXF
	 * external table code, which read the pxf_port and pxf_host from
	 * environment variables
	 */
	port_str = getenv(ENV_PXF_PORT);
	host_str = getenv(ENV_PXF_HOST);

	if (port_str)
		opt->pxf_port = atoi(port_str);

	if (host_str)
		opt->pxf_host = host_str;

	/*
	 * Extract options from FDW objects.
	 */
	table = GetForeignTable(foreigntableid);
	server = GetForeignServer(table->serverid);
	user = GetUserMapping(GetUserId(), server->serverid);
	wrapper = GetForeignDataWrapper(server->fdwid);

	options = NIL;
	/* order matters here for precedence enforcement */
	options = list_concat(options, table->options);
	options = list_concat(options, user->options);
	options = list_concat(options, server->options);
	options = list_concat(options, wrapper->options);

	/* Loop through the options, and get the server/port */
	foreach(lc, options)
	{
		DefElem    *def = (DefElem *) lfirst(lc);

		if (strcmp(def->defname, FDW_OPTION_PXF_HOST) == 0)
			opt->pxf_host = defGetString(def);
		else if (strcmp(def->defname, FDW_OPTION_PXF_PORT) == 0)
			opt->pxf_port = atoi(defGetString(def));
		else if (strcmp(def->defname, FDW_OPTION_PXF_PROTOCOL) == 0)
			opt->pxf_protocol = defGetString(def);
		else if (strcmp(def->defname, FDW_OPTION_PROTOCOL) == 0)
			opt->protocol = defGetString(def);
		else if (strcmp(def->defname, FDW_OPTION_RESOURCE) == 0)
			opt->resource = defGetString(def);
		else if (strcmp(def->defname, FDW_OPTION_REJECT_LIMIT) == 0)
			opt->reject_limit = atoi(defGetString(def));
		else if (strcmp(def->defname, FDW_OPTION_REJECT_LIMIT_TYPE) == 0)
			opt->is_reject_limit_rows = pg_strcasecmp(FDW_OPTION_REJECT_LIMIT_ROWS, defGetString(def)) == 0;
		else if (strcmp(def->defname, FDW_OPTION_LOG_ERRORS) == 0)
			opt->log_errors = defGetBoolean(def);
		else if (strcmp(def->defname, FDW_OPTION_DISABLE_PPD) == 0)
			opt->disable_ppd = defGetBoolean(def);
		else if (strcmp(def->defname, FDW_OPTION_FORMAT) == 0)
			opt->format = defGetString(def);
		else if (strcmp(def->defname, FDW_OPTION_SSL_VERBOSE) == 0)
			opt->ssl_options->verbose = defGetBoolean(def);
		else if (strcmp(def->defname, FDW_OPTION_SSL_DISABLE_VERIFICATION) == 0)
			opt->ssl_options->disable_verification = defGetBoolean(def);
		else if (strcmp(def->defname, FDW_OPTION_SSL_CLIENT_CERT_PATH) == 0)
			opt->ssl_options->client_cert_path = defGetString(def);
		else if (strcmp(def->defname, FDW_OPTION_SSL_PRIVATE_KEY_PASSWORD) == 0)
			opt->ssl_options->private_key_password = defGetString(def);
		else if (strcmp(def->defname, FDW_OPTION_SSL_PRIVATE_KEY_PATH) == 0)
			opt->ssl_options->private_key_path = defGetString(def);
		else if (strcmp(def->defname, FDW_OPTION_SSL_TRUSTED_CA_PATH) == 0)
			opt->ssl_options->trusted_ca_path = defGetString(def);
		else if (strcmp(def->defname, FDW_OPTION_SSL_VERSION) == 0)
			ssl_version = defGetString(def);
		else if (IsCopyOption(def->defname))
			copy_options = lappend(copy_options, def);
		else
		{
			Value	   *val = makeString(def->defname);

			/*
			 * if we have already seen this option before disregard the new
			 * value. We only take the first value that we see. And the
			 * precedence is table -> user mapping -> server -> wrapper
			 */
			if (list_member(other_option_name_strings, val))
				continue;
			other_options = lappend(other_options, def);
			other_option_name_strings = lappend(other_option_name_strings, val);
		}
	}							/* foreach */

	opt->wire_format = "TEXT";

	if (opt->format && pg_strcasecmp(opt->format, FDW_OPTION_WIRE_FORMAT_TEXT) == 0)
		wireFormat = (Node *) makeString(FDW_OPTION_WIRE_FORMAT_TEXT);
	else if (opt->format && pg_strcasecmp(opt->format, FDW_OPTION_WIRE_FORMAT_CSV) == 0)
		wireFormat = (Node *) makeString(FDW_OPTION_WIRE_FORMAT_CSV);
	else
	{
		/* default wire_format is binary */
		wireFormat = (Node *) makeString(FDW_OPTION_WIRE_FORMAT_BINARY);
		opt->wire_format = "BINARY";
	}

#if PG_VERSION_NUM >= 90600
	copy_options = lappend(copy_options, makeDefElem(FDW_COPY_OPTION_FORMAT, wireFormat, -1));
#else
	copy_options = lappend(copy_options, makeDefElem(FDW_COPY_OPTION_FORMAT, wireFormat));
#endif

	opt->copy_options = copy_options;
	opt->options = other_options;

	opt->server = server->servername;

	/* Follows precedence rules table > server > wrapper */
	opt->exec_location = table->exec_location;

	/* Set defaults when not provided */
	if (!opt->pxf_host || !opt->pxf_port || !opt->pxf_protocol)
	{
		const char *search_query =
				"SELECT pxf_host, pxf_port, pxf_protocol "
				"FROM pxf_config "
				"WHERE segment_id = $1";

		if (!opt->pxf_host)
			opt->pxf_host = pxf_host_guc_value;

		if (!opt->pxf_port)
			opt->pxf_port = pxf_port_guc_value;

		if (!opt->pxf_protocol)
			opt->pxf_protocol = pxf_protocol_guc_value;
	}

	if (pg_strcasecmp(opt->pxf_protocol, PXF_FDW_SECURE_PROTOCOL) == 0)
	{
		if (!ssl_version)
			opt->ssl_options->version = CURL_SSLVERSION_TLSv1;
		else
		{
			const struct PxfFdwSslVersionOption *entry;
			for (entry = valid_ssl_version_options; entry->optname; entry++)
			{
				if (!entry->optname)
					ereport(ERROR, (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
						errmsg("invalid SSL version option %s. valid values are 'default', 'TLSv1', 'SSLv2', 'SSLv3', 'TLSv1.0', 'TLSv1.1', 'TLSv1.2', and 'TLSv1.3'",
							ssl_version)));

				if (pg_strcasecmp(entry->optname, ssl_version) == 0)
				{
					opt->ssl_options->version = entry->version;
					break;
				}
			}
		}
	}

	return opt;
}

bool
IsValidPxfProtocolValue(char **newvalue, void **extra, GucSource source)
{
	return pg_strcasecmp(*newvalue, PXF_FDW_DEFAULT_PROTOCOL) == 0 ||
		pg_strcasecmp(*newvalue, PXF_FDW_SECURE_PROTOCOL) == 0;
}

/*
 * Check if the provided option is one of the valid options.
 * context is the Oid of the catalog holding the object the option is for.
 */
static bool
IsValidCopyOption(const char *option, Oid context)
{
	const struct PxfFdwOption *entry;

	for (entry = valid_copy_options; entry->optname; entry++)
	{
		if (context == entry->optcontext && strcmp(entry->optname, option) == 0)
			return true;
	}
	return false;
}

/*
 * Check if the option is a COPY option
 */
static bool
IsCopyOption(const char *option)
{
	const struct PxfFdwOption *entry;

	for (entry = valid_copy_options; entry->optname; entry++)
	{
		if (strcmp(entry->optname, option) == 0)
			return true;
	}
	return false;
}

/*
 * Goes through standard list of options to make sure option is defined at the correct catalog level
 */
static void
ValidateOption(char *option, Oid catalog)
{
	const struct PxfFdwOption *entry;

	for (entry = valid_options; entry->optname; entry++)
	{
		/* option can only be defined at its catalog level */
		if (strcmp(entry->optname, option) == 0 && catalog != entry->optcontext)
		{
			Relation	rel = RelationIdGetRelation(entry->optcontext);

			ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
					 errmsg(
							"the %s option can only be defined at the %s level",
							option,
							RelationGetRelationName(rel))));
		}
	}
}
