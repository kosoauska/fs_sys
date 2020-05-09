/**********************************************************************************************************
 *   Copyright  SinoChip Semiconductor Tech Corp             							  *
 *   All rights reserved.                                                      							  *
 *--------------------------------------------------------------------------------------------------------*
 *   This confidential and proprietary software must be used
 *   only as authorized by a licensing agreement from SCSemicon, Ltd.
 *   All Rights Reserved.
 * * Copyright    : SCSemicon Tech. Co., Ltd
 * * File name    : fs.c
 * * Description  : file system for cos
 * *
 * * Author       : yuxi sun
 * * Version      : 0.1
 * * Date         : 2020.03.16
 * * History      : none
 * *
***********************************************************************************************************/
#include        <demo_cmd.h>
#include        <usb300_main.h>
#include        <usb300_bot.h>
#include        <usb300_scsi.h>
#include        <io_req.h>
#include        <printx.h>
#include        <fs.h>
/**********************************************************************************************************/
#define          FS_TST_DEBUG                (1)
/**********************************************************************************************************/
#define          FS_CONNECT                  (0x00)                                  // ����
#define          FS_RD                       (0x01)                                  // ��ȡ      
#define          FS_WR                       (0x02)                                  // д��
#define          FS_DEL                      (0x03)                                  // ���
#define          FS_CHK                      (0x04)                                  // ����
#define          FS_REV                      (0x05)                                  //
#define          FS_FLASH_RD                 (0x06)                                  // ֱ�Ӷ�ȡPA
#define          FS_CLEAR                    (0x07)                                  // �ļ�ϵͳ���
#define          FS_REBOOT                   (0x08)                                  // оƬ����
#define          FS_GET_FREE                 (0x09)                                  // ��ȡ��������
#define          FS_GET_ID_LEN               (0x10)                                  // ��ȡĳ��ID����
#define          FS_PRINT_LA                 (0x11)                                  // ��ӡLA��
/**********************************************************************************************************/
#define          FS_TEST_W1                  (0x21)                                  // д���쳣���1
#define          FS_TEST_W2                  (0x22)                                  // д���쳣���2
#define          FS_TEST_W3                  (0x23)                                  // д���쳣���3
#define          FS_TEST_W4                  (0x24)                                  // д���쳣���4
#define          FS_TEST_W5                  (0x25)                                  // д���쳣���5
#define          FS_TEST_E1                  (0x31)                                  // ��������1
#define          FS_TEST_E2                  (0x32)                                  // ��������2
#define          FS_TEST_E3                  (0x33)                                  // ��������3
/**********************************************************************************************************/
extern           UINT8                        g_user_buf[2048];                      // demo. user data buffer
extern           UINT32                       fs_pa_free_cnt;                        // free pa cnt;    
static           UINT32                       test_id = 0;
static           UINT32                       test_offset = 0;                       //  write or read offset
UINT32           test_no = 0xFF;                                                    
/**********************************************************************************************************/
MassStorageState    fs_write_hook(UINT8 *buf, UINT32 byte_cnt)
{
    printx(FS_TST_DEBUG , "**fs write hook \n");            
//    fs_write_data(test_id , buf , test_offset , byte_cnt);   
//  ��λ��BUG,ÿ�������ļ����ͣ��ɳ����Լ�ȥ������Ҫ���ĵ����ݣ���ʽ�汾���� 
    fs_write_data(test_id , &buf[test_offset] , test_offset , byte_cnt - test_offset);    
#if 0
    for(UINT32 i = 0 ;i < byte_cnt ; i++)
    {
       printx(FS_TST_DEBUG , "fs_write[%d] = %x \n" , i , buf[i]);
    } 
#endif                
}
/**********************************************************************************************************/
MassStorageState    fs_read_hook(UINT8 *buf , UINT32 byte_cnt)
{
    printx(FS_TST_DEBUG , "**fs read hook id = %d \n" , test_id);   

    memset(buf , 0x00 ,byte_cnt);
    if(FS_SUCCESS != fs_read_data(test_id , buf , test_offset , byte_cnt)) 
    {  
       usb300_reg_epn_stall_set(BOT_IN_EP);
       csw.u8Status = CSW_STATUS_CMD_FAIL;
       u8RequestSenseData[2] = 0x05;   //sense_key
       u8RequestSenseData[12] = 0x21;  //logical block address out of range          
    }  
#if 0     
    for(UINT32 i = 0 ;i < g_scsi_data.trans_length ; i++)
    {
        printx(FS_TST_DEBUG , "fs_read[%d] = %x \n" , i , buf[i]);
    }
#endif                 
}
/**********************************************************************************************************/
MassStorageState    fs_data_read_hook(UINT8 *buf , UINT32 byte_cnt)
{
    printx(FS_TST_DEBUG , "**data read pa no = %d \n" , test_id);   
    norflash_word_read(0x20000 + 4096 * 4 + test_id * 4 * 64 , (UINT32 *)buf , 4 * byte_cnt);
    for(UINT32 i = 0 ;i < g_scsi_data.trans_length ; i++)
    {
        printx(FS_TST_DEBUG , "fs_read[%d] = %x \n" , i , buf[i]);
    }              
}
/**********************************************************************************************************/
MassStorageState    fs_free_read_hook(UINT8 *buf , UINT32 byte_cnt)
{
    UINT32       fs_valid_cap = fs_get_free_capacity() * PA_DATA_OF_PAGE;
    printx(FS_TST_DEBUG , "**free cap =  %d bytes \n" , fs_valid_cap);   
    buf[0] = (fs_valid_cap & 0xFF000000) >> 24;
    buf[1] = (fs_valid_cap & 0x00FF0000) >> 16;
    buf[2] = (fs_valid_cap & 0x0000FF00) >> 8;
    buf[3] = (fs_valid_cap & 0x000000FF);
}

/**********************************************************************************************************/
MassStorageState    fs_id_len_read_hook(UINT8 *buf , UINT32 byte_cnt)
{
    UINT32       fs_id_len = fs_get_id_len(test_id);
    printx(FS_TST_DEBUG , "**id = %d len = %d bytes \n" , test_id , fs_id_len);   
    buf[0] = (fs_id_len & 0xFF000000) >> 24;
    buf[1] = (fs_id_len & 0x00FF0000) >> 16;
    buf[2] = (fs_id_len & 0x0000FF00) >> 8;
    buf[3] = (fs_id_len & 0x000000FF);
}
/**********************************************************************************************************/

MassStorageState fs_test_handler(UINT8 *cdb)
{
    UINT32 byte_cnt;
    MassStorageState ms_state; 
    g_scsi_data.trans_mode = NON_FTL_IO | PIO;              /* note: must be NON_FLT_IO! */
    g_scsi_data.data_address = (UINT32)g_user_buf;          
    g_scsi_data.trans_length = cbw.u32DataTransferLength;
    g_scsi_data.part_len_limit = sizeof(g_user_buf);
    switch ((long long)cdb[1])
    {
         case FS_CONNECT:
        {
            printx(FS_TST_DEBUG , "**fs connect \n");
            ms_state = STATE_CB_DATA_IN;   
            break;
        }            
        
        case FS_RD:
        {
            test_id = cdb[2];
            test_offset = cdb[3];
            printx(FS_TST_DEBUG , "**fs read data len = %d \n" , g_scsi_data.trans_length);    
            printx(FS_TST_DEBUG , "**fs read test id = %d , offset = %d \n" , test_id , test_offset);     
            hook_in = fs_read_hook;        
            ms_state = STATE_CB_DATA_IN;   
            break;
        }
      
        case FS_WR:
        {
            test_id = cdb[2]; 
            test_offset = cdb[3];
            printx(FS_TST_DEBUG , "**fs write data len = %d \n" , g_scsi_data.trans_length);     
            printx(FS_TST_DEBUG , "**fs write test id = %d , offset = %d , len = %d \n" , test_id , test_offset , g_scsi_data.trans_length - test_offset);     
            hook_out = fs_write_hook;     
            ms_state = STATE_CB_DATA_OUT;     
            break;
        }

        case FS_DEL:
        {
            test_id = cdb[2];
            printx(FS_TST_DEBUG , "***fs delete test id = %d \n" , test_id);   
            fs_delete_data(test_id);
            ms_state = STATE_CB_DATA_IN;   
            break;
        }
      
        case FS_CHK:
        {
            test_id = cdb[2];
            if(FS_SUCCESS == fs_check_id(test_id))
            {    
                printx(FS_TST_DEBUG , "***fs check test id = %d exist \n" , test_id); 
            }   
            else
            {
                printx(FS_TST_DEBUG , "***fs check test id = %d not exist \n" , test_id); 
                csw.u8Status = CSW_STATUS_CMD_FAIL;
            }    
            ms_state = STATE_CB_DATA_IN;              
            break;
        }

        case FS_REV:
        {
            printx(FS_TST_DEBUG , "***fs rever test \n"); 
            ms_state = STATE_CB_DATA_IN;        
            break;
        }    
        
        case FS_FLASH_RD:
        {
            test_id = cdb[2];
            printx(FS_TST_DEBUG , "***data read \n"); 
            hook_in = fs_data_read_hook;        
            ms_state = STATE_CB_DATA_IN;    
            break;
        } 
        case FS_CLEAR:
        {
            printx(FS_TST_DEBUG , "***fs clear \n"); 
            fs_clear_all();    
            ms_state = STATE_CB_DATA_IN;        
            break;
        }  

        case FS_REBOOT:
        {
            printx(FS_TST_DEBUG , "***fs reboot \n");  
            chip_reset();    
            ms_state = STATE_CB_DATA_IN;        
            break;
        }  

        case FS_GET_FREE:
        {
            printx(FS_TST_DEBUG , "***fs get free cnt \n");  
            hook_in = fs_free_read_hook;        
            ms_state = STATE_CB_DATA_IN;        
            break;
        }

        case FS_GET_ID_LEN:
        {                
            printx(FS_TST_DEBUG , "***fs get id len \n"); 
            hook_in = fs_id_len_read_hook;    
            ms_state = STATE_CB_DATA_IN;        
            break;
        }      
        
        case FS_PRINT_LA:
        {                
            printx(FS_TST_DEBUG , "***fs print la \n"); 
            fs_print_la();    
            ms_state = STATE_CB_DATA_IN;        
            break;
        }


#if (1 == FS_TEST)       
        case FS_TEST_W1:                                  
        {
            test_no = FS_WRITE_TEST_NO1;
            printx(FS_TST_DEBUG , "***fs write test 1 , old data valid \n");      
            ms_state = STATE_CB_DATA_IN;        
            break;
        } 
               
        case FS_TEST_W2:                                  
        {
            test_no = FS_WRITE_TEST_NO2;
            printx(FS_TST_DEBUG , "***fs write test 2 , old data valid \n");      
            ms_state = STATE_CB_DATA_IN;       
            break;
        }        
                
        case FS_TEST_W3:
        {
            test_no = FS_WRITE_TEST_NO3;
            printx(FS_TST_DEBUG , "***fs write test 3 , old data valid \n");        
            ms_state = STATE_CB_DATA_IN;        
            break;
        }  
                 
        case FS_TEST_W4:
        {
            test_no = FS_WRITE_TEST_NO4;
            printx(FS_TST_DEBUG , "***fs write test 4 , old data valid \n"); 
            ms_state = STATE_CB_DATA_IN;        
            break;
        }              

        case FS_TEST_W5:
        {
            test_no = FS_WRITE_TEST_NO5;
            printx(FS_TST_DEBUG , "***fs write test 5 , new data valid \n"); 
            ms_state = STATE_CB_DATA_IN;        
            break;
        }   

        case FS_TEST_E1:
        {
            test_no = FS_ERASE_TEST_NO1;
            printx(FS_TST_DEBUG , "***fs erase test 1 , need int recover \n"); 
            ms_state = STATE_CB_DATA_IN;        
            break;
        }  
                 
        case FS_TEST_E2:
        {
            test_no = FS_ERASE_TEST_NO2;
            printx(FS_TST_DEBUG , "***fs erase test 2 , need int recover \n"); 
            ms_state = STATE_CB_DATA_IN;        
            break;
        }   
        case FS_TEST_E3:
        {
            test_no = FS_ERASE_TEST_NO3;
            printx(FS_TST_DEBUG , "***fs erase test 3 , do not need recover \n"); 
            ms_state = STATE_CB_DATA_IN;        
            break;
        }   
#endif                         
        default:
        {
            printx(FS_TST_DEBUG , "***invalid fs cmd = %d \n" , cdb[1]);
            ms_state = STATE_CSW;
            break;
        }         
    }
    return ms_state;
}