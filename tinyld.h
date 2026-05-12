#ifndef TINYLD_H
#define TINYLD_H

#include "tcc.h"

typedef TCCState TinyLDState;
typedef TCCState TinyASState;

TinyLDState *tinyld_new(void);
void tinyld_delete(TinyLDState *s);
int tinyld_add_file(TinyLDState *s, const char *filename);
int tinyld_add_library(TinyLDState *s, const char *library_name);
int tinyld_add_library_path(TinyLDState *s, const char *path);
int tinyld_output_file(TinyLDState *s, const char *filename);

TinyASState *tinyas_new(void);
void tinyas_delete(TinyASState *s);
int tinyas_assemble_file(TinyASState *s, const char *filename);
int tinyas_output_file(TinyASState *s, const char *filename);

#endif
