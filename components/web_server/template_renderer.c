/**
 * @file template_renderer.c
 * @brief Implements a renderer for taking an HTML template file and using code substitution to create 
 * a rendered HTML string
*/
#include "common.h"
#include "context_dictionary.h"
#include "template_renderer.h"

// Structure holding one segment of an HTML file/string.  The segment
// can hold either plain HTML, or a substitution code which can later
// be exchanged for an HTML fragment by the renderer
typedef struct segment
{
	// HTML held by the segment
	char *html;

	// Substitution code
	char *code;

	// Next segment in the list
	struct segment *next_segment;
}
_TSegment;

static _TSegment *split_template(const char *template);
static void perform_substitutions(_TSegment *segment_list, _TContextDictionary *context_dictionary);
static char *create_response_string(_TSegment *segment_list);
static void delete_segment_list(_TSegment *segment_list);

/**
 * @brief Renders an HTML template into a web page by substituting codes supplied in a context structure
 * 
 * @param template HTML template to render
 * @param context_dictionary A dictionary of substitution codes vs HTML fragments that is used to render the template
 * 
 * @returns Returns an HTTP response or NULL if there was an error.  It is the caller's responsibility to free the memory associated with this response
*/
char *render_template(const char *template, _TContextDictionary *context_dictionary)
{
	char *response;
	_TSegment *segment_list;

	// Split the template into segments delimited by {{ and }} brackets
	segment_list = split_template(template);
	if (segment_list == NULL)
	{
		return NULL;
	}

	// Make substitutions in the segment list
	perform_substitutions(segment_list, context_dictionary);

	// Convert the segment list to a rendered HTML string
	response = create_response_string(segment_list);

	// Free the memory associated with the template copy and segment list
	delete_segment_list(segment_list);

	return response;
}

/**
 * @brief Splits a template string into a linked list of segments delimited by {{ and }} brackets
 * 
 * @param template A string containing the HTML template
 * 
 * @returns Returns the first segment in the linked list or NULL if there was a memory error
*/
static _TSegment *split_template(const char *template)
{
	_TSegment *first_segment;
	_TSegment *html_segment;
	_TSegment *code_segment;
	_TSegment *next_html_segment;
	char *opening_delimiter;
	char *closing_delimiter;
	char *segment_html_start = (char *)template;
	int32_t length;

	// Allocate space for the first segment
	first_segment = (_TSegment *)malloc(sizeof(_TSegment));
	if (first_segment == NULL) return NULL;

	first_segment->next_segment = NULL;
	first_segment->html = NULL;
	first_segment->code = NULL;

	html_segment = first_segment;
	while (true)
	{
		// Search for an opening delimiter {{
		opening_delimiter = strstr(segment_html_start, "{{");
		if (opening_delimiter == NULL) break;

		// Copy the HTML before the opening delimiter into the segment
		length = opening_delimiter - segment_html_start;
		html_segment->html = (char *)malloc(length + 1);
		memcpy(html_segment->html, segment_html_start, length);
		html_segment->html[length] = '\0';

		// Search for the matching closing delimiter }}
		closing_delimiter = strstr(opening_delimiter, "}}");
		if (closing_delimiter == NULL) break;

		// Create a new segment for the substitution code
		code_segment = (_TSegment *)malloc(sizeof(_TSegment));
		if (code_segment == NULL)
		{
			delete_segment_list(first_segment);
			return NULL;
		}

		code_segment->next_segment = NULL;
		code_segment->html = NULL;
		code_segment->code = NULL;

		// Copy the substitution code into the segment
		length = closing_delimiter - opening_delimiter - 2;
		code_segment->code = (char *)malloc(length + 1);
		memcpy(code_segment->code, opening_delimiter + 2, length);
		code_segment->code[length] = '\0';

		// Connect the code segment to the HTML segment
		html_segment->next_segment = code_segment;

		// Create a new HTML segment starting after the closing }}
		next_html_segment = (_TSegment *)malloc(sizeof(_TSegment));
		if (next_html_segment == NULL)
		{
			delete_segment_list(first_segment);
			return NULL;
		}

		next_html_segment->next_segment = NULL;
		next_html_segment->html = NULL;
		next_html_segment->code = NULL;

		// Connect the new HTML segment to the code segment
		code_segment->next_segment = next_html_segment;

		// Move on to the next section of the template
		segment_html_start = closing_delimiter + 2;
		html_segment = next_html_segment;
	}

	// Complete the last segment
	length = (template + strlen(template)) - segment_html_start;
	html_segment->html = (char *)malloc(length + 1);
	memcpy(html_segment->html, segment_html_start, length);
	html_segment->html[length] = '\0';

	return first_segment;
}

/**
 * @brief Substitutes any codes in the linked segment list with HTML fragments from the context dictionary
 * 
 * @param segment_list Linked list of segments
 * @param context_dictionary Linked list containing code/html fragment pairs
*/
static void perform_substitutions(_TSegment *segment_list, _TContextDictionary *context_dictionary)
{
	_TSegment *current_segment = segment_list;
	_TContextDictionaryEntry *context_entry;

	while (current_segment != NULL)
	{
		if (current_segment->code != NULL)
		{
			// See if the code exists in the context dictionary
			context_entry = find_context_dictionary_entry(context_dictionary, current_segment->code);
			if (context_entry != NULL)
			{
				// If so, replace the code with the HTML fragment from the dictionary entry
				current_segment->html = context_entry->html;
			}
		}

		current_segment = current_segment->next_segment;
	}
}

/**
 * @brief Substitutes any codes in the linked segment list with HTML fragments from the context dictionary
 * 
 * @param segment_list Linked list of segments
 * 
 * @returns Returns a response string or NULL if there was an error.  It is the caller's responsibility to free the memory associated with this response
*/
static char *create_response_string(_TSegment *segment_list)
{
	_TSegment *current_segment;
	int32_t length = 0;
	char *response;

	// Determine the length of string required to hold the complete response
	current_segment = segment_list;
	while (current_segment != NULL)
	{
		if (current_segment->html != NULL)
		{
			length += strlen(current_segment->html);
		}

		current_segment = current_segment->next_segment;
	}

	// Allocate space for the response string
	response = (char *)malloc(length + 1);
	if (response == NULL) return NULL;
	response[0] = '\0';

	// Copy the HTML sections from the segment list into the response string
	current_segment = segment_list;
	while (current_segment != NULL)
	{
		if (current_segment->html != NULL)
		{
			strcat(response, current_segment->html);
		}

		current_segment = current_segment->next_segment;
	}

	return response;
}

/**
 * @brief Deletes all memory associated with a segment list
 * 
 * @param segment_list Linked list of segments
*/
static void delete_segment_list(_TSegment *segment_list)
{
	_TSegment *current_segment = segment_list;
	_TSegment *next_segment;

	while (current_segment != NULL)
	{
		next_segment = current_segment->next_segment;
		free(current_segment);
		current_segment = next_segment;
	}
}
