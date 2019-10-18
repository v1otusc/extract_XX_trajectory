#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <string.h>   // actually, cstring is a replacement of string.h
#include <iomanip>
#include <cmath>
#include <thread>
#include <mutex>
#include <map>
#include <algorithm>

#if defined(linux) || defined(__linux) || defined(__linux__)
    #include <sys/sysinfo.h>         // CPU core
#else
    #include <windows.h>
    #include <Winbase.h>
#endif

using namespace std;

#define BUFFER_SIZE  10024           // if file with one line larger than this number, it is a illegal file
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(AFX_BLBETWEENXY1_H__7E8A8BAB_9C50_4778_85BF_4AF48E9BE433__INCLUDED_)
#define AFX_BLBETWEENXY1_H__7E8A8BAB_9C50_4778_85BF_4AF48E9BE433__INCLUDED_

#define const_num 0.000000001

inline int get_cpu_core_num()
{
#if defined(WIN32)
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
#elif defined(LINUX) || defined(SOLARIS) || defined(AIX) || defined(linux) || defined(__linux__) || defined(__linux)
	return get_nprocs();   //GNU fuction
#else
    #error  "not supported!"
#endif
}

class process_notify
{
public:
	void push(int percent, const string& sNotify = "      Processed ")
	{
		if ((percent % __step == 0) && percent != __old_percent)
		{
			cout << sNotify << ":" << percent << "% ..." << endl;
			__old_percent = percent;
		}
	}
	void init(const int step1)
	{
		__step = step1;
		__old_percent = -1;
	}
public:
	process_notify(const int step1 = 5) :__old_percent(-1)
	{
		__step = step1;
	}
protected:
	int __old_percent;
	int __step;
};



