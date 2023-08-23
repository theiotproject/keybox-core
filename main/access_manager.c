#include <math.h>
#include "access_manager.h"
#include "esp_log.h"
#include "nvs.h"
#include "string.h"

#define MAX_AC_NUM 100

static const char *access_tag = "access";
static nvs_handle_t access_nvs_handle;
esp_err_t result;

static ac_t acl[MAX_AC_NUM];
static uint8_t acl_counter;

void access_init()
{
	result = nvs_open(access_tag, NVS_READWRITE, &access_nvs_handle);
	if(result != ESP_OK)
    {
		ESP_LOGE(access_tag, "Error opening NVS %d", access_nvs_handle);
		return;
	}
    ESP_LOGI(access_tag, "Successfuly initialized Access to NVS");

    result = nvs_set_i8(access_nvs_handle, "acl_counter", acl_counter);
    if(result != ESP_OK)
    {
		ESP_LOGE(access_tag, "Error with acl counter initialization");
		return;
	}
}

bool access_check_card_id_in_nvs(uint64_t card_id)
{
    ac_t card_id_bytes;
    uint64_t known_card_id = 0;
    uint8_t i, j;
    uint64_t len = (uint64_t) log10(card_id) + 1;
    char card_id_str[len];
    snprintf(card_id_str, len, "%llu", card_id);

    /* check if card is in nvs */
    for (i = 0; i < acl_counter; i++)
    {
        
        for (j = 0; j < 5; j++) 
            known_card_id |= (uint64_t)(acl[i].data[j] << (8 * i)); // compress each byte of the known card id in nvs
        
        if (known_card_id == card_id)
           return true;
    }

    /* save new card in acl */
    for (i = 0; i < 5; i++) 
        acl[acl_counter].data[i] = (uint8_t)(card_id >> (8 * i)); // extract each byte of the card id
    acl[acl_counter].data[SLOTS_BYTE] = 0x00;
    acl_counter++;
    return false;
}

esp_err_t access_get_acl_in_nvs(void)
{
    result = nvs_get_blob(access_nvs_handle, "acl", &acl, MAX_AC_NUM);
    if(result != ESP_OK)
    {
		ESP_LOGE(access_tag, "Error with acl initialization");
		return result;
	}
    return result;
}

esp_err_t access_set_acl_in_nvs(void)
{
    result = nvs_set_blob(access_nvs_handle, "acl", &acl, MAX_AC_NUM);
    if(result != ESP_OK)
    {
        ESP_LOGE(access_tag, "Error while setting acl in nvs");
        return result;
    }
    return result;
}

void access_fill_with_zeros_acl(void)
{
    size_t i;
    for (i = 0; i < MAX_AC_NUM; i++)
    {
        acl[i].data[CARD_ID_BYTE_0] = 0x00;
        acl[i].data[CARD_ID_BYTE_1] = 0x00;
        acl[i].data[CARD_ID_BYTE_2] = 0x00;
        acl[i].data[CARD_ID_BYTE_3] = 0x00;
        acl[i].data[CARD_ID_BYTE_4] = 0x00;
        acl[i].data[SLOTS_BYTE] = 0x00;
    }
    return;
}