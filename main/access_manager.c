#include "access_manager.h"
#include "esp_log.h"
#include "nvs.h"

#define ACL_SIZE 100

static const char *access_tag = "access";
static nvs_handle_t access_nvs_handle;
esp_err_t result;

static size_t acl_size = ACL_SIZE;
ac_t acl[ACL_SIZE];
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
    access_fill_with_zeros_acl();
    access_set_acl_in_nvs();
}

bool access_check_card_id_in_nvs(uint64_t card_id)
{
    int8_t i, j;
    
    ESP_LOGD(access_tag, "Checking card in NVS");
    /* check if card is in nvs */
    for (i = 0; i < acl_counter; i++)
    {
        uint64_t known_card_id = 0;
        for (j = 4; j >= 0; j--)
        {
            known_card_id = (known_card_id << 8) | acl[i].data[j]; // compress each byte of the known card id in nvs
        }
        ESP_LOGD(access_tag, "new card ID: %llu, known card ID: %llu", card_id, known_card_id);
        
        if (known_card_id == card_id)
        {
            ESP_LOGD(access_tag, "Found card in NVS");
            return true;
        }
    }

    ESP_LOGD(access_tag, "Card not found, saving card in NVS");
    /* save new card in acl */
    for (i = 0; i < 5; i++) 
    {
        acl[acl_counter].data[i] = (uint8_t)(card_id >> (8 * i)); // extract each byte of the card id
    }
    acl[acl_counter].data[SLOTS_BYTE] = 0x0;
    acl_counter++;
    
    return false;
}


/* get state of card id acl blob with acl counter */
esp_err_t access_get_acl_in_nvs(void)
{
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
    for (i = 0; i < acl_size; i++)
    {
        ESP_LOGD(access_tag, "acl id: %d, bytes: %d, %d, %d, %d, %d", i, acl[i].data[CARD_ID_BYTE_0], acl[i].data[CARD_ID_BYTE_1], acl[i].data[CARD_ID_BYTE_2], acl[i].data[CARD_ID_BYTE_3], acl[i].data[CARD_ID_BYTE_4]);
    } 

    return result;
}

/* save current state of card id acl blob with acl counter */
esp_err_t access_set_acl_in_nvs(void)
{
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
    for (i = 0; i < acl_size; i++)
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