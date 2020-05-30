/////////////////////////////////////////////////////////////
//  Developed for Prince QP
//  Utility Program to create tables of PWM values mapped  //
//  to FSR sensor ADC output.  There are four independant  //
//  voltage outputs: one for each sensor.                  //
//  This is the first version of this program and needs    //
//  some adjustments to determine where to start the table //
//  index row and what happens when the adjacent value is  //
//  0, 1 or even less than the ADC count.                  //
/////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

#define ADC_CNTS          0x00
#define DUTY_CYCLE        0x01
#define PWM_DEL           0x02
#define ADC_DEL           0x03
#define TABLE_INDEX       0x04

#define CONFIGROWS        0x1a          // There are 25 rows of 32 bytes in the config area
#define CONFIGCOLS        0x20

unsigned int resultConfigBuffer[CONFIGROWS][CONFIGCOLS];
unsigned int resultCharArray[1024];

int64_t my_getline(char **restrict line, size_t *restrict len, FILE *restrict fp);
unsigned int convert_to_int(char *myStringPart, unsigned char convertTo);
unsigned int convert_to_dec(char *myStringPart, unsigned int power);
unsigned char find_the_comma(char *myStringPart, char charToFind);
void write_master_array(unsigned int row, unsigned int col, unsigned int data); 
void make_delta(unsigned int placeResult, unsigned int startRow, unsigned int useThisCol, unsigned int offset);
void sub_col_offset(unsigned int placeResult, unsigned int startRow, unsigned int useThisCol, unsigned int offset);
void add_to_array(unsigned int loopCnt, unsigned int startIndex, unsigned int anchorRow);
void init_result_array(void);

/////////////////////////////////
// Start Main() module         //
/////////////////////////////////
int main(int argc, char *argv[])
{
    char header[20];
    char adcOffset, pwmOffset, aSpace, endOfLine;
    char *line = NULL;
    size_t len = 0;
    uint8_t lineCnt;
    unsigned int adcCounts, pwmDutyCycle;
    unsigned int rowCnt = 0;
    unsigned int colCnt = 0;


    if(argc < 3)
    {
        printf("\n\t Usage....\n");
        printf("\t<Input File Name>, <Output File Name>\n");
    }

    // Open source file
    FILE *fp = fopen(argv[1], "r");
    if(fp == NULL) 
    {
        perror("Unable to open file!");
        exit(1);
    }

    // Open destination file
    FILE *fpwrt = fopen(argv[2], "w");
    if(fp == NULL) 
    {
        perror("Unable to open file!");
        exit(1);
    }
    printf("\n");
    fflush(stdin);


    // Read lines from a text file using our own a portable getline implementation
    while(my_getline(&line, &len, fp) != -1)
    {
        strncpy(&header[0], &line[0], 10);
        adcOffset = find_the_comma(&header[0], ',');
        if(header[adcOffset] != '0')
        {
            adcCounts = convert_to_int(&header[0], adcOffset);
            write_master_array(rowCnt, 0, adcCounts); 
        }
        aSpace = find_the_comma(&header[adcOffset], 0x20) + adcOffset;
        endOfLine = find_the_comma(&header[aSpace], '\0') + aSpace;
        pwmDutyCycle = convert_to_int(&header[aSpace + 1], endOfLine - aSpace - 2);
        write_master_array(rowCnt, 1, pwmDutyCycle);
        rowCnt++;
        colCnt++;
    }

    // Create delta ADC counts place col 3
    // use start row 1 of col 0
    make_delta(3, 1, 0, 0);

    // Create delta PWM counts place col 2
    // use start row 1 of col 1
    make_delta(2, 1, 1, 0);

    // Create table index at col 4
    // use start row 1 of col 0 and offset at row1 col0
    sub_col_offset(4, 1, 0, resultConfigBuffer[1][0]); // if anchor row starts at 2
//    sub_col_offset(4, 2, 0, resultConfigBuffer[2][0]);   // if anchor row starts at 3 

    // Create a table of 1024
    // bytes initiated to 255.
    init_result_array();

/*  This prints the data table used for generating 
 *  FSR pwm table entries.
    for(lineCnt = 0; lineCnt < rowCnt; lineCnt++)
    {
        printf("  %d\t\t  %d\t\t  %d\t   %d\t\t   %d\n", resultConfigBuffer[lineCnt ][0], 
                                                     resultConfigBuffer[lineCnt][1],
                                                resultConfigBuffer[lineCnt][3], resultConfigBuffer[lineCnt][2],
                                                resultConfigBuffer[lineCnt][4]);
    }
*/
//    for(rowCnt = 3; rowCnt < 25; rowCnt++)
    for(rowCnt = 2; rowCnt < 25; rowCnt++)  // Anchor row starts at 2 when ADC counts starts in row 1
    {
        // Create the FSR Table
        add_to_array(resultConfigBuffer[rowCnt][3], resultConfigBuffer[rowCnt-1][4],  rowCnt);
    }

    //Print result array in blocks 
    //of four elements.
    colCnt = 0;
    for(rowCnt = 0; rowCnt < 1024; rowCnt++)
    {
        printf("0x%02hhx", resultCharArray[rowCnt]); 
        printf(", ");
        colCnt++;
        if(colCnt == 4)
        {
            printf("\n");
            colCnt = 0;
        }
    }
    printf("\n");

/*    // Create the FSR Table
    add_to_array(resultConfigBuffer[4][3], resultConfigBuffer[3][4],  4);

    // Create the FSR Table
    add_to_array(resultConfigBuffer[5][3], resultConfigBuffer[4][4],  5);

    // Create the FSR Table
    add_to_array(resultConfigBuffer[6][3], resultConfigBuffer[5][4],  6);

    // Create the FSR Table
    add_to_array(resultConfigBuffer[7][3], resultConfigBuffer[6][4],  7);

    // Create the FSR Table
    add_to_array(resultConfigBuffer[8][3], resultConfigBuffer[7][4],  8);

    // Create the FSR Table
    add_to_array(resultConfigBuffer[9][3], resultConfigBuffer[7][4],  9);

    // Create the FSR Table
    add_to_array(resultConfigBuffer[10][3], resultConfigBuffer[8][4],  10);

    // Create the FSR Table
    add_to_array(resultConfigBuffer[11][3], resultConfigBuffer[9][4],  11);
    // Print out the array

    printf(" ADC Cnts\tDuty Cycle\tADC Del\t PWM Del\t Tab Idx\n");
    colCnt = 0;
    for(lineCnt = 0; lineCnt < rowCnt; lineCnt++)
    {
        printf("  %d\t\t  %d\t\t  %d\t   %d\t\t   %d\n", resultConfigBuffer[lineCnt ][0], 
                                                     resultConfigBuffer[lineCnt][1],
                                                resultConfigBuffer[lineCnt][3], resultConfigBuffer[lineCnt][2],
                                                resultConfigBuffer[lineCnt][4]);
    }

*/
    return 1;
}


///////////////////////////////////////////////////////////////////////////
// POSIX getline replacement for non-POSIX systems (like Windows)        //
// Differences:                                                          //
//     - the function returns int64_t instead of ssize_t                 //
//     - does not accept NUL characters in the input file                //
// Warnings:                                                             //
//     - the function sets EINVAL, ENOMEM, EOVERFLOW in case of errors.  //
//       The above are not defined by ISO C17,                           //
//       but are supported by other C compilers like MSVC                //
///////////////////////////////////////////////////////////////////////////
int64_t my_getline(char **restrict line, size_t *restrict len, FILE *restrict fp)
{
    // Check if either line, len or fp are NULL pointers
    if(line == NULL || len == NULL || fp == NULL) 
    {
        errno = EINVAL;
        return -1;
    }

    // Use a chunk array of 128 bytes as parameter for fgets
    char chunk[128];

    // Allocate a block of memory for *line if it is NULL or smaller than the chunk array
    if(*line == NULL || *len < sizeof(chunk))
    {
        *len = sizeof(chunk);
        if((*line = malloc(*len)) == NULL)
        {
            errno = ENOMEM;
            return -1;
        }
    }

    // "Empty" the string
    (*line)[0] = '\0';

    while(fgets(chunk, sizeof(chunk), fp) != NULL) 
    {
        // Resize the line buffer if necessary
        size_t len_used = strlen(*line);
        size_t chunk_used = strlen(chunk);

        if(*len - len_used < chunk_used)
        {
            // Check for overflow
            if(*len > SIZE_MAX / 2)
            {
                errno = ENOMEM;                            //OVERFLOW;
                return -1;
            }
            else
            {
                *len *= 2;
            }

            if((*line = realloc(*line, *len)) == NULL) 
            {
                errno = ENOMEM;
                return -1;
            }
        }

        // Copy the chunk to the end of the line buffer
        memcpy(*line + len_used, chunk, chunk_used);
        len_used += chunk_used;
        (*line)[len_used] = '\0';

        // Check if *line contains '\n', if yes, return the *line length
        if((*line)[len_used - 1] == '\n') 
        {
            return len_used;
        }
    }
    return -1;
}

unsigned int convert_to_int(char *myStringPart, unsigned char convertTo)
{
    unsigned int decNumber = 0;
    unsigned char numberDigits = 0;
    char *stringToDec = myStringPart;

    if(convertTo > 1)
    {
        decNumber = convert_to_dec(stringToDec, convertTo);
        stringToDec++;
    }
    return decNumber;
}

unsigned int convert_to_dec(char *myStringPart, unsigned int power)
{
    unsigned int decimalNumber = 0;
    unsigned int dummyTest;

    if(power == 4)
    {
        dummyTest = (unsigned int)*(myStringPart++);
        decimalNumber+= (1000* (dummyTest - 0x30));
        power--;
    }

    if(power == 3)
    {
        dummyTest = (unsigned int)*(myStringPart++);
        decimalNumber+= (100* (dummyTest - 0x30));
        power--;
    }

    if(power == 2)
    {
        dummyTest = (unsigned int)*(myStringPart++);
        decimalNumber+= (10* (dummyTest - 0x30));
        power--;
    }

    if(power == 1)
    {
        dummyTest = (unsigned int)*(myStringPart++);
        decimalNumber+= (1 * (dummyTest - 0x30));
        power;
    }

    return decimalNumber;
}


unsigned char find_the_comma(char *myStringPart, char charToFind)
{
    unsigned char hereIsTheChar = 0;

    while(*myStringPart != charToFind)
    {
        hereIsTheChar++;
        myStringPart++;
    }

    return hereIsTheChar;
}

void write_master_array(unsigned int row, unsigned int col, unsigned int data) 
{
    resultConfigBuffer[row][col] = data;
}

void make_delta(unsigned int placeResult, unsigned int startRow, unsigned int useThisCol, unsigned int offset)
{
    unsigned int icnt, delta;

    for(icnt = startRow; icnt < 25; icnt++)
    {
       delta = resultConfigBuffer[icnt][useThisCol] -  resultConfigBuffer[icnt-1][useThisCol] - offset;
       write_master_array(icnt, placeResult, delta);
    }
}

void sub_col_offset(unsigned int placeResult, unsigned int startRow, unsigned int useThisCol, unsigned int offset)
{
    unsigned int icnt, delta;

    for(icnt = startRow; icnt < 25; icnt++)
    {
       delta = resultConfigBuffer[icnt][useThisCol] - offset;
       write_master_array(icnt, placeResult, delta);
    }

}

void init_result_array(void)
{
    unsigned int icnt; 

    for(icnt = 0; icnt < 1024; icnt+= 4)
    {
       resultCharArray[icnt] = 255; 
       resultCharArray[icnt + 1] = 255; 
       resultCharArray[icnt + 2] = 255; 
       resultCharArray[icnt + 3] = 255; 
 /*      printf("%d", resultCharArray[icnt]); 
       printf(", ");
       printf("%d", resultCharArray[icnt + 1]); 
       printf(", ");
       printf("%d", resultCharArray[icnt + 2]); 
       printf(", ");
       printf("%d", resultCharArray[icnt + 3]); 
       printf(",\n"); */
    }
}

void add_to_array(unsigned int loopCnt, unsigned int startIndex, unsigned int anchorRow)
{
    unsigned int icnt, jcnt;
    float ratioResult, newResult;

    ratioResult = (float)resultConfigBuffer[anchorRow][2] / (float)resultConfigBuffer[anchorRow][3];
//    newResult = ratioResult + (float)resultConfigBuffer[anchorRow - 1][1];  // If anchor row start at 3
    newResult = ratioResult + (float)resultConfigBuffer[anchorRow][1];    // If anchor row starts at 2

/*        printf("Newresult = %1.5f\n", newResult);
        printf("ratioResult = %1.5f\n", ratioResult);
        printf("ratioResult = %1.5f\n", ratioResult);
        printf("startIndex = %d\n", startIndex);
        printf("loopCnt = %d\n", loopCnt);
        printf("anchorRow = %d\n", anchorRow);
*/
//    resultCharArray[startIndex] = resultConfigBuffer[anchorRow - 1][1];   // If anchor row starts at 3
    resultCharArray[startIndex] = resultConfigBuffer[anchorRow][1] * 0.95;     // If anchor row starts at 2
    resultCharArray[startIndex + 1] = newResult * 0.95;


    for(jcnt = startIndex + 1; jcnt < loopCnt + startIndex; jcnt++)
    {
        newResult += ratioResult *0.95; 
        resultCharArray[jcnt + 1] = newResult;
    }

}


