#ifndef KEY_SCANNER_ESP32_ACCESS_MANAGER_H
#define KEY_SCANNER_ESP32_ACCESS_MANAGER_H


void access_init();
void access_set_magic(char *field);
void access_check_magic(char *field);

struct key_value_pair
{
	char *ID;
	char *VF;
	char *VT;
	char *L;
	char *S;
};

_Bool access_validate_code(struct key_value_pair pair);

#endif //KEY_SCANNER_ESP32_ACCESS_MANAGER_H