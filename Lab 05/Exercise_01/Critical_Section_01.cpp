#include <Windows.h>
#include <iostream>

using namespace std;

static int  g_n = 0;
CRITICAL_SECTION g_CS;

UINT ThreadOne(LPVOID lParam)
{
    EnterCriticalSection(&g_CS);
    for (int i = 0; i < 10; i++) {
        cout << "Thread 1: " << ++g_n << "\n";
    }
    LeaveCriticalSection(&g_CS);
    return 0;
}

UINT ThreadTwo(LPVOID lParam)
{
    EnterCriticalSection(&g_CS);
    for (int i = 0; i < 10; i++) {
        cout << "Thread 2: " << ++g_n << "\n";
    }
    LeaveCriticalSection(&g_CS);
    return 0;
}

int main(void)
{
    HANDLE hThread[2];
    unsigned long IDThread[2];

    InitializeCriticalSection(&g_CS);

    hThread[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadOne,
        (LPVOID)NULL, 0, &IDThread[0]);
    hThread[1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadTwo,
        (LPVOID)NULL, 0, &IDThread[1]);
    WaitForMultipleObjects(2, hThread, TRUE, INFINITE);

    DeleteCriticalSection(&g_CS);

    return 0;
}