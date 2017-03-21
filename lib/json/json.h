/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __JSON_H
#define __JSON_H

#include <misc/util.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

enum json_tokens {
	JSON_TOK_NONE = '_',
	JSON_TOK_OBJECT_START = '{',
	JSON_TOK_OBJECT_END = '}',
	JSON_TOK_LIST_START = '[',
	JSON_TOK_LIST_END = ']',
	JSON_TOK_STRING = '"',
	JSON_TOK_COLON = ':',
	JSON_TOK_COMMA = ',',
	JSON_TOK_NUMBER = '0',
	JSON_TOK_TRUE = 't',
	JSON_TOK_FALSE = 'f',
	JSON_TOK_NULL = 'n',
	JSON_TOK_ERROR = '!',
	JSON_TOK_EOF = '\0',
};

struct json_obj_descr {
	const char *field_name;
	size_t field_name_len;
	size_t offset;

	/* Valid values here: JSON_TOK_STRING, JSON_TOK_NUMBER,
	 * JSON_TOK_TRUE, JSON_TOK_FALSE, JSON_TOK_OBJECT_START,
	 * JSON_TOK_LIST_START. (All others ignored.)
	 */
	enum json_tokens type;

	union {
		struct {
			const struct json_obj_descr *sub_descr;
			size_t sub_descr_len;
		};
		struct {
			const struct json_obj_descr *element_descr;
			size_t n_elements;
		};
	};
};

/**
 * @brief Helper macro to declare a descriptor for an object value
 *
 * @param struct_ Struct packing the values
 *
 * @param field_name_ Field name in the struct
 *
 * @param sub_descr_ Array of json_obj_descr describing the subobject
 *
 * Here's an example of use:
 *      struct nested {
 *          int foo;
 *          struct {
 *             int baz;
 *          } bar;
 *      };
 *
 *      struct json_obj_descr nested_bar[] = {
 *          { ... declare bar.baz descriptor ... },
 *      };
 *      struct json_obj_descr nested[] = {
 *          { ... declare foo descriptor ... },
 *          JSON_OBJ_DESCR_OBJECT(struct nested, bar, nested_bar),
 *      };
 */
#define JSON_OBJ_DESCR_OBJECT(struct_, field_name_, sub_descr_) \
	{ \
		.field_name = (#field_name_), \
		.field_name_len = (sizeof(#field_name_) - 1), \
		.offset = offsetof(struct_, field_name_), \
		.type = JSON_TOK_OBJECT_START, \
		.sub_descr = sub_descr_, \
		.sub_descr_len = ARRAY_SIZE(sub_descr_) \
	}

/**
 * @brief Helper macro to declare a descriptor for an array value
 *
 * @param struct_ Struct packing the values
 *
 * @param field_name_ Field name in the struct
 *
 * @param max_len_ Maximum number of elements in array
 *
 * @param len_field_ Field name in the struct for the number of elements
 * in the array
 *
 * @param elem_type_ Element type
 *
 * Here's an example of use:
 *      struct example {
 *          int foo[10];
 *          size_t foo_len;
 *      };
 *
 *      struct json_obj_descr array[] = {
 *           JSON_OBJ_DESCR_ARRAY(struct example, foo, JSON_TOK_NUMBER)
 *      };
 */
#define JSON_OBJ_DESCR_ARRAY(struct_, field_name_, max_len_, \
			     len_field_, elem_type_) \
	{ \
		.field_name = (#field_name_), \
		.field_name_len = sizeof(#field_name_) - 1, \
		.offset = offsetof(struct_, field_name_), \
		.type = JSON_TOK_LIST_START, \
		.element_descr = &(struct json_obj_descr) { \
			.type = elem_type_, \
			.offset = offsetof(struct_, len_field_), \
		}, \
		.n_elements = (max_len_), \
	}

/**
 * @brief Parses the JSON-encoded object pointer to by @param json, with
 * size @param len, according to the descriptor pointed to by @param descr.
 * Values are stored in a struct pointed to by @param val.  Set up the
 * descriptor like this:
 *
 *    struct s { int foo; char *bar; }
 *    struct json_obj_descr descr[] = {
 *       { .field_name = "foo",
 *         .field_name_len = 3,
 *         .offset = offsetof(struct s, foo),
 *         .type = JSON_TOK_NUMBER },
 *       { .field_name = "bar",
 *         .field_name_len = 3,
 *         .offset = offsetof(struct s, bar),
 *         .type = JSON_TOK_STRING }
 *    };
 *
 * Since this parser is designed for machine-to-machine communications, some
 * liberties were taken to simplify the design:
 * (1) strings are not unescaped (but only valid escape sequences are
 * accepted);
 * (2) no UTF-8 validation is performed; and
 * (3) only integer numbers are supported (no strtod() in the minimal libc).
 *
 * @param json Pointer to JSON-encoded value to be parsed
 *
 * @param len Length of JSON-encoded value
 *
 * @param descr Pointer to the descriptor array
 *
 * @param descr_len Number of elements in the descriptor array. Must be less
 * than 31 due to implementation detail reasons (if more fields are
 * necessary, use two descriptors)
 *
 * @param val Pointer to the struct to hold the decoded values
 *
 * @return < 0 if error, bitmap of decoded fields on success (bit 0
 * is set if first field in the descriptor has been properly decoded, etc).
 */
int json_obj_parse(char *json, size_t len,
	const struct json_obj_descr *descr, size_t descr_len,
	void *val);

/**
 * @brief Escapes the string so it can be used to encode JSON objects
 *
 * @param str The string to escape; the escape string is stored the
 * buffer pointed to by this parameter
 *
 * @param len Points to a size_t containing the size before and after
 * the escaping process
 *
 * @param buf_size The size of buffer str points to
 *
 * @return 0 if string has been escaped properly, or -ENOMEM if there
 * was not enough space to escape the buffer
 */
ssize_t json_escape(char *str, size_t *len, size_t buf_size);

/**
 * @brief Calculates the JSON-escaped string length
 *
 * @param str The string to analyze
 *
 * @param len String size
 *
 * @return The length str would have if it were escaped
 */
size_t json_calc_escaped_len(const char *str, size_t len);

#endif /* __JSON_H */
