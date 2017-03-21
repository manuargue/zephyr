/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

struct token {
	enum json_tokens type;
	char *start;
	char *end;
};

struct lexer {
	void *(*state)(struct lexer *lexer);
	char *start;
	char *pos;
	char *end;
	struct token token;
};

struct json_obj {
	struct lexer lexer;
};

struct json_obj_key_value {
	const char *key;
	size_t key_len;
	struct token value;
};

static bool lexer_consume(struct lexer *lexer, struct token *token,
			  enum json_tokens empty_token)
{
	if (lexer->token.type == empty_token) {
		return false;
	}

	*token = lexer->token;
	lexer->token.type = empty_token;

	return true;
}

static bool lexer_next(struct lexer *lexer, struct token *token)
{
	while (lexer->state) {
		if (lexer_consume(lexer, token, JSON_TOK_NONE)) {
			return true;
		}

		lexer->state = lexer->state(lexer);
	}

	return lexer_consume(lexer, token, JSON_TOK_EOF);
}

static void *lexer_json(struct lexer *lexer);

static void emit(struct lexer *lexer, enum json_tokens token)
{
	lexer->token.type = token;
	lexer->token.start = lexer->start;
	lexer->token.end = lexer->pos;
	lexer->start = lexer->pos;
}

static char next(struct lexer *lexer)
{
	if (lexer->pos >= lexer->end) {
		lexer->pos = lexer->end + 1;

		return '\0';
	}

	return *lexer->pos++;
}

static void ignore(struct lexer *lexer)
{
	lexer->start = lexer->pos;
}

static void backup(struct lexer *lexer)
{
	lexer->pos--;
}

static char peek(struct lexer *lexer)
{
	char chr = next(lexer);

	backup(lexer);

	return chr;
}

static void *lexer_string(struct lexer *lexer)
{
	ignore(lexer);

	while (true) {
		char chr = next(lexer);

		if (chr == '\0') {
			emit(lexer, JSON_TOK_ERROR);
			return NULL;
		}

		if (chr == '\\') {
			switch (next(lexer)) {
			case '"':
			case '\\':
			case '/':
			case 'b':
			case 'f':
			case 'n':
			case 'r':
			case 't':
				continue;
			case 'u':
				if (!isxdigit(next(lexer))) {
					goto error;
				}

				if (!isxdigit(next(lexer))) {
					goto error;
				}

				if (!isxdigit(next(lexer))) {
					goto error;
				}

				if (!isxdigit(next(lexer))) {
					goto error;
				}

				break;
			default:
				goto error;
			}
		}

		if (chr == '"') {
			backup(lexer);
			emit(lexer, JSON_TOK_STRING);

			next(lexer);
			ignore(lexer);

			return lexer_json;
		}
	}

error:
	emit(lexer, JSON_TOK_ERROR);
	return NULL;
}

static void *lexer_boolean(struct lexer *lexer)
{
	backup(lexer);

	switch (next(lexer)) {
	case 't':
		if (next(lexer) != 'r') {
			goto error;
		}

		if (next(lexer) != 'u') {
			goto error;
		}

		if (next(lexer) != 'e') {
			goto error;
		}

		emit(lexer, JSON_TOK_TRUE);
		return lexer_json;
	case 'f':
		if (next(lexer) != 'a') {
			goto error;
		}

		if (next(lexer) != 'l') {
			goto error;
		}

		if (next(lexer) != 's') {
			goto error;
		}

		if (next(lexer) != 'e') {
			goto error;
		}

		emit(lexer, JSON_TOK_FALSE);
		return lexer_json;
	}

error:
	emit(lexer, JSON_TOK_ERROR);
	return NULL;
}

static void *lexer_null(struct lexer *lexer)
{
	if (next(lexer) != 'u') {
		goto error;
	}

	if (next(lexer) != 'l') {
		goto error;
	}

	if (next(lexer) != 'l') {
		goto error;
	}

	emit(lexer, JSON_TOK_NULL);
	return lexer_json;

error:
	emit(lexer, JSON_TOK_ERROR);
	return NULL;
}


static void *lexer_number(struct lexer *lexer)
{
	while (true) {
		char chr = next(lexer);

		if (isdigit(chr) || chr == '.') {
			continue;
		}

		backup(lexer);
		emit(lexer, JSON_TOK_NUMBER);

		return lexer_json;
	}
}

static void *lexer_json(struct lexer *lexer)
{
	while (true) {
		char chr = next(lexer);

		switch (chr) {
		case '\0':
			emit(lexer, JSON_TOK_EOF);
			return NULL;
		case '}':
		case '{':
		case '[':
		case ']':
		case ',':
		case ':':
			emit(lexer, (enum json_tokens)chr);
			return lexer_json;
		case '"':
			return lexer_string;
		case 'n':
			return lexer_null;
		case 't':
		case 'f':
			return lexer_boolean;
		case '-':
			if (isdigit(peek(lexer))) {
				return lexer_number;
			}

			/* fallthrough */
		default:
			if (isspace(chr)) {
				continue;
			}

			if (isdigit(chr)) {
				return lexer_number;
			}

			emit(lexer, JSON_TOK_ERROR);
			return NULL;
		}
	}
}

static void lexer_init(struct lexer *lexer, char *data, size_t len)
{
	lexer->state = lexer_json;
	lexer->start = data;
	lexer->pos = data;
	lexer->end = data + len;
	lexer->token.type = JSON_TOK_NONE;
}

static int obj_init(struct json_obj *json, char *data, size_t len)
{
	struct token token;

	lexer_init(&json->lexer, data, len);

	if (!lexer_next(&json->lexer, &token)) {
		return -EINVAL;
	}

	if (token.type != JSON_TOK_OBJECT_START) {
		return -EINVAL;
	}

	return 0;
}

static int element_token(enum json_tokens token)
{
	switch (token) {
	case JSON_TOK_OBJECT_START:
	case JSON_TOK_LIST_START:
	case JSON_TOK_STRING:
	case JSON_TOK_NUMBER:
	case JSON_TOK_TRUE:
	case JSON_TOK_FALSE:
		return 0;
	default:
		return -EINVAL;
	}
}

static int obj_next(struct json_obj *json,
		    struct json_obj_key_value *kv)
{
	struct token token;

	if (!lexer_next(&json->lexer, &token)) {
		return -EINVAL;
	}

	/* Match end of object or next key */
	switch (token.type) {
	case JSON_TOK_OBJECT_END:
		kv->key = NULL;
		kv->key_len = 0;
		kv->value = token;

		return 0;
	case JSON_TOK_COMMA:
		if (!lexer_next(&json->lexer, &token)) {
			return -EINVAL;
		}

		if (token.type != JSON_TOK_STRING) {
			return -EINVAL;
		}

		/* fallthrough */
	case JSON_TOK_STRING:
		kv->key = token.start;
		kv->key_len = (size_t)(token.end - token.start);
		break;
	default:
		return -EINVAL;
	}

	/* Match : after key */
	if (!lexer_next(&json->lexer, &token)) {
		return -EINVAL;
	}

	if (token.type != JSON_TOK_COLON) {
		return -EINVAL;
	}

	/* Match value */
	if (!lexer_next(&json->lexer, &kv->value)) {
		return -EINVAL;
	}

	return element_token(kv->value.type);
}

static int arr_next(struct json_obj *json, struct token *value)
{
	if (!lexer_next(&json->lexer, value)) {
		return -EINVAL;
	}

	if (value->type == JSON_TOK_LIST_END) {
		return 0;
	}

	if (value->type == JSON_TOK_COMMA) {
		if (!lexer_next(&json->lexer, value)) {
			return -EINVAL;
		}
	}

	return element_token(value->type);
}

static int decode_num(const struct token *token, int32_t *num)
{
	/* FIXME: strtod() is not available in newlib/minimal libc,
	 * so using strtol() here; this means no floating point
	 * numbers.
	 */
	char *endptr;
	char prev_end;

	prev_end = *token->end;
	*token->end = '\0';

	errno = 0;
	*num = strtol(token->start, &endptr, 10);

	*token->end = prev_end;

	if (errno != 0) {
		return -errno;
	}

	if (*endptr) {
		return -EINVAL;
	}

	return 0;
}

static bool equivalent_types(enum json_tokens type1, enum json_tokens type2)
{
	if (type1 == JSON_TOK_TRUE || type1 == JSON_TOK_FALSE) {
		return type2 == JSON_TOK_TRUE || type2 == JSON_TOK_FALSE;
	}

	return type1 == type2;
}

static int obj_parse(struct json_obj *obj,
		     const struct json_obj_descr *descr, size_t descr_len,
		     void *val);
static int arr_parse(struct json_obj *obj,
		     const struct json_obj_descr *elem_descr,
		     size_t max_elements, void *field, void *val);

static int decode_value(struct json_obj *obj,
			const struct json_obj_descr *descr,
			struct token *value, void *field, void *val)
{

	if (!equivalent_types(value->type, descr->type)) {
		return -EINVAL;
	}

	switch (descr->type) {
	case JSON_TOK_OBJECT_START:
		return obj_parse(obj, descr->sub_descr,
				 descr->sub_descr_len,
				 field);
	case JSON_TOK_LIST_START:
		return arr_parse(obj, descr->element_descr,
				 descr->n_elements, field, val);
	case JSON_TOK_FALSE:
	case JSON_TOK_TRUE: {
		bool *value = field;

		*value = descr->type == JSON_TOK_TRUE;

		return 0;
	}
	case JSON_TOK_NUMBER: {
		int32_t *num = field;

		return decode_num(value, num);
	}
	case JSON_TOK_STRING: {
		char **str = field;

		*value->end = '\0';
		*str = value->start;

		return 0;
	}
	default:
		return -EINVAL;
	}
}

static ptrdiff_t get_elem_size(const struct json_obj_descr *descr)
{
	switch (descr->type) {
	case JSON_TOK_NUMBER:
		return sizeof(int32_t);
	case JSON_TOK_STRING:
		return sizeof(char *);
	case JSON_TOK_TRUE:
	case JSON_TOK_FALSE:
		return sizeof(bool);
	case JSON_TOK_LIST_START:
		return descr->n_elements * get_elem_size(descr->element_descr);
	case JSON_TOK_OBJECT_START: {
		ptrdiff_t total = 0;
		size_t i;

		for (i = 0; i < descr->sub_descr_len; i++) {
			total += get_elem_size(&descr->sub_descr[i]);
		}

		return total;
	}
	default:
		return -EINVAL;
	}
}

static int arr_parse(struct json_obj *obj,
		     const struct json_obj_descr *elem_descr,
		     size_t max_elements, void *field, void *val)
{
	ptrdiff_t elem_size = get_elem_size(elem_descr);
	void *last_elem = (char *)field + elem_size * max_elements;
	size_t *elements = (size_t *)((char *)val + elem_descr->offset);
	struct token value;

	assert(elem_size > 0);

	*elements = 0;

	while (!arr_next(obj, &value)) {
		if (value.type == JSON_TOK_LIST_END) {
			return 0;
		}

		if (decode_value(obj, elem_descr, &value, field, val) < 0) {
			return -EINVAL;
		}

		(*elements)++;

		field = (char *)field + elem_size;
		if (field == last_elem) {
			return -ENOSPC;
		}
	}

	return -EINVAL;
}

static int obj_parse(struct json_obj *obj, const struct json_obj_descr *descr,
		     size_t descr_len, void *val)
{
	struct json_obj_key_value kv;
	int32_t decoded_fields = 0;
	size_t i;
	int ret;

	while (!obj_next(obj, &kv)) {
		if (kv.value.type == JSON_TOK_OBJECT_END) {
			return decoded_fields;
		}

		for (i = 0; i < descr_len; i++) {
			void *decode_field = (char *)val + descr[i].offset;

			/* Field has been decoded already, skip */
			if (decoded_fields & (1 << i)) {
				continue;
			}

			/* Check if it's the i-th field */
			if (kv.key_len != descr[i].field_name_len) {
				continue;
			}

			if (memcmp(kv.key, descr[i].field_name,
				   descr[i].field_name_len)) {
				continue;
			}

			/* Store the decoded value */
			ret = decode_value(obj, &descr[i], &kv.value,
					   decode_field, val);
			if (ret < 0) {
				return ret;
			}

			decoded_fields |= 1<<i;
			break;
		}
	}

	return -EINVAL;
}

int json_obj_parse(char *payload, size_t len,
		   const struct json_obj_descr *descr, size_t descr_len,
		   void *val)
{
	struct json_obj obj;
	int ret;

	assert(descr_len < (sizeof(ret) * CHAR_BIT - 1));

	ret = obj_init(&obj, payload, len);
	if (ret < 0) {
		return ret;
	}

	return obj_parse(&obj, descr, descr_len, val);
}

static const char escapable[] = "\"\\/\b\f\n\r\t";

static int json_escape_internal(char *str, size_t *len, size_t buf_size)
{
	char tmp_buf[buf_size + 1];
	char *cur, *out = tmp_buf, *escape;

	for (cur = str; *cur; cur++) {
		escape = memchr(escapable, *cur, sizeof(escapable) - 1);
		if (escape) {
			*out++ = '\\';
			*out++ = "\"\\/bfnrt"[escape - escapable];
		} else {
			*out++ = *cur;
		}
	}

	*out = '\0';
	*len = out - tmp_buf;
	memcpy(str, tmp_buf, *len);

	return 0;
}

size_t json_calc_escaped_len(const char *str, size_t len)
{
	size_t escaped_len = len;
	size_t pos;

	for (pos = 0; pos < len; pos++) {
		if (memchr(escapable, str[pos], sizeof(escapable) - 1)) {
			escaped_len++;
		}
	}

	return escaped_len;
}

ssize_t json_escape(char *str, size_t *len, size_t buf_size)
{
	size_t escaped_len;

	escaped_len = json_calc_escaped_len(str, *len);

	if (escaped_len == *len) {
		/* If no escape is necessary, don't bother using up temporary
		 * stack space to copy the string.
		 */
		return 0;
	}

	if (escaped_len >= buf_size) {
		return -ENOMEM;
	}

	return json_escape_internal(str, len, escaped_len);
}
