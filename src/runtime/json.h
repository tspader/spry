#ifndef JSON_H
#define JSON_H

#include "sp.h"

typedef enum {
  JSON_NULL = 0,
  JSON_BOOL,
  JSON_NUMBER,
  JSON_STRING,
  JSON_ARRAY,
  JSON_OBJECT,
} json_type_t;

typedef struct json_value json_value_t;

struct json_value {
  json_type_t type;
  bool boolean;
  f64 number;
  sp_str_t string;
  sp_str_t key;
  json_value_t* first_child;
  json_value_t* next;
};

typedef struct {
  sp_str_t src;
  u32 pos;
  bool ok;
  sp_mem_t mem;
} json_parser_t;

static void json_skip_ws(json_parser_t* p) {
  while (p->pos < p->src.len) {
    c8 c = p->src.data[p->pos];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->pos++;
    else break;
  }
}

static bool json_expect(json_parser_t* p, c8 c) {
  if (p->pos < p->src.len && p->src.data[p->pos] == c) { p->pos++; return true; }
  p->ok = false;
  return false;
}

static json_value_t* json_new(json_parser_t* p, json_type_t type) {
  json_value_t* v = (json_value_t*)sp_alloc(p->mem, sizeof(json_value_t));
  *v = sp_zero_s(json_value_t);
  v->type = type;
  return v;
}

static sp_str_t json_parse_string(json_parser_t* p) {
  sp_str_t result = sp_zero_s(sp_str_t);
  if (!json_expect(p, '"')) return result;
  c8* buf = (c8*)sp_alloc(p->mem, p->src.len - p->pos);
  u32 n = 0;
  while (p->pos < p->src.len) {
    c8 c = p->src.data[p->pos++];
    if (c == '"') { result.data = buf; result.len = n; return result; }
    if (c == '\\' && p->pos < p->src.len) {
      c8 e = p->src.data[p->pos++];
      c = (e == 'n') ? '\n' : (e == 't') ? '\t' : (e == 'r') ? '\r' : e;
    }
    buf[n++] = c;
  }
  p->ok = false;
  return result;
}

static f64 json_parse_number(json_parser_t* p) {
  bool neg = false;
  if (p->pos < p->src.len && p->src.data[p->pos] == '-') { neg = true; p->pos++; }
  f64 v = 0;
  while (p->pos < p->src.len && p->src.data[p->pos] >= '0' && p->src.data[p->pos] <= '9') {
    v = v * 10 + (f64)(p->src.data[p->pos] - '0');
    p->pos++;
  }
  if (p->pos < p->src.len && p->src.data[p->pos] == '.') {
    p->pos++;
    f64 scale = (f64)0.1;
    while (p->pos < p->src.len && p->src.data[p->pos] >= '0' && p->src.data[p->pos] <= '9') {
      v += (f64)(p->src.data[p->pos] - '0') * scale;
      scale *= (f64)0.1;
      p->pos++;
    }
  }
  return neg ? -v : v;
}

static bool json_match_lit(json_parser_t* p, const c8* lit) {
  u32 start = p->pos;
  u32 i = 0;
  while (lit[i]) {
    if (p->pos >= p->src.len || p->src.data[p->pos] != lit[i]) { p->pos = start; return false; }
    p->pos++; i++;
  }
  return true;
}

static json_value_t* json_parse_value(json_parser_t* p);

static json_value_t* json_parse_array(json_parser_t* p) {
  json_value_t* arr = json_new(p, JSON_ARRAY);
  if (!json_expect(p, '[')) return arr;
  json_skip_ws(p);
  if (p->pos < p->src.len && p->src.data[p->pos] == ']') { p->pos++; return arr; }
  json_value_t* tail = SP_NULLPTR;
  while (p->ok) {
    json_value_t* item = json_parse_value(p);
    if (!p->ok) return arr;
    if (!tail) arr->first_child = item; else tail->next = item;
    tail = item;
    json_skip_ws(p);
    if (p->pos < p->src.len && p->src.data[p->pos] == ',') { p->pos++; json_skip_ws(p); continue; }
    json_expect(p, ']');
    return arr;
  }
  return arr;
}

static json_value_t* json_parse_object(json_parser_t* p) {
  json_value_t* obj = json_new(p, JSON_OBJECT);
  if (!json_expect(p, '{')) return obj;
  json_skip_ws(p);
  if (p->pos < p->src.len && p->src.data[p->pos] == '}') { p->pos++; return obj; }
  json_value_t* tail = SP_NULLPTR;
  while (p->ok) {
    json_skip_ws(p);
    sp_str_t key = json_parse_string(p);
    if (!p->ok) return obj;
    json_skip_ws(p);
    if (!json_expect(p, ':')) return obj;
    json_skip_ws(p);
    json_value_t* val = json_parse_value(p);
    if (!p->ok) return obj;
    val->key = key;
    if (!tail) obj->first_child = val; else tail->next = val;
    tail = val;
    json_skip_ws(p);
    if (p->pos < p->src.len && p->src.data[p->pos] == ',') { p->pos++; continue; }
    json_expect(p, '}');
    return obj;
  }
  return obj;
}

static json_value_t* json_parse_value(json_parser_t* p) {
  json_skip_ws(p);
  if (p->pos >= p->src.len) { p->ok = false; return SP_NULLPTR; }
  c8 c = p->src.data[p->pos];
  if (c == '{') return json_parse_object(p);
  if (c == '[') return json_parse_array(p);
  if (c == '"') {
    json_value_t* v = json_new(p, JSON_STRING);
    v->string = json_parse_string(p);
    return v;
  }
  if (c == '-' || (c >= '0' && c <= '9')) {
    json_value_t* v = json_new(p, JSON_NUMBER);
    v->number = json_parse_number(p);
    return v;
  }
  if (json_match_lit(p, "true"))  { json_value_t* v = json_new(p, JSON_BOOL); v->boolean = true;  return v; }
  if (json_match_lit(p, "false")) { json_value_t* v = json_new(p, JSON_BOOL); v->boolean = false; return v; }
  if (json_match_lit(p, "null"))  return json_new(p, JSON_NULL);
  p->ok = false;
  return SP_NULLPTR;
}

static json_value_t* json_parse(sp_mem_t mem, sp_str_t src) {
  json_parser_t p = { .src = src, .pos = 0, .ok = true, .mem = mem };
  json_value_t* root = json_parse_value(&p);
  if (!p.ok) return SP_NULLPTR;
  return root;
}

static json_value_t* json_get(json_value_t* obj, const c8* key) {
  if (!obj || obj->type != JSON_OBJECT) return SP_NULLPTR;
  for (json_value_t* m = obj->first_child; m; m = m->next) {
    if (sp_str_equal_cstr(m->key, key)) return m;
  }
  return SP_NULLPTR;
}

#endif
