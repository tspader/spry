typedef unsigned long usize;

#define WEAK __attribute__((weak))

WEAK void* memset(void* d, int c, usize n) {
  unsigned char* p = d;
  while (n--) *p++ = (unsigned char)c;
  return d;
}

WEAK void* memcpy(void* d, const void* s, usize n) {
  unsigned char* a = d; const unsigned char* b = s;
  while (n--) *a++ = *b++;
  return d;
}

WEAK void* memmove(void* d, const void* s, usize n) {
  unsigned char* a = d; const unsigned char* b = s;
  if (a < b) { while (n--) *a++ = *b++; }
  else { a += n; b += n; while (n--) *--a = *--b; }
  return d;
}

WEAK int memcmp(const void* a, const void* b, usize n) {
  const unsigned char* x = a; const unsigned char* y = b;
  while (n--) { if (*x != *y) return *x - *y; x++; y++; }
  return 0;
}

WEAK usize strlen(const char* s) {
  const char* p = s;
  while (*p) p++;
  return (usize)(p - s);
}

__int128 __multi3(__int128 a, __int128 b) { return a * b; }

void* malloc(usize n) { (void)n; return 0; }
void* realloc(void* p, usize n) { (void)p; (void)n; return 0; }
void free(void* p) { (void)p; }

void* fopen(const char* p, const char* m) { (void)p; (void)m; return 0; }
int fclose(void* f) { (void)f; return 0; }
usize fread(void* p, usize s, usize n, void* f) { (void)p; (void)s; (void)n; (void)f; return 0; }
int fseek(void* f, long o, int w) { (void)f; (void)o; (void)w; return -1; }
long ftell(void* f) { (void)f; return -1; }
