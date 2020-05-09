/**********************************************************************************************************
 *   Copyright  SinoChip Semiconductor Tech Corp             							  *
 *   All rights reserved.                                                      							  *
 *--------------------------------------------------------------------------------------------------------*
 *   This confidential and proprietary software must be used
 *   only as authorized by a licensing agreement from SCSemicon, Ltd.
 *   All Rights Reserved.
 * * Copyright    : SCSemicon Tech. Co., Ltd
 * * File name    : fs.h
 * * Description  : file system for cos
 * *
 * * Author       : yuxi sun
 * * Version      : 0.1
 * * Date         : 2020.02.24
 * * History      : none
 * *
***********************************************************************************************************/
#ifndef 		   _FS_H
#define 	       _FS_H 
#include 	       <sysdep.h>
#include           <trng.h>
#include           <norflash.h>
/**********************************************************************************************************/
#define            FS_DEBUG             (0)
#define            FS_DEBUG_LV2         (0)
#define            FS_TEST              (1)                                     // Òì³£²âÊÔ²âµã
/**********************************************************************************************************/
void               fs_trim();
void               fs_init(void);
void               fs_clear_all();
UINT32             fs_get_free_capacity();
UINT32             fs_check_id(UINT16 id);
UINT32             fs_get_id_len(UINT32 id);
UINT32             fs_delete_data(UINT16 id);
UINT32             fs_read_data(UINT16 id , UINT8 *data , UINT32 offset , UINT32 len);
UINT32             fs_write_data(UINT16 id , UINT8 *data , UINT32 offset , UINT32 len);
/**********************************************************************************************************/
#define            FS_SUCCESS                   (0x00)
#define            FS_ERROR                     (0x01)
#define            FS_CRC_ERROR                 (0x02)
#define            FS_LA_ERROR                  (0x03)
#define            FS_PA_ERROR                  (0x04)
#define            FS_LEN_ERROR                 (0x05)
#define            FS_LA_NEED_UPDATE            (0x06)
#define            FS_INPUT_ERROR               (0x07)
#define            FS_ID_EXIST                  (0x08)
#define            FS_INIT                      (0xff)
/**********************************************************************************************************/
#define            FS_WRITE_TEST_NO1            (0x11)
#define            FS_WRITE_TEST_NO2            (0x12)
#define            FS_WRITE_TEST_NO3            (0x13)
#define            FS_WRITE_TEST_NO4            (0x14)
#define            FS_WRITE_TEST_NO5            (0x15)
#define            FS_ERASE_TEST_NO1            (0x21)
#define            FS_ERASE_TEST_NO2            (0x22)
#define            FS_ERASE_TEST_NO3            (0x23)
/**********************************************************************************************************/
#define            PA_DATA_OF_PAGE              (208)
#define            PA_REVERSE_DATA              (28)
#define            PA_CRC_OFFSET                (0)
#define            PA_LEV_OFFSET                (4)
#define            PA_LA_OFFSET                 (8)
#define            PA_NLA_OFFSET                (12)
#define            PA_LEN_OFFSET                (16)
#define            PA_DAT_OFFSET                (48)
typedef            struct{
    UINT32   crc;                               // pa crc  
    UINT32   level;                             // level: 00000000 -> MF
                                                //        1000XXXX -> DF
                                                //        1100XXXX -> EF
    UINT32   la;                                // la
    UINT32   next_la;                           // next la
    UINT32   len;                               // valid byte cnt
    UINT8    reverse[PA_REVERSE_DATA];          // reverse
    UINT8    data[PA_DATA_OF_PAGE];             // 52 word = 208 byte data 
}fs_pa_type;
/**********************************************************************************************************/
#endif
