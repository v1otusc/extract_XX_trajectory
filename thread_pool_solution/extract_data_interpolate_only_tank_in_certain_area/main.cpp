#include <vector>
#include <fstream>
/*stringstream*/
#include <sstream>
#include <cstring>
#include <iomanip>	
#include <cmath>
#include <thread>
#include <mutex>
#include <map>
#include <algorithm>

/*计时*/
#include "Timer.h"
/*定时汇报消息*/
#include "notify.h"

using namespace std;

#if defined(linux) || defined(__linux) || defined(__linux__)
    //CPU core
	#include <sys/sysinfo.h>
#else
    #include <windows.h>
    #include <Winbase.h>
#endif

// if file with one line larger than this number, it is a illegal file
#define BUFFER_SIZE  10024           
#if !defined(AFX_BLBETWEENXY1_H__7E8A8BAB_9C50_4778_85BF_4AF48E9BE433__INCLUDED_)
#define AFX_BLBETWEENXY1_H__7E8A8BAB_9C50_4778_85BF_4AF48E9BE433__INCLUDED_

/* code */
//TODO:

#endif // !defined(AFX_BLBETWEENXY1_H__7E8A8BAB_9C50_4778_85BF_4AF48E9BE433__INCLUDED_)

inline int get_CPU_core_num()
{
#if defined(WIN32)
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
#elif defined(LINUX) || defined(SOLARIS) || defined(AIX) || defined(linux) || defined(__linux__) || defined(__linux)
	// GNU fuction
	return get_nprocs();   
#else
    #error  "not supported!"
#endif
}

class MBR
{
public:
	// 删除默认构造函数
	MBR() = delete;
	MBR(const double* b): boundary{*(b+min_longti), *(b+min_lanti),
								   *(b+max_longti), *(b+max_lanti)} {}
	~MBR();

protected:
	void set(double*);
	void update(double, double);
	double* get_boundary();
	bool is_inside(double, double);

protected:
	enum {min_longti, min_lanti, max_longti, max_lanti};
	double boundary[4];
};

MBR::~MBR() {}

void MBR::set(double *b)
{
	for(unsigned i = 0; i < 4; ++i)
		boundary[i] = *(b+i);
}

void MBR::update(double longti, double lanti)
{
	if(longti < boundary[min_longti])
		boundary[min_longti] = longti;
	if(longti > boundary[max_longti])
		boundary[max_longti] = longti;
	if(lanti  < boundary[min_lanti])
		boundary[min_lanti] = lanti;
	if(lanti > boundary[max_lanti])
		boundary[max_lanti] = lanti;
}

double* MBR::get_boundary()
{
	double* b = boundary;
	return b;
}

bool MBR::is_inside(double longti, double lanti)
{
	
	if(longti < boundary[max_longti] && 
	   longti > boundary[min_lanti] && 
	   lanti  < boundary[max_longti] && 
	   lanti  > boundary[min_lanti])
		return true;
	else
		return false;
}

class VesselPos
{

};

void help(int argc, const char** argv)
{
	cout << "========================================================================" << endl;
    cout << "Tools to extract tracks of tanker(AND ONLY tanker) to a new csv file"     << endl;
    cout << "Usage:" << endl;
    cout << "  " << argv << " source_AIS_file  [minx miny maxx maxy]" << endl;
    cout << "parameters:" << endl;
    cout << "       source_AIS_file - Source AIS file " << endl;
    cout << "       minx - minx of clip area " << endl;
    cout << "       miny - miny of clip area " << endl;
    cout << "       maxx - maxx of clip area " << endl;
    cout << "       maxy - maxy of clip area " << endl;
    cout << "If no minx,miny,maxx,maxy are specified, whole map will be processed" << endl;
    cout << "========================================================================" << endl;

}

int main(int argc, char const *argv[])
{
	if(argc < 2)
	{
		help(argc, argv);
		return -1;
	}

	const char* input_longtilanti_file = argv[1];
	char* temp = new char[strlen(input_longtilanti_file) + 1];
	delete temp;

	return 0;
}
