#include "stdafx.h"
#include "RM_Manager.h"
#include "str.h"


RC OpenScan(RM_FileScan *rmFileScan,RM_FileHandle *fileHandle,int conNum,Con *conditions)//³õÊ¼»¯É¨Ãè
{
	if (rmFileScan->bOpen) {
		return RM_FSOPEN;
	}
	rmFileScan->pRMFileHandle = fileHandle;
	rmFileScan->conNum = conNum;
	if (conNum == 0)
		rmFileScan->conditions = NULL;
	else
	{
		rmFileScan->conditions = (Con *)malloc(conNum * sizeof(Con));
		memcpy(rmFileScan->conditions, conditions, conNum * sizeof(Con));
	}
	rmFileScan->pinnedPageCount = 0;
	rmFileScan->phIx = 0;
	rmFileScan->snIx = 0;
	rmFileScan->pnLast = 1;
	rmFileScan->bOpen = true;
	return SUCCESS;
}

RC GetNextRec(RM_FileScan *rmFileScan,RM_Record *rec)
{
	return SUCCESS;
}

RC GetRec (RM_FileHandle *fileHandle,RID *rid, RM_Record *rec) 
{
	return SUCCESS;
}

RC InsertRec (RM_FileHandle *fileHandle,char *pData, RID *rid)
{
	return SUCCESS;
}

RC DeleteRec (RM_FileHandle *fileHandle,const RID *rid)
{
	return SUCCESS;
}

RC UpdateRec (RM_FileHandle *fileHandle,const RM_Record *rec)
{
	return SUCCESS;
}

RC RM_CreateFile(char *fileName, int recordSize)
{
	int recordsperpage;
	double d1 = sizeof(Page) - 4.0 - sizeof(RM_PageSubHeader) - 1;
	double d2 = recordSize + 0.125;
	recordsperpage = int(d1 / d2);

	if (recordsperpage < 1) {
		return RM_INVALIDRECSIZE;
	}
	RC rc;
	PF_FileHandle pfFileHandle;
	PF_PageHandle pfPageHandle;

	rc = CreateFile(fileName);
	if (rc != SUCCESS) {
		return rc;
	}

	rc = openFile(fileName, &pfFileHandle);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = AllocatePage(&pfFileHandle, &pfPageHandle);
	if (rc != SUCCESS) {
		return rc;
	}
	char *pData;
	rc = GetData(&pfPageHandle, &pData);
	if (rc != SUCCESS) {
		return rc;
	}
	RM_FileSubHeader *rmFileSubHeader;
	rmFileSubHeader = (RM_FileSubHeader *)pData;
	rmFileSubHeader->nRecords = 0;
	rmFileSubHeader->recordSize = recordSize;
	rmFileSubHeader->recordsPerPage = recordsperpage;
	rmFileSubHeader->firstRecordOffset = sizeof(RM_PageSubHeader) + (recordsperpage + 8) / 8;
	memset(pData + sizeof(RM_FileSubHeader), 0, PF_PAGE_SIZE - sizeof(RM_FileSubHeader));
	rc = MarkDirty(&pfPageHandle);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = UnpinPage(&pfPageHandle);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = CloseFile(&pfFileHandle);
	if (rc != SUCCESS) {
		return rc;
	}
	return SUCCESS;
}

RC RM_OpenFile(char *fileName, RM_FileHandle *fileHandle)
{
	char *pData;
	if (fileHandle->bOpen) {
		return RM_FHOPENNED;
	}
	RC rc;
	rc = openFile(fileName, &(fileHandle->pfFileHandle));
	if (rc != SUCCESS) {
		return rc;
	}
	rc = GetThisPage(&(fileHandle->pfFileHandle), 1, &(fileHandle->pfPageHandle_FileHdr));
	if (rc != SUCCESS) {
		return rc;
	}
	rc = GetData(&(fileHandle->pfPageHandle_FileHdr), &pData);
	if (rc != SUCCESS) {
		return rc;
	}
	fileHandle->fileSubHeader = (RM_FileSubHeader *)pData;
	fileHandle->pBitmap = pData + sizeof(RM_FileSubHeader);
	fileHandle->pageNum_FileHdr = 1;
	fileHandle->bOpen = true;
	return SUCCESS;
}

RC RM_CloseFile(RM_FileHandle *fileHandle)
{
	RC rc;
	if (fileHandle->bOpen == false) {
		return RM_FHCLOSED;
	}
	rc = UnpinPage(&(fileHandle->pfPageHandle_FileHdr));
	if (rc != SUCCESS) {
		return rc;
	}
	rc = CloseFile(&(fileHandle->pfFileHandle));
	if (rc != SUCCESS) {
		return rc;
	}
	fileHandle->bOpen = false;
	return SUCCESS;
}

