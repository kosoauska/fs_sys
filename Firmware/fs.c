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
#define            FS_ID_MAX                    (0xFFF0)            // 最大ID序号
#define            FS_MID_LIST                  (0xFFFE)            // 某ID的中间链表
#define            FS_END_LIST                  (0xFFFD)            // 某ID的结束链表 
#define            FS_RANDOM_PA                 (20)                // 随机尝试有效PA次数
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
extern  UINT32     test_no;                                                             // 测点标号
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
  * @brief         获取多个空闲LA表中的位置
  * @param  
  * @retval        valid_la_offset: 在LA表中空闲的位置编号(0 , 1 , 2 ... FS_LA_MAX_NO）
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
  * @brief         获取一个可以更新整个LA表的NORFLASH位置，到达FS_PA_START后回滚
  * @param         fs_current_pos: 当前已用位置(0 , 1 , 2 ... )
  * @retval        下一个可用位置(0 , 1 , 2 ... )
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
// 到达边界，回滚 & 擦除首块          
            printx(FS_DEBUG , "roll back to start addr \n");
// la的0地址不用，为了节省fs_pa_table的RAM空间
            la_next_valid_pos = 1;        
            norflash_erase_sector(FS_LA_START);    
            return la_next_valid_pos;   
        }       
        if(0 == (FS_WORD_TO_BYTE(FS_LA_PAGE) * la_next_valid_pos % FS_LA_BLOCK))                
        {
// 到达边界， 擦除            
            norflash_erase_sector(FS_LA_START + 4 * FS_LA_PAGE * la_next_valid_pos);         
            printx(FS_DEBUG , "next addr = %x beyond block , just need erase \n" , la_next_valid_pos);   
            return la_next_valid_pos;
        }
// 读取数据
        norflash_word_read(FS_LA_START + FS_WORD_TO_BYTE(FS_LA_PAGE) * la_next_valid_pos , la_buff ,  FS_LA_PAGE);
        printx(FS_DEBUG , "next addr = %x , check data \n" , la_next_valid_pos);
// 判断是否数据为全FF
        if(FS_SUCCESS == fs_check_data(la_buff , FS_LA_PAGE , FS_BLANK_DATA))         
        {
            printx(FS_DEBUG , "next addr = %x  valid \n" , la_next_valid_pos);  
// 可写位置                  
            return la_next_valid_pos;
        } 
        else
        {
// 尝试下一个           
            printx(FS_DEBUG , "next addr = %x  invalid \n" , la_next_valid_pos);    
        }      
    }
}
/**********************************************************************************************************
  * @brief         更新LA表，并更新LA写入计数，不更新CRC
  * @param         *la_table：当前LA表
  * @retval 
***********************************************************************************************************/
static  void       fs_update_la(UINT32  *la_table)
{
    UINT32  la_next_valid_pos = 0; 
//  当前最后一次写入LA的计数
    printx(FS_DEBUG , "current fs la no = %d \n" , fs_la_max_cnt);
//  下一个可写入的计数
    fs_la_max_cnt = fs_la_max_cnt + 1;
    la_table[FS_LA_PAGE - 2] = fs_la_max_cnt;                                                         
    printx(FS_DEBUG , "fs la no = %d \n" , fs_la_max_cnt);
//  获取从FS_LA_START ~ FS_PA_START 范围内LA可写入的下一个位置编号
    la_next_valid_pos = fs_get_next_la_map(fs_last_poinner);
//  更新当前LA写入NORFLASH位置的指针
    fs_last_poinner   = la_next_valid_pos;
//  写入LA表
#if  (1 == FS_TEST)
//  写入测点3 旧数据应该完好，有LA恢复过程
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
  * @brief          更新LA表的CRC
  * @param          *la_table：当前LA表
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
  * @brief         获取一个有效的PA
  * @param  
  * @retval        *pa_elem : 有效的PA编号(1 , 2 , ....)
***********************************************************************************************************/
static  UINT32     fs_get_free_pa(UINT32 *pa_elem)
{
    UINT16  pa_no = 0;
    trng_generate((UINT8 *)&pa_no , 2);
    if(pa_no >= FS_PA_MAX_NO)
        pa_no = pa_no % FS_PA_MAX_NO; 
    for(UINT32 i = 0 ; i < FS_PA_MAX_NO ; i++)
    { 
// pa的0地址不用，为了节省fs_pa_table的RAM空间     
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
  * @brief         根据PA数据内容中的LA指针判断LA表对应的指针是否一致
  * @param         pa_la：fs_la_table中的编号
  * @retval        FS_SUCCESS: 一致
                   FS_ERROR  : 不一致 
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
  * @brief         根据LA表中的偏移计算出PA的物理地址
  * @param         la_pa: fs_la_table表中的编号
  * @retval        *pa_offset: PA的物理地址
                   FS_SUCCESS: 成功
                   FS_ERROR  : 编号错误 
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
  * @brief         枚举某个ID的长度或者所含的PA块序列
  * @param         id       :  ID号
                   *la_no   ： 该ID对应的LA序列表
                   *file_len:  ID的长度
                    func    :  FS_FUNC_GET_ID_LEN : *file_len 有效
                                                    *la_no    无效 
                               FS_FUNC_GET_ID_LA  : *file_len 无效
                                                    *la_no    有效 
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
// 查找到ID,获得第一个地址                                                                
            if(FS_SUCCESS != fs_la_to_pa_addr(i , &fs_pa_addr))
            {    
                FS_POS_DEBUG(1 , "system bug \n");
                while(1);              
            }
// 读取PA内容，只需要next la 和 len
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
// 读取整个链表 所有LA序列                             
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
  * @brief         回收PA无效数据
  * @param  
  * @retval        UINT32 *pa_invalid_table : PA中的无效数据表, 每位对应一个PA编号
                   0x00000001：PA[0]
                   0x00000003：PA[0] PA[1]
                   0x00000007：PA[0] PA[1] PA[2]
***********************************************************************************************************/
static  void       fs_recover(UINT32 *pa_invalid_table)
{
    UINT16   pa_offset_data = 0;
    UINT16   pa_offset_flag = 0;
    UINT16   pa_cnt = 0;
// 每16位循环一次，一个元素需要循环两次
    for(UINT32 i = 0 ; i < 2 * (FS_PA_MAX_NO / 32) ; i++)             
    {       
        pa_offset_flag = i % 2;
        if(0 == pa_offset_flag)
        {
            pa_offset_data = FS_MASK_L16(pa_invalid_table[pa_cnt]);        // 偶次循环判断低16bit
            printx(FS_DEBUG , "even block[%d] = %x \n" , i , pa_offset_data);
        }
        else
        {
            pa_offset_data = FS_MASK_H16(pa_invalid_table[pa_cnt]);        // 奇次循环判断高16bit
            printx(FS_DEBUG , "odd block[%d] = %x \n" , i , pa_offset_data);
            pa_cnt++;
        }  
// 先按照4KB查询，如果整个4KB都需要擦除，调用块擦除            
        if(pa_offset_data == 0xFFFF)                                        // block擦除
        {                                                            
           norflash_erase_sector(FS_PA_START + i * NORFLASH_SECTOR_4K);    
        }  
        else
        {
// 否则调用页擦除          
           for(UINT32 j = 0 ; j < 16 ; j++)
           {
               if(0x0001 == ((pa_offset_data >> j) & 0x0001))
               {
                   printx(FS_DEBUG , "block[%d]: page[%d]: addr = %x : need page erase \n" , i , j , i * NORFLASH_SECTOR_4K + j * NORFLASH_PAGE_256B);
#if  (1 == FS_TEST)
//  擦除测点1,2  下次上电  PA应该有回收过程  原数据不变
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
//   擦除测点3    下次上电应该无回收过程
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
  * @brief         计算CRC
  * @param         
  * @retval 
***********************************************************************************************************/
static  UINT32     fs_crc(UINT8 *data , UINT32 len)
{
    return crc32(0 , data , len);
}
/**********************************************************************************************************
  * @brief         设置需要清除的PA表
  * @param         pa_no        : PA编号 
                   *output_data : 无效的PA表记录
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
  * @brief         扫描PA表，回收无用的PA块
  * @param         input_data  : LA表               
  * @retval        output_data ：需要回收的PA列表
***********************************************************************************************************/
static  UINT32     fs_scan_pa(UINT32  *input_data , UINT32  *output_data)
{   
    UINT32  pa_no = 0;
    UINT32  pa_buff[FS_PA_PAGE];
    __attribute__((aligned(4)))    fs_pa_type    fs_pa_data;
// 无效PA表每位代表一个PA编号
    memset(output_data , 0 , FS_WORD_TO_BYTE(FS_PA_MAX_NO) / 32);
// 根据LA表指针检测PA CRC是否正确，错误在pa invalid bit map表置位
    for(UINT32 i = 0 ; i < FS_LA_MAX_NO ; i++)
    {
        pa_no = FS_GET_PA(input_data[i]);
        if(FS_BLANK_DATA == input_data[i])
        {
// 无LA->PA映射指针          
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
// 有映射，且PA的CRC正确，有效映射              
                 printx(FS_DEBUG , "la[%d]: pa[%d] , data valid \n" , i , pa_no );
            } 
            else
            {
// 有映射,但是CRC错误,清除该LA->PA的映射，标记PA的映射表对应的bit为需要擦除              
                 input_data[i] = FS_BLANK_DATA;          
                 fs_set_pa_map(pa_no , output_data);
            }
        }
        watchdog_feed();
    }
    printx(FS_DEBUG , "************************ check all pa *************************** \n");
    fs_pa_free_cnt = 0;
// 查询所有的PA表，检测是否有映射错误的，即PA中有LA指针，但LA指针未指向PA
    for(UINT32 i = 0 ; i < FS_PA_MAX_NO ; i++)
    {
        norflash_word_read(FS_PA_START + i * FS_WORD_TO_BYTE(FS_PA_PAGE) , (UINT32 *)&fs_pa_data.crc  , FS_PA_PAGE);
// 该PA块无数据
        if(FS_SUCCESS == fs_check_data((UINT32 *)&fs_pa_data.crc , FS_PA_PAGE , FS_BLANK_DATA)) 
        {
            fs_pa_table[i] = FS_NOUSED;
            fs_pa_free_cnt = fs_pa_free_cnt + 1;
            continue;
        }   
        else if((fs_pa_data.la < FS_LA_MAX_NO))          
        {                    
// 该PA块对应LA的映射不一致，该PA块无效，需要清除LA表，并且清除该PA块           
            pa_no = FS_GET_PA(input_data[fs_pa_data.la]);        
// 根据PA中读取到的LA，找到LA表的位置，再去检测LA表中记录的对应PA是否和当前检测的PA编号一致
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
// 该PA块对应LA的索引越界，该PA块无效，需要清除           
            printx(FS_DEBUG , "pa = %d beyond %d \n" , i , FS_LA_MAX_NO);
            fs_pa_table[i] = FS_NOUSED;
            fs_set_pa_map(i , output_data);   
        }
        watchdog_feed();
    }
    printx(FS_DEBUG , "free pa cnt = %d \n" , fs_pa_free_cnt);
}
/**********************************************************************************************************
  * @brief         检测数据是否全是check_flag
  * @param         *input : 输入数据
                   len    : 输入数据长度，按照word（32bit）计数
                   check_flag：一致数据
  * @retval        FS_SUCCESS : 一致
                   FS_CRC_ERROR：不一致
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
  * @brief         检测CRC
  * @param         *input : 输入数据
                   len    : 输入数据长度，按照word（32bit）计数
                   crc    ：对比CRC结果
  * @retval        FS_SUCCESS : 一致
                   FS_CRC_ERROR：不一致
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
  * @brief         扫描整个LA表，获取最后一次正确的LA表
  * @param  
  * @retval        *la_table: 最后一次正确的LA表
                   fs_last_poinner: 最后一次有效的LA表地址编号
                                    0 , 1 , 2 ...  (FS_PA_START - FS_LA_START) / FS_BYTE_TO_WORD(FS_LA_PAGE)
                   fs_la_max_cn t:  最后一次有效的LA计数 
***********************************************************************************************************/
static  UINT32     fs_scan_la(UINT32  *la_table)
{
   UINT32  result = FS_SUCCESS;
   printx(FS_DEBUG , "find max poinner_t \n"); 
   if(FS_SUCCESS == fs_find_max_la_cnt(&fs_last_poinner))
   {
// 找到最后一次操作的LA表，数据有可能错误，倒叙查找最后一次正确的表
// 如果最后一次的LA表错误，需要找到并更新一下最后一次正确的LA表
// 如果最后一次的LA表正确，无需回滚操作，检测PA表
       fs_load_la(fs_last_poinner , la_table);         // 检测最后一次LA表数据是否正确
#if (1 == FS_DEBUG_LV2)
       fs_lv2_debug("read la table " , la_table , FS_LA_PAGE , FS_BLANK_DATA);
#endif
       if(FS_SUCCESS == fs_check_crc(la_table , FS_WORD_TO_BYTE(FS_LA_PAGE - 1) , la_table[FS_LA_PAGE - 1]))
       {                                              
 //  最后一次LA表正确           
           printx(FS_DEBUG , "fs addr = %x is right , la cnt = %d , last op  success \n" , fs_last_poinner , fs_la_max_cnt);                             
           return FS_SUCCESS;
       }
       else
       {                                               
//  最后一次LA表CRC错误，需要回滚           
           printx(FS_DEBUG , "fs addr = %x is fail , max number = %d , last op fail , need roll back \n" , fs_last_poinner , fs_la_max_cnt);                             
           result = FS_LA_NEED_UPDATE;                                           
       }       
       for(UINT32 i = fs_la_max_cnt - 1 ; i > 0 ; i = i - 1)
       {
//  按照序号递减方式查找           
           if(FS_ERROR == fs_find_sel_la_cnt(i , &fs_last_poinner))    
           {                      
               printx(FS_DEBUG , "fs no = %d not exist , check next \n" , i);                         
           }
           else
           {
// 读取指定LA表数据               
               fs_load_la(fs_last_poinner , la_table);        
               if(FS_SUCCESS == fs_check_crc(la_table , 4 * (FS_LA_PAGE - 1) , la_table[FS_LA_PAGE - 1]))
               {                                               
// 检测指定LA表数据是否正确
                    printx(FS_DEBUG , "fs addr = %x , la cnt = %d valid \n"  , fs_last_poinner , fs_la_max_cnt);                                      
// 找到最后一次可靠的LA记录，需要回滚 , 返回FS_LA_NEED_UPDATE
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
  * @brief         读取LA表
  * @param         la_addr: FS_LA_START ~ FS_PA_START 之间某个序号
                            0 , 1 , 2 ...  (FS_PA_START - FS_LA_START) / FS_BYTE_TO_WORD(FS_LA_PAGE)
  * @retval        *la_table : 整个LA表
***********************************************************************************************************/
static  void       fs_load_la(UINT32 la_addr , UINT32  *la_table)
{
    norflash_word_read(FS_LA_START + FS_WORD_TO_BYTE(FS_LA_PAGE) * la_addr ,  la_table , FS_LA_PAGE);
}
/**********************************************************************************************************
  * @brief         读取每次更新LA表中现存的最大计数值，该计数值可能是正确的最后一次操作的LA数据
  * @param        
  * @retval        *fs_max_cnt_addr ：FS_LA_START ~ FS_PA_START 之间某个序号 
                                      0 , 1 , 2 ...  (FS_PA_START - FS_LA_START) / FS_BYTE_TO_WORD(FS_LA_PAGE)
***********************************************************************************************************/
static  UINT32     fs_find_max_la_cnt(UINT32 *fs_max_cnt_addr)
{
    UINT32    fs_la_cnt = 0;
// 扫描整个LA存储空间 , 只读取LA表中的计数内容，即FS_LA_CNT_OFFSET
    for(UINT32 i = FS_LA_START + FS_LA_CNT_OFFSET ; i < FS_PA_START ; i = i + FS_WORD_TO_BYTE(FS_LA_PAGE))
    {
        norflash_word_read(i , &fs_la_cnt , 1);
        if(FS_BLANK_DATA != fs_la_cnt)
        {    
            if(fs_la_cnt > fs_la_max_cnt)
            {
                fs_la_max_cnt = fs_la_cnt;
// 获取整个LA迭代存储空间的位置 , FS_LA_START ~ FS_PA_START 之间某个序号
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
  * @brief         读取指定序号是否在LA迭代存储空间中出现
  * @param         select_la_cnt:指定编号
  * @retval        *fs_select_cnt_addr :FS_LA_START ~ FS_PA_START 之间某个序号 
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
// 获取整个LA迭代存储空间的位置 , FS_LA_START ~ FS_PA_START 之间某个序号
            *fs_select_cnt_addr = (i - FS_LA_CNT_OFFSET - FS_LA_START) / FS_WORD_TO_BYTE(FS_LA_PAGE); 
            printx(FS_DEBUG , "find select la cnt = %d , logic addr = %x \n" , select_la_cnt , *fs_select_cnt_addr);
            return FS_SUCCESS;
        } 
    } 
    printx(FS_DEBUG , "invalid select la cnt \n");
    return  FS_ERROR;
}
/**********************************************************************************************************
  * @brief         文件系统初始化
  * @param  
  * @retval        读取LA表，PA表，重建整个LA表，回收PA中无效空间
***********************************************************************************************************/
void               fs_init(void)
{
    UINT32     fs_pa_invalid[FS_PA_MAX_NO / 32];
    UINT32     fs_t_crc32 = 0;   
    UINT32     pa_offset = 0;
    UINT32     fs_scan_result; 
// 初始化LA表 , PA表
// PA回收表在fs_scan_pa 中初始化，赋值
    memset(fs_pa_table   , FS_USED   , sizeof(fs_pa_table));
    memset(fs_la_table   , FS_MASK_H16(FS_BLANK_DATA) , sizeof(fs_la_table));  
// 查询FS_LA_START ~ FS_PA_START 之间 最后一次记录并且可靠的LA表
    fs_scan_result = fs_scan_la(fs_la_table); 
    if(FS_INIT == fs_scan_result)
    {
// 初次使用 
        printx(FS_DEBUG , "init mode first use \n");
        memset(fs_la_table , FS_MASK_H16(FS_BLANK_DATA) , sizeof(fs_la_table));
        memset(fs_pa_table , FS_NOUSED    , sizeof(fs_pa_table));
        return ;
    }
    else if(FS_LA_NEED_UPDATE == fs_scan_result)
    {
// 最后一次操作异常，需要回滚
        printx(FS_DEBUG , "have valid la \n");
        fs_update_la(fs_la_table);
        fs_update_la_crc(fs_la_table);
        printx(FS_DEBUG , "update la table finish \n");
    }
    else if(FS_ERROR == fs_scan_result)
    {
// 系统BUG       
        FS_POS_DEBUG(1 , "system bug \n");
        while(1);
    }
//  根据LA表，扫描PA表，查询无效的PA块   
    fs_scan_pa(fs_la_table , fs_pa_invalid);
#if (1 == FS_DEBUG_LV2)
    fs_lv2_debug("print pa invalid table" , fs_pa_invalid , FS_PA_MAX_NO / 32 , 0x00);
#endif
//  根据PA回收表，擦除对应的PA块数据  
    fs_recover(fs_pa_invalid);
    printx(1 , "free blk = %d \n" , fs_pa_free_cnt);
}
/**********************************************************************************************************
  * @brief         查询某个ID是否存在
  * @param         ID: ID号 0x0000 ~ 0xFFFF
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
  * @brief       将数据流格式化写入PA结构体中
  * @param       fs_pa_type* fs_pa_write: PA存储结构体
                 *data : 数据流
                 data_len : 字节长度
                 level    : 目录等级
                 la       : 当前块指向的LA表中的某个偏移
                 next_la  : 链表中下一跳指向的LA表中的某个偏移
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
  * @brief       重写函数，重写
  * @param       id：写入的ID号
                 *data: 写入数据
                 len: 写入数据长度，按照字节计数
  * @retval      FS_SUCCESS : 写入成功
                 FS_INPUT_ERROR : 输入ID错误，或者输入数据过长
                 FS_LA_ERROR: 无足够空闲块    
  测点1: 写入所有PA ，但是未更新LA表，重建应该是可以读出旧的数据
  测点2: 写入所有PA ，更新LA表，但未更新LA的CRC , 重建应该是可以读出旧的数据                  
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
//  找到旧的ID的长度
    UINT32  id_page_cnt = (id_len + PA_DATA_OF_PAGE - 1) / PA_DATA_OF_PAGE;
//  该ID要修改的开始数据,在整个ID链中的偏移
    UINT32  s_la_oft    = offset / PA_DATA_OF_PAGE;
//  该ID要修改的最后数据,在整个ID链中的偏移
    UINT32  e_la_oft    = (offset + len + PA_DATA_OF_PAGE - 1) / PA_DATA_OF_PAGE;
//  修改起始地址不应该超过旧ID的长度
    printx(FS_DEBUG , "offset = %d , len = %d \n" , offset , len);
    if(offset > id_len)
    {
        printx(FS_DEBUG , "offset not contiunes \n");
        return FS_LA_ERROR;
    }
//  判断是否需要申请更多的LA块
    if((offset + len) > id_len)
    {
//  增加数据，要做4对齐处理
        len = len + (4 - len % 4);
//  之前还没用的空间
        if((id_len % PA_DATA_OF_PAGE) != 0)
            free_len  = PA_DATA_OF_PAGE - (id_len % PA_DATA_OF_PAGE);
//  新修改长度 - 本身已经用的id_len - 空余的块 = 需要增加的块
        la_add  = (offset + len - id_len - free_len + PA_DATA_OF_PAGE - 1) / PA_DATA_OF_PAGE;
        printx(FS_DEBUG , "add pa cot = %d \n" , la_add);
    }
//  记录已有的ID的LA号,并且可能申请更多的LA号,用于增加
    la_list = malloc(FS_WORD_TO_BYTE(id_page_cnt + la_add));
//  枚举出已经存在ID的所有LA号
    fs_enum_la(id , la_list , FS_NULL , FS_FUNC_GET_ID_LA);
    for(UINT32 i = 0 ; i < id_page_cnt ; i++)
        printx(FS_DEBUG , "old la list[%d] = %d \n" , i , la_list[i]);
    if(la_add != 0)
    {
//  追加更多的空LA块    
        if(FS_SUCCESS != fs_get_free_la(&la_list[id_page_cnt] , la_add))
        {
           return FS_LA_ERROR;
        }
//  标注新增加的LA链表的为中间块
        for(UINT32 i = id_page_cnt ; i < id_page_cnt + la_add  ; i++)
        {
             fs_la_table[la_list[i]] = FS_MID_LIST;
        }
    }
//  整个LA的数据已经在la_list中
    printx(FS_DEBUG , "start offset = %d , end offset = %d \n" , s_la_oft , e_la_oft);
    for(UINT32 i = s_la_oft ; i < e_la_oft ; i++)
    {
        printx(FS_DEBUG , "leave len = %d \n" , len);
// 获取一个空闲PA块        
        if(FS_SUCCESS != fs_get_free_pa(&fs_dst_no))   
        {
            printx(FS_DEBUG , "have no free pa \n");
            return FS_PA_ERROR;
        }
        fs_pa_table[fs_dst_no] = FS_USED;
        printx(FS_DEBUG , "la[%d] ,free pa  = %d \n" , la_list[i] , fs_dst_no);
/********************************起始包**********************************/                          
        if(i == s_la_oft)
        {
            printx(FS_DEBUG , "start la_offset = %d \n" , i);
            if(FS_SUCCESS != fs_la_to_pa_addr(la_list[i] , &fs_src_addr))
            {    
                FS_POS_DEBUG(1 , "start system bug \n");
                while(1);              
            }
//  读取旧数据                                              
            norflash_word_read(FS_PA_START + fs_src_addr , (UINT32 *)&pa_data->crc ,  FS_PA_PAGE);  
//  分两种情况
//  1.当前数据包为中间数据包,则写入数据长度为原长度，只更新数据，长度不变
//                           a.更新中间部分
//                           b.从中间更新到结尾
//  2.当前数据包为结束数据包,判断增加数据是否超过旧的数据
//                           a.不超过,与一致
//                           b.超过,变更为实际写入长度 
            if((pa_data->len) >= PA_DATA_OF_PAGE)        // 中间包
            {              
                if((len + (offset % PA_DATA_OF_PAGE)) <= PA_DATA_OF_PAGE)
                {
                    printx(FS_DEBUG , "1.a \n"); 
                    write_len = len;                     // 1.a  只可能是处理一个包       
                }                                                 
                else
                {
                    printx(FS_DEBUG , "1.b \n"); 
                    write_len = PA_DATA_OF_PAGE - (offset % PA_DATA_OF_PAGE); 
                    len = len - write_len;               // 1.b  后续肯定有别的包
                    pa_package = write_len;              
                }
            }
            else                                         // 结尾包
            {                                            // 2.a  只可能是处理一个包   
                if((len + (offset % PA_DATA_OF_PAGE)) < pa_data->len)       
                {
                    printx(FS_DEBUG , "2.a \n");  
                    write_len = len;
                }
                else                                     // 2.b   
                {                                        // 更新长度,后续还有包
                    if((len + (offset % PA_DATA_OF_PAGE)) >= PA_DATA_OF_PAGE)         
                    {
                        printx(FS_DEBUG , "2.b1 \n");   
                        write_len = PA_DATA_OF_PAGE - offset % PA_DATA_OF_PAGE; 
                        len = len - write_len; 
                        pa_package = write_len;  
                        pa_data->len = PA_DATA_OF_PAGE;                      
                    }
                    else                                 // 后续没有包了
                    {
                        printx(FS_DEBUG , "2.b2 \n");   
                        write_len = len;
                        pa_data->len = offset % PA_DATA_OF_PAGE + len;
                    }
                }                     
            }      
//  插入修改数据
            memcpy(&pa_data->data[offset % PA_DATA_OF_PAGE] , data , write_len);
//  后面还有数据，链要完整
            pa_data->next_la = la_list[i + 1];
//  更新CRC
            pa_data->crc = fs_crc((UINT8 *)&pa_data->level , 4 * (FS_PA_PAGE - 1));
//  写入PA
            norflash_word_write(FS_PA_START + fs_dst_no * FS_WORD_TO_BYTE(FS_PA_PAGE) , (UINT32 *)&pa_data->crc , FS_PA_PAGE);         
        }
/********************************中间包**********************************/    
        else if((i > s_la_oft) && (i < (e_la_oft - 1)))
        {
            printx(FS_DEBUG , "middle la_offset = %d \n" , i);
//  直接在新的PA表中写数据            
            fs_update_pa(pa_data , &data[pa_package] , PA_DATA_OF_PAGE , 0x00 , la_list[i] , la_list[i + 1]);  
            norflash_word_write(FS_PA_START + fs_dst_no * FS_WORD_TO_BYTE(FS_PA_PAGE) , (UINT32 *)&pa_data->crc , FS_PA_PAGE);
            len = len - PA_DATA_OF_PAGE;
            pa_package = pa_package + PA_DATA_OF_PAGE;
        }
/********************************最后包**********************************/      
        else 
        {
            printx(FS_DEBUG , "end la_offset = %d \n" , i);          
//  无旧数据，直接写到PA                      
            if(la_add != 0)
            {
//  直接在新的PA表中写数据            
                fs_update_pa(pa_data , &data[pa_package] , len , 0x00 , la_list[i] , FS_END_LIST);  
                norflash_word_write(FS_PA_START + fs_dst_no * FS_WORD_TO_BYTE(FS_PA_PAGE) , (UINT32 *)&pa_data->crc , FS_PA_PAGE);
            } 
            else
            {
//  读取旧数据                  
                if(FS_SUCCESS != fs_la_to_pa_addr(la_list[i] , &fs_src_addr))
                {    
                    FS_POS_DEBUG(1 , "end system bug \n");
                    while(1);              
                } 
//  读取旧数据                                              
                norflash_word_read(FS_PA_START + fs_src_addr , (UINT32 *)pa_data ,  FS_PA_PAGE);  
//  插入修改数据
                memcpy(&pa_data->data , &data[pa_package] , len);
//  数据可能增加
                pa_data->len = (len > pa_data->len) ? len : pa_data->len;
//  更新CRC
                pa_data->crc = fs_crc((UINT8 *)&pa_data->level , 4 * (FS_PA_PAGE - 1));
//  写入PA
                norflash_word_write(FS_PA_START + fs_dst_no * FS_WORD_TO_BYTE(FS_PA_PAGE) , (UINT32 *)&pa_data->crc , FS_PA_PAGE);
            }                
          }
//  更新LA映射  
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
  * @brief       写入函数，支持重写   
  * @param       id：写入的ID号
                 *data: 写入数据
                 len: 写入数据长度，按照字节计数
  * @retval      FS_SUCCESS : 写入成功
                 FS_INPUT_ERROR : 输入ID错误，或者输入数据过长
                 FS_LA_ERROR: 无足够空闲块    
  测点1: 写入所有PA ，但是未更新LA表，重建应该是可以读出旧的数据
  测点2: 写入所有PA ，更新LA表，但未更新LA的CRC , 重建应该是可以读出旧的数据                  
***********************************************************************************************************/
UINT32           fs_write_data(UINT16 id , UINT8 *data , UINT32 offset , UINT32 len)
{                               
// 需要申请的PA块数量    
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
// ID已存在，将已存在的ID在LA表中清除，并且回收一遍PA表
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
// 写入数据按照32bit对齐       
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
// PA写入结构体
        la_no = malloc(FS_WORD_TO_BYTE(page_cnt + 1));       // get free la range        
// 判断是否有足够的空闲块可供使用，构造出链表序列
        if(FS_SUCCESS == fs_get_free_la(la_no , page_cnt))
        {
// 首块，记录ID
            fs_la_table[la_no[0]] = id;
// 中间块，做好标识, 如果只有一个块，这里不执行
            for(UINT32 i = 1 ; i < page_cnt ;  i++)
            {
                fs_la_table[la_no[i]] = FS_MID_LIST;    
            }
// 结束指针，为了记录到PA中最后一个块的next_la为结束
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
// 获取一个空闲PA块        
            if(FS_SUCCESS == fs_get_free_pa(&pa_offset))
            {
                fs_pa_table[pa_offset] = FS_USED;
                if(fs_pa_free_cnt > 1)
                {
                    fs_pa_free_cnt = fs_pa_free_cnt - 1;    
                }
                else
                {
// 无多余的PA块，需要回收先            
                    FS_POS_DEBUG(1 , "free pa blk not match la");
                    fs_need_recover = 1;
                    result = FS_PA_ERROR;
                    goto EXPECT;    
                }
            }
            else
            {
// 无多余的PA块，一定是LA与PA不一致引起        
                FS_POS_DEBUG(1 , "system bug la not match pa");
                while(1);
            }
            fs_la_table[la_no[i]] = fs_la_table[la_no[i]] | (pa_offset << 16);
            printx(FS_DEBUG , "la[%d] = pa_no = %x  id = %x \n" , la_no[i] , FS_GET_PA(fs_la_table[la_no[i]]) , FS_GET_ID(fs_la_table[la_no[i]])); 
            pa_write_len = (len > PA_DATA_OF_PAGE) ? PA_DATA_OF_PAGE : len;
            len = len - pa_write_len;
// 将数据流存到结构体中
            fs_update_pa(&fs_pa_write , &data[pa_package] , pa_write_len , 0x00 , la_no[i] , la_no[i + 1]);  
// 将结构化数据写入Norflash中
#if  (1 == FS_TEST)
// 写入测点1  旧数据应该完好
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
    //  待写入数据减少          
            pa_package = pa_package + pa_write_len;  
            watchdog_feed();      
        }
#if  (1 == FS_TEST)
    //  写入测点2 旧数据应该完好
            if(FS_WRITE_TEST_NO2 == test_no)
            {
                printx(FS_DEBUG , "write test point 2 happen \n");  
                chip_reset(SYSTEM_REBOOT);
            }
#endif
    //  更新LA表到Norflash中
        fs_update_la(fs_la_table);
    //  更新LA表的CRC到Norflash中
#if  (1 == FS_TEST)
    //  写入测点4 旧数据应该完好，有LA恢复过程
            if(FS_WRITE_TEST_NO4 == test_no)
            {
                printx(FS_DEBUG , "write test point 4 happen \n");  
                chip_reset(SYSTEM_REBOOT);
            }
#endif
        fs_update_la_crc(fs_la_table);
#if  (1 == FS_TEST)
    //  写入测点5  新数据应该被更新
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
  * @brief         读取函数
  * @param         id：读取的ID号
                   len: 读取数据长度，按照字节计数
  * @retval         *data: 读取数据

                   FS_SUCCESS : 读取成功
                   FS_ID_EXIST: ID不存在
***********************************************************************************************************/
UINT32             fs_read_data(UINT16 id , UINT8 *data , UINT32 offset , UINT32 len)
{
    UINT32   id_len = 0;
    UINT32   *la_elem = FS_NULL;
    UINT32   pa_no = 0;
    UINT32   la_str_offset = offset / PA_DATA_OF_PAGE;
    UINT32   la_end_offset = (offset + len + PA_DATA_OF_PAGE - 1) / PA_DATA_OF_PAGE;
    UINT32   *read_buf = FS_NULL;
//  ID不存在USB报错
    if(FS_SUCCESS != fs_check_id(id))
    {
        printx(FS_DEBUG , "id not exist , fail \n");
        return FS_ID_EXIST;
    }

//  读取该ID的数据有效数据长度
    if(FS_SUCCESS == fs_enum_la(id , FS_NULL , &id_len , FS_FUNC_GET_ID_LEN))
    {
        if((offset + len) > id_len)
        {
            printx(FS_DEBUG , "invalid read input %d , id = %d , len = %d\n" , offset + len , id , id_len);
            return FS_INPUT_ERROR; 
        }      
        la_elem = malloc(la_end_offset);                           
//  读取该ID的所有数据内容      
        fs_enum_la(id , la_elem , FS_NULL , FS_FUNC_GET_ID_LA);
//  读取数据       
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
  * @brief         删除某个ID
  * @param         id：删除的ID号
  * @retval      
***********************************************************************************************************/
static      void   fs_clear_id(UINT16 id)
{
    UINT32   id_len = 0;
    UINT32   *la_elem = FS_NULL;  
    UINT32   id_page_cnt = 0;
    UINT32   pa_no = 0;
//  读取该ID的数据有效数据长度
    if(FS_SUCCESS == fs_enum_la(id , FS_NULL , &id_len , FS_FUNC_GET_ID_LEN))
    {
        id_page_cnt = (id_len + PA_DATA_OF_PAGE - 1) / PA_DATA_OF_PAGE;
        la_elem = malloc(FS_WORD_TO_BYTE(id_page_cnt));                           // get free la range
//  读取该ID的所有数据内容           
        fs_enum_la(id , la_elem , FS_NULL , FS_FUNC_GET_ID_LA);
        for(UINT32 i = 0 ; i < id_page_cnt ; i++)
        {
//  清除LA表       
            fs_la_table[la_elem[i]] = FS_BLANK_DATA;
            printx(FS_DEBUG , "clear la[%d] unused  = %x \n"  , la_elem[i] , fs_la_table[la_elem[i]]);            
        }
    }
    free(la_elem);
    fs_need_recover = 1;
}

/**********************************************************************************************************
  * @brief         删除某个ID
  * @param         id：删除的ID号
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
// 清除LA表中的内容
    fs_clear_id(id);
// 将LA表写入Norflash
    fs_update_la(fs_la_table);
    fs_update_la_crc(fs_la_table);
// 回收
    fs_need_recover = 1;
    return FS_SUCCESS;
}
/**********************************************************************************************************
  * @brief         删除整个文件系统，回归初始化状态
  * @param         id：删除的ID号
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
  * @brief         删除整个文件系统，回归初始化状态
  * @param         id：删除的ID号
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
//  根据PA回收表，擦除对应的PA块数据  
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