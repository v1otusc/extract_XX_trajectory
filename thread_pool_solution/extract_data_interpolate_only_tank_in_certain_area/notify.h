#ifndef NOTIFY_H
#define NOTIFY_H

#include <iostream>
#include <string>

using namespace std;

class Process_notify
{
public:
	// this API will trig a notification if the first time reach 5%, 15%, ....
	void push(int current_num, const string& sNotify = "      Processed ")
    {
		// 强制类型转换
    	int percent = static_cast<int>(100.0*current_num/__iall);
        
        if((percent % __step == 0) && percent != __old_percent)
        {
            cout << sNotify << ":" << percent << "% ..." << endl;
            __old_percent = percent;
        }
	}
	void init(const int total_number,
              const int percent_step_to_show_notice = 5)
	{
    	__step = percent_step_to_show_notice;
    	__old_percent = -1;
       
     	__iall = total_number;
	}
public:
    Process_notify(const int total_number,
				   const int percent_step_to_show_notice = 5):__old_percent(-1)
    {
        __step = percent_step_to_show_notice;
        __iall = total_number;
    }
protected:
    int __old_percent;
    int __step;
    int __iall;
};

#endif