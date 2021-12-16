/**
 * DXGI based Screen capture tool. This is used to take a single screen shot and save it too a file.
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
  bool preset;

  enum OptionType type;

  union
  {
    int    x_int;
    char * x_string;
    bool   x_bool;
    float  x_float;
  } value;
} Option;

void printHelp();
struct Option* findOption(const char* optionName, struct Option options[]);
static int handler(void* config, const char* section, const char* name, const char* value);
static void printOptions(Option fOptions[]);
