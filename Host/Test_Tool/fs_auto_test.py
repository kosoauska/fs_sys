# coding=utf-8
import re
import os
import time
import string
import sys
from   ctypes import *
import filecmp
import random
import platform
import struct
import shutil
# id debug
fs_debug = 1

# id init
cmd_connect     = [0x96 , 0x00]
cmd_read        = [0x96 , 0x01]
cmd_write       = [0x96 , 0x02]
cmd_delete      = [0x96 , 0x03]
cmd_check       = [0x96 , 0x04]
cmd_clear       = [0x96 , 0x07]
cmd_reboot      = [0x96 , 0x08]
cmd_capacity    = [0x96 , 0x09]
# 写入异常测点
cmd_w_point_1   = [0x96 , 0x21]
cmd_w_point_2   = [0x96 , 0x22]
cmd_w_point_3   = [0x96 , 0x23]
cmd_w_point_4   = [0x96 , 0x24]
cmd_w_point_5   = [0x96 , 0x25]

# 擦除异常测试
cmd_e_point_1   = [0x96 , 0x31]
cmd_e_point_2   = [0x96 , 0x32]
cmd_e_point_3   = [0x96 , 0x33]
cmd_e_except_test = [cmd_e_point_1 , cmd_e_point_2 , cmd_e_point_3]
# id init
id_no_range     = [0 , 128]
id_table        = [x for x in range (id_no_range[0] , id_no_range[1])]
id_list         = [None] * (id_no_range[1] + 1)
disk_letter     = ord('G')

write_file_path =  '.\\write\\'
read_file_path  =  '.\\read\\'

#sys.path.append('.')
(os_bit,os_temp) = platform.architecture()
if (os_bit == "32bit"):
    print("32bit")
    api = windll.LoadLibrary('.\\cmdAPI_32.dll')
else:
    print("64bit")
    api = windll.LoadLibrary('.\\cmdAPI_64.dll')

############################################# 指令发送函数 ########################################################
# define SCSI_OUT  0x01
# define SCSI_IN   0x02
# extern "C" CMD_DLL_API INT32 WINAPI TransferCMD(CHAR cLetter , UINT8 first,UINT8 second ,UINT8 third , UINT32 forth ,UINT32 fifth , UINT8 direction, UCHAR* path,UINT32 data_len = 0);
def     fs_cmd(cmd , id , offset , dir , file_name , len):
        cmd_dev  = c_char(disk_letter)
        cmd_fst  = c_int(cmd[0])
        cmd_sec  = c_int(cmd[1])
        cmd_trd  = c_int(id)
        cmd_for  = c_int(offset)
        cmd_fif  = c_int(0x00)
        cmd_six  = c_int(0x00)
        cmd_dir  = c_int(dir)
        cmd_file = c_char_p()
        cmd_file.value = bytes(file_name , encoding="utf-8")
        cmd_len  = c_int(len)
        try:
            ret = api.TransferCMD(cmd_dev , cmd_fst , cmd_sec , cmd_trd ,  cmd_for , cmd_fif , cmd_six , cmd_dir , cmd_file , cmd_len)
        except:
            print("cmd_dev = %s , cmd[0] = %x , cmd[1] = %x , id = %x except" % (cmd_dev , cmd[0] , cmd[1] , id))
            return False
        if(ret != 0):
            if 1 == 0:
                print("cmd_dev = %s , cmd[0] = %x , cmd[1] = %x , id = %x fail" % (cmd_dev, cmd[0], cmd[1], id))
            return False
        return True

############################################# 对比测试数据 ########################################################
def    fs_compare_data(id_list):
    for i in id_list:
        if(None != i):
            try:
                write_id = write_file_path + str(i)
                read_id  = read_file_path  + str(i)
                if filecmp.cmp(write_id , read_id) != True:
                    print("cmp write_id = %s , read_id = %s , id_len = %5d , fail" % (write_id, read_id, os.path.getsize(write_id)))
                    return False
                else:
                    print("cmp write_id = %s , read_id = %s , id_len = %5d , pass" % (write_id , read_id , os.path.getsize(write_id)))
            except:
                print("%s or %s not exits " % (write_id , read_id))
    return True
############################################# 产生样本数据 ########################################################
def     fs_del_test_file(filepath):
    del_list = os.listdir(filepath)
    for f in del_list:
        file_path = os.path.join(filepath, f)
        if os.path.isfile(file_path):
            os.remove(file_path)
        elif os.path.isdir(file_path):
            shutil.rmtree(file_path)

############################################# 产生样本数据 ########################################################
def    fs_gen_sig_data(fs_id , start_len , end_len):
    id_len = random.randint(start_len , end_len)
    fs_id = write_file_path + fs_id
    if 0 == 1:
        print("id len = %d , id name = %s \n" % (id_len , fs_id))
    blist = [random.randint(-128 , 127) for i in range(id_len)]
    with open(fs_id , "wb+") as id_handler:
        for i in blist:
            write_data = struct.pack('b' , i)
            id_handler.write(write_data)

############################################# 产生样本数据 ########################################################
def     fs_rewrite_data(fs_id):
    fs_id = '.\\write\\' + fs_id
    rand_offset = 1
    file_len = os.path.getsize(fs_id)                                   # 读取文件长度
    while (rand_offset % 4) != 0:
        rand_offset = random.randint(0 , file_len - 10)                 # 偏移必须在已有文件内部
    rand_len = random.randint(0 , file_len - rand_offset + 128)         # 任意产生长度
    print("offset = %d , len = %d" % (rand_offset , rand_len))
    blist = [random.randint(-128 , 127) for i in range(rand_len)]
    with open(fs_id, "r+b") as id_handler:
        id_handler.seek(rand_offset , 0)
        for i in blist:
            write_data = struct.pack('b', i)
            id_handler.write(write_data)
            id_handler.flush()
    print("new id = %s , offset = %d , len = %d" % (fs_id , rand_offset , rand_len))
    return rand_offset , (rand_len + rand_offset)
############################################# 产生多个样本数据 ###################################################
def    fs_gen_mul_data(start_len , end_len):
    fs_del_test_file(write_file_path)
    for i in id_table:
        fs_gen_sig_data(str(i) , start_len, end_len)

############################################# 写入单个ID  ########################################################
def    fs_write_id(fs_id , offset , len = 0):
    write_id = write_file_path + str(fs_id)
    if len == 0:
        id_len = os.path.getsize(write_id)
    else:
        id_len = len
    if 1 == 1:
        print("write id = %s , offset = %d , len = %d , cnt = %d " % (fs_id , offset , id_len , (id_len + 207) / 208))
    return fs_cmd(cmd_write , fs_id , offset , 0x01 , write_id , id_len)

############################################# 读取单个ID  ########################################################
def    fs_read_id(fs_id , offset):
    write_id = write_file_path + str(fs_id)
    read_id  = read_file_path + str(fs_id)
    id_len   = os.path.getsize(write_id)
    if fs_debug == 1:
        print("orig id len = %d " % id_len)
    return  fs_cmd(cmd_read  , fs_id , offset , 0x02 , read_id , id_len)

############################################# 上电连接    ########################################################
def    fs_connect():
    while False == fs_cmd(cmd_connect  , 0x00 , 0x00 , 0x02 , 'invalid.bin' , 1):
        time.sleep(5)
    return True

############################################# 清除文件系统 #######################################################
def    fs_clear_all():
    return fs_cmd(cmd_clear  , 0x00 , 0x00 , 0x02 , 'invalid.bin' , 1)

############################################# 芯片重启 ###########################################################
def    fs_reboot():
    return fs_cmd(cmd_reboot , 0x00 , 0x00 , 0x02 , 'invalid.bin' , 1)

############################################# 删除单个ID  ########################################################
def    fs_delete_id(fs_id):
    return fs_cmd(cmd_delete , fs_id , 0x00 , 0x02 , 'invalid.bin' , 1)

############################################# 检测单个ID  ########################################################
def    fs_check_id(fs_id):
    return fs_cmd(cmd_check  , fs_id , 0x00 , 0x02 , 'invalid.bin' , 1)

############################################# 写入测点1   ########################################################
def    fs_write_test_1():
    return fs_cmd(cmd_w_point_1  , 0x00 , 0x00 , 0x02 , 'invalid.bin' , 1)

############################################# 写入测点2   ########################################################
def    fs_write_test_2():
    return fs_cmd(cmd_w_point_2  , 0x00 , 0x00 , 0x02 , 'invalid.bin' , 1)

############################################# 写入测点3   ########################################################
def    fs_write_test_3():
    return fs_cmd(cmd_w_point_3  , 0x00 , 0x00 , 0x02 , 'invalid.bin' , 1)

############################################# 写入测点4   ########################################################
def    fs_write_test_4():
    return fs_cmd(cmd_w_point_4  , 0x00 , 0x00 , 0x02 , 'invalid.bin' , 1)

############################################# 写入测点5   ########################################################
def    fs_write_test_5():
    return fs_cmd(cmd_w_point_5  , 0x00 , 0x00 , 0x02 , 'invalid.bin' , 1)

############################################# 擦除测点1   ########################################################
def    fs_erase_test_1():
    return fs_cmd(cmd_e_point_1  , 0x00 , 0x00 , 0x02 , 'invalid.bin' , 1)

############################################# 擦除测点2   ########################################################
def    fs_erase_test_2():
    return fs_cmd(cmd_e_point_2  , 0x00 , 0x00 , 0x02 , 'invalid.bin' , 1)

############################################# 擦除测点3   ########################################################
def    fs_erase_test_3():
    return fs_cmd(cmd_e_point_3  , 0x00 , 0x00 , 0x02 , 'invalid.bin' , 1)

############################################# 恢复现场   #########################################################
def    fs_clear(fs_id_list):
    for i in range(0 , len(fs_id_list) - 1):
        if False == fs_delete_id(fs_id_list[i]):
            print("del id = %d test fail " % fs_id_list[i])
            return False
    return True

############################################# 重启 #############################################################
def    fs_start():
    fs_connect()
    fs_clear_all()
    print("clear all")
    fs_reboot()
    print("reboot")
    fs_connect()
    print("connect success")

############################################# 检测ID #############################################################
def    fs_check_all_id(fs_id_list):
    j = 0
    for i in range (id_no_range[0] , id_no_range[1]):
        if True == fs_cmd(cmd_check  , i , 0x00 , 0x02 , 'invalid.bin' , 1):
            fs_id_list[j] = i
        j = j + 1
    return fs_id_list

############################################# 读取指定ID #########################################################
def     fs_check_select_id(fs_id_list):
    for i in range(0 , id_no_range[1]):
# 读取样本ID
        if None != fs_id_list[i]:
            if False == fs_read_id(fs_id_list[i] , 0x00):
                print("fs check select id read id = %d fail " % fs_id_list[i])
                return False
# 比对结果
    if True == fs_compare_data(fs_id_list):
        print("fs check select id list success ")
        return True
    else:
         print("fs check select id list fail")
         return False

############################################# 大文件写入    ###################################################
def     fs_big_file(round , sample , file_size_min , file_size_max):
    result = False
    test_id_list = [None] * sample
    left_id_list = [None] * (sample - 1)
    fs_start()
    for k in range(0 , round):
        j = 0
# 产生样本数据
        fs_gen_mul_data(file_size_min , file_size_max)
# 产生测试ID
        test_id_list =  random.sample(range(id_no_range[0] , id_no_range[1]) , sample)
        for i in range(0 , sample):
            if False == fs_write_id(test_id_list[i] , 0x00):
                print("fs big id write fail " % test_id_list[i])
                return False
        if 1 == fs_debug:
            print("fs big del id list:" , end='')
            print(test_id_list)
        del_id = random.choice(test_id_list)
        print("del id = " , end='')
        print(del_id)
        if False == fs_delete_id(del_id):
            print("fs del id = %d test fail " % del_id)
            return False
# 读取剩余ID
        for i in range(0, sample):
            if test_id_list[i] != del_id:
                left_id_list[j] = test_id_list[i]
                j = j + 1
                if False == fs_read_id(test_id_list[i] , 0x00):
                      print("fs big read id = %d fail " % test_id_list[i])
                      return False
# 比对剩余数据是否被破坏
        if True == fs_compare_data(left_id_list):
            print("fs big round %d success" % k)
        else:
            print("fs big round %d fail" % k)
            return False
# 删除ID
        if 1 == fs_debug:
             print("left id list:", end='')
             print(left_id_list)
    return  True

############################################# 写入异常        ################################################
def     fs_write_except(test_no , round , sample , file_size_min , file_size_max):
    test_id_list = [None] * sample
    if test_no >= 6:
        return False
    for k in range(0, round):
        fs_start()
# 产生样本数据
        fs_gen_mul_data(file_size_min, file_size_max)
# 产生测试ID
        test_id_list = random.sample(range(id_no_range[0], id_no_range[1]), sample)
        for i in range(0, sample - 1):
            if False == fs_write_id(test_id_list[i] , 0x00):
                print("write except id = %d " % test_id_list[i])
                return False
# 写入测试测点
        del_id = test_id_list[sample - 1]
        print("write except test no = %d , id : %d" % (test_no, del_id))
        if 0 == test_no:
            fs_write_test_1()
            fs_write_id(del_id , 0x00)
        elif 1 == test_no:
            fs_write_test_2()
            fs_write_id(del_id , 0x00)
        elif 2 == test_no:
            fs_write_test_3()
            fs_write_id(del_id , 0x00)
        elif 3 == test_no:
            fs_write_test_4()
            fs_write_id(del_id , 0x00)
        elif 4 == test_no:
            fs_write_test_5()
            fs_write_id(del_id , 0x00)
# 重写过程中，在更新LA表之前断电
        elif 5 == test_no:
# 写入最后一个ID
            fs_write_id(del_id , 0x00)
            fs_write_test_4()
            fs_gen_sig_data(str(del_id), file_size_min, file_size_max)
# 重写最后一个ID
            fs_write_id(del_id , 0x00)
# 等待连接
        fs_connect()
        for i in range(0, sample - 1):
            if False == fs_read_id(test_id_list[i] , 0x00):
                print("write except id = %d " % test_id_list[i])
                return False
        if True != fs_compare_data(test_id_list[0: sample - 1]):
            print("fs write except round %d fail" % k)
            return False
# check last op fail id
        result = fs_check_id(del_id)
# 0 ~ 4 测点，最后一笔数据丢弃，原数据应该都在, 但是最后一个ID一定不存在
        if test_no < 4:
            if True == result:
                fs_read_id(del_id , 0x00)
                write_id = write_file_path + str(del_id)
                read_id = read_file_path + str(del_id)
                if True == filecmp.cmp(write_id, read_id):
                    print("fs write except fail %d , id = %d should not same" % (test_no, del_id))
                    return False
# 5 测点，最后一笔数据写入，最后一个ID被写入
        elif test_no == 4:
            fs_read_id(del_id , 0x00)
            write_id = write_file_path + str(del_id)
            read_id = read_file_path + str(del_id)
            if False == filecmp.cmp(write_id, read_id):
                print("fs write except %d fail , id = %d should exist" % (test_no, del_id))
                return False
# 6 测点，最后一笔数据写入，最后一个ID被写入失败
        elif test_no == 5:
            fs_read_id(del_id , 0x00)
            write_id = write_file_path + str(del_id)
            read_id = read_file_path + str(del_id)
            if True == filecmp.cmp(write_id, read_id):
                print("fs write except %d fail, id = %d should exist" % (test_no, del_id))
                return False
    return True

############################################# 擦除异常          ################################################
def     fs_erase_except(test_no , round , sample , file_size_min , file_size_max):
    test_id_list = [None] * sample
    if test_no >= 3:
        return False
    for k in range(0, round):
        fs_start()
# 产生样本数据
        fs_gen_mul_data(file_size_min, file_size_max)
# 产生测试ID
        test_id_list = random.sample(range(id_no_range[0], id_no_range[1]), sample)
# 取一个ID
        del_id = test_id_list[sample - 1]
        for i in range(0, sample):
            if False == fs_write_id(test_id_list[i] , 0x00):
                print("erase except id = %d " % test_id_list[i])
                return False
# 删除过程中
        if 0 == test_no:
            fs_erase_test_1()
            fs_delete_id(del_id)
# 重写一个ID
        elif 1 == test_no:
            fs_erase_test_2()
            fs_gen_sig_data(str(del_id), file_size_min, file_size_max)
            fs_write_id(del_id , 0x00)
# 重写一个ID
        elif 2 == test_no:
            fs_erase_test_3()
            fs_gen_sig_data(str(del_id), file_size_min, file_size_max)
            fs_write_id(del_id , 0x00)
        print("erase except test no = %d , id : %d" % (test_no, del_id))

# 等待连接
        fs_connect()
        for i in range(0, sample - 1):
            if False == fs_read_id(test_id_list[i] , 0x00):
                print("erase except id = %d " % test_id_list[i])
                return False
        if True != fs_compare_data(test_id_list[0: sample - 1]):
            print("fs erase except round %d fail" % k)
            return False
        result = fs_check_id(del_id)
        if True == result:
            fs_read_id(del_id , 0x00)
# 1 测点 , 旧数据应该删除
        if test_no == 0:
            if True == result:
                print("fs erase except %d fail" % test_no)
                return False
# 2 测点 , 新数据应该写入
        elif test_no == 1:
            if False == result:
                print("fs erase except %d id fail" % test_no)
                return False
            else:
                write_id = write_file_path + str(del_id)
                read_id = read_file_path + str(del_id)
                if False == filecmp.cmp(write_id , read_id):
                    print("fs erase except %d compare fail" % test_no)
                    return False
# 3 测点 , 新数据应该写入
        elif test_no == 2:
            if False == result:
                print("fs erase except %d id fail" % test_no)
                return False
            else:
                write_id = write_file_path + str(del_id)
                read_id = read_file_path + str(del_id)
                if False == filecmp.cmp(write_id, read_id):
                    print("fs erase except %d compare fail" % test_no)
                    return False
    return True
############################################# 读取剩余容量         ################################################
def     fs_read_capacity():
    fs_cmd(cmd_capacity , 0x00 , 0x00 , 0x02 , 'cap.bin', 4)
    with open("cap.bin" , "rb+") as fs_handler:
        fs_cap = fs_handler.readlines(4)
    fs_free_space = int.from_bytes(fs_cap[0]  , byteorder='big')
    print(fs_free_space)
    return fs_free_space
####################################################################################################################
#                                                                                                                  #
#                                                                                                                  #
#                                               以下是测试样例                                                     #
#                                                                                                                  #
#                                                                                                                  #
############################################# ID覆盖写入测试样例  ##################################################
def     fs_test_sample_seq_recover(round , sample):
# 每次随机测试样本ID
    for k in range(0 , round):
        test_id_list = [None] * sample
        print("fs_test_sample_seq_recover")
        fs_start()
# 产生样本数据
        fs_gen_mul_data(20 , 1024)
# 产生测试ID
        test_id_list = random.sample(range(id_no_range[0] , id_no_range[1]) , sample)
        for i in range(0 , sample):
# 写入样本ID
            if False == fs_write_id(test_id_list[i] , 0x00):
                print("fs seq write id = %d fail " % test_id_list[i])
                os.system("pause")
        for i in range(0, sample):
# 读取样本ID
            if False == fs_read_id(test_id_list[i] , 0x00):
                print("fs seq read id = %d fail " % test_id_list[i])
                os.system("pause")
# 比对结果
        if True != fs_compare_data(test_id_list):
            print("fs seq compare data fail ")
            os.system("pause")
# 删除ID
        result = fs_clear(test_id_list)
############################################# ID删除测试  ########################################################
def     fs_test_sample_del(round, sample):
# 每次随机测试样本ID
    for k in range(0 , round):
        test_id_list = [None] * sample
        left_id_list = [None] * (sample - 1)
        print("fs_test_sample_del")
        fs_start()
        j = 0
# 产生样本数据
        fs_gen_mul_data(20 , 1023)
# 产生测试ID
        test_id_list = random.sample(range(id_no_range[0] , id_no_range[1]) , sample)
        for i in range(0, sample):
# 写入样本ID
            if False == fs_write_id(test_id_list[i] , 0x00):
                print("fs del write id = %d fail " % test_id_list[i])
                os.system("pause")
# 删除某个ID
        if 1 == fs_debug:
            print("test id list:" , end='')
            print(test_id_list)
        del_id = random.choice(test_id_list)
        print("del id = " , end='')
        print(del_id)
        if False == fs_delete_id(del_id):
            print("fs del id = %d test fail " % del_id)
            os.system("pause")
# 读取剩余ID
        for i in range(0 , sample):
            if test_id_list[i] != del_id:
                left_id_list[j] = test_id_list[i]
                j = j + 1
                if False == fs_read_id(test_id_list[i] , 0x00):
                    print("fs del read id = %d fail " % test_id_list[i])
                    os.system("pause")
# 比对剩余数据是否被破坏
        if True != fs_compare_data(left_id_list):
            print("fs del round %d fail" % k)
            os.system("pause")
# 删除ID
        if 1 == fs_debug:
            print("left id list:", end='')
            print(left_id_list)
        result = fs_clear(left_id_list)
    return result

########################################### 大文件写入测试样例#####################################################
def         fs_test_sample_big_file(round , sample):
    for i in range(0 , round):
        print("big file test round %d" % i)
        if False == fs_big_file(1 , sample , 1023 , 1024):
            print("big file test fail")
            os.system("pause")
        id_list = [None] * (id_no_range[1] + 1)
        id_list = fs_check_all_id(id_list)
        if False == fs_check_select_id(id_list):
            print("check select id fail")
            os.system("pause")

############################################# 中间重写        ################################################
def         fs_test_rewrite(round , sample):
    # 每次随机测试样本ID
    for k in range(0, round):
        test_id_list = [None] * sample
        offset_list  = [None] * sample
        len_list     = [None] * sample
        print("fs_rewrite_test")
        fs_start()
# 产生样本数据
        fs_gen_mul_data(20 , 1024)
# 产生测试ID
        test_id_list = random.sample(range(id_no_range[0], id_no_range[1]), sample)
        for i in range(0 , sample):
# 写入样本ID
            if False == fs_write_id(test_id_list[i] , 0x00):
                print("fs origal write id = %d fail " % test_id_list[i])
                os.system("pause")
            src_id = write_file_path + str(test_id_list[i])
            dst_id = write_file_path + str(test_id_list[i]) + '_back'
            print("src_id = %s" % src_id)
            print("dst_id = %s" % dst_id)
            cp_cmd = 'copy ' + src_id  +  ' ' + dst_id
            print("cp cmd = %s" % cp_cmd)
            os.popen(cp_cmd)
        for i in range(0, sample):
# 读取样本ID
            if False == fs_read_id(test_id_list[i] , 0x00):
                print("fs origal read id = %d fail " % test_id_list[i])
                os.system("pause")
# 比对结果
        if True != fs_compare_data(test_id_list):
            print("fs origal compare data fail ")
            os.system("pause")
# 产生一个文件内的随机偏移和随机长度重新写入
        for i in range(0, sample):
            offset_list[i] , len_list[i] = fs_rewrite_data(str(test_id_list[i]))
            print("**new id = %d , offset_list = %d , len_list = %d " % (test_id_list[i] , offset_list[i] , len_list[i]))
# 重写样本ID
        for i in range(0, sample):
            if False == fs_write_id(test_id_list[i] , offset_list[i] , len_list[i]):
                print("fs new write id = %d fail " % test_id_list[i])
                os.system("pause")
        for i in range(0, sample):
# 读取样本ID
            if False == fs_read_id(test_id_list[i] , 0):
                print("fs new read id = %d fail " % test_id_list[i])
                os.system("pause")
                # 比对结果
        if True != fs_compare_data(test_id_list):
            print("fs new compare data fail ")
            os.system("pause")
    result = fs_clear(test_id_list)
############################################# 异常写入测试样例######################################################
def         fs_test_sample_write_except(round , sample):
    for i in range(0, 5):
        print("write except test %d" % i)
        if False == fs_write_except(i , round , sample , 512 , 1024):
            print("fs write except %d fail " % i)
            os.system("pause")
############################## ############### 异常擦除测试样例#####################################################
def         fs_test_sample_erase_except(round , sample):
    for i in range(0, 3):
        print("erase except test %d" % i)
        if False == fs_erase_except(i , round , sample , 512, 1024):
            print("fs erase except test no %d fail " % i)
            os.system("pause")

############################################# 主循环 ############################################################
if __name__ == '__main__':
     # fs_read_capacity()
     # fs_test_sample_seq_recover(2 , 20)
     # fs_read_capacity()
     # fs_test_sample_del(2 , 20)
     # fs_read_capacity()
     # fs_test_sample_big_file(2 , 20)
     # fs_read_capacity()
     # fs_test_rewrite(2 , 20)
     # fs_read_capacity()
     # fs_test_sample_write_except(2, 20)
     # fs_read_capacity()
     # fs_test_sample_erase_except(2, 2)
     # fs_read_capacity()
     os._exit(0)