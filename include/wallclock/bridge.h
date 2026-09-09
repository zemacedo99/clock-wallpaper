#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* C ABI for JNI/Swift hosts. Serialize calls; UTF-8 paths; Unix seconds.
   No exceptions cross this boundary. Errors live until the next call. */
typedef struct wc_engine wc_engine;
wc_engine* wc_create(const char* config_path);
void wc_destroy(wc_engine* engine);
const char* wc_error(void);
int wc_render(wc_engine* engine,int64_t now,int width,int height,const char* output_path);
int64_t wc_next_change(wc_engine* engine,int64_t now); /* -1 on error */
#ifdef __cplusplus
}
#endif
