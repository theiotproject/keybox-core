#include "access_manager.h"

static const char *acces_tag = "access";
static nvs_handle_t conf_nvs_handle;
esp_err_t result;
report_data_t report_data;
char magic [40];
void access_init()
{
	ESP_LOGI(acces_tag, "dupa123");
	esp_err_t result = nvs_open(acces_tag, NVS_READWRITE, &conf_nvs_handle);
	if(result != ESP_OK){
		ESP_LOGE(acces_tag, "Error opening NVS %d",result);
		return;
	}
	size_t magic_len = 0;
	ESP_LOGI(acces_tag, "1");
	result = nvs_get_str(conf_nvs_handle, "magic", NULL,&magic_len);
	if(result != ESP_OK){
		ESP_LOGE(acces_tag, "Error getting magic len %d",result);
		nvs_close(conf_nvs_handle);
		return;
	}
	ESP_LOGI(acces_tag, "resultt: %d", result);
	ESP_LOGI(acces_tag, "size: %zu", magic_len);
	ESP_LOGI(acces_tag, "magic size: %d", sizeof(magic));
	if (result != ESP_OK || magic_len > sizeof(magic))
		return;
	if(nvs_get_str(conf_nvs_handle, "magic", magic,&magic_len) != ESP_OK)
		return;
	ESP_LOGI(acces_tag, "magic: %s", magic);
}
void check_magic(char *field)
{
	ESP_LOGI(acces_tag, "field %s", field);
	ESP_LOGI(acces_tag, "got magic");
	ESP_LOGI(acces_tag, "dupa1234");
	if(!field)
		return;
	size_t field_len = strlen(field);
	ESP_LOGI(acces_tag, "dupa12345");
	if(!field_len || field_len != 36)
		return;
	report_data.when = 0;
	report_data.kind = REPORT_KIND_OPEN;
	ESP_LOGI(acces_tag, "dupa123456");
	if(strcmp(field, magic))
	{
		report_data.data.open.access = true;
		report_add(&report_data);
		ui_rg_beep_open(UI_ACCESS_GRANTED);
		ESP_LOGI(acces_tag, "Access granted using code: %s", field);
	}
	else
	{
		report_data.data.open.access = false;
		report_add(&report_data);
		ui_rg_beep_open(UI_ACCESS_DENIED);
		ESP_LOGI(acces_tag, "Access denied using code: %s", magic);
	}
}
void set_magic(char *field)
{
	ESP_LOGI("set magic","%s",field );
	if(!field)
		return;
	size_t field_len = strlen(field);
	if(!field_len || field_len != 36)
		return;
	ESP_LOGI(acces_tag,"dobry magic %s", field);
	ESP_ERROR_CHECK(nvs_set_str(conf_nvs_handle, "magic", field));
	ESP_ERROR_CHECK(nvs_commit(conf_nvs_handle));
	ui_rg_beep_open(UI_ACCESS_GRANTED);
}