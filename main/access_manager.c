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
	time_t from;
	time_t to;
}time_period_t;

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
bool is_period_valid(time_period_t period)
{
	time_t current_time = time(NULL);
	ESP_LOGI(acces_tag,"Current Unix timestamp: %ld\n", (long)current_time);
	ESP_LOGI(acces_tag, "VF: %ld, VT: %ld\n", (long)period.from, (long)period.to);

	if(current_time >= period.from && current_time <= period.to){
		return true;
	}
	return false;
}
bool access_process_code_open(char *data)
{
	time_period_t period;
	char valueString[sizeof("9223372036854775807")];
	mecfmt_value_t value;

	value = mecfmt_get_velue(data, "VF");
	mecfmt_value_to_string(&value, valueString);
	period.from = (time_t)(atol(valueString));

	value = mecfmt_get_velue(data, "VT");
	mecfmt_value_to_string(&value, valueString);
	period.to = (time_t)(atol(valueString));

	return is_period_valid(period);
}