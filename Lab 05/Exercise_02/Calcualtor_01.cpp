#include <Windows.h>
#include <iostream>

using namespace std;

unsigned int gNumberTo = 0;
unsigned int gSum = 0;

CRITICAL_SECTION g_CS;

DWORD WINAPI sum_numbers(void *lpVoid)
{
    unsigned int Number = 1;

    EnterCriticalSection(&g_CS);
    while (Number <= gNumberTo)
    {
        gSum = gSum + Number;
        Number = Number + 1;
    }

    LeaveCriticalSection(&g_CS);

    return 0;
}


DWORD WINAPI print_sum_numbers(void *lpVoid)
{
    EnterCriticalSection(&g_CS);
    cout << "The sum from 1 to " << gNumberTo << " is " << gSum << endl;
    LeaveCriticalSection(&g_CS);

    return 0;
}

int main(void)
{
    HANDLE hThread[2];

    unsigned int Answer = 0;

    InitializeCriticalSection(&g_CS);

    cout << "Calculate the summation from 1 to ";
    cin >> gNumberTo;

    hThread[0] = CreateThread(NULL, 0, sum_numbers, NULL, 0, NULL);
    hThread[1] = CreateThread(NULL, 0, print_sum_numbers, NULL, 0, NULL);

    WaitForMultipleObjects(2, hThread, TRUE, INFINITE);

    for (unsigned int i = 0; i <= gNumberTo; i++)
    {
        Answer += i;
    }
    cout << "[Answer] The sum from 1 to " << gNumberTo << " is " << Answer << endl;

    DeleteCriticalSection(&g_CS);

    return 0;
}