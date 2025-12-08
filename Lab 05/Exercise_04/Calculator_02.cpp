#include <Windows.h>
#include <iostream>

using namespace std;

struct NumberRange {
    unsigned int Start;
    unsigned int End;
    unsigned int ThreadNumber;
};

unsigned int gNumberTo = 0;
unsigned int gSum = 0;
unsigned int gNumberOfThreads = 1;

CRITICAL_SECTION g_CS;

DWORD WINAPI sum_numbers(void *arg)
{
    struct NumberRange *pRange;

    EnterCriticalSection(&g_CS);

    pRange = (struct NumberRange *)arg;
    cout << "Thread Number:" << pRange->ThreadNumber
         << ", Start:" << pRange->Start << ", End:" << pRange->End;
    for( unsigned int i = pRange->Start; i <= pRange->End; i++ )
    {
        gSum += i;
    }
    cout << ", Sum:" << gSum << endl;

    LeaveCriticalSection(&g_CS);

    return 0;
}

int main(void)
{
    unsigned int ThreadCount;
    HANDLE hThread[10];

    struct NumberRange Range[10];
    unsigned int Answer = 0;

    InitializeCriticalSection(&g_CS);

    cout << "Calculate the summation from 1 to ";
    cin >> gNumberTo;
    cout << "How many numbers of thread do you want to run? (Max:10) ";
    cin >> gNumberOfThreads;

    for( ThreadCount = 0; ThreadCount < gNumberOfThreads; ThreadCount++ )
    {
        Range[ThreadCount].Start = ((gNumberTo / gNumberOfThreads) * ThreadCount) + 1;
        Range[ThreadCount].End = (gNumberTo / gNumberOfThreads) * (ThreadCount + 1);
        Range[ThreadCount].ThreadNumber = ThreadCount + 1;
        hThread[ThreadCount] = CreateThread(NULL, 0, sum_numbers,
            (void *)&Range[ThreadCount], 0, NULL);
    }

    WaitForMultipleObjects(ThreadCount, hThread, TRUE, INFINITE);
    DeleteCriticalSection(&g_CS);

    cout << "[Calc] The sum from 1 to " << gNumberTo << " is " << gSum << endl;

    for( unsigned int i = 0; i <= gNumberTo; i++ )
    {
        Answer += i;
    }
    cout << "[Answer] The sum from 1 to " << gNumberTo << " is " << Answer << endl;

    return 0;
}