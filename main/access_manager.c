#include "access_manager.h"
#include "report_manager.h"
#include "esp_log.h"
#include "nvs.h"
#include "ui_manager.h"
#include "string.h"
#include <time.h>

static const char *acces_tag = "access";
static nvs_handle_t conf_nvs_handle;
esp_err_t result;
report_data_t report_data;
char magic [40];
void access_init()
{
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
	if (result != ESP_OK || magic_len > sizeof(magic))
		return;
	if(nvs_get_str(conf_nvs_handle, "magic", magic,&magic_len) != ESP_OK)
		return;
	ESP_LOGI(acces_tag, "magic: %s", magic);
}
void access_check_magic(char *field)
{
	ESP_LOGI(acces_tag, "got magic");
	if(!field)
		return;
	size_t field_len = strlen(field);
	if(!field_len || field_len != 36)
		return;
	report_data.when = 0;
	report_data.kind = REPORT_KIND_OPEN;
	if(strcmp(field, magic) == 0)
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
void access_set_magic(char *field)
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
}
bool access_validate_code(struct key_value_pair pair)
{
	time_t current_time = time(NULL);
	ESP_LOGI(acces_tag,"Current Unix timestamp: %ld\n", (long)current_time);
	ESP_LOGI(acces_tag, "ID: %s, VF: %s, VT: %s, L: %s, S: %s\n", pair.ID, pair.VF, pair.VT, pair.L, pair.S);
	long timestamp_vf_string = atol(pair.VF);
	long timestamp_vt_string = atol(pair.VT);
	time_t timestamp_vf_unix = (time_t)timestamp_vf_string;
	time_t timestamp_vt_unix = (time_t)timestamp_vt_string;
	ESP_LOGI(acces_tag, "timestampy: %ld ||| %ld", timestamp_vf_unix, timestamp_vt_unix);
	if(current_time >= timestamp_vf_unix && current_time <= timestamp_vt_unix){
		return true;
	}
	return false;
}