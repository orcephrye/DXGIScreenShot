/**
 * DXGI based Screen capture tool. This is used to take a single screen shot and save it too a file.
 * This is to manage all the options either passed in by the command line or via an ini file.
 */
#include <stdbool.h>
#include <stdio.h>


enum OptionType
{
  OPTION_TYPE_NONE = 0,
  OPTION_TYPE_INT,
  OPTION_TYPE_STRING,
  OPTION_TYPE_BOOL,
  OPTION_TYPE_FLOAT,
};

typedef struct Option
{
  char * name;
  char * description;

  enum OptionType type;

  union
  {
    int    x_int;
    char * x_string;
    bool   x_bool;
    float  x_float;
  } value;
} Option;

struct Option* find_option(const char* optionName, struct Option options[]);
int handler(void* config, const char* section, const char* name, const char* value);
void* getValue(const char* optionName, Option fOptions[]);
int get_int_value(const char* optionName, Option fOptions[]);
float get_float_value(const char* optionName, Option fOptions[]);
char* get_string_value(const char* optionName, Option fOptions[]);
bool get_bool_value(const char* optionName, Option fOptions[]);
void print_options(Option fOptions[]);
