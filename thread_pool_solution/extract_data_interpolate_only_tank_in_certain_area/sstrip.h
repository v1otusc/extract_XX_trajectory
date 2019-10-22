#ifndef STRIP_H
#define STRIP_H

// 在头文件中编写函数实现需要注意
#include "sstrip.h"
#include <iostream>

std::string do_strip(const std::string &str,
                     int striptype,
                     const std::string &chars);

std::string strip(const std::string &str,
                  const std::string &chars = " ");

std::string lstrip(const std::string &str,
                   const std::string &chars = " ");

std::string rstrip(const std::string &str, const std::string &chars = " ");

class Sstrip
{
    /*
    #define left 0
    #define right 1
    #define both 2
    */
    friend std::string do_strip(const std::string &str,
                                int striptype,
                                const std::string &chars)
    {
        std::string::size_type strlen = str.size();
        std::string::size_type charslen = chars.size();
        std::string::size_type i, j;

        //默认情况下，去除空白符
        if (0 == charslen)
        {
            i = 0;
            //去掉左边空白字符
            if (striptype != 1)
            {
                while (i < strlen&&::isspace(str[i]))
                {
                    i++;
                }
            }
            j = strlen;
            //去掉右边空白字符
            if (striptype != 0)
            {
                j--;
                while (j >= i&&::isspace(str[j]))
                {
                    j--;
                }
                j++;
            }
        }
        else
        {
            //把删除序列转为c字符串
            const char*sep = chars.c_str();
            i = 0;
            if (striptype != 1)
            {
                //memchr函数：从sep指向的内存区域的前charslen个字节查找str[i]
                while (i < strlen&&memchr(sep, str[i], charslen))
                {
                    i++;
                }
            }
            j = strlen;
            if (striptype != 0)
            {
                j--;
                while (j >= i&&memchr(sep, str[j], charslen))
                {
                    j--;
                }
                j++;
            }
            //如果无需要删除的字符
            if (0 == i&& j == strlen)
            {
                return str;
            }
            else
            {
                return str.substr(i, j - i);
            }
        }
    }

    friend std::string strip(const std::string &str,
                             const std::string &chars = " ")
    {
        return do_strip(str, 2, chars);
    } 

    friend std::string lstrip(const std::string &str,
                              const std::string &chars = " ")
    {
        return do_strip(str, 0, chars);
    }                               

    friend std::string rstrip(const std::string &str, 
                              const std::string &chars = " ")
    {
        return do_strip(str, 1, chars );
    }      

};

#endif