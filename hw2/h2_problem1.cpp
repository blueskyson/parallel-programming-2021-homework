#include <mpi.h>
#include <iostream>
#include <string>
#include <fstream>
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include "bmp.h"

using namespace std;

//定義平滑運算的次數
#define NSmooth 1000

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

RGBTRIPLE **partition = NULL;
RGBTRIPLE **partswap = NULL;
RGBTRIPLE **upper_border = NULL;
RGBTRIPLE **lower_border = NULL;

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

int main(int argc, char *argv[])
{
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

    int my_rank, comm_sz;
    MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    //設定 RGBTRIPLE
    MPI_Datatype MPI_RGBTRIPLE;
    MPI_Type_contiguous(3, MPI_UNSIGNED_CHAR, &MPI_RGBTRIPLE);
    MPI_Type_commit(&MPI_RGBTRIPLE);

    //讀取檔案
    if (readBMP(infileName))
        cout << "Read file successfully!!" << endl;
    else
        cout << "Read file fails!!" << endl;

    MPI_Barrier(MPI_COMM_WORLD);
    startwtime = MPI_Wtime();
    
    //計算分割大小
    int *localrows, *localsizes, *offsets;
    int row, column;
    if (my_rank == 0) {
        column = bmpInfo.biWidth;
        int step = bmpInfo.biHeight / comm_sz;
        localrows = (int*)malloc(sizeof(int) * comm_sz);
        localsizes = (int*)malloc(sizeof(int) * comm_sz);
        offsets = (int*)malloc(sizeof(int) * comm_sz);
        int i, rowsum;
        for (i = 0, rowsum = 0; i < comm_sz; i++, rowsum += step) {
            localrows[i] = step;
            localsizes[i] = localrows[i] * column;
            offsets[i] = rowsum * column;
        }
        if (rowsum < bmpInfo.biHeight) {
            localrows[comm_sz - 1] += (bmpInfo.biHeight - rowsum);
            localsizes[comm_sz - 1] = localrows[comm_sz - 1] * column;
        }
    }
    MPI_Scatter(localrows, 1, MPI_INT, &row, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&column, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    //分割圖檔
    int rgb_size = row * column;
    partition = alloc_memory(row, column);
    partswap = alloc_memory(row, column);
    upper_border = alloc_memory(1, column);
    lower_border = alloc_memory(1, column);
    MPI_Scatterv(*BMPSaveData, localsizes, offsets, MPI_RGBTRIPLE, 
                 *partition, rgb_size, MPI_RGBTRIPLE, 0, MPI_COMM_WORLD);
    
    // 計算前後 process 編號
    int upper_rank = my_rank - 1;
    int lower_rank = my_rank + 1;
    if (my_rank == 0) {
        upper_rank = comm_sz - 1;
    } else if (my_rank == comm_sz - 1) {
        lower_rank = 0;
    }

    //進行多次的平滑運算
    for (int count = 0; count < NSmooth; count++) {
        //傳送邊界
        MPI_Send(partition[0], column, MPI_RGBTRIPLE, upper_rank, 0, MPI_COMM_WORLD);
        MPI_Send(partition[row - 1], column, MPI_RGBTRIPLE, lower_rank, 0, MPI_COMM_WORLD);
        MPI_Recv(upper_border[0], column, MPI_RGBTRIPLE, upper_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(lower_border[0], column, MPI_RGBTRIPLE, lower_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        swap(partition, partswap);

        //上邊界平滑運算
        for (int i = 0; i < column; i++) {
            int Left = (i > 0) ? (i - 1) : (column - 1);
            int Right = (i < column - 1) ? (i + 1) : 0;
            partition[0][i].rgbBlue = (double)(
                upper_border[0][Left].rgbBlue + upper_border[0][i].rgbBlue + upper_border[0][Right].rgbBlue +
                partswap[0][Left].rgbBlue + partswap[0][i].rgbBlue + partswap[0][Right].rgbBlue + 
                partswap[1][Left].rgbBlue + partswap[1][i].rgbBlue + partswap[1][Right].rgbBlue) / 9 + 0.5;
            partition[0][i].rgbGreen = (double)(
                upper_border[0][Left].rgbGreen + upper_border[0][i].rgbGreen + upper_border[0][Right].rgbGreen +
                partswap[0][Left].rgbGreen + partswap[0][i].rgbGreen + partswap[0][Right].rgbGreen + 
                partswap[1][Left].rgbGreen + partswap[1][i].rgbGreen + partswap[1][Right].rgbGreen) / 9 + 0.5;
            partition[0][i].rgbRed = (double)(
                upper_border[0][Left].rgbRed + upper_border[0][i].rgbRed + upper_border[0][Right].rgbRed +
                partswap[0][Left].rgbRed + partswap[0][i].rgbRed + partswap[0][Right].rgbRed + 
                partswap[1][Left].rgbRed + partswap[1][i].rgbRed + partswap[1][Right].rgbRed) / 9 + 0.5;
        }

        //平滑運算
        for (int i = 1; i < row - 1; i++) {
            for (int j = 0; j < column; j++) {
                int Left = (j > 0) ? (j - 1) : (column - 1);
                int Right = (j < column - 1) ? (j + 1) : 0;
                partition[i][j].rgbBlue = (double)(
                    partswap[i - 1][Left].rgbBlue + partswap[i - 1][j].rgbBlue + partswap[i - 1][Right].rgbBlue +
                    partswap[i][Left].rgbBlue + partswap[i][j].rgbBlue + partswap[i][Right].rgbBlue + 
                    partswap[i + 1][Left].rgbBlue + partswap[i + 1][j].rgbBlue + partswap[i + 1][Right].rgbBlue) / 9 + 0.5;
                partition[i][j].rgbGreen = (double)(
                    partswap[i - 1][Left].rgbGreen + partswap[i - 1][j].rgbGreen + partswap[i - 1][Right].rgbGreen +
                    partswap[i][Left].rgbGreen + partswap[i][j].rgbGreen + partswap[i][Right].rgbGreen + 
                    partswap[i + 1][Left].rgbGreen + partswap[i + 1][j].rgbGreen + partswap[i + 1][Right].rgbGreen) / 9 + 0.5;
                partition[i][j].rgbRed = (double)(
                    partswap[i - 1][Left].rgbRed + partswap[i - 1][j].rgbRed + partswap[i - 1][Right].rgbRed +
                    partswap[i][Left].rgbRed + partswap[i][j].rgbRed + partswap[i][Right].rgbRed + 
                    partswap[i + 1][Left].rgbRed + partswap[i + 1][j].rgbRed + partswap[i + 1][Right].rgbRed) / 9 + 0.5;
            }
        }

        //下邊界平滑運算
        for (int i = 0; i < column; i++) {
            int Left = (i > 0) ? (i - 1) : (column - 1);
            int Right = (i < column - 1) ? (i + 1) : 0;
            partition[row - 1][i].rgbBlue = (double)(
                partswap[row - 2][Left].rgbBlue + partswap[row - 2][i].rgbBlue + partswap[row - 2][Right].rgbBlue + 
                partswap[row - 1][Left].rgbBlue + partswap[row - 1][i].rgbBlue + partswap[row - 1][Right].rgbBlue +
                lower_border[0][Left].rgbBlue + lower_border[0][i].rgbBlue + lower_border[0][Right].rgbBlue) / 9 + 0.5;
            partition[row - 1][i].rgbGreen = (double)(
                partswap[row - 2][Left].rgbGreen + partswap[row - 2][i].rgbGreen + partswap[row - 2][Right].rgbGreen + 
                partswap[row - 1][Left].rgbGreen + partswap[row - 1][i].rgbGreen + partswap[row - 1][Right].rgbGreen +
                lower_border[0][Left].rgbGreen + lower_border[0][i].rgbGreen + lower_border[0][Right].rgbGreen) / 9 + 0.5;
            partition[row - 1][i].rgbRed = (double)(
                partswap[row - 2][Left].rgbRed + partswap[row - 2][i].rgbRed + partswap[row - 2][Right].rgbRed + 
                partswap[row - 1][Left].rgbRed + partswap[row - 1][i].rgbRed + partswap[row - 1][Right].rgbRed +
                lower_border[0][Left].rgbRed + lower_border[0][i].rgbRed + lower_border[0][Right].rgbRed) / 9 + 0.5;
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    MPI_Gatherv(*partition, rgb_size, MPI_RGBTRIPLE, 
                *BMPSaveData, localsizes, offsets, MPI_RGBTRIPLE, 0, MPI_COMM_WORLD);
    
    MPI_Barrier(MPI_COMM_WORLD);
    endwtime = MPI_Wtime();
    if (my_rank == 0) {
        cout << "The execution time = " << endwtime - startwtime << endl;

        //寫入檔案
        if (saveBMP(outfileName))
            cout << "Save file successfully!!" << endl;
        else
            cout << "Save file fails!!" << endl;
    }
    MPI_Finalize();

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
    RGBTRIPLE **temp = new RGBTRIPLE*[Y];
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

