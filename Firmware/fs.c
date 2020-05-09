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
 * * Date         : 2020.02.24
 * * History      : none
 * *
***********************************************************************************************************/
#include           "fs.h"                   
#include           "printx.h"
#include           "minilib.h"
/**********************************************************************************************************/
#define            FS_LA_START                  (0x18000)
#define            FS_LA_CNT                    (4)
#define            FS_LA_BLOCK                  (4096)              // cnt by bytes
#define            FS_LA_PAGE                   (512)               // cnt by word
#define            FS_PA_START                  (FS_LA_START + FS_LA_CNT * FS_LA_BLOCK)
#define            FS_PA_END                    (FS_PA_START + 0x18000)
#define            FS_PA_MAX_NO                 (FS_PA_END - FS_PA_START) / 256    // 256 byte is 1 pa block 
#define            FS_PA_PAGE                   (64)                // cnt by word
#define            FS_LA_MAX_NO                 (509)               // max la mapping
#define            FS_LA_DATA_MAX_OFFSET        (FS_LA_MAX_NO * 4)  // cnt by bytes
#define            FS_LA_CNT_OFFSET             (510 * 4)           // cnt by bytes
#define            FS_LA_CRC_OFFSET             (511 * 4)           // cnt by bytes
#define            FS_BLANK_DATA                (0xffffffff)        // no used data
#define            FS_ID_MAX                    (0xFFF0)            // ���ID���
#define            FS_MID_LIST                  (0xFFFE)            // ĳID���м�����
#define            FS_END_LIST                  (0xFFFD)            // ĳID�Ľ������� 
#define            FS_RANDOM_PA                 (20)                // ���������ЧPA����
#define            FS_MASK_H16(input)           ((input & 0xffff0000) >> 16)
#define            FS_MASK_L16(input)           (input  & 0x0000ffff)
#define            FS_SET_H16(input)            (input  | 0xffff0000)
#define            FS_CLR_H16(input)            (input  & 0x0000ffff)
#define            FS_SET_L16(input)            (input  | 0x0000ffff)                            
#define            FS_CLR_L16(input)            (input  & 0xffff0000)
#define            FS_SET_BIT(input , bit)      (input  | (1 << bit))
#define            FS_CLR_BIT(input , bit)      (input  & (~(1 << bit)))
#define            FS_BYTE_TO_WORD(bytes)       (bytes >> 2)
#define            FS_WORD_TO_BYTE(bytes)       (bytes << 2)
#define            FS_GET_PA(input)             FS_MASK_H16(input)
#define            FS_GET_ID(input)             FS_MASK_L16(input)
#define            FS_NOUSED                    (0xff)
#define            FS_USED                      (0x00) 
/**********************************************************************************************************/
#define            FS_FUNC_GET_ID_LEN           (0x00)
#define            FS_FUNC_GET_ID_LA            (0x01)
/**********************************************************************************************************/
#define            FS_NULL                      ((void *)0)
/**********************************************************************************************************/   
#define            FS_POS_DEBUG(cond , ...)                         \
do{                        											\
	if(cond)														\
	{																\
	    printf(__VA_ARGS__);									    \
	    printf("at line : %d , fun : %s \n" , __LINE__ , __func__);	\
	}  																\
}while(0)
/**********************************************************************************************************/
static  void       fs_recover();
static  UINT32     fs_crc(UINT8 *data , UINT32 len);
static  UINT32     fs_scan_la(UINT32  *la_table);
static  UINT32     fs_scan_pa(UINT32  *input_data , UINT32  *output_data);

static  void       fs_load_la(UINT32 la_addr , UINT32  *la_table);
static  UINT32     fs_load_pa(UINT32  *la_table);

static  void       fs_update_la(UINT32  *la_table);
static	void       fs_update_la_crc(UINT32  *la_table);

static  UINT32     fs_get_next_la_map(UINT32  fs_current_pos);
static  UINT32     fs_set_pa_map(UINT32 pa_no , UINT32 *output_data);

static  UINT32     fs_find_max_la_cnt(UINT32 *fs_max_cnt_addr);
static  UINT32     fs_find_sel_la_cnt(UINT32 select_la_cnt , UINT32 *fs_select_cnt_addr);

static  UINT32     fs_check_crc(UINT32  *input_data , UINT32 input_len , UINT32 crc);
static  UINT32     fs_check_data(UINT32  *invald_pa , UINT32 len , UINT32 check_flag);

static  UINT32     fs_get_free_pa(UINT32 *pa_elem);
static  UINT32     fs_get_free_la(UINT32  *valid_la_offset , UINT32 cnt);

static  void       fs_clear_id(UINT16 id);
static  UINT32     fs_enum_la(UINT16 id , UINT32 *la_no , UINT32 *file_len , UINT32 func);
/**********************************************************************************************************/
static  UINT32     fs_test_point(UINT32 point_num);
static  UINT32     fs_last_poinner = 0; 
static  UINT32     fs_la_max_cnt = 0;
static  UINT32     fs_need_recover = 0;
/**********************************************************************************************************/
extern  UINT32     test_no;                                                             // �����
extern  UINT32     fs_w_test;
extern  UINT32     fs_e_test;
#if     (1 == FS_DEBUG_LV2)                                            
static  UINT32     fs_read_test[FS_LA_PAGE];                                        
#endif
/**********************************************************************************************************/
__attribute__((aligned(4)))    static  UINT32      fs_la_table[FS_LA_PAGE];             // la table: store la -> pa mapping
__attribute__((aligned(4)))    static  UINT8       fs_pa_table[FS_PA_MAX_NO];           // pa table: store which pa page is used speed up
__attribute__((aligned(4)))    static  UINT32      fs_pa_free_cnt = FS_PA_MAX_NO;       // free pa cnt;       
/**********************************************************************************************************/
static  UINT32     fs_lv2_debug(UINT8 *title , UINT32  *input , UINT32 cnt , UINT32 invalid)
{
#if (1 == FS_DEBUG_LV2)   
    printf("title %s : \n" , title);
    for(UINT32 k = 0 ; k < cnt ; k++)
    {
        if(input[k] != invalid)
           printf("read data[%d] = %x \n" , k , input[k]);
    }
#endif
}
/**********************************************************************************************************
  * @brief         ��ȡ�������LA���е�λ��
  * @param  
  * @retval        valid_la_offset: ��LA���п��е�λ�ñ��(0 , 1 , 2 ... FS_LA_MAX_NO��
***********************************************************************************************************/
static  UINT32     fs_get_free_la(UINT32  *valid_la_offset , UINT32 cnt)
{
    UINT32  j = 0; 
    UINT32  result = FS_ERROR;
    UINT32  free_cnt = 0;
    for(UINT32 i = 0 ; i < FS_LA_MAX_NO ; i++)
    {
        if(FS_BLANK_DATA == fs_la_table[i])
        {
            printx(FS_DEBUG , "la offset = %d valid \n" , i);            
            valid_la_offset[free_cnt++] = i;
            if(free_cnt == cnt)
            {
                result = FS_SUCCESS;
                break;
            }
        }
    }
    if(FS_SUCCESS != result)
    {
        printx(FS_DEBUG , "la full \n");            
        return FS_ERROR;        
    } 
    free_cnt = 0;
    for(UINT32 i = 1 ; i < FS_PA_MAX_NO ; i++)
    {
        if(FS_NOUSED == fs_pa_table[i])
        {
            printx(FS_DEBUG , "pa offset = %d valid \n" , i);            
            free_cnt++;
            if(free_cnt == cnt)
                return FS_SUCCESS;
        }
    }
    return FS_ERROR;
}
/**********************************************************************************************************
  * @brief         ��ȡһ�����Ը�������LA���NORFLASHλ�ã�����FS_PA_START��ع�
  * @param         fs_current_pos: ��ǰ����λ��(0 , 1 , 2 ... )
  * @retval        ��һ������λ��(0 , 1 , 2 ... )
***********************************************************************************************************/
static  UINT32     fs_get_next_la_map(UINT32  fs_current_pos)
{
    UINT32  la_next_valid_pos = fs_current_pos;
    UINT32  la_buff[FS_LA_PAGE];
    printx(FS_DEBUG , "current poinner = %d \n" , fs_current_pos);
    for(UINT32 i = FS_WORD_TO_BYTE(FS_LA_PAGE) ; i < (FS_LA_CNT * FS_LA_BLOCK) ; i = i + FS_WORD_TO_BYTE(FS_LA_PAGE))    
    { 
        la_next_valid_pos = la_next_valid_pos + 1; 
        if((FS_LA_START + FS_WORD_TO_BYTE(FS_LA_PAGE) * la_next_valid_pos) >= FS_PA_START)
        {
// ����߽磬�ع� & �����׿�          
            printx(FS_DEBUG , "roll back to start addr \n");
// la��0��ַ���ã�Ϊ�˽�ʡfs_pa_table��RAM�ռ�
            la_next_valid_pos = 1;        
            norflash_erase_sector(FS_LA_START);    
            return la_next_valid_pos;   
        }       
        if(0 == (FS_WORD_TO_BYTE(FS_LA_PAGE) * la_next_valid_pos % FS_LA_BLOCK))                
        {
// ����߽磬 ����            
            norflash_erase_sector(FS_LA_START + 4 * FS_LA_PAGE * la_next_valid_pos);         
            printx(FS_DEBUG , "next addr = %x beyond block , just need erase \n" , la_next_valid_pos);   
            return la_next_valid_pos;
        }
// ��ȡ����
        norflash_word_read(FS_LA_START + FS_WORD_TO_BYTE(FS_LA_PAGE) * la_next_valid_pos , la_buff ,  FS_LA_PAGE);
        printx(FS_DEBUG , "next addr = %x , check data \n" , la_next_valid_pos);
// �ж��Ƿ�����ΪȫFF
        if(FS_SUCCESS == fs_check_data(la_buff , FS_LA_PAGE , FS_BLANK_DATA))         
        {
            printx(FS_DEBUG , "next addr = %x  valid \n" , la_next_valid_pos);  
// ��дλ��                  
            return la_next_valid_pos;
        } 
        else
        {
// ������һ��           
            printx(FS_DEBUG , "next addr = %x  invalid \n" , la_next_valid_pos);    
        }      
    }
}
/**********************************************************************************************************
  * @brief         ����LA��������LAд�������������CRC
  * @param         *la_table����ǰLA��
  * @retval 
***********************************************************************************************************/
static  void       fs_update_la(UINT32  *la_table)
{
    UINT32  la_next_valid_pos = 0; 
//  ��ǰ���һ��д��LA�ļ���
    printx(FS_DEBUG , "current fs la no = %d \n" , fs_la_max_cnt);
//  ��һ����д��ļ���
    fs_la_max_cnt = fs_la_max_cnt + 1;
    la_table[FS_LA_PAGE - 2] = fs_la_max_cnt;                                                         
    printx(FS_DEBUG , "fs la no = %d \n" , fs_la_max_cnt);
//  ��ȡ��FS_LA_START ~ FS_PA_START ��Χ��LA��д�����һ��λ�ñ��
    la_next_valid_pos = fs_get_next_la_map(fs_last_poinner);
//  ���µ�ǰLAд��NORFLASHλ�õ�ָ��
    fs_last_poinner   = la_next_valid_pos;
//  д��LA��
#if  (1 == FS_TEST)
//  д����3 ������Ӧ����ã���LA�ָ�����
        if(FS_WRITE_TEST_NO3 == test_no)
        {
            fs_w_test = FS_LA_PAGE - 10;
            printx(FS_DEBUG , "write test point 3 happen \n");  
        }
#endif
    norflash_word_write(FS_LA_START + FS_WORD_TO_BYTE(FS_LA_PAGE) * fs_last_poinner , la_table , FS_LA_PAGE - 1);   
    printx(FS_DEBUG , "addr = %x updata la without crc\n" , la_next_valid_pos); 
}
/**********************************************************************************************************
  * @brief          ����LA���CRC
  * @param          *la_table����ǰLA��
  * @retval 
***********************************************************************************************************/
static	void       fs_update_la_crc(UINT32  *la_table)
{
    la_table[FS_LA_PAGE - 1] = fs_crc((UINT8 *)la_table , 4 * (FS_LA_PAGE - 1));
#if (FS_DEBUG_LV2 == 1)
    fs_lv2_debug("read la" , la_table , FS_LA_PAGE , FS_BLANK_DATA);
#endif 
    norflash_word_write(FS_LA_START + FS_WORD_TO_BYTE(FS_LA_PAGE) * fs_last_poinner + FS_LA_CRC_OFFSET ,  &la_table[FS_LA_PAGE - 1] , 1);     // updata la crc
    printx(FS_DEBUG , "addr = %x updata crc = %x \n" , fs_last_poinner , la_table[FS_LA_PAGE - 1]); 
}
/**********************************************************************************************************
  * @brief         ��ȡһ����Ч��PA
  * @param  
  * @retval        *pa_elem : ��Ч��PA���(1 , 2 , ....)
***********************************************************************************************************/
static  UINT32     fs_get_free_pa(UINT32 *pa_elem)
{
    UINT16  pa_no = 0;
    trng_generate((UINT8 *)&pa_no , 2);
    if(pa_no >= FS_PA_MAX_NO)
        pa_no = pa_no % FS_PA_MAX_NO; 
    for(UINT32 i = 0 ; i < FS_PA_MAX_NO ; i++)
    { 
// pa��0��ַ���ã�Ϊ�˽�ʡfs_pa_table��RAM�ռ�     
        pa_no++;
        if(pa_no >= FS_PA_MAX_NO)
            pa_no = 1;
        printx(FS_DEBUG , "random test pa no = %d \n" , pa_no);
        if(FS_NOUSED == fs_pa_table[pa_no])
        {
            *pa_elem = pa_no;  
            printx(FS_DEBUG , "pa no = %d can use \n" , pa_no);    
            return FS_SUCCESS;
        }  
    }
    return FS_ERROR;
}

/**********************************************************************************************************
  * @brief         ����PA���������е�LAָ���ж�LA���Ӧ��ָ���Ƿ�һ��
  * @param         pa_la��fs_la_table�еı��
  * @retval        FS_SUCCESS: һ��
                   FS_ERROR  : ��һ�� 
***********************************************************************************************************/
static  UINT32     fs_check_pa_la(UINT32 pa_la)
{
    if(FS_GET_PA(fs_la_table[pa_la]) == pa_la)
    {
        printx(FS_DEBUG , "pa_la = %d  \n" , pa_la);
        return FS_SUCCESS;
    }
    printx(FS_DEBUG ,"pa_la = %d , la offset = %d invalid \n" , pa_la , FS_GET_PA(fs_la_table[pa_la]));
    return FS_ERROR;
}
/**********************************************************************************************************
  * @brief         ����LA���е�ƫ�Ƽ����PA�������ַ
  * @param         la_pa: fs_la_table���еı��
  * @retval        *pa_offset: PA�������ַ
                   FS_SUCCESS: �ɹ�
                   FS_ERROR  : ��Ŵ��� 
***********************************************************************************************************/
static  UINT32     fs_la_to_pa_addr(UINT32 la_no , UINT32 *pa_offset)
{
    UINT32 pa_no = 0;
    if(la_no >= FS_LA_MAX_NO)
    {
        printx(FS_DEBUG , "la no = %d invalid \n" , pa_no);
        return FS_ERROR;   
    }
    pa_no = FS_GET_PA(fs_la_table[la_no]);
    if(pa_no >= FS_PA_MAX_NO)
    {
        printx(FS_DEBUG , "pa no = %d invalid \n" , pa_no);
        return FS_ERROR;   
    }
    *pa_offset = pa_no * FS_WORD_TO_BYTE(FS_PA_PAGE);
    printx(FS_DEBUG , "pa no = %d , pa offset = %x \n" , pa_no , *pa_offset);
    return FS_SUCCESS;   
}
/**********************************************************************************************************
  * @brief         ö��ĳ��ID�ĳ��Ȼ���������PA������
  * @param         id       :  ID��
                   *la_no   �� ��ID��Ӧ��LA���б�
                   *file_len:  ID�ĳ���
                    func    :  FS_FUNC_GET_ID_LEN : *file_len ��Ч
                                                    *la_no    ��Ч 
                               FS_FUNC_GET_ID_LA  : *file_len ��Ч
                                                    *la_no    ��Ч 
  * @retval 
***********************************************************************************************************/
static  UINT32     fs_enum_la(UINT16 id , UINT32 *la_no , UINT32 *file_len , UINT32 func)
{     
    UINT32    fs_next_la = 0;               
    UINT32    fs_offset  = 0;
    UINT32    fs_read_buff[2];
    UINT32    fs_pa_addr = FS_BLANK_DATA;   
    if(id >= FS_ID_MAX)
    {
        printx(FS_DEBUG , "invalid id = %d \n" , id);
        return FS_ERROR;
    }
    for(UINT32 i = 0 ; i < FS_LA_MAX_NO ; i = i + 1)
    {     
        if(FS_GET_ID(fs_la_table[i]) == id)
        {
// ���ҵ�ID,��õ�һ����ַ                                                                
            if(FS_SUCCESS != fs_la_to_pa_addr(i , &fs_pa_addr))
            {    
                FS_POS_DEBUG(1 , "system bug \n");
                while(1);              
            }
// ��ȡPA���ݣ�ֻ��Ҫnext la �� len
            norflash_word_read(FS_PA_START + fs_pa_addr + PA_NLA_OFFSET , fs_read_buff ,  2);    
            fs_next_la = fs_read_buff[0];       
            if(FS_FUNC_GET_ID_LA == func)  
            {        
                la_no[fs_offset++] = i;  
                la_no[fs_offset++] = fs_next_la;  
                printx(FS_DEBUG , "header: pa_addr = %x , start la = %d , next la = %d \n" , fs_pa_addr , i , fs_next_la);       
            } 
            else if(FS_FUNC_GET_ID_LEN == func)
            {
                *file_len  = *file_len + fs_read_buff[1];
                printx(FS_DEBUG , "header: pa_addr = %x , len = %d \n" ,  fs_pa_addr , fs_read_buff[1]);       
            }          
            break;
        }
    } 
    while((FS_END_LIST != fs_next_la) && (fs_next_la < FS_LA_MAX_NO))
    {           
        if(FS_SUCCESS != fs_la_to_pa_addr(fs_next_la , &fs_pa_addr))
        {    
            FS_POS_DEBUG(1 , "system bug \n");
            while(1);              
        } 
// ��ȡ�������� ����LA����                             
        norflash_word_read(FS_PA_START + fs_pa_addr + PA_NLA_OFFSET , fs_read_buff ,  2);        
        fs_next_la = fs_read_buff[0];        
        printx(FS_DEBUG , "body: check id = %d , pa_offset = %x , next la = %d \n" , id , fs_pa_addr , fs_next_la);
        if(FS_FUNC_GET_ID_LA == func)  
        {        
            la_no[fs_offset++] = fs_next_la;  
            printx(FS_DEBUG , "body: pa_addr = %x , next la = %d \n" , fs_pa_addr , fs_next_la);       
        }   
        else if(FS_FUNC_GET_ID_LEN == func)
        {
            *file_len  = *file_len + fs_read_buff[1];
            printx(FS_DEBUG , "body: pa_addr = %x , len = %d \n" ,  fs_pa_addr , fs_read_buff[1]);                  
        }  
    }
    if(FS_FUNC_GET_ID_LEN == func)
        printx(FS_DEBUG , "id = %d , file len = %d \n" , id , *file_len);
    return FS_SUCCESS;
}
/**********************************************************************************************************
  * @brief         ����PA��Ч����
  * @param  
  * @retval        UINT32 *pa_invalid_table : PA�е���Ч���ݱ�, ÿλ��Ӧһ��PA���
                   0x00000001��PA[0]
                   0x00000003��PA[0] PA[1]
                   0x00000007��PA[0] PA[1] PA[2]
***********************************************************************************************************/
static  void       fs_recover(UINT32 *pa_invalid_table)
{
    UINT16   pa_offset_data = 0;
    UINT16   pa_offset_flag = 0;
    UINT16   pa_cnt = 0;
// ÿ16λѭ��һ�Σ�һ��Ԫ����Ҫѭ������
    for(UINT32 i = 0 ; i < 2 * (FS_PA_MAX_NO / 32) ; i++)             
    {       
        pa_offset_flag = i % 2;
        if(0 == pa_offset_flag)
        {
            pa_offset_data = FS_MASK_L16(pa_invalid_table[pa_cnt]);        // ż��ѭ���жϵ�16bit
            printx(FS_DEBUG , "even block[%d] = %x \n" , i , pa_offset_data);
        }
        else
        {
            pa_offset_data = FS_MASK_H16(pa_invalid_table[pa_cnt]);        // ���ѭ���жϸ�16bit
            printx(FS_DEBUG , "odd block[%d] = %x \n" , i , pa_offset_data);
            pa_cnt++;
        }  
// �Ȱ���4KB��ѯ���������4KB����Ҫ���������ÿ����            
        if(pa_offset_data == 0xFFFF)                                        // block����
        {                                                            
           norflash_erase_sector(FS_PA_START + i * NORFLASH_SECTOR_4K);    
        }  
        else
        {
// �������ҳ����          
           for(UINT32 j = 0 ; j < 16 ; j++)
           {
               if(0x0001 == ((pa_offset_data >> j) & 0x0001))
               {
                   printx(FS_DEBUG , "block[%d]: page[%d]: addr = %x : need page erase \n" , i , j , i * NORFLASH_SECTOR_4K + j * NORFLASH_PAGE_256B);
#if  (1 == FS_TEST)
//  �������1,2  �´��ϵ�  PAӦ���л��չ���  ԭ���ݲ���
                   if(FS_ERASE_TEST_NO1 == test_no)
                   {
                       if(j == 0)    
                       {
                          fs_e_test = 1;
                       }
                       printx(FS_DEBUG , "erase test point 1 happen \n");  
                   }  
                   else if(FS_ERASE_TEST_NO2 == test_no)
                   {
                       if(j == 3)
                       {
                          fs_e_test = 3;
                       }
                       printx(FS_DEBUG , "erase test point 2 happen \n");  
                   }
#endif                                   
                   norflash_erase_page(FS_PA_START + i * NORFLASH_SECTOR_4K + j * NORFLASH_PAGE_256B);
               }
           }
        }
    }
#if  (1 == FS_TEST)
//   �������3    �´��ϵ�Ӧ���޻��չ���
     if(FS_ERASE_TEST_NO3 == test_no)
     {
          printx(FS_DEBUG , "erase test point 3 happen \n");  
          chip_reset();
     }  
#endif
    fs_need_recover = 0;
    memset(pa_invalid_table , 0x00  , 4 * FS_PA_MAX_NO / 32);
}
/**********************************************************************************************************
  * @brief         ����CRC
  * @param         
  * @retval 
***********************************************************************************************************/
static  UINT32     fs_crc(UINT8 *data , UINT32 len)
{
    return crc32(0 , data , len);
}
/**********************************************************************************************************
  * @brief         ������Ҫ�����PA��
  * @param         pa_no        : PA��� 
                   *output_data : ��Ч��PA���¼
  * @retval 
***********************************************************************************************************/
static  UINT32     fs_set_pa_map(UINT32 pa_no , UINT32  *output_data)
{
    UINT32  pa_bit_map_no  = pa_no / 32;
    UINT32  pa_bit_map_off = pa_no % 32;    
    output_data[pa_bit_map_no] = output_data[pa_bit_map_no] | (1 << pa_bit_map_off);
    printx(FS_DEBUG , "pa no = %x : set pa bit map[%d] = %x \n" , pa_no , pa_bit_map_no , output_data[pa_bit_map_no]);
}
/**********************************************************************************************************
  * @brief         ɨ��PA���������õ�PA��
  * @param         input_data  : LA��               
  * @retval        output_data ����Ҫ���յ�PA�б�
***********************************************************************************************************/
static  UINT32     fs_scan_pa(UINT32  *input_data , UINT32  *output_data)
{   
    UINT32  pa_no = 0;
    UINT32  pa_buff[FS_PA_PAGE];
    __attribute__((aligned(4)))    fs_pa_type    fs_pa_data;
// ��ЧPA��ÿλ����һ��PA���
    memset(output_data , 0 , FS_WORD_TO_BYTE(FS_PA_MAX_NO) / 32);
// ����LA��ָ����PA CRC�Ƿ���ȷ��������pa invalid bit map����λ
    for(UINT32 i = 0 ; i < FS_LA_MAX_NO ; i++)
    {
        pa_no = FS_GET_PA(input_data[i]);
        if(FS_BLANK_DATA == input_data[i])
        {
// ��LA->PAӳ��ָ��          
            continue;
        }
        else if(pa_no <= FS_PA_MAX_NO)
        {
            norflash_word_read(FS_PA_START + pa_no * FS_WORD_TO_BYTE(FS_PA_PAGE) , (UINT32 *)&fs_pa_data.crc , FS_PA_PAGE);
            printx(FS_DEBUG , "la[%d] : pa[%d] have data \n" , i , pa_no);
#if (1 == FS_DEBUG_LV2)   
            memcpy(fs_read_test , &fs_pa_data.crc , FS_WORD_TO_BYTE(FS_PA_PAGE));
            fs_lv2_debug("scan pa content "  , fs_read_test , FS_PA_PAGE , FS_BLANK_DATA);
#endif
            if(FS_SUCCESS == fs_check_crc(&fs_pa_data.level , FS_WORD_TO_BYTE(FS_PA_PAGE - 1) , fs_pa_data.crc))
            {             
// ��ӳ�䣬��PA��CRC��ȷ����Чӳ��              
                 printx(FS_DEBUG , "la[%d]: pa[%d] , data valid \n" , i , pa_no );
            } 
            else
            {
// ��ӳ��,����CRC����,�����LA->PA��ӳ�䣬���PA��ӳ����Ӧ��bitΪ��Ҫ����              
                 input_data[i] = FS_BLANK_DATA;          
                 fs_set_pa_map(pa_no , output_data);
            }
        }
        watchdog_feed();
    }
    printx(FS_DEBUG , "************************ check all pa *************************** \n");
    fs_pa_free_cnt = 0;
// ��ѯ���е�PA������Ƿ���ӳ�����ģ���PA����LAָ�룬��LAָ��δָ��PA
    for(UINT32 i = 0 ; i < FS_PA_MAX_NO ; i++)
    {
        norflash_word_read(FS_PA_START + i * FS_WORD_TO_BYTE(FS_PA_PAGE) , (UINT32 *)&fs_pa_data.crc  , FS_PA_PAGE);
// ��PA��������
        if(FS_SUCCESS == fs_check_data((UINT32 *)&fs_pa_data.crc , FS_PA_PAGE , FS_BLANK_DATA)) 
        {
            fs_pa_table[i] = FS_NOUSED;
            fs_pa_free_cnt = fs_pa_free_cnt + 1;
            continue;
        }   
        else if((fs_pa_data.la < FS_LA_MAX_NO))          
        {                    
// ��PA���ӦLA��ӳ�䲻һ�£���PA����Ч����Ҫ���LA�����������PA��           
            pa_no = FS_GET_PA(input_data[fs_pa_data.la]);        
// ����PA�ж�ȡ����LA���ҵ�LA���λ�ã���ȥ���LA���м�¼�Ķ�ӦPA�Ƿ�͵�ǰ����PA���һ��
            if(pa_no != i)
            {
                printx(FS_DEBUG , "pa no = %d , la = %d not match \n" , i , pa_no);
#if (1 == FS_DEBUG_LV2)   
                memcpy(fs_read_test , &fs_pa_data.crc , FS_WORD_TO_BYTE(FS_PA_PAGE));
                fs_lv2_debug("scan pa content"  , fs_read_test , FS_PA_PAGE , FS_BLANK_DATA);
#endif              
                fs_pa_table[i] = FS_NOUSED;
                fs_set_pa_map(i , output_data);
            }
            continue;
        }
        else
        {
// ��PA���ӦLA������Խ�磬��PA����Ч����Ҫ���           
            printx(FS_DEBUG , "pa = %d beyond %d \n" , i , FS_LA_MAX_NO);
            fs_pa_table[i] = FS_NOUSED;
            fs_set_pa_map(i , output_data);   
        }
        watchdog_feed();
    }
    printx(FS_DEBUG , "free pa cnt = %d \n" , fs_pa_free_cnt);
}
/**********************************************************************************************************
  * @brief         ��������Ƿ�ȫ��check_flag
  * @param         *input : ��������
                   len    : �������ݳ��ȣ�����word��32bit������
                   check_flag��һ������
  * @retval        FS_SUCCESS : һ��
                   FS_CRC_ERROR����һ��
***********************************************************************************************************/
static  UINT32     fs_check_data(UINT32  *input , UINT32 len , UINT32 check_flag)
{
    for(UINT32 i = 0 ; i < len ; i = i + 1)
    {
       if(input[i] != check_flag)
       {          
          printx(FS_DEBUG , "fs_check_data , offset[%d] = %x is not match \n" , i , input[i]); 
          return FS_ERROR;
       }
    }
    return FS_SUCCESS;
}
/**********************************************************************************************************
  * @brief         ���CRC
  * @param         *input : ��������
                   len    : �������ݳ��ȣ�����word��32bit������
                   crc    ���Ա�CRC���
  * @retval        FS_SUCCESS : һ��
                   FS_CRC_ERROR����һ��
***********************************************************************************************************/
static  UINT32     fs_check_crc(UINT32  *input_data , UINT32 input_len , UINT32 crc)
{
    UINT32 crc_result = fs_crc((UINT8 *)input_data , input_len);
    if(crc_result == crc)
    {
        printx(FS_DEBUG , "crc check valid \n");
        return FS_SUCCESS;
    }
    else
    {
        printx(FS_DEBUG , "crc check invalid , read crc = %x , cnt crc = %x \n" , crc , crc_result);
        return FS_CRC_ERROR;
    }    
}
/**********************************************************************************************************
  * @brief         ɨ������LA����ȡ���һ����ȷ��LA��
  * @param  
  * @retval        *la_table: ���һ����ȷ��LA��
                   fs_last_poinner: ���һ����Ч��LA���ַ���
                                    0 , 1 , 2 ...  (FS_PA_START - FS_LA_START) / FS_BYTE_TO_WORD(FS_LA_PAGE)
                   fs_la_max_cn t:  ���һ����Ч��LA���� 
***********************************************************************************************************/
static  UINT32     fs_scan_la(UINT32  *la_table)
{
   UINT32  result = FS_SUCCESS;
   printx(FS_DEBUG , "find max poinner_t \n"); 
   if(FS_SUCCESS == fs_find_max_la_cnt(&fs_last_poinner))
   {
// �ҵ����һ�β�����LA�������п��ܴ��󣬵���������һ����ȷ�ı�
// ������һ�ε�LA�������Ҫ�ҵ�������һ�����һ����ȷ��LA��
// ������һ�ε�LA����ȷ������ع����������PA��
       fs_load_la(fs_last_poinner , la_table);         // ������һ��LA�������Ƿ���ȷ
#if (1 == FS_DEBUG_LV2)
       fs_lv2_debug("read la table " , la_table , FS_LA_PAGE , FS_BLANK_DATA);
#endif
       if(FS_SUCCESS == fs_check_crc(la_table , FS_WORD_TO_BYTE(FS_LA_PAGE - 1) , la_table[FS_LA_PAGE - 1]))
       {                                              
 //  ���һ��LA����ȷ           
           printx(FS_DEBUG , "fs addr = %x is right , la cnt = %d , last op  success \n" , fs_last_poinner , fs_la_max_cnt);                             
           return FS_SUCCESS;
       }
       else
       {                                               
//  ���һ��LA��CRC������Ҫ�ع�           
           printx(FS_DEBUG , "fs addr = %x is fail , max number = %d , last op fail , need roll back \n" , fs_last_poinner , fs_la_max_cnt);                             
           result = FS_LA_NEED_UPDATE;                                           
       }       
       for(UINT32 i = fs_la_max_cnt - 1 ; i > 0 ; i = i - 1)
       {
//  ������ŵݼ���ʽ����           
           if(FS_ERROR == fs_find_sel_la_cnt(i , &fs_last_poinner))    
           {                      
               printx(FS_DEBUG , "fs no = %d not exist , check next \n" , i);                         
           }
           else
           {
// ��ȡָ��LA������               
               fs_load_la(fs_last_poinner , la_table);        
               if(FS_SUCCESS == fs_check_crc(la_table , 4 * (FS_LA_PAGE - 1) , la_table[FS_LA_PAGE - 1]))
               {                                               
// ���ָ��LA�������Ƿ���ȷ
                    printx(FS_DEBUG , "fs addr = %x , la cnt = %d valid \n"  , fs_last_poinner , fs_la_max_cnt);                                      
// �ҵ����һ�οɿ���LA��¼����Ҫ�ع� , ����FS_LA_NEED_UPDATE
                    break;              
               }   
               else
               {              
                   printx(FS_DEBUG , "fs no = %d not exist , check next \n" , i);   
               }     
           }
           watchdog_feed();  
       }               
   }
   else
   {
       printx(FS_DEBUG , "fs first time use \n"); 
       result = FS_INIT;
   }
   return result;
}
/**********************************************************************************************************
  * @brief         ��ȡLA��
  * @param         la_addr: FS_LA_START ~ FS_PA_START ֮��ĳ�����
                            0 , 1 , 2 ...  (FS_PA_START - FS_LA_START) / FS_BYTE_TO_WORD(FS_LA_PAGE)
  * @retval        *la_table : ����LA��
***********************************************************************************************************/
static  void       fs_load_la(UINT32 la_addr , UINT32  *la_table)
{
    norflash_word_read(FS_LA_START + FS_WORD_TO_BYTE(FS_LA_PAGE) * la_addr ,  la_table , FS_LA_PAGE);
}
/**********************************************************************************************************
  * @brief         ��ȡÿ�θ���LA�����ִ��������ֵ���ü���ֵ��������ȷ�����һ�β�����LA����
  * @param        
  * @retval        *fs_max_cnt_addr ��FS_LA_START ~ FS_PA_START ֮��ĳ����� 
                                      0 , 1 , 2 ...  (FS_PA_START - FS_LA_START) / FS_BYTE_TO_WORD(FS_LA_PAGE)
***********************************************************************************************************/
static  UINT32     fs_find_max_la_cnt(UINT32 *fs_max_cnt_addr)
{
    UINT32    fs_la_cnt = 0;
// ɨ������LA�洢�ռ� , ֻ��ȡLA���еļ������ݣ���FS_LA_CNT_OFFSET
    for(UINT32 i = FS_LA_START + FS_LA_CNT_OFFSET ; i < FS_PA_START ; i = i + FS_WORD_TO_BYTE(FS_LA_PAGE))
    {
        norflash_word_read(i , &fs_la_cnt , 1);
        if(FS_BLANK_DATA != fs_la_cnt)
        {    
            if(fs_la_cnt > fs_la_max_cnt)
            {
                fs_la_max_cnt = fs_la_cnt;
// ��ȡ����LA�����洢�ռ��λ�� , FS_LA_START ~ FS_PA_START ֮��ĳ�����
                *fs_max_cnt_addr = (i - FS_LA_CNT_OFFSET - FS_LA_START) / FS_WORD_TO_BYTE(FS_LA_PAGE);   
            }
        } 
    } 
    if(0 == fs_la_max_cnt)
    {
        printx(FS_DEBUG , "first use , not valid la cnt \n");
        *fs_max_cnt_addr = 0;
        return FS_INIT;
    } 
    else
    {      
        printx(FS_DEBUG , "max la cnt = %x , logic addr = %x \n" , fs_la_max_cnt , *fs_max_cnt_addr);
        return FS_SUCCESS;
    }
}
/**********************************************************************************************************
  * @brief         ��ȡָ������Ƿ���LA�����洢�ռ��г���
  * @param         select_la_cnt:ָ�����
  * @retval        *fs_select_cnt_addr :FS_LA_START ~ FS_PA_START ֮��ĳ����� 
                                        0 , 1 , 2 ...  (FS_PA_START - FS_LA_START) / FS_BYTE_TO_WORD(FS_LA_PAGE)
***********************************************************************************************************/
static  UINT32     fs_find_sel_la_cnt(UINT32 select_la_cnt , UINT32 *fs_select_cnt_addr)
{
    UINT32   fs_la_cnt = 0;  
    for(UINT32 i = FS_LA_START + FS_LA_CNT_OFFSET ; i < FS_PA_START ; i = i + FS_WORD_TO_BYTE(FS_LA_PAGE))
    {
        norflash_word_read(i ,  &fs_la_cnt , 1);
        if(fs_la_cnt == select_la_cnt)
        {
// ��ȡ����LA�����洢�ռ��λ�� , FS_LA_START ~ FS_PA_START ֮��ĳ�����
            *fs_select_cnt_addr = (i - FS_LA_CNT_OFFSET - FS_LA_START) / FS_WORD_TO_BYTE(FS_LA_PAGE); 
            printx(FS_DEBUG , "find select la cnt = %d , logic addr = %x \n" , select_la_cnt , *fs_select_cnt_addr);
            return FS_SUCCESS;
        } 
    } 
    printx(FS_DEBUG , "invalid select la cnt \n");
    return  FS_ERROR;
}
/**********************************************************************************************************
  * @brief         �ļ�ϵͳ��ʼ��
  * @param  
  * @retval        ��ȡLA��PA���ؽ�����LA������PA����Ч�ռ�
***********************************************************************************************************/
void               fs_init(void)
{
    UINT32     fs_pa_invalid[FS_PA_MAX_NO / 32];
    UINT32     fs_t_crc32 = 0;   
    UINT32     pa_offset = 0;
    UINT32     fs_scan_result; 
// ��ʼ��LA�� , PA��
// PA���ձ���fs_scan_pa �г�ʼ������ֵ
    memset(fs_pa_table   , FS_USED   , sizeof(fs_pa_table));
    memset(fs_la_table   , FS_MASK_H16(FS_BLANK_DATA) , sizeof(fs_la_table));  
// ��ѯFS_LA_START ~ FS_PA_START ֮�� ���һ�μ�¼���ҿɿ���LA��
    fs_scan_result = fs_scan_la(fs_la_table); 
    if(FS_INIT == fs_scan_result)
    {
// ����ʹ�� 
        printx(FS_DEBUG , "init mode first use \n");
        memset(fs_la_table , FS_MASK_H16(FS_BLANK_DATA) , sizeof(fs_la_table));
        memset(fs_pa_table , FS_NOUSED    , sizeof(fs_pa_table));
        return ;
    }
    else if(FS_LA_NEED_UPDATE == fs_scan_result)
    {
// ���һ�β����쳣����Ҫ�ع�
        printx(FS_DEBUG , "have valid la \n");
        fs_update_la(fs_la_table);
        fs_update_la_crc(fs_la_table);
        printx(FS_DEBUG , "update la table finish \n");
    }
    else if(FS_ERROR == fs_scan_result)
    {
// ϵͳBUG       
        FS_POS_DEBUG(1 , "system bug \n");
        while(1);
    }
//  ����LA��ɨ��PA����ѯ��Ч��PA��   
    fs_scan_pa(fs_la_table , fs_pa_invalid);
#if (1 == FS_DEBUG_LV2)
    fs_lv2_debug("print pa invalid table" , fs_pa_invalid , FS_PA_MAX_NO / 32 , 0x00);
#endif
//  ����PA���ձ�������Ӧ��PA������  
    fs_recover(fs_pa_invalid);
    printx(1 , "free blk = %d \n" , fs_pa_free_cnt);
}
/**********************************************************************************************************
  * @brief         ��ѯĳ��ID�Ƿ����
  * @param         ID: ID�� 0x0000 ~ 0xFFFF
  * @retval 
***********************************************************************************************************/
UINT32             fs_check_id(UINT16 id)
{
    if(id >= FS_ID_MAX)
    {                                                       
        printx(FS_DEBUG , "invalid id = %d \n" , id);
        return FS_ERROR;
    } 
    for(UINT32 i = 0 ; i < FS_LA_MAX_NO ; i++)
    {
       if(FS_GET_ID(fs_la_table[i]) == id)
       {
           printx(FS_DEBUG , "id exist = %d \n" , id);
           return FS_SUCCESS;
       }
    }
    printx(FS_DEBUG , "not exist id = %d \n" , id);
    return FS_ERROR;
}
/**********************************************************************************************************
  * @brief       ����������ʽ��д��PA�ṹ����
  * @param       fs_pa_type* fs_pa_write: PA�洢�ṹ��
                 *data : ������
                 data_len : �ֽڳ���
                 level    : Ŀ¼�ȼ�
                 la       : ��ǰ��ָ���LA���е�ĳ��ƫ��
                 next_la  : ��������һ��ָ���LA���е�ĳ��ƫ��
  * @retval 
***********************************************************************************************************/
void             fs_update_pa(fs_pa_type* fs_pa_write , UINT8 *data , UINT32 data_len ,  UINT32 level , UINT32 la , UINT32 next_la)
{
    fs_pa_write->level = level;
    fs_pa_write->la = la;
    fs_pa_write->next_la = next_la;
    fs_pa_write->len = data_len;
    memset(fs_pa_write->reverse , 0xff , PA_REVERSE_DATA);
    memcpy(fs_pa_write->data , data , data_len);
    if(data_len < PA_DATA_OF_PAGE) 
         memset(&fs_pa_write->data[data_len] , 0xFF ,  (PA_DATA_OF_PAGE - data_len));
    fs_pa_write->crc = fs_crc((UINT8 *)&fs_pa_write->level , 4 * (FS_PA_PAGE - 1));
}
/**********************************************************************************************************
  * @brief       ��д��������д
  * @param       id��д���ID��
                 *data: д������
                 len: д�����ݳ��ȣ������ֽڼ���
  * @retval      FS_SUCCESS : д��ɹ�
                 FS_INPUT_ERROR : ����ID���󣬻����������ݹ���
                 FS_LA_ERROR: ���㹻���п�    
  ���1: д������PA ������δ����LA���ؽ�Ӧ���ǿ��Զ����ɵ�����
  ���2: д������PA ������LA����δ����LA��CRC , �ؽ�Ӧ���ǿ��Զ����ɵ�����                  
***********************************************************************************************************/
static  UINT32   fs_update_data(UINT16 id , UINT8 *data , UINT32 offset , UINT32 len , fs_pa_type *pa_data)
{
    UINT32  la_add      = 0;
    UINT32  pa_package  = 0;
    UINT32  *pa_no      = FS_NULL;
    UINT32  write_len   = 0;
    UINT32  fs_src_addr = FS_BLANK_DATA;   
    UINT32  fs_dst_no   = FS_BLANK_DATA;   
    UINT32  *la_list    = FS_NULL;      
    UINT32  id_len      = fs_get_id_len(id);
    UINT32  free_len    = 0;
//  �ҵ��ɵ�ID�ĳ���
    UINT32  id_page_cnt = (id_len + PA_DATA_OF_PAGE - 1) / PA_DATA_OF_PAGE;
//  ��IDҪ�޸ĵĿ�ʼ����,������ID���е�ƫ��
    UINT32  s_la_oft    = offset / PA_DATA_OF_PAGE;
//  ��IDҪ�޸ĵ��������,������ID���е�ƫ��
    UINT32  e_la_oft    = (offset + len + PA_DATA_OF_PAGE - 1) / PA_DATA_OF_PAGE;
//  �޸���ʼ��ַ��Ӧ�ó�����ID�ĳ���
    printx(FS_DEBUG , "offset = %d , len = %d \n" , offset , len);
    if(offset > id_len)
    {
        printx(FS_DEBUG , "offset not contiunes \n");
        return FS_LA_ERROR;
    }
//  �ж��Ƿ���Ҫ��������LA��
    if((offset + len) > id_len)
    {
//  �������ݣ�Ҫ��4���봦��
        len = len + (4 - len % 4);
//  ֮ǰ��û�õĿռ�
        if((id_len % PA_DATA_OF_PAGE) != 0)
            free_len  = PA_DATA_OF_PAGE - (id_len % PA_DATA_OF_PAGE);
//  ���޸ĳ��� - �����Ѿ��õ�id_len - ����Ŀ� = ��Ҫ���ӵĿ�
        la_add  = (offset + len - id_len - free_len + PA_DATA_OF_PAGE - 1) / PA_DATA_OF_PAGE;
        printx(FS_DEBUG , "add pa cot = %d \n" , la_add);
    }
//  ��¼���е�ID��LA��,���ҿ�����������LA��,��������
    la_list = malloc(FS_WORD_TO_BYTE(id_page_cnt + la_add));
//  ö�ٳ��Ѿ�����ID������LA��
    fs_enum_la(id , la_list , FS_NULL , FS_FUNC_GET_ID_LA);
    for(UINT32 i = 0 ; i < id_page_cnt ; i++)
        printx(FS_DEBUG , "old la list[%d] = %d \n" , i , la_list[i]);
    if(la_add != 0)
    {
//  ׷�Ӹ���Ŀ�LA��    
        if(FS_SUCCESS != fs_get_free_la(&la_list[id_page_cnt] , la_add))
        {
           return FS_LA_ERROR;
        }
//  ��ע�����ӵ�LA�����Ϊ�м��
        for(UINT32 i = id_page_cnt ; i < id_page_cnt + la_add  ; i++)
        {
             fs_la_table[la_list[i]] = FS_MID_LIST;
        }
    }
//  ����LA�������Ѿ���la_list��
    printx(FS_DEBUG , "start offset = %d , end offset = %d \n" , s_la_oft , e_la_oft);
    for(UINT32 i = s_la_oft ; i < e_la_oft ; i++)
    {
        printx(FS_DEBUG , "leave len = %d \n" , len);
// ��ȡһ������PA��        
        if(FS_SUCCESS != fs_get_free_pa(&fs_dst_no))   
        {
            printx(FS_DEBUG , "have no free pa \n");
            return FS_PA_ERROR;
        }
        fs_pa_table[fs_dst_no] = FS_USED;
        printx(FS_DEBUG , "la[%d] ,free pa  = %d \n" , la_list[i] , fs_dst_no);
/********************************��ʼ��**********************************/                          
        if(i == s_la_oft)
        {
            printx(FS_DEBUG , "start la_offset = %d \n" , i);
            if(FS_SUCCESS != fs_la_to_pa_addr(la_list[i] , &fs_src_addr))
            {    
                FS_POS_DEBUG(1 , "start system bug \n");
                while(1);              
            }
//  ��ȡ������                                              
            norflash_word_read(FS_PA_START + fs_src_addr , (UINT32 *)&pa_data->crc ,  FS_PA_PAGE);  
//  ���������
//  1.��ǰ���ݰ�Ϊ�м����ݰ�,��д�����ݳ���Ϊԭ���ȣ�ֻ�������ݣ����Ȳ���
//                           a.�����м䲿��
//                           b.���м���µ���β
//  2.��ǰ���ݰ�Ϊ�������ݰ�,�ж����������Ƿ񳬹��ɵ�����
//                           a.������,��һ��
//                           b.����,���Ϊʵ��д�볤�� 
            if((pa_data->len) >= PA_DATA_OF_PAGE)        // �м��
            {              
                if((len + (offset % PA_DATA_OF_PAGE)) <= PA_DATA_OF_PAGE)
                {
                    printx(FS_DEBUG , "1.a \n"); 
                    write_len = len;                     // 1.a  ֻ�����Ǵ���һ����       
                }                                                 
                else
                {
                    printx(FS_DEBUG , "1.b \n"); 
                    write_len = PA_DATA_OF_PAGE - (offset % PA_DATA_OF_PAGE); 
                    len = len - write_len;               // 1.b  �����϶��б�İ�
                    pa_package = write_len;              
                }
            }
            else                                         // ��β��
            {                                            // 2.a  ֻ�����Ǵ���һ����   
                if((len + (offset % PA_DATA_OF_PAGE)) < pa_data->len)       
                {
                    printx(FS_DEBUG , "2.a \n");  
                    write_len = len;
                }
                else                                     // 2.b   
                {                                        // ���³���,�������а�
                    if((len + (offset % PA_DATA_OF_PAGE)) >= PA_DATA_OF_PAGE)         
                    {
                        printx(FS_DEBUG , "2.b1 \n");   
                        write_len = PA_DATA_OF_PAGE - offset % PA_DATA_OF_PAGE; 
                        len = len - write_len; 
                        pa_package = write_len;  
                        pa_data->len = PA_DATA_OF_PAGE;                      
                    }
                    else                                 // ����û�а���
                    {
                        printx(FS_DEBUG , "2.b2 \n");   
                        write_len = len;
                        pa_data->len = offset % PA_DATA_OF_PAGE + len;
                    }
                }                     
            }      
//  �����޸�����
            memcpy(&pa_data->data[offset % PA_DATA_OF_PAGE] , data , write_len);
//  ���滹�����ݣ���Ҫ����
            pa_data->next_la = la_list[i + 1];
//  ����CRC
            pa_data->crc = fs_crc((UINT8 *)&pa_data->level , 4 * (FS_PA_PAGE - 1));
//  д��PA
            norflash_word_write(FS_PA_START + fs_dst_no * FS_WORD_TO_BYTE(FS_PA_PAGE) , (UINT32 *)&pa_data->crc , FS_PA_PAGE);         
        }
/********************************�м��**********************************/    
        else if((i > s_la_oft) && (i < (e_la_oft - 1)))
        {
            printx(FS_DEBUG , "middle la_offset = %d \n" , i);
//  ֱ�����µ�PA����д����            
            fs_update_pa(pa_data , &data[pa_package] , PA_DATA_OF_PAGE , 0x00 , la_list[i] , la_list[i + 1]);  
            norflash_word_write(FS_PA_START + fs_dst_no * FS_WORD_TO_BYTE(FS_PA_PAGE) , (UINT32 *)&pa_data->crc , FS_PA_PAGE);
            len = len - PA_DATA_OF_PAGE;
            pa_package = pa_package + PA_DATA_OF_PAGE;
        }
/********************************����**********************************/      
        else 
        {
            printx(FS_DEBUG , "end la_offset = %d \n" , i);          
//  �޾����ݣ�ֱ��д��PA                      
            if(la_add != 0)
            {
//  ֱ�����µ�PA����д����            
                fs_update_pa(pa_data , &data[pa_package] , len , 0x00 , la_list[i] , FS_END_LIST);  
                norflash_word_write(FS_PA_START + fs_dst_no * FS_WORD_TO_BYTE(FS_PA_PAGE) , (UINT32 *)&pa_data->crc , FS_PA_PAGE);
            } 
            else
            {
//  ��ȡ������                  
                if(FS_SUCCESS != fs_la_to_pa_addr(la_list[i] , &fs_src_addr))
                {    
                    FS_POS_DEBUG(1 , "end system bug \n");
                    while(1);              
                } 
//  ��ȡ������                                              
                norflash_word_read(FS_PA_START + fs_src_addr , (UINT32 *)pa_data ,  FS_PA_PAGE);  
//  �����޸�����
                memcpy(&pa_data->data , &data[pa_package] , len);
//  ���ݿ�������
                pa_data->len = (len > pa_data->len) ? len : pa_data->len;
//  ����CRC
                pa_data->crc = fs_crc((UINT8 *)&pa_data->level , 4 * (FS_PA_PAGE - 1));
//  д��PA
                norflash_word_write(FS_PA_START + fs_dst_no * FS_WORD_TO_BYTE(FS_PA_PAGE) , (UINT32 *)&pa_data->crc , FS_PA_PAGE);
            }                
          }
//  ����LAӳ��  
          printx(FS_DEBUG , "old fs[%d] = %x \n"  , la_list[i] , fs_la_table[la_list[i]]);
          fs_la_table[la_list[i]] = (fs_la_table[la_list[i]] & 0x0000FFFF) | (fs_dst_no << 16);
          printx(FS_DEBUG , "new fs[%d] = %x \n"  , la_list[i] , fs_la_table[la_list[i]]); 
          watchdog_feed();
   }
   fs_need_recover = 1;
   fs_update_la(fs_la_table);
   fs_update_la_crc(fs_la_table);
   free(la_list);
#if 0
   for(UINT32 i = 0 ; i < FS_LA_PAGE ; i++)
   {
      printx(1 , "la[%d] = %x \n"  , i , fs_la_table[i]); 
   }
#endif
}

/**********************************************************************************************************
  * @brief       д�뺯����֧����д   
  * @param       id��д���ID��
                 *data: д������
                 len: д�����ݳ��ȣ������ֽڼ���
  * @retval      FS_SUCCESS : д��ɹ�
                 FS_INPUT_ERROR : ����ID���󣬻����������ݹ���
                 FS_LA_ERROR: ���㹻���п�    
  ���1: д������PA ������δ����LA���ؽ�Ӧ���ǿ��Զ����ɵ�����
  ���2: д������PA ������LA����δ����LA��CRC , �ؽ�Ӧ���ǿ��Զ����ɵ�����                  
***********************************************************************************************************/
UINT32           fs_write_data(UINT16 id , UINT8 *data , UINT32 offset , UINT32 len)
{                               
// ��Ҫ�����PA������    
    UINT32       page_cnt = (len + PA_DATA_OF_PAGE - 1) / PA_DATA_OF_PAGE;
    UINT32       *la_no = FS_NULL , *la_list = FS_NULL;
    UINT32       la_valid = 0;
    UINT16       pa_package = 0;
    UINT32       pa_write_len = 0;
    UINT32       result = FS_SUCCESS;
    UINT32       pa_offset = FS_BLANK_DATA;
     __attribute__((aligned(4)))  fs_pa_type   fs_pa_write;
    printx(FS_DEBUG , "write id = %d \n" , id);            
    if((id >=  FS_ID_MAX) || (page_cnt >= FS_PA_MAX_NO))
    {
        printx(FS_DEBUG , "invalid input id = %d , len = %d \n" , id , len);
        result = FS_INPUT_ERROR; 
        goto EXPECT;    
    }
// ID�Ѵ��ڣ����Ѵ��ڵ�ID��LA������������һ���һ��PA��
    if(FS_SUCCESS == fs_check_id(id))
    {  
        printx(FS_DEBUG , "id exist , need update data \n");  
        if(offset % 4)
        {
            printx(FS_DEBUG , "write not algin by 4 \n");  
            result = FS_INPUT_ERROR; 
            goto EXPECT;    
        }   
        return fs_update_data(id , data , offset , len , &fs_pa_write);  
    }
    else
    {
        if(0 != (len % 4))
        {
// д�����ݰ���32bit����       
            printx(FS_DEBUG , "len is not algin by word \n");
            len = len + (4 - len % 4);
            printx(FS_DEBUG , "new len = %d \n" , len);
        }         
        if(0 != offset)
        {
            printx(FS_DEBUG , "new id , offset need from start to 0 \n");           
            result = FS_INPUT_ERROR; 
            goto EXPECT;    
        }
// PAд��ṹ��
        la_no = malloc(FS_WORD_TO_BYTE(page_cnt + 1));       // get free la range        
// �ж��Ƿ����㹻�Ŀ��п�ɹ�ʹ�ã��������������
        if(FS_SUCCESS == fs_get_free_la(la_no , page_cnt))
        {
// �׿飬��¼ID
            fs_la_table[la_no[0]] = id;
// �м�飬���ñ�ʶ, ���ֻ��һ���飬���ﲻִ��
            for(UINT32 i = 1 ; i < page_cnt ;  i++)
            {
                fs_la_table[la_no[i]] = FS_MID_LIST;    
            }
// ����ָ�룬Ϊ�˼�¼��PA�����һ�����next_laΪ����
            if(page_cnt >= 1)
                la_no[page_cnt] = FS_END_LIST;
        }   
        else
        {
             printx(FS_DEBUG , "get free la fail \n");
             result = FS_LA_ERROR;
             goto EXPECT;    
        }
#if (1 == FS_DEBUG_LV2)
        fs_lv2_debug("fs write check free la no" , la_no , (page_cnt + 1) , FS_BLANK_DATA);
#endif
        for(UINT32 i = 0 ; i < page_cnt ; i = i + 1)
        {      
// ��ȡһ������PA��        
            if(FS_SUCCESS == fs_get_free_pa(&pa_offset))
            {
                fs_pa_table[pa_offset] = FS_USED;
                if(fs_pa_free_cnt > 1)
                {
                    fs_pa_free_cnt = fs_pa_free_cnt - 1;    
                }
                else
                {
// �޶����PA�飬��Ҫ������            
                    FS_POS_DEBUG(1 , "free pa blk not match la");
                    fs_need_recover = 1;
                    result = FS_PA_ERROR;
                    goto EXPECT;    
                }
            }
            else
            {
// �޶����PA�飬һ����LA��PA��һ������        
                FS_POS_DEBUG(1 , "system bug la not match pa");
                while(1);
            }
            fs_la_table[la_no[i]] = fs_la_table[la_no[i]] | (pa_offset << 16);
            printx(FS_DEBUG , "la[%d] = pa_no = %x  id = %x \n" , la_no[i] , FS_GET_PA(fs_la_table[la_no[i]]) , FS_GET_ID(fs_la_table[la_no[i]])); 
            pa_write_len = (len > PA_DATA_OF_PAGE) ? PA_DATA_OF_PAGE : len;
            len = len - pa_write_len;
// ���������浽�ṹ����
            fs_update_pa(&fs_pa_write , &data[pa_package] , pa_write_len , 0x00 , la_no[i] , la_no[i + 1]);  
// ���ṹ������д��Norflash��
#if  (1 == FS_TEST)
// д����1  ������Ӧ�����
            if(FS_WRITE_TEST_NO1 == test_no)
            {
                fs_w_test = pa_write_len / 2;
                printx(FS_DEBUG , "write test point 1 happen \n");  
            }
#endif
            norflash_word_write(FS_PA_START + pa_offset * FS_WORD_TO_BYTE(FS_PA_PAGE) , (UINT32 *)&fs_pa_write.crc , FS_PA_PAGE);
            printx(FS_DEBUG , "pa no = %d  , pa write len = %d py addr = %x finish \n" , pa_offset , pa_write_len , FS_PA_START + pa_offset * 4 * FS_PA_PAGE);  
#if (1 == FS_DEBUG_LV2)
            norflash_word_read(FS_PA_START + pa_offset * FS_WORD_TO_BYTE(FS_PA_PAGE) ,  fs_read_test , FS_PA_PAGE);
            fs_lv2_debug("read just write data" , fs_read_test , FS_PA_PAGE , FS_BLANK_DATA);
#endif    
    //  ��д�����ݼ���          
            pa_package = pa_package + pa_write_len;  
            watchdog_feed();      
        }
#if  (1 == FS_TEST)
    //  д����2 ������Ӧ�����
            if(FS_WRITE_TEST_NO2 == test_no)
            {
                printx(FS_DEBUG , "write test point 2 happen \n");  
                chip_reset(SYSTEM_REBOOT);
            }
#endif
    //  ����LA��Norflash��
        fs_update_la(fs_la_table);
    //  ����LA���CRC��Norflash��
#if  (1 == FS_TEST)
    //  д����4 ������Ӧ����ã���LA�ָ�����
            if(FS_WRITE_TEST_NO4 == test_no)
            {
                printx(FS_DEBUG , "write test point 4 happen \n");  
                chip_reset(SYSTEM_REBOOT);
            }
#endif
        fs_update_la_crc(fs_la_table);
#if  (1 == FS_TEST)
    //  д����5  ������Ӧ�ñ�����
            if(FS_WRITE_TEST_NO5 == test_no)
            {
                printx(FS_DEBUG , "write test point 5 happen \n");  
                chip_reset(SYSTEM_REBOOT);
            }
#endif
        }
EXPECT:
    if(FS_INPUT_ERROR != result)
        free(la_no);
    return result;
}
/**********************************************************************************************************
  * @brief         ��ȡ����
  * @param         id����ȡ��ID��
                   len: ��ȡ���ݳ��ȣ������ֽڼ���
  * @retval         *data: ��ȡ����

                   FS_SUCCESS : ��ȡ�ɹ�
                   FS_ID_EXIST: ID������
***********************************************************************************************************/
UINT32             fs_read_data(UINT16 id , UINT8 *data , UINT32 offset , UINT32 len)
{
    UINT32   id_len = 0;
    UINT32   *la_elem = FS_NULL;
    UINT32   pa_no = 0;
    UINT32   la_str_offset = offset / PA_DATA_OF_PAGE;
    UINT32   la_end_offset = (offset + len + PA_DATA_OF_PAGE - 1) / PA_DATA_OF_PAGE;
    UINT32   *read_buf = FS_NULL;
//  ID������USB����
    if(FS_SUCCESS != fs_check_id(id))
    {
        printx(FS_DEBUG , "id not exist , fail \n");
        return FS_ID_EXIST;
    }

//  ��ȡ��ID��������Ч���ݳ���
    if(FS_SUCCESS == fs_enum_la(id , FS_NULL , &id_len , FS_FUNC_GET_ID_LEN))
    {
        if((offset + len) > id_len)
        {
            printx(FS_DEBUG , "invalid read input %d , id = %d , len = %d\n" , offset + len , id , id_len);
            return FS_INPUT_ERROR; 
        }      
        la_elem = malloc(la_end_offset);                           
//  ��ȡ��ID��������������      
        fs_enum_la(id , la_elem , FS_NULL , FS_FUNC_GET_ID_LA);
//  ��ȡ����       
        for(UINT32 i = la_str_offset ; i < la_end_offset ; i++)
        {
            pa_no = FS_GET_PA(fs_la_table[la_elem[i]]); 
            norflash_word_read(FS_PA_START + pa_no * FS_WORD_TO_BYTE(FS_PA_PAGE) + PA_DAT_OFFSET , \
                               (UINT32 *)&data[i * PA_DATA_OF_PAGE]  ,  FS_BYTE_TO_WORD(PA_DATA_OF_PAGE));
            printx(FS_DEBUG , "fs read id page cnt = %d \n" , i);
        }
    }
    free(la_elem);
    return FS_SUCCESS;
}
/**********************************************************************************************************
  * @brief         ɾ��ĳ��ID
  * @param         id��ɾ����ID��
  * @retval      
***********************************************************************************************************/
static      void   fs_clear_id(UINT16 id)
{
    UINT32   id_len = 0;
    UINT32   *la_elem = FS_NULL;  
    UINT32   id_page_cnt = 0;
    UINT32   pa_no = 0;
//  ��ȡ��ID��������Ч���ݳ���
    if(FS_SUCCESS == fs_enum_la(id , FS_NULL , &id_len , FS_FUNC_GET_ID_LEN))
    {
        id_page_cnt = (id_len + PA_DATA_OF_PAGE - 1) / PA_DATA_OF_PAGE;
        la_elem = malloc(FS_WORD_TO_BYTE(id_page_cnt));                           // get free la range
//  ��ȡ��ID��������������           
        fs_enum_la(id , la_elem , FS_NULL , FS_FUNC_GET_ID_LA);
        for(UINT32 i = 0 ; i < id_page_cnt ; i++)
        {
//  ���LA��       
            fs_la_table[la_elem[i]] = FS_BLANK_DATA;
            printx(FS_DEBUG , "clear la[%d] unused  = %x \n"  , la_elem[i] , fs_la_table[la_elem[i]]);            
        }
    }
    free(la_elem);
    fs_need_recover = 1;
}

/**********************************************************************************************************
  * @brief         ɾ��ĳ��ID
  * @param         id��ɾ����ID��
  * @retval 
***********************************************************************************************************/
UINT32             fs_delete_data(UINT16 id)
{
    UINT32   id_len = 0;
    UINT32   *la_elem = FS_NULL;
    UINT32   id_page_cnt = 0;
    UINT32   pa_no = 0;
    if(FS_SUCCESS != fs_check_id(id))
    {
        printx(FS_DEBUG , "id not exist , fail \n");
        return FS_ERROR;
    }
// ���LA���е�����
    fs_clear_id(id);
// ��LA��д��Norflash
    fs_update_la(fs_la_table);
    fs_update_la_crc(fs_la_table);
// ����
    fs_need_recover = 1;
    return FS_SUCCESS;
}
/**********************************************************************************************************
  * @brief         ɾ�������ļ�ϵͳ���ع��ʼ��״̬
  * @param         id��ɾ����ID��
  * @retval 
***********************************************************************************************************/
void               fs_clear_all()
{
    for(UINT32 i = FS_LA_START ; i < FS_PA_END ; i = i + FS_LA_BLOCK)
    {     
        norflash_erase_sector(i);
    }
    memset(fs_la_table   , FS_MASK_H16(FS_BLANK_DATA) , sizeof(fs_la_table));  
    while(1)
        chip_reset(SYSTEM_REBOOT);
}
/**********************************************************************************************************
  * @brief         ɾ�������ļ�ϵͳ���ع��ʼ��״̬
  * @param         id��ɾ����ID��
  * @retval 
***********************************************************************************************************/
void               fs_trim()
{
    UINT32     fs_pa_invalid[FS_PA_MAX_NO / 32];
    if(1 == fs_need_recover)
    {
        fs_scan_pa(fs_la_table , fs_pa_invalid);
#if (1 == FS_DEBUG_LV2)
         fs_lv2_debug("print pa invalid table" , fs_pa_invalid , FS_PA_MAX_NO / 32 , 0x00);
#endif
//  ����PA���ձ�������Ӧ��PA������  
        fs_recover(fs_pa_invalid);    
        fs_need_recover = 0;
    }
}
/**********************************************************************************************************
  * @brief        
  * @param        
  * @retval 
***********************************************************************************************************/
UINT32             fs_get_free_capacity()
{
    UINT32  free_cnt = 0;
    for(UINT32 i = 0 ; i < FS_LA_MAX_NO ; i++)
    {
        if(FS_BLANK_DATA == fs_la_table[i])
        {
             free_cnt++;
        }
    }
    printx(FS_DEBUG , "pa free = %d , la free = %d \n" , fs_pa_free_cnt , free_cnt);
    if(free_cnt > fs_pa_free_cnt)
        return fs_pa_free_cnt;
    else 
        return free_cnt;
}

/**********************************************************************************************************
  * @brief        
  * @param        
  * @retval 
***********************************************************************************************************/
UINT32             fs_get_id_len(UINT32 id)
{
    UINT32 id_len = 0;
    if(FS_SUCCESS != fs_check_id(id))
    {
        printx(FS_DEBUG , "id not exist , fail \n");
        return 0;
    }
    if(FS_SUCCESS == fs_enum_la(id , FS_NULL , &id_len , FS_FUNC_GET_ID_LEN))
    {
       return id_len;
    }
    return 0;
}
/**********************************************************************************************************
  * @brief        
  * @param        
  * @retval 
***********************************************************************************************************/
void               fs_print_la()
{
    for(UINT32 i = 0 ; i < FS_LA_MAX_NO ; i++)
    {
       printx(1 , "la offset = %d data = %x \n" , i , fs_la_table[i]);            
    }
}