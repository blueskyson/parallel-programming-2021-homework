#include <iostream>
#include <string>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include "bmp.h"

using namespace std;

long NSmooth = 1000;

/*********************************************************/
/*變數宣告：                                             */
/*  bmpHeader    ： BMP檔的標頭                          */
/*  bmpInfo      ： BMP檔的資訊                          */
/*  **BMPSaveData： 儲存要被寫入的像素資料               */
/*  **BMPData    ： 暫時儲存要被寫入的像素資料           */
/*********************************************************/
BMPHEADER bmpHeader;
BMPINFO bmpInfo;
RGBTRIPLE **BMPSaveData = NULL;
RGBTRIPLE **BMPData = NULL;

/*********************************************************/
/*函數宣告：                                             */
/*  readBMP    ： 讀取圖檔，並把像素資料儲存在BMPSaveData*/
/*  saveBMP    ： 寫入圖檔，並把像素資料BMPSaveData寫入  */
/*  swap       ： 交換二個指標                           */
/*  **alloc_memory： 動態分配一個Y * X矩陣               */
/*********************************************************/
int readBMP(char *fileName); //read file
int saveBMP(char *fileName); //save file
void swap(RGBTRIPLE *a, RGBTRIPLE *b);
RGBTRIPLE **alloc_memory(int Y, int X); //allocate memory

/* pthread variables */
sem_t *semaphore;
bool *ready_child;
struct child_args {
    int my_rank;
    int start_row, end_row;
};

void *smooth(void *args)
{
    child_args *a = (child_args*)args;
    int start_row = a->start_row;
    int end_row = a->end_row;
    int my_rank = a->my_rank;
    for (int count = 0; count < NSmooth; count++) {
        sem_wait(&semaphore[my_rank]);
        
        //進行平滑運算
        for (int i = start_row; i < end_row; i++) {
            for (int j = 0; j < bmpInfo.biWidth; j++) {
                /*********************************************************/
                /*設定上下左右像素的位置                                 */
                /*********************************************************/
                int Top = i > 0 ? i - 1 : bmpInfo.biHeight - 1;
                int Down = i < bmpInfo.biHeight - 1 ? i + 1 : 0;
                int Left = j > 0 ? j - 1 : bmpInfo.biWidth - 1;
                int Right = j < bmpInfo.biWidth - 1 ? j + 1 : 0;
                /*********************************************************/
                /*與上下左右像素做平均，並四捨五入                       */
                /*********************************************************/
                BMPSaveData[i][j].rgbBlue = (double)(BMPData[i][j].rgbBlue + BMPData[Top][j].rgbBlue + BMPData[Top][Left].rgbBlue + BMPData[Top][Right].rgbBlue + BMPData[Down][j].rgbBlue + BMPData[Down][Left].rgbBlue + BMPData[Down][Right].rgbBlue + BMPData[i][Left].rgbBlue + BMPData[i][Right].rgbBlue) / 9 + 0.5;
                BMPSaveData[i][j].rgbGreen = (double)(BMPData[i][j].rgbGreen + BMPData[Top][j].rgbGreen + BMPData[Top][Left].rgbGreen + BMPData[Top][Right].rgbGreen + BMPData[Down][j].rgbGreen + BMPData[Down][Left].rgbGreen + BMPData[Down][Right].rgbGreen + BMPData[i][Left].rgbGreen + BMPData[i][Right].rgbGreen) / 9 + 0.5;
                BMPSaveData[i][j].rgbRed = (double)(BMPData[i][j].rgbRed + BMPData[Top][j].rgbRed + BMPData[Top][Left].rgbRed + BMPData[Top][Right].rgbRed + BMPData[Down][j].rgbRed + BMPData[Down][Left].rgbRed + BMPData[Down][Right].rgbRed + BMPData[i][Left].rgbRed + BMPData[i][Right].rgbRed) / 9 + 0.5;
            }
        }
        
        ready_child[my_rank] = true;
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    timespec begin, end;
    clock_gettime(CLOCK_REALTIME, &begin);

    /*********************************************************/
    /*變數宣告：                                             */
    /*  *infileName  ： 讀取檔名                             */
    /*  *outfileName ： 寫入檔名                             */
    /*  startwtime   ： 記錄開始時間                         */
    /*  endwtime     ： 記錄結束時間                         */
    /*********************************************************/
    char *infileName = "input.bmp";
    char *outfileName = "output.bmp";
    double startwtime = 0.0, endwtime = 0;

    //讀取檔案
    if (readBMP(infileName))
        cout << "Read file successfully!!" << endl;
    else
        cout << "Read file fails!!" << endl;

    //動態分配記憶體給暫存空間
    BMPData = alloc_memory(bmpInfo.biHeight, bmpInfo.biWidth);

    // Initialize pthread variables
    long n_thread = 4;
    if (argc >= 2) {
        n_thread = strtol(argv[1], NULL, 10);
        if (n_thread < 1) {
            printf("n_thread must > 0! abort.\n");
            return 0;
        }
    }

    if (argc >= 3) {
        NSmooth = strtol(argv[2], NULL, 10);
        if (NSmooth < 1) {
            printf("NSmooth must > 0! abort.\n");
            return 0;
        }
    }

    pthread_t child[n_thread];
    child_args args[n_thread];
    semaphore = new sem_t[n_thread];
    ready_child = new bool[n_thread];
    int step = bmpInfo.biHeight / n_thread;
    int column = bmpInfo.biWidth;
    int rowsum = 0;

    for (int i = 0; i < n_thread; i++) {
        args[i].my_rank = i;
        args[i].start_row = rowsum;
        rowsum += step;
        args[i].end_row = rowsum;
        sem_init(&semaphore[i], 0, 0);
    }

    if (rowsum < bmpInfo.biHeight) {
        args[n_thread - 1].end_row = bmpInfo.biHeight;
    }

    for (int i = 0; i < n_thread; i++) {
        pthread_create(&child[i], NULL, smooth, (void*) &args[i]);
    }

    //進行多次的平滑運算
    for (int count = 0; count < NSmooth; count++)
    {
        //把像素資料與暫存指標做交換
        swap(BMPSaveData, BMPData);
        for (int i = 0; i < n_thread; i++) {
            ready_child[i] = false;
            sem_post(&semaphore[i]);
        }

        // wait for all children finish
        bool busy = true;
        while (busy) {
            busy = false;
            for (int i = 0; i < n_thread; i++) {
                if (ready_child[i] == false) {
                    busy = true;
                    break;
                }
            }
        }
    }

    //寫入檔案
    if (saveBMP(outfileName))
        cout << "Save file successfully!!" << endl;
    else
        cout << "Save file fails!!" << endl;

    free(BMPData[0]);
    free(BMPSaveData[0]);
    free(BMPData);
    free(BMPSaveData);

    clock_gettime(CLOCK_REALTIME, &end);
    long seconds = end.tv_sec - begin.tv_sec;
    long nanoseconds = end.tv_nsec - begin.tv_nsec;
    double elapsed = seconds + nanoseconds*1e-9;
    printf("time in sec: %.2f\n", elapsed);
    return 0;
}

/*********************************************************/
/* 讀取圖檔                                              */
/*********************************************************/
int readBMP(char *fileName)
{
    //建立輸入檔案物件
    ifstream bmpFile(fileName, ios::in | ios::binary);

    //檔案無法開啟
    if (!bmpFile)
    {
        cout << "It can't open file!!" << endl;
        return 0;
    }

    //讀取BMP圖檔的標頭資料
    bmpFile.read((char *)&bmpHeader, sizeof(BMPHEADER));

    //判決是否為BMP圖檔
    if (bmpHeader.bfType != 0x4d42)
    {
        cout << "This file is not .BMP!!" << endl;
        return 0;
    }

    //讀取BMP的資訊
    bmpFile.read((char *)&bmpInfo, sizeof(BMPINFO));

    //判斷位元深度是否為24 bits
    if (bmpInfo.biBitCount != 24)
    {
        cout << "The file is not 24 bits!!" << endl;
        return 0;
    }

    //修正圖片的寬度為4的倍數
    while (bmpInfo.biWidth % 4 != 0)
        bmpInfo.biWidth++;

    //動態分配記憶體
    BMPSaveData = alloc_memory(bmpInfo.biHeight, bmpInfo.biWidth);

    //讀取像素資料
    //for(int i = 0; i < bmpInfo.biHeight; i++)
    //	bmpFile.read( (char* )BMPSaveData[i], bmpInfo.biWidth*sizeof(RGBTRIPLE));
    bmpFile.read((char *)BMPSaveData[0], bmpInfo.biWidth * sizeof(RGBTRIPLE) * bmpInfo.biHeight);

    //關閉檔案
    bmpFile.close();

    return 1;
}

/*********************************************************/
/* 儲存圖檔                                              */
/*********************************************************/
int saveBMP(char *fileName)
{
    //判決是否為BMP圖檔
    if (bmpHeader.bfType != 0x4d42)
    {
        cout << "This file is not .BMP!!" << endl;
        return 0;
    }

    //建立輸出檔案物件
    ofstream newFile(fileName, ios::out | ios::binary);

    //檔案無法建立
    if (!newFile)
    {
        cout << "The File can't create!!" << endl;
        return 0;
    }

    //寫入BMP圖檔的標頭資料
    newFile.write((char *)&bmpHeader, sizeof(BMPHEADER));

    //寫入BMP的資訊
    newFile.write((char *)&bmpInfo, sizeof(BMPINFO));

    //寫入像素資料
    //for( int i = 0; i < bmpInfo.biHeight; i++ )
    //        newFile.write( ( char* )BMPSaveData[i], bmpInfo.biWidth*sizeof(RGBTRIPLE) );
    newFile.write((char *)BMPSaveData[0], bmpInfo.biWidth * sizeof(RGBTRIPLE) * bmpInfo.biHeight);

    //寫入檔案
    newFile.close();

    return 1;
}

/*********************************************************/
/* 分配記憶體：回傳為Y*X的矩陣                           */
/*********************************************************/
RGBTRIPLE **alloc_memory(int Y, int X)
{
    //建立長度為Y的指標陣列
    RGBTRIPLE **temp = new RGBTRIPLE *[Y];
    RGBTRIPLE *temp2 = new RGBTRIPLE[Y * X];
    memset(temp, 0, sizeof(RGBTRIPLE) * Y);
    memset(temp2, 0, sizeof(RGBTRIPLE) * Y * X);

    //對每個指標陣列裡的指標宣告一個長度為X的陣列
    for (int i = 0; i < Y; i++)
    {
        temp[i] = &temp2[i * X];
    }

    return temp;
}

/*********************************************************/
/* 交換二個指標                                          */
/*********************************************************/
void swap(RGBTRIPLE *a, RGBTRIPLE *b)
{
    RGBTRIPLE *temp;
    temp = a;
    a = b;
    b = temp;
}