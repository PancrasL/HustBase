#include "stdafx.h"
#include "DBTest.h"
#include "EditArea.h"
#include "QU_Manager.h"
#include "SYS_Manager.h"
#include <direct.h>
#include <iostream>
#include <conio.h>
int TableTest(char tabname[][20],int *tabnum,char colname[][20][20],int colnum[],AttrType coltype[][20],int collength[][20],int coloffset[][20],int iscolindex[][20],char indexname[][20][20],char *temppath)
{

	RM_FileHandle fileHandle,colfilehandle;
	fileHandle.bOpen=0;
	colfilehandle.bOpen=0;
	RM_FileScan FileScan1,FileScan2;
	FileScan1.bOpen=0;
	FileScan2.bOpen=0;
	RM_Record rec1,rec2;
	Con condition;
	RC rc;
	CString t,table,column;//test

	int i=0,j=0;
	DWORD cchCurDir=BUFFER; 
	LPTSTR lpszCurDir; 	
	TCHAR tchBuffer[BUFFER]; 
	lpszCurDir = tchBuffer; 
	GetCurrentDirectory(cchCurDir, lpszCurDir); 
	if(!strcmp(temppath,"")){
		CString	Path=lpszCurDir;
		table=Path+"\\SYSTABLES";
		column=Path+"\\SYSCOLUMNS";
	}
	else{
		CString Path=temppath;
		table=Path+"\\SYSTABLES";
		column=Path+"\\SYSCOLUMNS";
	}
	rc=RM_OpenFile((LPSTR)(LPCTSTR)table,&fileHandle);//去SYSTABLES表中获取表名
	if(rc!=SUCCESS){
		//AfxMessageBox("打开系统表文件失败");
		return -1;
	}
	rc=RM_OpenFile((LPSTR)(LPCTSTR)column,&colfilehandle);//去SYSCOLUMNS表中获取列名
	if(rc!=SUCCESS){
		//AfxMessageBox("打开系统列文件失败");
		return -1;
	}
	rc=OpenScan(&FileScan1,&fileHandle,0,NULL);
	if(rc!=SUCCESS){
		//AfxMessageBox("初始化表文件扫描失败");
		return -1;
	}
	while(GetNextRec(&FileScan1,&rec1)==SUCCESS)
	{
		strcpy(tabname[i],rec1.pData);
		condition.bLhsIsAttr=1;
		condition.bRhsIsAttr=0;
		condition.LattrLength=strlen(tabname[i])+1;
		condition.LattrOffset=0;
		condition.attrType=chars;
		condition.compOp=EQual;
		condition.Rvalue=tabname[i];
		rc=OpenScan(&FileScan2,&colfilehandle,1,&condition);
		if(rc!=SUCCESS){
			AfxMessageBox("初始化列文件扫描失败");
			return -1;
		}
		while(GetNextRec(&FileScan2,&rec2)==SUCCESS)
		{
			strcpy(colname[i][j],rec2.pData+21);
			memcpy(&coltype[i][j],rec2.pData+42,sizeof(AttrType));
			memcpy(&collength[i][j],rec2.pData+46,sizeof(int));
			memcpy(&coloffset[i][j],rec2.pData+50,sizeof(int));
			if(*(rec2.pData+54)=='1'){
				iscolindex[i][j]=1;
				strcpy(indexname[i][j],rec2.pData+55);
			}
			else{
				iscolindex[i][j]=0;
				strcpy(indexname[i][j],"\0");
			}

			j++;
		}
		colnum[i]=j;
		j=0;
		i++;
		FileScan2.bOpen=0;
	}
	*tabnum=i;
	rc=RM_CloseFile(&fileHandle);
	if(rc!=SUCCESS){
		//AfxMessageBox("关闭系统表文件失败");
		return -1;
	}
	rc=RM_CloseFile(&colfilehandle);
	if(rc!=SUCCESS){
		//AfxMessageBox("关闭系统列文件失败");
		return -1;
	}
	return 1;
}
int TableContent(int *colNum, int *rowNum, AttrType colType[20], char unit[][20][100],char *dbname,char *tablename)
{
	RM_FileHandle fileHandle;
	fileHandle.bOpen=0;
	RC rc;
	RM_Record rec;
	RM_FileScan FileScan;
	FileScan.bOpen=0;
	Con condition;
	*rowNum=0;
	char tabname[20][20];
	char colname[20][20][20];
	AttrType coltype[20][20];
	int tabnum,colnum[20];
	int collength[20][20];
	int coloffset[20][20];
	int iscolix[20][20];
	char indexname[20][20][20];
	int i,j,k=0;
	CString path;
	CString t;

	DWORD cchCurDir=BUFFER; 
	LPTSTR lpszCurDir; 	
	TCHAR tchBuffer[BUFFER]; 
	// 	lpszCurDir = tchBuffer; 
	// 	GetCurrentDirectory(cchCurDir, lpszCurDir); 
	// 	path=lpszCurDir;
	CString cspath=dbname;
	path=cspath+"\\SYSTABLES";
	char cpath[100];
	strcpy(cpath,cspath);
	rc=RM_OpenFile((LPSTR)(LPCTSTR)path,&fileHandle);//去SYSTABLES表中获取表名
	if(rc!=SUCCESS)
	{
		//	AfxMessageBox("打开系统表文件失败");
		return -1;
	}
	condition.bLhsIsAttr=1;
	condition.bRhsIsAttr=0;
	condition.LattrOffset=0;
	condition.LattrLength=20;
	condition.attrType=chars;
	condition.compOp=EQual;
	condition.Rvalue=(LPSTR)(LPCTSTR)tablename;

	rc=OpenScan(&FileScan,&fileHandle,1,&condition);
	if(rc!=SUCCESS){
		//AfxMessageBox("初始化文件扫描失败");
		return -1;
	}
	rc=GetNextRec(&FileScan,&rec);
	if(rc!=SUCCESS)//点击的不是表名，不做显示处理
	{
		rc=RM_CloseFile(&fileHandle);
		if(rc!=SUCCESS)
			//	AfxMessageBox("系统表文件关闭失败");
			return -1;
	}
	rc=RM_CloseFile(&fileHandle);
	if(rc!=SUCCESS){
		//	AfxMessageBox("表系统文件关闭失败");
		return -1;
	}


	int rc1=TableTest(tabname,&tabnum,colname,colnum,coltype,collength,coloffset,iscolix,indexname,cpath);

	for(i=0;i<tabnum;i++)
	{
		if(strcmp((LPSTR)(LPCTSTR)tablename,tabname[i])==0)
			break;
	}
	fileHandle.bOpen=0;
	char temp1[100];
	strcpy(temp1,dbname);
	strcat(temp1,"\\");
	strcat(temp1,tablename);
	rc=RM_OpenFile((LPSTR)(LPCTSTR)temp1,&fileHandle);
	if(rc!=SUCCESS){
		//	AfxMessageBox("数据表文件打开失败");
		return -1;
	}
	FileScan.bOpen=0;
	rc=OpenScan(&FileScan,&fileHandle,0,NULL);
	if(rc!=SUCCESS){
		//AfxMessageBox("初始化文件扫描失败");
		return -1;
	}
	while(GetNextRec(&FileScan,&rec)==SUCCESS)
	{

		*colNum=colnum[i];
		for(j=0;j<colnum[i];j++)
		{
			//memcpy(&colType[j],&(coltype[i][j]),sizeof(AttrType));
			colType[j]=coltype[i][j];
			if(coltype[i][j]==chars)
			{
				char temp[21];
				memcpy(temp,rec.pData+coloffset[i][j],collength[i][j]);
				t=temp;				
				strcpy((unit)[*rowNum][j],t);
			}
			else if(coltype[i][j]==ints)
			{
				int temp;
				memcpy(&temp,rec.pData+coloffset[i][j],sizeof(int));
				t.Format("%d",temp);
				strcpy((unit)[*rowNum][j],t);
			}
			else if(coltype[i][j]==floats)
			{
				float temp;
				memcpy(&temp,rec.pData+coloffset[i][j],sizeof(float));
				t.Format("%f",temp);
				strcpy((unit)[*rowNum][j],t);
			}
		}
		(*rowNum)++;
	}
	rc=RM_CloseFile(&fileHandle);
	if(rc!=SUCCESS){
		//AfxMessageBox("关闭数据表文件失败");
		return -1;
	}
	return 1;
}