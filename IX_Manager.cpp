#include "stdafx.h"
#include "IX_Manager.h"
int threshold;
RC CreateIndex(char * fileName, AttrType attrType, int attrLength) {
	CreateFile(fileName);

	PF_FileHandle fileHandle;
	openFile(fileName, &fileHandle);

	PF_PageHandle firstPageHandle;
	AllocatePage(&fileHandle, &firstPageHandle);
	PageNum pageNum;
	GetPageNum(&firstPageHandle, &pageNum);

	char *pData;
	GetData(&firstPageHandle, &pData);
	IX_FileHeader *header = (IX_FileHeader *)pData;
	IX_Node* root = (IX_Node*)(pData + sizeof(IX_FileHeader));

	header->attrLength = attrLength;
	header->keyLength = attrLength + sizeof(RID);
	header->attrType = attrType;
	header->rootPage = pageNum;
	header->first_leaf = pageNum;
	int order = (PF_PAGE_SIZE - (sizeof(IX_FileHeader) + sizeof(IX_Node))) / (2 * sizeof(RID) + attrLength);
	header->order = order;

	root->brother = 0;
	root->is_leaf = 1;
	root->keynum = 0;
	root->parent = 0;

	MarkDirty(&firstPageHandle);
	UnpinPage(&firstPageHandle);
	CloseFile(&fileHandle);
	return SUCCESS;
}

RC OpenIndex(char *fileName, IX_IndexHandle *indexHandle) {
	RC rc;
	if ((rc = openFile(fileName, &indexHandle->fileHandle)) != SUCCESS) {
		return rc;
	}

	PF_PageHandle pageHandle;
	GetThisPage(&indexHandle->fileHandle, 1, &pageHandle);

	char *pData;
	GetData(&pageHandle, &pData);
	memcpy(&indexHandle->fileHeader, pData, sizeof(IX_FileHeader));  //复制第一页索引控制信息

	indexHandle->bOpen = true;
	return SUCCESS;
}

RC CloseIndex(IX_IndexHandle *indexHandle) {
	CloseFile(&indexHandle->fileHandle);
	return SUCCESS;
}

RC InsertEntry(IX_IndexHandle * indexHandle, void * pData, const RID * rid)
{
	return SUCCESS;
}

RC DeleteEntry(IX_IndexHandle * indexHandle, void * pData, const RID * rid)
{
	return SUCCESS;
}

RC OpenIndexScan(IX_IndexScan *indexScan, IX_IndexHandle *indexHandle, CompOp compOp, char *value) {
	return SUCCESS;
}

RC IX_GetNextEntry(IX_IndexScan *indexScan, RID * rid) {
	return SUCCESS;
}

RC CloseIndexScan(IX_IndexScan *indexScan) {
	return SUCCESS;
}

RC GetIndexTree(char *fileName, Tree *index) {
	return SUCCESS;
}


