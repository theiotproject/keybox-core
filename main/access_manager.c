#include "access_manager.h"
#include "esp_log.h"
#include "nvs.h"

#define ACL_LEN 100

static const char *access_tag = "access";
static nvs_handle_t access_nvs_handle;
esp_err_t result;

static size_t acl_size = sizeof(ac_t) * ACL_LEN;
ac_t acl[ACL_LEN];
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

    /* load acl from NVS */ 
    access_get_acl_from_nvs();
}

bool access_find_card_id_in_nvs(uint64_t card_id, uint8_t *privilege_to_slots)
{
    ESP_LOGI(access_tag, "Checking card %llu in nvs", card_id);
    int8_t i, j;
    for (i = 0; i < acl_counter; i++)
    {
        uint64_t nvs_card_id = 0;
        for (j = 4; j >= 0; j--)
        {
            /* compress each byte of the known card id in nvs */
            nvs_card_id = (nvs_card_id << 8) | acl[i].data[j]; 
        }
        
        if (nvs_card_id == card_id)
        {
            ESP_LOGI(access_tag, "Found card %llu in nvs", card_id);
            if (!privilege_to_slots)
            {
                ESP_LOGE(access_tag, "Could not get privilege to slots");
                return false;
            }

            *privilege_to_slots = acl[i].data[SLOTS_BYTE];
            return true;
        }
    }

    ESP_LOGI(access_tag, "Could not find card in nvs");
    return false;
}

void access_save_card_id_in_ram(uint64_t card_id, uint8_t privilege_to_slots)
{
    ESP_LOGI(access_tag, "Saving card %llu in ram", card_id);
    int8_t i;
    for (i = 0; i < 5; i++) 
    {
        /* extract each byte of the card id */
        acl[acl_counter].data[i] = (uint8_t)(card_id >> (8 * i)); 
    }

    if (acl_counter < ACL_LEN)
    {
        acl[acl_counter].data[SLOTS_BYTE] = privilege_to_slots;
        acl_counter++;
    }
}

esp_err_t access_get_acl_from_nvs(void)
{
    ESP_LOGI(access_tag, "Fetching acl from nvs");
    result = nvs_get_u8(access_nvs_handle, "acl_counter", &acl_counter);
    if(result != ESP_OK)
    {
		ESP_LOGE(access_tag, "Error with acl counter initialization");
		return result;
	}

    result = nvs_get_blob(access_nvs_handle, "acl", &acl, &acl_size);
    if(result != ESP_OK)
    {
		ESP_LOGE(access_tag, "Error with acl initialization");
		return result;
	}
    
    size_t i;
    for (i = 0; i < acl_counter; i++)
    {
        ESP_LOGD(access_tag, "acl id: %d, card ID: %x, %x, %x, %x, %x, slots: %x", i, acl[i].data[CARD_ID_BYTE_0], acl[i].data[CARD_ID_BYTE_1], acl[i].data[CARD_ID_BYTE_2], acl[i].data[CARD_ID_BYTE_3], acl[i].data[CARD_ID_BYTE_4], acl[i].data[SLOTS_BYTE]);
    } 

    return result;
}

esp_err_t access_set_acl_in_nvs(void)
{
    ESP_LOGI(access_tag, "Saving acl in NVS");
    result = nvs_set_u8(access_nvs_handle, "acl_counter", acl_counter);
    if(result != ESP_OK)
    {
		ESP_LOGE(access_tag, "Error with acl counter initialization");
		return result;
	}

    result = nvs_set_blob(access_nvs_handle, "acl", &acl, acl_size);
    if(result != ESP_OK)
    {
        ESP_LOGE(access_tag, "Error while setting acl in nvs");
        return result;
    }
    nvs_commit(access_nvs_handle);
    return result;
}

void access_fill_with_zeros_acl(void)
{
    size_t i;
    for (i = 0; i < ACL_LEN; i++)
    {
        acl[i].data[CARD_ID_BYTE_0] = 0x00;
        acl[i].data[CARD_ID_BYTE_1] = 0x00;
        acl[i].data[CARD_ID_BYTE_2] = 0x00;
        acl[i].data[CARD_ID_BYTE_3] = 0x00;
        acl[i].data[CARD_ID_BYTE_4] = 0x00;
        acl[i].data[SLOTS_BYTE] = 0x00;
    }
    
    acl_counter = 0;
    return;
}