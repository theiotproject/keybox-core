#include "ntxfr.h"

#include <string.h>

static const char baseFrameSize = 5;

uint16_t calc_crc(const ntxfr_data_t payload)
{
    uint16_t i;
    uint8_t j;
    uint16_t ui16CRC = 0;

    for ( i = 0; i < payload.len - 2; i++ )
    {
        ui16CRC = (uint16_t)( ui16CRC ^ (payload.ptr[i] << 8) );
        for ( j = 0; j < 8; j++ )
        {
            if ( (ui16CRC & 0x8000U) > 0 )
            {
                ui16CRC = (uint16_t) ( (ui16CRC << 1) ^ 0x1021U );
            }
            else
            {
                ui16CRC = (uint16_t) ( ui16CRC << 1 );
            }
        }
    }
    return ui16CRC;
}

uint8_t get_addr(const uint8_t *payload)
{
	return payload[0];
}


uint8_t get_len(const uint8_t *payload)
{
	return payload[1];
}

uint8_t get_cmd_res(const uint8_t *payload)
{
	return payload[2];
}

uint16_t get_crc(const ntxfr_data_t payload)
{
	uint16_t crc = 0x0000;
	memcpy(&crc, payload.ptr + payload.len - 2, sizeof(crc));
	return crc;
}

const uint8_t *get_data(const uint8_t *payload)
{
	return  payload + 3;
}

uint8_t ntxfr_get_addr(const ntxfr_data_t payload)
{
	uint8_t addr = 0;
	if (payload.len >= baseFrameSize) addr = get_addr(payload.ptr);
	return addr;
}

uint8_t ntxfr_get_cmd(const ntxfr_data_t payload)
{
	uint8_t cmd = 0;
	if (payload.len >= baseFrameSize) cmd = get_cmd_res(payload.ptr);
	return cmd;
}

ntxfr_data_t ntxfr_get_data(const ntxfr_data_t payload)
{
	ntxfr_data_t data = {NULL , 0};

	if (!ntxfr_is_valid(payload))
		return data;

        data.ptr = get_data(payload.ptr);
        data.len = get_len(payload.ptr) - baseFrameSize;
	return data;
}

bool ntxfr_is_valid(const ntxfr_data_t payload)
{
	if (payload.len < baseFrameSize || get_len(payload.ptr) < baseFrameSize)
		return false;
	if (get_crc(payload) != calc_crc(payload))
		return false;
	return true;
}
