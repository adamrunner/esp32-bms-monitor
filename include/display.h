#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void display_init();
void display_update_basic(float v, float i, float soc, float p, double ewh);
void display_print_line(const char* s);
#ifdef __cplusplus
}
#endif
