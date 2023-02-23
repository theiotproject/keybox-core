#include "access_manager.h"
#include "report_manager.h"
#include "esp_log.h"
#include "nvs.h"
#include "ui_manager.h"
#include "string.h"
#include "mecfmt.h"
#include <time.h>

static const char *acces_tag = "access";
static nvs_handle_t conf_nvs_handle;
esp_err_t result;
report_data_t report_data;
char magic [40];
typedef struct
{
	char *VF;
	char *VT;
}valid_period;

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
bool access_check_magic(char *field)
{
	ESP_LOGI(acces_tag, "got magic");
	if(!field)
		return false;
	size_t field_len = strlen(field);
	if(!field_len || field_len != 36)
		return false;
	if(strcmp(field, magic) == 0)
	{
		return true;
	}
	else
	{
		return false;
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
bool check_validity_period(valid_period period)
{
	time_t current_time = time(NULL);
	ESP_LOGI(acces_tag,"Current Unix timestamp: %ld\n", (long)current_time);
	ESP_LOGI(acces_tag, "VF: %s, VT: %s\n", period.VF, period.VT);
	long timestamp_vf_string = atol(period.VF);
	long timestamp_vt_string = atol(period.VT);
	time_t timestamp_vf_unix = (time_t)timestamp_vf_string;
	time_t timestamp_vt_unix = (time_t)timestamp_vt_string;
	ESP_LOGI(acces_tag, "timestampy: %ld ||| %ld", timestamp_vf_unix, timestamp_vt_unix);
	if(current_time >= timestamp_vf_unix && current_time <= timestamp_vt_unix){
		return true;
	}
	return false;
}
bool access_process_code_open(char *data)
{
	char *data1= strdup(data);
	valid_period period;
	char valueString[128];
	mecfmt_value_t value;
	const char *keys[] = { "VF", "VT"};
	for (unsigned int i = 0; i < sizeof(keys) / sizeof(char *); i++) {
		value = mecfmt_get_velue(data1, keys[i]);
		mecfmt_value_to_string(&value, valueString);
		switch (i) {
			case 0:
				period.VF = strdup(valueString);
				break;
			case 1:
				period.VT = strdup(valueString);
				break;
		}
	}
	if(check_validity_period(period)){
		free(period.VF);
		free(period.VT);
		return true;
	}
	return false;
}