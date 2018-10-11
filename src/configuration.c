#include <stdlib.h>

#include <yaml.h>

#include "configuration.h"

typedef struct
{
	efflu_destination_t	global;
	efflu_destination_t	*by_type[EFFLU_DATA_TYPE_COUNT];
}
efflu_configuration_t;

efflu_configuration_t	configuration;

static int	efflu_scalar_cmp(const char *str, const yaml_node_t *node)
{
	size_t	len;

	if (YAML_SCALAR_NODE != node->type)
		printf("Internal error: scalar comparison of nonscalar node.\n");

	len = strlen(str);

	if (len != node->data.scalar.length)
		return 1;

	return memcmp(str, node->data.scalar.value, len);
}

static char	*efflu_strdup(const yaml_node_t *node)
{
	char	*tmp;

	if (YAML_SCALAR_NODE != node->type)
		printf("Internal error: attempt to copy nonscalar node as string.\n");

	/* TODO malloc() may return NULL, handle it */
	tmp = malloc(node->data.scalar.length + 1);
	memcpy(tmp, node->data.scalar.value, node->data.scalar.length);
	tmp[node->data.scalar.length] = '\0';
	return tmp;
}

typedef struct
{
	const char	*name;
	char		**storage;
}
efflu_configuration_option_t;

static int	efflu_parse_node(yaml_document_t *config, const yaml_node_t *node, efflu_destination_t *top, efflu_destination_t **sub)
{
	const yaml_node_pair_t	*pair;

	if (YAML_MAPPING_NODE != node->type)
	{
		printf("Node must be a mapping.\n");
		return -1;
	}

	for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++)
	{
		const yaml_node_t	*key;

		key = yaml_document_get_node(config, pair->key);

		if (YAML_SCALAR_NODE == key->type)
		{
			const efflu_configuration_option_t	*option, options[] =
			{
				{"url",		&top->url},
				{"db",		&top->db},
				{"user",	&top->user},
				{"pass",	&top->pass},
				{0}
			};

			for (option = options; NULL != option->name; option++)
			{
				if (0 == efflu_scalar_cmp(option->name, key))
					break;
			}

			if (NULL != option->name)
			{
				*option->storage = efflu_strdup(yaml_document_get_node(config, pair->value));
				continue;
			}

			if (NULL != sub)
			{
				efflu_data_type_t	type = 0;

				do
				{
					if (0 == efflu_scalar_cmp(efflu_data_type_string(type), key))
						break;
				}
				while (EFFLU_DATA_TYPE_COUNT > ++type);

				if (EFFLU_DATA_TYPE_COUNT > type)
				{
					efflu_destination_t	tmp = {0};

					if (NULL != sub[type])
					{
						printf("Configuration for \"%s\" is specified twice.\n", efflu_data_type_string(type));
						return -1;
					}

					printf("Parsing \"%s\" configuration...\n", efflu_data_type_string(type));

					if (-1 == efflu_parse_node(config, yaml_document_get_node(config, pair->value),
							&tmp, NULL))
					{
						return -1;
					}

					sub[type] = malloc(sizeof(efflu_destination_t));
					*sub[type] = tmp;
					continue;
				}
			}

			printf("Unexpected key \"%.*s\" in mapping.\n",
					(int)key->data.scalar.length, key->data.scalar.value);
			return -1;
		}
		else if (YAML_SEQUENCE_NODE == key->type && NULL != sub)
		{
			const yaml_node_item_t	*item;

			for (item = key->data.sequence.items.start; item < key->data.sequence.items.top; item++)
			{
				const yaml_node_t	*elem;
				efflu_data_type_t	type = 0;

				elem = yaml_document_get_node(config, *item);

				if (YAML_SCALAR_NODE != elem->type)
				{
					printf("Sequence item must be a scalar.\n");
					return -1;
				}

				do
				{
					if (0 == efflu_scalar_cmp(efflu_data_type_string(type), elem))
						break;
				}
				while (EFFLU_DATA_TYPE_COUNT > ++type);

				if (EFFLU_DATA_TYPE_COUNT > type)
				{
					efflu_destination_t	tmp = {0};

					if (NULL != sub[type])
					{
						printf("Configuration for \"%s\" is specified twice.\n", efflu_data_type_string(type));
						return -1;
					}

					printf("Parsing \"%s\" configuration...\n", efflu_data_type_string(type));

					if (-1 == efflu_parse_node(config, yaml_document_get_node(config, pair->value),
							&tmp, NULL))
					{
						return -1;
					}

					sub[type] = malloc(sizeof(efflu_destination_t));
					*sub[type] = tmp;
					continue;
				}

				printf("Unexpected item \"%.*s\" in sequence.\n",
						(int)elem->data.scalar.length, elem->data.scalar.value);
				return -1;
			}
		}
		else
		{
			printf("Unexpected type of key in mapping.\n");
			return -1;
		}
	}

	return 0;
}

int	efflu_parse_configuration(FILE *config_file)
{
	yaml_parser_t	parser;
	yaml_document_t	config;
	yaml_node_t	*root;
	int		ret = -1;

	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, config_file);

	if (0 != yaml_parser_load(&parser, &config))
	{
		if (NULL != (root = yaml_document_get_root_node(&config)))
		{
			printf("Parsing global configuration...\n");
			ret = efflu_parse_node(&config, root, &configuration.global, configuration.by_type);
			yaml_document_delete(&config);
		}
		else
			printf("There are no YAML documents in the configuration file.\n");
	}
	else
		printf("Cannot parse configuration file. Please check if it is a valid YAML file.\n");

	yaml_parser_delete(&parser);

	if (0 == ret)
		printf("Configuration file has been successfully parsed.\n");

	return ret;
}

efflu_destination_t	efflu_configured_destination(efflu_data_type_t type)
{
#define EFFLU_GET_CONFIG(field)	(NULL != configuration.by_type[type] ? configuration.by_type[type]->field : configuration.global.field)

	return (efflu_destination_t){
		.url = EFFLU_GET_CONFIG(url),
		.db = EFFLU_GET_CONFIG(db),
		.user = EFFLU_GET_CONFIG(user),
		.pass = EFFLU_GET_CONFIG(pass)
	};

#undef EFFLU_GET_CONFIG
}

const char	*efflu_data_type_string(efflu_data_type_t type)
{
	switch (type)
	{
		case EFFLU_DATA_TYPE_FLOAT:
			return "dbl";
		case EFFLU_DATA_TYPE_INTEGER:
			return "uint";
		case EFFLU_DATA_TYPE_STRING:
			return "str";
		case EFFLU_DATA_TYPE_TEXT:
			return "text";
		case EFFLU_DATA_TYPE_LOG:
			return "log";
		default:
			return "unknown";
	}
}

static void	efflu_clean_destination(efflu_destination_t destination)
{
	char	*buffers_to_free[] = {destination.url, destination.db, destination.user, destination.pass};
	size_t	index = sizeof(buffers_to_free) / sizeof(char *);

	while (0 < index--)
	{
		if (NULL != buffers_to_free[index])
			free(buffers_to_free[index]);
	}
}

void	efflu_clean_configuration(void)
{
	efflu_data_type_t	type = 0;

	do
	{
		if (NULL == configuration.by_type[type])
			continue;

		efflu_clean_destination(*configuration.by_type[type]);
		free(configuration.by_type[type]);
	}
	while (EFFLU_DATA_TYPE_COUNT > ++type);

	efflu_clean_destination(configuration.global);
}
