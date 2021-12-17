/**
 * DXGI based Screen capture tool. This is used to take a single screen shot and save it too a file.
 * This is to manage all the options either passed in by the command line or via an ini file.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "inih/ini.h"
#include "options.h"


Option* find_option(const char* optionName, Option fOptions[]) {
    Option* pOption;
    int index = 0;
    for(index = 0; fOptions[index].type != OPTION_TYPE_NONE; ++index) {
        if (strcmp(fOptions[index].name, optionName) == 0) {
            pOption = fOptions + index;
            break;
        }
    }
    return pOption;
}

int handler(void* config, const char* section, const char* name, const char* value) {
    // config instance for filling in the values.
    Option* pconfig = (Option*)config;
    Option* crntOption;
    crntOption = find_option(name, pconfig);
    if(crntOption == NULL) {
        return 0;
    }

    if (crntOption->type == OPTION_TYPE_INT) {
        crntOption->value.x_int = atoi(value);
    } else if (crntOption->type == OPTION_TYPE_STRING) {
         crntOption->value.x_string = strdup(value);
    } else if (crntOption->type == OPTION_TYPE_BOOL) {
         crntOption->value.x_bool = atoi(value);
    } else if (crntOption->type == OPTION_TYPE_FLOAT) {
         crntOption->value.x_float = strtof(value, NULL);
    } else {
        return 0;
    }
    return 1;
}


void* get_value(const char* optionName, Option fOptions[]) {
    Option* pOption = find_option(optionName, fOptions);
    if (pOption->type == OPTION_TYPE_STRING) {
        return (void *)pOption->value.x_string;
    }
    return (void *)&pOption->value;
}


int get_int_value(const char* optionName, Option fOptions[]) {
    void * tmpValue = NULL;
    tmpValue = get_value(optionName, fOptions);
    if (tmpValue == NULL) {
        return 0;
    }
    return *(int*)tmpValue;
}


float get_float_value(const char* optionName, Option fOptions[]) {
    void * tmpValue = NULL;
    tmpValue = get_value(optionName, fOptions);
    if (tmpValue == NULL) {
        return 0.0;
    }
    return *(float*)tmpValue;
}


char* get_string_value(const char* optionName, Option fOptions[]) {
    void * tmpValue = NULL;
    tmpValue = get_value(optionName, fOptions);
    if (tmpValue == NULL) {
        char * returnV;
        return returnV;
    }
    return (char*)tmpValue;
}


bool get_bool_value(const char* optionName, Option fOptions[]) {
    void * tmpValue = NULL;
    tmpValue = get_value(optionName, fOptions);
    if (tmpValue == NULL) {
        return 0;
    }
    return *(bool*)tmpValue;
}


void print_options(Option fOptions[]) {
    for(int index = 0; fOptions[index].type != OPTION_TYPE_NONE; ++index) {
            if (fOptions[index].type == OPTION_TYPE_INT) {
                printf("Name: %s\nDescription: %s\nType: %d\nValue: %d\n", fOptions[index].name, fOptions[index].description, fOptions[index].type, fOptions[index].value);
            } else if (fOptions[index].type == OPTION_TYPE_STRING) {
                printf("Name: %s\nDescription: %s\nType: %d\nValue: %s\n", fOptions[index].name, fOptions[index].description, fOptions[index].type, fOptions[index].value);
            } else if (fOptions[index].type == OPTION_TYPE_BOOL) {
                printf("Name: %s\nDescription: %s\nType: %d\nValue: %d\n", fOptions[index].name, fOptions[index].description, fOptions[index].type, fOptions[index].value);
            } else if (fOptions[index].type == OPTION_TYPE_FLOAT) {
                printf("Name: %s\nDescription: %s\nType: %d\nValue: %f\n", fOptions[index].name, fOptions[index].description, fOptions[index].type, fOptions[index].value);
            }
    }
}
