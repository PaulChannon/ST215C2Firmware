/**
 * @file context_dictionary.h
 * @brief Implements a context dictionary used to substitute codes for HTML in HTML templates
*/
#ifndef CONTEXT_DICTIONARY_H_
#define CONTEXT_DICTIONARY_H_

#include "common.h"

// Entry in a context dictionary which links a substitution code to an HTML fragment
typedef struct context_dictionary_entry
{
	// Null terminated substitution code
	char *code;

	// HTML fragment to substitute
	char *html;

	// Next entry in the dictionary
	struct context_dictionary_entry *next_entry;
}
_TContextDictionaryEntry;

// Representation of a complete context dictionary
typedef struct
{
	_TContextDictionaryEntry *first_entry;
}
_TContextDictionary;

extern _TContextDictionary *create_context_dictionary();
extern bool add_context_dictionary_entry(_TContextDictionary *context_dictionary, char *code, char *html);
extern _TContextDictionaryEntry *find_context_dictionary_entry(_TContextDictionary *context_dictionary, char *code);
extern void delete_context_dictionary(_TContextDictionary *context_dictionary);

#endif // CONTEXT_DICTIONARY_H_
