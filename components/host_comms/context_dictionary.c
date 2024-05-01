/**
 * @file context_dictionary.c
 * @brief Implements a context dictionary used to substitute codes for HTML in HTML templates
*/
#include "common.h"
#include "context_dictionary.h"

static void trim_whitespace(char *string);

/**
 * @brief Creates an empty context dictionary
 * 
 * @returns Returns the new context dictionary object or NULL if there was insufficient memory
*/
_TContextDictionary *create_context_dictionary()
{
	_TContextDictionary *context_dictionary;

	context_dictionary = (_TContextDictionary *)malloc(sizeof(_TContextDictionary));
	if (context_dictionary == NULL) return NULL;

	context_dictionary->first_entry = NULL;

	return context_dictionary;
}

/**
 * @brief Adds a new entry to the context dictionary
 * 
 * @param context_dictionary The context dictionary to add to
 * @param code A substitution code string
 * @param html The HTML to substitute for the code
 * 
 * @returns Returns true if successful
*/
bool add_context_dictionary_entry(_TContextDictionary *context_dictionary, char *code, char *html)
{
	_TContextDictionaryEntry *entry;

	// Create the new entry
	entry = (_TContextDictionaryEntry *)malloc(sizeof(_TContextDictionaryEntry));
	if (entry == NULL) return false;

	// Copy the code
	entry->code = (char *)malloc(strlen(code) + 1);
	if (entry->code == NULL)
	{
		free(entry);
		return false;
	}
	strcpy(entry->code, code);

	// Copy the HTML fragment
	entry->html = (char *)malloc(strlen(html) + 1);
	if (entry->html == NULL)
	{
		free(entry->code);
		free(entry);
		return false;
	}
	strcpy(entry->html, html);

	// Insert the entry at the start of the dictionary (the order doesn't matter)
	entry->next_entry = context_dictionary->first_entry;
	context_dictionary->first_entry = entry;

	return true;
}

/**
 * @brief Attempts to find a code string in a context dictionary
 * 
 * @param context_dictionary The context dictionary to search
 * @param code The code to search for
 * 
 * @returns Returns the matching context dictionary entry or NULL if none was found
*/
_TContextDictionaryEntry *find_context_dictionary_entry(_TContextDictionary *context_dictionary, char *code)
{
	_TContextDictionaryEntry *current_entry = context_dictionary->first_entry;

	// Trim whitespace from the start and end of the code
	trim_whitespace(code);

	// Search for a match
	while (current_entry != NULL)
	{
		if (strcmp(code, current_entry->code) == 0) return current_entry;
		current_entry = current_entry->next_entry;
	}

	return NULL;
}

/**
 * @brief Frees all heap memory associated with a context dictionary
 * 
 * @param context_dictionary The context dictionary to free
*/
void delete_context_dictionary(_TContextDictionary *context_dictionary)
{
	_TContextDictionaryEntry *current_entry = context_dictionary->first_entry;
	_TContextDictionaryEntry *next_entry;

	// Free memory associated with dictionary entries
	while (current_entry != NULL)
	{
		next_entry = current_entry->next_entry;

		free(current_entry->code);
		free(current_entry->html);
		free(current_entry);

		current_entry = next_entry;
	}

	// Free memory associated with the dictionary
	free(context_dictionary);
}

/**
 * @brief Trims whitespace from the start and end of a string
 * 
 * @param string The string to trim
*/
static void trim_whitespace(char *string)
{
    int32_t source;
    int32_t destination;
    int32_t length;

    // Check the length of the original string
    length = strlen(string);
    if (length < 1) return;

    // Advance source to the first non-whitespace character
    source = 0;
    while (string[source] == ' ') source++;

    // Copy the string to the front of the buffer
    for (destination = 0; source < length; destination++, source++)
    {
    	string[destination] = string[source];
    }

    // Working backwards, set all trailing spaces to NULL
    for(destination = length - 1; string[destination] == ' '; --destination)
    {
    	string[destination] = '\0';
    }
}



