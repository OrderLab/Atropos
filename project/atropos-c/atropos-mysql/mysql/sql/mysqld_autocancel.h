#ifndef MYSQLD_AUTOCANCEL_H
#define MYSQLD_AUTOCANCEL_H
#include <map>
#include <deque>
extern "C"{
    #include <cancellable.h>
    #include <stat.h>
}
class THD;

typedef struct Benefit_components
{
    double I_io;
    double I_cpu;
    unsigned long I_app_lock;
    unsigned long I_lock;
    unsigned long I_network;
    unsigned long I_app_queue;
    unsigned int mdl_namespace;
    void *thd;

    bool operator<(const Benefit_components &b) const
	{
		return thd < b.thd;
	}
} Benefit_components;

typedef struct candidate
{
    long long appTid;
    cancellable *c;
    int benifit[10];
    bool operator<(const candidate &c) const
    {
        return appTid < c.appTid;
    }
} candidate;

typedef struct benefit
{
    long long appTid;
    double I_cpu;
    double I_table_lock;
    double i_mdl_lock;
    double I_app_mem;

    bool operator<(const benefit &b) const
	{
		return appTid < b.appTid;
	}
} benefit;

void autocancel_kill(THD * thd);

extern int auto_cancel_flag;
#endif