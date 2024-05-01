/**
 * @file template_renderer.h
 * @brief Implements a renderer for taking an HTML template file and using code substitution to create 
 * a rendered HTML string
*/
#ifndef TEMPLATE_RENDERER_H_
#define TEMPLATE_RENDERER_H_

#include "common.h"

extern char *render_template(const char *html_template, _TContextDictionary *contextDictionary);

#endif // TEMPLATE_RENDERER_H_
