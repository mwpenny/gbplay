#ifndef _STORAGE_H
#define _STORAGE_H

/* Configures and opens non-volatile storage. */
void storage_initialize();

/* Closes non-volatile storage. */
void storage_deinitialize();

/*
    Retrieves a buffer from non-volatile storage.

    @param key Identifier of data

    @returns Dynamically allocated copy of buffer. Caller must free.
             NULL if no data was found for key.
*/
void* storage_get_blob(const char* key);

/*
    Stores a buffer in non-volatile storage.

    @param key    Identifier of data
    @param value  Buffer to store
    @param length Size of buffer
*/
void storage_set_blob(const char* key, const void* value, size_t length);

/*
    Retrieves a string from non-volatile storage.

    @param key Identifier of string

    @returns Dynamically allocated copy of string. Caller must free.
             NULL if no data was found for key.
*/
char* storage_get_string(const char* key);

/*
    Stores a string in non-volatile storage.

    @param key    Identifier of string
    @param value  String to store
*/
void storage_set_string(const char* key, const char* value);

/*
    Removes a value from non-volatile storage.

    @param key Identifier of the value
*/
void storage_delete(const char* key);

#endif
