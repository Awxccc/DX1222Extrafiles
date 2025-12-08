#include <Windows.h>
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <stdio.h>

using namespace std;

#define MAX_ROW_COUNT          25
#define MAX_CHACTER_NUMBER     50000
#define PRINT_SORTING_PROGRESS 20

char gRandomCharacters[MAX_ROW_COUNT][MAX_CHACTER_NUMBER] = { { '\0', }, };

HANDLE hStdout;

int write_to_screen_xy( int Pos_x, int Pos_y, char *OutTextBuffer )
{
    COORD PosXY;

    PosXY.X = Pos_x;
    PosXY.Y = Pos_y;
    SetConsoleCursorPosition( hStdout, PosXY );
    cout << OutTextBuffer;
    fflush( stdout );

    return 0;
}

int generate_random_characters( int Row )
{
    int Ch;
    char TextBuffer[BUFSIZ];

    sprintf_s( TextBuffer, "Row Number %02d: ", (Row + 1) );
    write_to_screen_xy( 1, Row, TextBuffer );
    for( Ch = 0; Ch < MAX_CHACTER_NUMBER; Ch++ )
    {
        gRandomCharacters[Row][Ch] = (char)('A' + (rand( ) % 26));
    }
    write_to_screen_xy( (int)(strlen( TextBuffer ) + 1), Row, "Generated!" );

    return 0;
}

int sort_characters( int Row )
{
    int First;
    int Second;
    char TempCh;

    int ProgressCount = 0;
    int ProgressPrint = 0;

    write_to_screen_xy( 27, Row, " - Now sorting" );
    for( First = 0; First < (MAX_CHACTER_NUMBER - 1); First++ )
    {
        for( Second = (First + 1); Second < MAX_CHACTER_NUMBER; Second++ )
        {
            if( gRandomCharacters[Row][First] > gRandomCharacters[Row][Second] )
            {
                TempCh = gRandomCharacters[Row][First];
                gRandomCharacters[Row][First] = gRandomCharacters[Row][Second];
                gRandomCharacters[Row][Second] = TempCh;
            }
        }

        if( ProgressCount > (MAX_CHACTER_NUMBER / PRINT_SORTING_PROGRESS) )
        {
            write_to_screen_xy( (41 + ProgressPrint), Row, "." );
            ProgressPrint++;
            ProgressCount = 0;
        }
        else
        {
            ProgressCount++;
        }
    }

    write_to_screen_xy( (41 + PRINT_SORTING_PROGRESS), Row, "Sorting Finished!" );

    return 0;
}

int check_errors( void )
{
    int Error = 0;
    int Row;
    int Ch;

    for( Row = 0; Row < MAX_ROW_COUNT; Row++ )
    {
        for( Ch = 0; Ch < (MAX_CHACTER_NUMBER - 1); Ch++ )
        {
            if( ('\0' == gRandomCharacters[Row][Ch]) ||
                (gRandomCharacters[Row][Ch] > gRandomCharacters[Row][Ch]) )
            {
                Error++;
            }
        }
    }

    return Error;
}

int main( void )
{
    int RowCount;
    char TextBuffer[BUFSIZ];

    clock_t BeforeClock, AfterClock;
    double  ElapsedSec;

    srand( (unsigned int)time( 0 ) );

    // Get handles to STDOUT. 
    hStdout = GetStdHandle( STD_OUTPUT_HANDLE );
    if( hStdout == INVALID_HANDLE_VALUE )
    {
        cout << "Fail to get standard output handle!" << endl;
        return 1;
    }

    for( RowCount = 0; RowCount < MAX_ROW_COUNT; RowCount++ )
    {
        generate_random_characters( RowCount );
    }

    BeforeClock = clock( );
    for( RowCount = 0; RowCount < MAX_ROW_COUNT; RowCount++ )
    {
        sort_characters( RowCount );
    }
    AfterClock = clock( );

    write_to_screen_xy( 1, (MAX_ROW_COUNT + 1), "Varifing the results: " );
    sprintf_s( TextBuffer, "%d number(s) of error found.", check_errors( ) );
    write_to_screen_xy( 23, (MAX_ROW_COUNT + 1), TextBuffer );

    ElapsedSec = (double)(AfterClock - BeforeClock) / CLOCKS_PER_SEC;
    sprintf_s( TextBuffer, "Elapsed Time: %f seconds\n", ElapsedSec );
    write_to_screen_xy( 1, (MAX_ROW_COUNT + 2), TextBuffer );

    getchar( );
    return 0;
}