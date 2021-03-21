#include "common.h"
#include "frontend/main.h"
#include "frontend/config.h"

typedef enum {
  CE_TYPE_STR = 0,
  CE_TYPE_NUM = 4,
} config_entry_type;

#define CE_STR(val) \
	{ #val, CE_TYPE_STRING, val }

#define CE_NUM(val) \
	{ #val, CE_TYPE_NUM, &val }

static const struct {
  const char *name;
  config_entry_type type;
  void *val;
} config_data[] = {
  CE_NUM(dynarec_enable),
  CE_NUM(frameskip_style),
  CE_NUM(max_frameskip),
  CE_NUM(scaling_mode),
  CE_NUM(color_correct),
  CE_NUM(lcd_blend),
  CE_NUM(show_fps),
};

void config_write(FILE *f)
{
  for (int i = 0; i < array_size(config_data); i++) {
    switch (config_data[i].type)
    {
      case CE_TYPE_STR:
        fprintf(f, "%s = %s\n", config_data[i].name, (char *)config_data[i].val);
        break;
      case CE_TYPE_NUM:
        fprintf(f, "%s = %u\n", config_data[i].name, *(uint32_t *)config_data[i].val);
        break;
      default:
        printf("unhandled type %d for %s\n", config_data[i].type, (char *)config_data[i].val);
        break;
    }
  }
}

static void parse_str_val(char *cval, const char *src)
{
	char *tmp;
	strncpy(cval, src, 256);
	cval[256 - 1] = 0;
	tmp = strchr(cval, '\n');
	if (tmp == NULL)
		tmp = strchr(cval, '\r');
	if (tmp != NULL)
		*tmp = 0;
}

static void parse_num_val(uint32_t *cval, const char *src)
{
  char *tmp = NULL;
  uint32_t val;
  val = strtoul(src, &tmp, 10);
  if (tmp == NULL || src == tmp)
    return; // parse failed

  *cval = val;
}

void config_read(const char* cfg)
{
  for (int i = 0; i < array_size(config_data); i++) {
    char *tmp;
    
    tmp = strstr(cfg, config_data[i].name);
		if (tmp == NULL)
			continue;
		tmp += strlen(config_data[i].name);
		if (strncmp(tmp, " = ", 3) != 0)
			continue;
		tmp += 3;

    if (config_data[i].type == CE_TYPE_STR) {
			parse_str_val(config_data[i].val, tmp);
			continue;
		}

    parse_num_val(config_data[i].val, tmp);
  }
}
