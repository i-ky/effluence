#ifndef EFFLU_CONFIGURATION_H
#define EFFLU_CONFIGURATION_H

#include <stdio.h>

typedef enum
{
	EFFLU_DATA_TYPE_FLOAT = 0,
	EFFLU_DATA_TYPE_INTEGER,
	EFFLU_DATA_TYPE_STRING,
	EFFLU_DATA_TYPE_TEXT,
	EFFLU_DATA_TYPE_LOG,
	EFFLU_DATA_TYPE_COUNT
}
efflu_data_type_t;

typedef struct
{
	char	*url;
	char	*db;
	char	*user;
	char	*pass;
}
efflu_destination_t;

int			efflu_parse_configuration(FILE *config_file);
efflu_destination_t	efflu_configured_destination(efflu_data_type_t type);
void			efflu_clean_configuration(void);

#endif	/* EFFLU_CONFIGURATION_H */
