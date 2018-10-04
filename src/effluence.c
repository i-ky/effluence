#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <curl/curl.h>

#include "sysinc.h"
#include "module.h"

#include "configuration.h"
#include "utils.h"

CURL	*hnd = NULL;

int	zbx_module_api_version(void)
{
	return ZBX_MODULE_API_VERSION;
}

int	zbx_module_init(void)
{
	const char	efflu_config[] = "EFFLU_CONFIG", *config_file_name;
	FILE		*config_file;
	int		ret = ZBX_MODULE_OK;

	printf("Initializing \"effluence\" module...\n");

	if (NULL == (config_file_name = secure_getenv(efflu_config)))
	{
		printf("Path to configuration file must be set using \"%s\" environment variable.\n", efflu_config);
		return ZBX_MODULE_FAIL;
	}

	printf("Reading configuration from \"%s\"...\n", config_file_name);

	if (NULL == (config_file = fopen(config_file_name, "r")))
	{
		printf("Failure to open \"%s\": %s.\n", config_file_name, strerror(errno));
		return ZBX_MODULE_FAIL;
	}

	if (0 != efflu_parse_configuration(config_file))
	{
		printf("Failure to read configuration from \"%s\".\n", config_file_name);
		ret = ZBX_MODULE_FAIL;
	}

	if (0 != fclose(config_file))
		printf("Failure to close \"%s\": %s.\n", config_file_name, strerror(errno));

	/* TODO ping InfluxDB to see if it's up */

	if (ZBX_MODULE_OK == ret)
		printf("Module \"effluence\" has been successfully initialized.\n");

	return ret;
}

int	zbx_module_uninit(void)
{
	printf("Uninitializing \"effluence\" module...\n");

	printf("Cleaning up cURL handle...\n");

	if (NULL != hnd)
		curl_easy_cleanup(hnd);

	printf("Cleaning up configuration data...\n");

	efflu_clean_configuration();

	printf("Module \"effluence\" has been successfully uninitialized.\n");

	return ZBX_MODULE_OK;
}

static char	*efflu_escape(const char *string)
{
	size_t		size_esc;
	const char	*p;
	char		*string_esc, *q;

	p = string;
	size_esc = 0;

	do
	{
		if ('\"' == *p || '\\' == *p)
			size_esc++;

		size_esc++;
	}
	while ('\0' != *p++);

	p = string;
	q = string_esc = malloc(size_esc);	/* TODO malloc() may return NULL, handle that */

	do
	{
		if ('\"' == *p || '\\' == *p)
			*q++ = '\\';
	}
	while ('\0' != (*q++ = *p++));
	
	return string_esc;
}

/* To avoid confusion data is split into measurements according to Zabbix data types, measurements are named  */
/* after Zabbix DB tables: history, history_uint, history_str, history_text, history_log. Actually, there is  */
/* no real difference in how "Numeric (float)" and "Numeric (unsigned)" or "Character" and "Text" are stored. */

static void	efflu_append_FLOAT_line(char **post, size_t *size, size_t *offset, const ZBX_HISTORY_FLOAT *data)
{
	efflu_strappf(post, size, offset, "history,itemid=" ZBX_FS_UI64 " value=%f %" PRIu64 "\n",
			data->itemid, data->value, (uint64_t)data->clock * 1000000000L + data->ns);
}

static void	efflu_append_INTEGER_line(char **post, size_t *size, size_t *offset, const ZBX_HISTORY_INTEGER *data)
{
	/* 64 bit unsigned integers used by Zabbix won't necessarily fit into 64 bit signed integer used by InfluxDB */
	/* To avoid overflows we trade off some accuracy by sending integers as floats (without "i" after the value) */
	efflu_strappf(post, size, offset, "history_uint,itemid=" ZBX_FS_UI64 " value=" ZBX_FS_UI64 " %" PRIu64 "\n",
			data->itemid, data->value, (uint64_t)data->clock * 1000000000L + data->ns);
}

static void	efflu_append_STRING_line(char **post, size_t *size, size_t *offset, const ZBX_HISTORY_STRING *data)
{
	char	*value_esc;

	value_esc = efflu_escape(data->value);
	efflu_strappf(post, size, offset, "history_str,itemid=" ZBX_FS_UI64 " value=\"%s\" %" PRIu64 "\n",
			data->itemid, value_esc, (uint64_t)data->clock * 1000000000L + data->ns);
	free(value_esc);
}

static void	efflu_append_TEXT_line(char **post, size_t *size, size_t *offset, const ZBX_HISTORY_TEXT *data)
{
	char	*value_esc;

	value_esc = efflu_escape(data->value);
	efflu_strappf(post, size, offset, "history_text,itemid=" ZBX_FS_UI64 " value=\"%s\" %" PRIu64 "\n",
			data->itemid, value_esc, (uint64_t)data->clock * 1000000000L + data->ns);
	free(value_esc);
}

static void	efflu_append_LOG_line(char **post, size_t *size, size_t *offset, const ZBX_HISTORY_LOG *data)
{
	char	*value_esc, *source_esc;

	value_esc = efflu_escape(data->value);
	source_esc = efflu_escape(data->source);
	efflu_strappf(post, size, offset, "history_log,itemid=" ZBX_FS_UI64 " value=\"%s\",source=\"%s\",timestamp=%d,"
			"logeventid=%d,severity=%d %" PRIu64 "\n",
			data->itemid, value_esc, source_esc, data->timestamp, data->logeventid, data->severity,
			(uint64_t)data->clock * 1000000000L + data->ns);
	free(value_esc);
	free(source_esc);
}

static void	efflu_write(efflu_destination_t destination, const char *post)
{
	char		*endpoint;
	CURLcode	ret;

	if (NULL == hnd)
		hnd = curl_easy_init();

	/* TODO Need a portable alternative to asprintf() */
	asprintf(&endpoint, "%s/write?db=%s", destination.url, destination.db);
	/* TODO handle potential errors of curl_easy_setopt() */
	curl_easy_setopt(hnd, CURLOPT_URL, endpoint);

	if (NULL != destination.user)
		curl_easy_setopt(hnd, CURLOPT_USERNAME, destination.user);

	if (NULL != destination.pass)
		curl_easy_setopt(hnd, CURLOPT_PASSWORD, destination.pass);

	curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, post);

	if (CURLE_OK != (ret = curl_easy_perform(hnd)))
		printf("Error while POSTing data: %s", curl_easy_strerror(ret));

	free(endpoint);
}

#define EFFLU_CALLBACK(data_type)	\
static void	efflu_##data_type##_cb(const ZBX_HISTORY_##data_type *history, int history_num)	\
{												\
	int	i;										\
	char	*post = NULL;									\
	size_t	size = 0, offset = 0;								\
												\
	for (i = 0; i < history_num; i++)							\
	{											\
		efflu_append_##data_type##_line(&post, &size, &offset, history++);		\
												\
		if (NULL == post)								\
		{										\
			printf("Failed to allocate memory for data to POST.\n");		\
			return;									\
		}										\
	}											\
												\
	efflu_write(efflu_configured_destination(EFFLU_DATA_TYPE_##data_type), post);		\
	free(post);										\
}

EFFLU_CALLBACK(FLOAT)
EFFLU_CALLBACK(INTEGER)
EFFLU_CALLBACK(STRING)
EFFLU_CALLBACK(TEXT)
EFFLU_CALLBACK(LOG)

ZBX_HISTORY_WRITE_CBS	zbx_module_history_write_cbs(void)
{
	static ZBX_HISTORY_WRITE_CBS	callbacks =
	{
		.history_float_cb = efflu_FLOAT_cb,
		.history_integer_cb = efflu_INTEGER_cb,
		.history_string_cb = efflu_STRING_cb,
		.history_text_cb = efflu_TEXT_cb,
		.history_log_cb = efflu_LOG_cb
	};

	return callbacks;
}
