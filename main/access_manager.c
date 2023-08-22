#include "access_manager.h"
#include "esp_log.h"
#include "nvs.h"
#include "string.h"

#define CARD_ID_KEY "card_id"

static const char *access_tag = "access";
static nvs_handle_t conf_nvs_handle;
esp_err_t result;
uint64_t card_id;

void access_init()
{
	result = nvs_open(access_tag, NVS_READWRITE, &conf_nvs_handle);
	if(result != ESP_OK)
    {
		ESP_LOGE(access_tag, "Error opening NVS %d",result);
		return;
	}
    ESP_LOGI(access_tag, "Successfuly initialized Access to NVS");

}

// bool access_check_card_id(uint64_t *card_id)
// {
	// ESP_LOGI(acces_tag, "got magic");
	// if(!field)
	// 	return false;
	// size_t field_len = strlen(field);
	// if(!field_len || field_len != 36)
	// 	return false;
	// if(strcmp(field, magic) == 0)
	// {
	// 	return true;
	// }
	// else
	// {
	// 	return false;
	// }
// }

// get card_id from nvs
// void access_get_card_id(char *field)
// {
    // result = nvs_get_str(conf_nvs_handle, CARD_ID_KEY, magic,&magic_len);
	// if(result != ESP_OK)
    // {
	// 	ESP_LOGE(acces_tag, "Error getting card_id from NVS %d",result);
	// 	return;
    // }

	// ESP_LOGI(acces_tag, "card_id: %s", card_id);

	// ESP_LOGI("set magic","%s",field );
	// if(!field)
	// 	return;
	// size_t field_len = strlen(field);
	// if(!field_len || field_len != 36)
	// 	return;
	// ESP_LOGI(acces_tag,"dobry magic %s", field);
	// ESP_ERROR_CHECK(nvs_set_str(conf_nvs_handle, "magic", field));
	// ESP_ERROR_CHECK(nvs_commit(conf_nvs_handle));
// }

// save card_id in nvs
// void access_save_card_id(char *field)
// {
	// ESP_LOGI("set magic","%s",field );
	// if(!field)
	// 	return;
	// size_t field_len = strlen(field);
	// if(!field_len || field_len != 36)
	// 	return;
	// ESP_LOGI(acces_tag,"dobry magic %s", field);
	// ESP_ERROR_CHECK(nvs_set_str(conf_nvs_handle, "magic", field));
	// ESP_ERROR_CHECK(nvs_commit(conf_nvs_handle));
// }