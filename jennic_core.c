/*
 * Copyright (C) 2015 Focalcrest, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "jennic_core.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
    unsigned char jennic_id;
    unsigned char vendor[32];
    unsigned char type[32];
}_st_jennic_flash_t, *_pst_jennic_flash_t;

typedef struct
{
    unsigned char vendor_id;
    unsigned char type_id;
    _st_jennic_flash_t info;
}_st_flash_t, *_pst_flash_t;

static stjn_wrapper_t _g_wrapper;
static pstjn_wrapper_t _gp_wrapper = &_g_wrapper;

static _st_flash_t _g_flashs[] =
{
    {0x10, 0x10, {0x00, "ST",   "M25P10-A"}},
    {0xBF, 0x49, {0x01, "SST",  "25VF010A"}},
    {0x1f, 0x60, {0x02, "Atmel", "25F512"}},
    {0x1f, 0x65, {0x02, "Atmel", "25F512"}},
    {0x12, 0x12, {0x03, "ST",   "M25P40"}},
    {0x05, 0x05, {0x04, "ST",   "M25P05-A"}},
    {0x11, 0x11, {0x05, "ST",   "M25P20-A"}},
    {0xCC, 0xEE, {0x08, "JN516x", "Inner"}},
};

static _st_jennic_flash_t _unknown_jennic_flash =
{
    0xff, "unknown", "unknown"
};

void jennic_wrapper_init(pstjn_wrapper_t pwrapper)
{
    _g_wrapper = *pwrapper;
}

static inline int _jn_init(int para0, int para1)
{
    return _gp_wrapper->init(para0, para1);
}

static inline int _jn_prepare(void)
{
    return _gp_wrapper->prepare();
}

static inline int _jn_talk(ezb_ll_msg_t stype, pezb_ll_msg_t prtype,
                           u_int32_t* paddr, u_int16_t mlen, u_int8_t sdatalen,
                           u_int8_t *psdata,u_int8_t *prlen, u_int8_t *prbuf)
{
    return _gp_wrapper->talk(stype, prtype, paddr, mlen, sdatalen, psdata, prlen, prbuf);
}


static inline int _jn_fini(void)
{
    return _gp_wrapper->fini();
}

int jennic_init(int para0, int para1)
{
    int ret = 0;
    ret = _jn_init(para0, para1);

    if(0 != ret)
    {
        return -1;
    }

    ret = _jn_prepare();
    return ret;
}

int jennic_fini(void)
{
    return _jn_fini();
}

int jennic_identify_flash(_pst_jennic_flash_t pflash)
{
    ezb_ll_msg_t stype = E_ZB_CMD_FLASH_READ_ID_REQUEST;
    ezb_ll_msg_t rtype = E_ZB_CMD_FLASH_READ_ID_RESPONSE;
    u_int8_t rbuf[16];
    u_int8_t rlen = 16;

    if(0 == _jn_talk(stype, &rtype, NULL, 0, 0, NULL, &rlen, rbuf))
    {
        int ii = 0;
        int num = sizeof(_g_flashs)/sizeof(_st_flash_t);
        u_int8_t status = rbuf[0];
        u_int8_t vendor_id = rbuf[1];
        u_int8_t type_id = rbuf[2];
        if(0 != status)
        {
            printf("jennic: flash status 0x%02x!\n", status);
            return -1;
        }

        for(ii=0; ii<num; ii++)
        {
            if(vendor_id == _g_flashs[ii].vendor_id
               && type_id == _g_flashs[ii].type_id)
            {
                *pflash = _g_flashs[ii].info;
                break;
            }
        }

        if(num == ii)
        {
            *pflash = _unknown_jennic_flash;
        }

        printf("jennic: flash vendor: %s, type: %s, jennic_id: 0x%02x \n", pflash->vendor, pflash->type, pflash->jennic_id);
        return 0;
    }
    else
    {
        return -1;
    }
}

int jennic_select_flash(void)
{
    ezb_ll_msg_t stype = E_ZB_CMD_FLASH_SELECT_TYPE_REQUEST;
    ezb_ll_msg_t rtype = E_ZB_CMD_FLASH_SELECT_TYPE_RESPONSE;
    unsigned char sbuf = 0;
    unsigned char rbuf[16];
    u_int8_t rlen = sizeof(rbuf);

    _st_jennic_flash_t jennic_flash;

    if(0 != jennic_identify_flash(&jennic_flash))
    {
        printf("jennic: identify flash type failed \n");
        return -1;
    }

    if(0xFF == jennic_flash.jennic_id)
    {
        printf("jennic: select flash type unsupported! \n");
        return -1;
    }

    sbuf = jennic_flash.jennic_id;

    if(0 == _jn_talk(stype, &rtype, NULL, 0, 1, &sbuf, &rlen, rbuf))
    {
        if(0 == rbuf[0])
        {
            return 0;
        }
    }

    printf("jennic: could not select detected flash type 0x%02x \n", sbuf);

    return -1;
}

int jennic_change_baudrate(int baudrate)
{
    ezb_ll_msg_t stype = E_ZB_CMD_SET_BAUD_REQUEST;
    ezb_ll_msg_t rtype = E_ZB_CMD_SET_BAUD_RESPONSE;
    u_int8_t bd = (u_int8_t)1000000/baudrate;
    u_int8_t rlen = 16;
    u_int8_t rbuf[16];
    if(0 != _jn_talk(stype, &rtype, NULL, 0, 1, &bd, &rlen, rbuf))
    {
        return -1;
    }

    return rbuf[0];
}

int jennic_write_ram(u_int32_t addr, u_int8_t wlen, u_int8_t* pwbuf)
{
    ezb_ll_msg_t stype = E_ZB_CMD_RAM_WRITE_REQUEST;
    ezb_ll_msg_t rtype = E_ZB_CMD_RAM_WRITE_RESPONSE;
    u_int8_t rlen = 16;
    u_int8_t rbuf[16];
    if(0 != _jn_talk(stype, &rtype, &addr, 0, wlen, pwbuf, &rlen, rbuf))
    {
        return -1;
    }
    return rbuf[0];
}

int jennic_read_ram(u_int32_t addr, u_int16_t len, u_int8_t* prlen, u_int8_t* prbuf)
{
    ezb_ll_msg_t stype = E_ZB_CMD_RAM_READ_REQUEST;
    ezb_ll_msg_t rtype = E_ZB_CMD_RAM_READ_RESPONSE;
    u_int8_t rlen = 255;
    u_int8_t rbuf[255];
    if(0 != _jn_talk(stype, &rtype, &addr, len, 0, NULL, &rlen, rbuf))
    {
        return -1;
    }

    rlen = rlen - 1;

    *prlen = *prlen >= rlen ? rlen:*prlen;
    memcpy(prbuf, &rbuf[1], *prlen);

    return rbuf[0];
}

int jennic_run_ram(u_int32_t addr)
{
    ezb_ll_msg_t stype = E_ZB_CMD_RAM_RUN_REQUEST;
    ezb_ll_msg_t rtype = E_ZB_CMD_RAM_RUN_RESPONSE;
    u_int8_t rlen = 16;
    u_int8_t rbuf[16];
    if(0 != _jn_talk(stype, &rtype, &addr, 0, 0, NULL, &rlen, rbuf))
    {
        return -1;
    }
    return rbuf[0];
}

int jennic_write_flash(u_int32_t addr, u_int8_t wlen, u_int8_t* pwbuf)
{
    ezb_ll_msg_t stype = E_ZB_CMD_FLASH_PROGRAM_REQUEST;
    ezb_ll_msg_t rtype = E_ZB_CMD_FLASH_PROGRAM_RESPONSE;
    u_int8_t rlen = 16;
    u_int8_t rbuf[16];
    if(0 != _jn_talk(stype, &rtype, &addr, 0, wlen, pwbuf, &rlen, rbuf))
    {
        return -1;
    }
    return rbuf[0];
}

int jennic_read_flash(u_int32_t addr, u_int16_t len, u_int8_t* prlen, u_int8_t* prbuf)
{
    ezb_ll_msg_t stype = E_ZB_CMD_FLASH_READ_REQUEST;
    ezb_ll_msg_t rtype = E_ZB_CMD_FLASH_READ_RESPONSE;
    u_int8_t rlen = 255;
    u_int8_t rbuf[255];
    if(0 != _jn_talk(stype, &rtype, &addr, len, 0, NULL, &rlen, rbuf))
    {
        return -1;
    }

    rlen = rlen - 1;

    *prlen = *prlen >= rlen ? rlen:*prlen;
    memcpy(prbuf, &rbuf[1], *prlen);

    return rbuf[0];
}

int jennic_erase_flash(void)
{
    ezb_ll_msg_t stype = E_ZB_CMD_FLASH_ERASE_REQUEST;
    ezb_ll_msg_t rtype = E_ZB_CMD_FLASH_ERASE_RESPONSE;
    u_int8_t rlen = 16;
    u_int8_t rbuf[16];
    if(0 != _jn_talk(stype, &rtype, NULL, 0, 0, NULL, &rlen, rbuf))
    {
        printf("jennic: erase flash failed!, error 0x%02x \n", rbuf[0]);
        return -1;
    }

    return rbuf[0];
}

int jennic_erase_flash_sector(u_int8_t sector)
{
    ezb_ll_msg_t stype = E_ZB_CMD_FLASH_ERASE_REQUEST;
    ezb_ll_msg_t rtype = E_ZB_CMD_FLASH_ERASE_RESPONSE;
    u_int8_t rlen = 16;
    u_int8_t rbuf[16];

    if(0 != _jn_talk(stype, &rtype, NULL, 0, 1, &sector, &rlen, rbuf))
    {
        return -1;
    }

    return rbuf[0];
}

int jennic_set_flash_register(u_int8_t status)
{
    ezb_ll_msg_t stype = E_ZB_CMD_FLASH_WRITE_STATUS_REGISTER_REQUEST;
    ezb_ll_msg_t rtype = E_ZB_CMD_FLASH_WRITE_STATUS_REGISTER_RESPONSE;
    u_int8_t rlen = 16;
    u_int8_t rbuf[16];

    if(0 != _jn_talk(stype, &rtype, NULL, 0, 1, &status, &rlen, rbuf))
    {
        return -1;
    }

    return rbuf[0];
}

int jennic_get_chip_id(u_int32_t* pid)
{
    ezb_ll_msg_t stype = E_ZB_CMD_GET_CHIPID_REQUEST;
    ezb_ll_msg_t rtype = E_ZB_CMD_GET_CHIPID_RESPONSE;
    u_int8_t rlen = 16;
    u_int8_t rbuf[16];
    if(0 != _jn_talk(stype, &rtype, NULL, 0, 0, NULL, &rlen, rbuf))
    {
        return -1;
    }

    if(0 != rbuf[0])
    {
        return rbuf[0];
    }

    *pid = rbuf[1] << 24 | rbuf[2] << 16 | rbuf[3] << 8 | rbuf[4];

    return 0;
}

static u_int32_t jn5139_mac_addrs[2] = {0x00000010, 0x00000010};
static u_int32_t jn5142_mac_addrs[2] = {0x00000010, 0x00000010};
static u_int32_t jn5148_mac_addrs[2] = {0x00000010, 0x00000010};
static u_int32_t jn516X_mac_addrs[2] = {0x01001580, 0x01001570};

int jennic_read_mac(u_int8_t pmac[], ejennic_chip_t chip, int busermac)
{
    u_int8_t mac_len = 8;
    switch(chip)
    {
        case JN_CHIP_JN5139:
        {
            jennic_read_ram(jn5139_mac_addrs[busermac], 8, &mac_len, pmac);
            break;
        }

        case JN_CHIP_JN5142:
        {
            jennic_read_ram(jn5142_mac_addrs[busermac], 8, &mac_len, pmac);
            break;
        }

        case JN_CHIP_JN5148:
        {
            jennic_read_ram(jn5148_mac_addrs[busermac], 8, &mac_len, pmac);
            break;
        }

        case JN_CHIP_JN516X:
        {
            jennic_read_ram(jn516X_mac_addrs[busermac], 8, &mac_len, pmac);
            break;
        }
        default:
        {
            printf("jennic: reading mac failed, unknown chip type! \n");
            break;
        }
    }
    return 0;
}


int jennic_write_mac(u_int8_t pmac[], ejennic_chip_t chip)
{
    u_int8_t mac_len = 8;
    switch(chip)
    {
        case JN_CHIP_JN5139:
        {
            jennic_write_ram(jn5139_mac_addrs[1], mac_len, pmac);
            break;
        }

        case JN_CHIP_JN5142:
        {
            jennic_write_ram(jn5142_mac_addrs[1], mac_len, pmac);
            break;
        }

        case JN_CHIP_JN5148:
        {
            jennic_write_ram(jn5148_mac_addrs[1], mac_len, pmac);
            break;
        }

        case JN_CHIP_JN516X:
        {
            jennic_write_ram(jn516X_mac_addrs[1], mac_len, pmac);
            break;
        }
        default:
        {
            printf("jennic: reading mac failed, unknown chip type! \n");
            break;
        }
    }
    return 0;
}
