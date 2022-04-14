#ifndef _STORAGE_H
#define _STORAGE_H

void storage_initialize();
void storage_deinitialize();

void* storage_get_blob(const char* key);
void storage_set_blob(const char* key, const void* value, size_t length);

char* storage_get_string(const char* key);
void storage_set_string(const char* key, const char* value);

#endif
