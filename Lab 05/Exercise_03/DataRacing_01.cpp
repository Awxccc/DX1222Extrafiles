#include <Windows.h>
#include <iostream>

using namespace std;

#define EXPECTING_NUMBER 100000000 // 100M
#define THREAD_COUNT 2
#define LOOP_COUNT (EXPECTING_NUMBER / (THREAD_COUNT*2))
int sum = 0;

DWORD WINAPI ThreadFunction(void *arg)
{
    int i;
    for( i = 0; i < LOOP_COUNT; i++ ) {
        sum += 2;
    }
    return 0;
}

int main()
{
    HANDLE hThread[THREAD_COUNT];
    DWORD dwThreadID[THREAD_COUNT];
    int i;

    cout << "Calculating expectation result is " << EXPECTING_NUMBER
        << endl;
    for( i = 0; i < THREAD_COUNT; i++ ) {
        hThread[i] = CreateThread(NULL,
            0,
            ThreadFunction,
            NULL,
            0,
            &dwThreadID[i]);
        if( hThread == 0 ){
            cout << "CreateThread() error" << endl;
            exit(1);
        }
    }
    WaitForMultipleObjects(THREAD_COUNT, hThread, TRUE, INFINITE);
    cout << "Calculation result is with thread is " << sum << endl;
}