#include <windows.h>
#include <iostream>

using namespace std;

CRITICAL_SECTION g_CS_Phonecall;

DWORD WINAPI thread_func_boy( void *arg )
{
    EnterCriticalSection( &g_CS_Phonecall );
    
    cout << "Boy: Please call me!" << endl;

    return 0;
}

DWORD WINAPI thread_func_girl( void *arg )
{
    EnterCriticalSection( &g_CS_Phonecall );

    cout << "Girl: I am waiting your call" << endl;

    return 0;
}

int main(void)
{
    HANDLE hThread[2];

    InitializeCriticalSection( &g_CS_Phonecall );

    cout << "Boy and Girl have argued! Buy they still love each other..." << endl;
    cout << "They are waiting a phone call... but..." << endl;

    EnterCriticalSection( &g_CS_Phonecall );
    hThread[0] = CreateThread( NULL, 0, thread_func_boy, NULL, 0, NULL );
    hThread[1] = CreateThread( NULL, 0, thread_func_girl, NULL, 0, NULL );

    while( WAIT_TIMEOUT == WaitForMultipleObjects( 2, hThread, TRUE, 1000 ) )
    {
        cout << "Still waiting!!!" << endl;
    }

    DeleteCriticalSection( &g_CS_Phonecall );

    return 0;
}