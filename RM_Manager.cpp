#include "stdafx.h"
#include "RM_Manager.h"
#include "str.h"

bool SatisfyCondition(RM_FileScan *rmFileScan, char *pRec);
RC GetNextRecInMem(RM_FileScan *rmFileScan, RM_Record *rec);
RC FetchPages(RM_FileScan *rmFileScan);

RC OpenScan(RM_FileScan *rmFileScan, RM_FileHandle *fileHandle, int conNum, Con *conditions)//初始化扫描
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

RC CloseScan(RM_FileScan * rmFileScan)
{
	rmFileScan->bOpen = false;
	rmFileScan->pRMFileHandle = NULL;
	rmFileScan->conNum = 0;
	rmFileScan->conditions = NULL;
	//if (rmFileScan->PageHandle != NULL)
	//{
	//	/*UnpinPage(rmFileScan->PageHandle);
	//	free(rmFileScan->PageHandle);
	//	rmFileScan->PageHandle = NULL;*/
	//}
	return SUCCESS;
}

RC GetNextRec(RM_FileScan *rmFileScan, RM_Record *rec)
{
	if (rmFileScan->bOpen == false) {
		return RM_FSCLOSED;
	}
	RC rc;
	while (GetNextRecInMem(rmFileScan, rec) == RM_NOMORERECINMEM)
	{
		rc = FetchPages(rmFileScan);
		if (rc != SUCCESS) {
			return rc;
		}
	}
	return SUCCESS;
}

RC FetchPages(RM_FileScan *rmFileScan)
{
	int i;
	RC rc;

	if (rmFileScan->bOpen == false) {
		return RM_FSCLOSED;
	}

	if (rmFileScan->pinnedPageCount > 0) {
		for (i = 0; i < rmFileScan->pinnedPageCount; i++) {
			rc = UnpinPage(rmFileScan->pfPageHandles + i);
			if (rc != SUCCESS) {
				return rc;
			}
		}
	}
	rmFileScan->pinnedPageCount = 0;
	rmFileScan->phIx = 0;
	rmFileScan->snIx = 0;
	rmFileScan->N = 1;

	for (i = 0; i < rmFileScan->N; i++) {
		rc = GetThisPage(&(rmFileScan->pRMFileHandle->pfFileHandle), rmFileScan->pnLast + 1, rmFileScan->pfPageHandles + i);
		if (rc != SUCCESS) {
			break;
		}
		rc = GetPageNum(rmFileScan->pfPageHandles + i, &(rmFileScan->pnLast));
		if (rc != SUCCESS) {
			return rc;
		}
		rmFileScan->pinnedPageCount++;
	}

	if (rmFileScan->pinnedPageCount == 0) {
		return PF_EOF;
	}

	return SUCCESS;
}

RC GetNextRecInMem(RM_FileScan *rmFileScan, RM_Record *rec)
{
	RC rc;
	int byte, bit, offset;
	char *pbitmap, *pRec;

	if (rmFileScan->bOpen == false) {
		return RM_FSCLOSED;
	}

	if (rmFileScan->phIx >= rmFileScan->pinnedPageCount) {
		return RM_NOMORERECINMEM;
	}

	for (; rmFileScan->phIx < rmFileScan->pinnedPageCount; rmFileScan->phIx++) {

		for (; rmFileScan->snIx < (rmFileScan->pRMFileHandle->fileSubHeader->recordsPerPage); rmFileScan->snIx++) {
			byte = rmFileScan->snIx / 8;
			bit = rmFileScan->snIx % 8;
			rc = GetData(rmFileScan->pfPageHandles + rmFileScan->phIx, &pbitmap);
			if (rc != SUCCESS) {
				return rc;
			}
			pbitmap += sizeof(RM_PageSubHeader);

			rc = GetData(rmFileScan->pfPageHandles + rmFileScan->phIx, &pRec);
			if (rc != SUCCESS) {
				return rc;
			}
			offset = rmFileScan->pRMFileHandle->fileSubHeader->firstRecordOffset + (rmFileScan->pRMFileHandle->fileSubHeader->recordSize)*(rmFileScan->snIx);
			pRec += offset;

			if ((pbitmap[byte] & (1 << bit)) != 0) {
				if (SatisfyCondition(rmFileScan, pRec) == false) {
					continue;
				}

				if (rec->pData) {
					//	free(rec->pData);
				}
				rec->pData = (char *)malloc(rmFileScan->pRMFileHandle->fileSubHeader->recordSize);
				memcpy(rec->pData, pRec, (rmFileScan->pRMFileHandle->fileSubHeader->recordSize));

				rc = GetPageNum(rmFileScan->pfPageHandles + rmFileScan->phIx, &(rec->rid.pageNum));
				if (rc != SUCCESS) {
					return rc;
				}
				rec->rid.slotNum = rmFileScan->snIx;
				rec->bValid = true;
				rmFileScan->snIx++;
				return SUCCESS;
			}
		}
		rmFileScan->snIx = 0;
	}
	return RM_NOMORERECINMEM;
}


bool SatisfyCondition(RM_FileScan *rmFileScan, char *pRec)
{
	int i, i1 = 0, i2 = 0;
	float f1 = 0, f2 = 0;
	char *s1 = NULL, *s2 = NULL;
	if (rmFileScan->conNum == 0)
		return true;
	for (i = 0; i < rmFileScan->conNum; i++)
	{
		switch (rmFileScan->conditions[i].attrType) {
		case ints:
			if (rmFileScan->conditions[i].bLhsIsAttr == 1)
				memcpy(&i1, pRec + rmFileScan->conditions[i].LattrOffset, sizeof(int));
			else
				i1 = *(int *)rmFileScan->conditions[i].Lvalue;
			if (rmFileScan->conditions[i].bRhsIsAttr == 1)
				memcpy(&i2, pRec + rmFileScan->conditions[i].RattrOffset, sizeof(int));
			else
				i2 = *(int *)rmFileScan->conditions[i].Rvalue;
			break;
		case floats:
			if (rmFileScan->conditions[i].bLhsIsAttr == 1)
				memcpy(&f1, pRec + rmFileScan->conditions[i].LattrOffset, sizeof(float));
			else
				f1 = *(float *)rmFileScan->conditions[i].Lvalue;
			if (rmFileScan->conditions[i].bRhsIsAttr == 1)
				memcpy(&f2, pRec + rmFileScan->conditions[i].RattrOffset, sizeof(float));
			else
				f2 = *(float *)rmFileScan->conditions[i].Rvalue;
			break;
		case chars:
			if (rmFileScan->conditions[i].bLhsIsAttr == 1)
				s1 = pRec + rmFileScan->conditions[i].LattrOffset;
			else
				s1 = (char *)rmFileScan->conditions[i].Lvalue;
			if (rmFileScan->conditions[i].bRhsIsAttr == 1)
				s2 = pRec + rmFileScan->conditions[i].RattrOffset;
			else
				s2 = (char *)rmFileScan->conditions[i].Rvalue;
			break;
		}

		bool flag = false;

		switch (rmFileScan->conditions[i].compOp) {
		case EQual:
			switch (rmFileScan->conditions[i].attrType) {
			case ints:
				flag = (i1 == i2);
				break;
			case floats:
				flag = (f1 == f2);
				break;
			case chars:
				flag = (strcmp(s1, s2) == 0);
				break;
			}
			break;
		case LessT:
			switch (rmFileScan->conditions[i].attrType) {
			case ints:
				flag = (i1 < i2);
				break;
			case floats:
				flag = (f1 < f2);
				break;
			case chars:
				flag = (strcmp(s1, s2) < 0);
				break;
			}
			break;
		case GreatT:
			switch (rmFileScan->conditions[i].attrType) {
			case ints:
				flag = (i1 > i2);
				break;
			case floats:
				flag = (f1 > f2);
				break;
			case chars:
				flag = (strcmp(s1, s2) > 0);
				break;
			}
			break;
		case LEqual:
			switch (rmFileScan->conditions[i].attrType) {
			case ints:
				flag = (i1 <= i2);
				break;
			case floats:
				flag = (f1 <= f2);
				break;
			case chars:
				flag = (strcmp(s1, s2) <= 0);
				break;
			}
			break;
		case GEqual:
			switch (rmFileScan->conditions[i].attrType) {
			case ints:
				flag = (i1 >= i2);
				break;
			case floats:
				flag = (f1 >= f2);
				break;
			case chars:
				flag = (strcmp(s1, s2) >= 0);
				break;
			}
			break;
		case NEqual:
			switch (rmFileScan->conditions[i].attrType) {
			case ints:
				flag = (i1 != i2);
				break;
			case floats:
				flag = (f1 != f2);
				break;
			case chars:
				flag = (strcmp(s1, s2) != 0);
				break;
			}
			break;
		}
		if (flag == 0)
			return false;

	}
	return true;
}

RC GetRec(RM_FileHandle *fileHandle, RID *rid, RM_Record *rec)
{
	PageNum pageNum;
	SlotNum slotNum;
	int byte, bit, offset;
	char *pBitmap, *pRec;
	RC rc;
	if (fileHandle->bOpen == false) {
		return RM_FHCLOSED;
	}
	pageNum = rid->pageNum;
	slotNum = rid->slotNum;
	PF_PageHandle pfPageHandle;

	rc = GetThisPage(&(fileHandle->pfFileHandle), pageNum, &pfPageHandle);

	if (rc != SUCCESS) {
		return rc;
	}

	rc = GetData(&pfPageHandle, &pBitmap);
	if (rc != SUCCESS) {
		return rc;
	}
	pBitmap += sizeof(RM_PageSubHeader);
	byte = slotNum / 8;
	bit = slotNum % 8;
	if ((pBitmap[byte] & (1 << bit)) == 0) {
		return RM_INVALIDRID;
	}

	rc = GetData(&pfPageHandle, &pRec);

	if (rc != SUCCESS) {
		return rc;
	}
	offset = fileHandle->fileSubHeader->firstRecordOffset + (fileHandle->fileSubHeader->recordSize)*slotNum;
	pRec += offset;

	if (rec->pData) {
		//	free(rec->pData);
	}

	rec->pData = (char *)malloc(fileHandle->fileSubHeader->recordSize);
	memcpy(rec->pData, pRec, fileHandle->fileSubHeader->recordSize);

	rec->rid.pageNum = pageNum;
	rec->rid.slotNum = slotNum;
	rec->bValid = true;

	rc = UnpinPage(&pfPageHandle);
	if (rc != SUCCESS) {
		return rc;
	}
	return SUCCESS;
}

RC InsertRec(RM_FileHandle *fileHandle, char *pData, RID *rid)
{
	PageNum pageNum = fileHandle->pageNum_FileHdr + 1;
	PageNum pageCount = fileHandle->pfFileHandle.pFileSubHeader->pageCount;
	SlotNum slotNum;
	int byte, bit, offset;
	char *pRec, *pBitmap, *pdata;
	if (fileHandle->bOpen == false) {
		return RM_FHCLOSED;
	}
	for (; pageNum <= pageCount; pageNum++) {
		byte = pageNum / 8;
		bit = pageNum % 8;
		if (((fileHandle->pfFileHandle.pBitmap[byte] & (1 << bit)) != 0) && ((fileHandle->pBitmap[byte] & (1 << bit)) == 0)) {
			break;
		}
	}
	PF_PageHandle pfPageHandle;
	RC rc;

	if (pageNum > pageCount) {
		rc = AllocatePage(&(fileHandle->pfFileHandle), &pfPageHandle);
		if (rc != SUCCESS) {
			return rc;
		}
		rc = GetPageNum(&pfPageHandle, &pageNum);
		if (rc != SUCCESS) {
			return rc;
		}
	}
	else {
		rc = GetThisPage(&(fileHandle->pfFileHandle), pageNum, &pfPageHandle);
		if (rc != SUCCESS) {
			return rc;
		}
	}

	rc = GetData(&pfPageHandle, &pBitmap);
	if (rc != SUCCESS) {
		return rc;
	}
	pBitmap += sizeof(RM_PageSubHeader);//数据页开始处，最先存储该页面已分配的记录数，之后是一个位图！！！
	for (slotNum = 0; slotNum < fileHandle->fileSubHeader->recordsPerPage; slotNum++) {
		byte = slotNum / 8;
		bit = slotNum % 8;
		if ((pBitmap[byte] & (1 << bit)) == 0)
			break;//读取位图，分配一个空闲插槽号
	}

	rc = GetData(&pfPageHandle, &pRec);
	if (rc != SUCCESS) {
		return rc;
	}
	offset = fileHandle->fileSubHeader->firstRecordOffset + slotNum * (fileHandle->fileSubHeader->recordSize);
	pRec += offset;//定位至找到的插槽号地址
	memcpy(pRec, pData, fileHandle->fileSubHeader->recordSize);//插入
	//将该插槽号对应的位图置1
	byte = slotNum / 8;
	bit = slotNum % 8;
	pBitmap[byte] = pBitmap[byte] | (1 << bit);

	RM_PageSubHeader *rmPageSubHeader;
	rc = GetData(&pfPageHandle, &pdata);
	if (rc != SUCCESS) {
		return rc;
	}
	rmPageSubHeader = (RM_PageSubHeader *)pdata;
	rmPageSubHeader->nRecords++;
	fileHandle->fileSubHeader->nRecords++;
	//如果插入记录后页满，则置分页控制页该页对应的的位图位置1
	if (rmPageSubHeader->nRecords == fileHandle->fileSubHeader->recordsPerPage) {
		byte = pageNum / 8;
		bit = pageNum % 8;
		fileHandle->pBitmap[byte] |= (1 << bit);
	}

	rc = MarkDirty(&pfPageHandle);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = UnpinPage(&pfPageHandle);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = MarkDirty(&(fileHandle->pfPageHandle_FileHdr));
	if (rc != SUCCESS) {
		return rc;
	}

	rid->pageNum = pageNum;
	rid->slotNum = slotNum;
	rid->bValid = true;

	return SUCCESS;
}

RC DeleteRec(RM_FileHandle *fileHandle, const RID *rid)
{
	int byte, bit;
	char *pBitmap;
	RC rc;
	if (fileHandle->bOpen == false) {
		return RM_FHCLOSED;
	}

	PageNum pageNum = rid->pageNum;
	SlotNum slotNum = rid->slotNum;
	char *pData;
	RM_PageSubHeader *rmPageSubHeader;
	PF_PageHandle pfPageHandle;

	rc = GetThisPage(&(fileHandle->pfFileHandle), pageNum, &pfPageHandle);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = GetData(&pfPageHandle, &pBitmap);
	if (rc != SUCCESS) {
		return rc;
	}
	pBitmap += sizeof(RM_PageSubHeader);
	byte = slotNum / 8;
	bit = slotNum % 8;
	if ((pBitmap[byte] & (1 << bit)) == 0) {
		return RM_INVALIDRID;
	}
	pBitmap[byte] &= ~(1 << bit);

	rc = GetData(&pfPageHandle, &pData);
	if (rc != SUCCESS) {
		return rc;
	}
	rmPageSubHeader = (RM_PageSubHeader *)pData;
	rmPageSubHeader->nRecords--;
	fileHandle->fileSubHeader->nRecords--;

	byte = pageNum / 8;
	bit = pageNum % 8;
	fileHandle->pBitmap[byte] &= ~(1 << bit);

	rc = MarkDirty(&pfPageHandle);
	if (rc != SUCCESS) {
		return rc;
	}

	rc = UnpinPage(&pfPageHandle);
	if (rc != SUCCESS) {
		return rc;
	}

	rc = MarkDirty(&(fileHandle->pfPageHandle_FileHdr));
	if (rc != SUCCESS) {
		return rc;
	}
	return SUCCESS;
}

RC UpdateRec(RM_FileHandle *fileHandle, const RM_Record *rec)
{
	PageNum pageNum = rec->rid.pageNum;
	SlotNum slotNum = rec->rid.slotNum;
	int byte, bit;
	char *pData, *pBitmap, *pRec;
	RC rc;
	PF_PageHandle pfPageHandle;
	rc = GetThisPage(&(fileHandle->pfFileHandle), pageNum, &pfPageHandle);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = GetData(&pfPageHandle, &pData);
	pBitmap = pData + sizeof(RM_PageSubHeader);
	byte = slotNum / 8;
	bit = slotNum % 8;
	if ((pBitmap[byte] & (1 << bit)) == 0) {
		return RM_INVALIDRID;
	}

	pRec = pData + fileHandle->fileSubHeader->firstRecordOffset + slotNum * (fileHandle->fileSubHeader->recordSize);
	memcpy(pRec, rec->pData, fileHandle->fileSubHeader->recordSize);

	rc = MarkDirty(&pfPageHandle);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = UnpinPage(&pfPageHandle);
	if (rc != SUCCESS) {
		return rc;
	}
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

