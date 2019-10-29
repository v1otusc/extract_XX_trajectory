#ifndef NOTIFY_H
#define NOTIFY_H

#include <iostream>
#include <string>

using namespace std;

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

#endif