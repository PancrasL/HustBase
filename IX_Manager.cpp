#include "stdafx.h"
#include "IX_Manager.h"
int threshold;
RC CreateIndex(const char * fileName, AttrType attrType, int attrLength)
{
	CreateFile(fileName);  //���������ļ�

	PF_FileHandle *fileHandle = NULL;
	fileHandle = (PF_FileHandle *)malloc(sizeof(PF_FileHandle));
	openFile((char *)(void *)fileName, fileHandle);	//�������ļ�

	PF_PageHandle *firstPageHandle = NULL;
	firstPageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
	AllocatePage(fileHandle, firstPageHandle);		//���������ļ��ĵ�һ��ҳ��
	PageNum pageNum;
	GetPageNum(firstPageHandle, &pageNum);

	//��������������Ϣ
	IX_FileHeader index_FileHeader;
	index_FileHeader.attrLength = attrLength;
	index_FileHeader.keyLength = attrLength + sizeof(RID);
	index_FileHeader.attrType = attrType;
	index_FileHeader.rootPage = pageNum;
	index_FileHeader.first_leaf = pageNum;
	int order = (PF_PAGE_SIZE - (sizeof(IX_FileHeader) + sizeof(IX_Node))) / (2 * sizeof(RID) + attrLength);
	index_FileHeader.order = order;
	threshold = order >> 1;

	//��ȡ��һҳ��������
	char *pData;
	GetData(firstPageHandle, &pData);
	memcpy(pData, &index_FileHeader, sizeof(IX_FileHeader));	//������������Ϣ���Ƶ���һҳ

																//��ʼ���ڵ������Ϣ�������ڵ����ΪҶ�ӽڵ㣬�ؼ�����Ϊ0��
	IX_Node index_NodeControl;
	index_NodeControl.brother = 0;
	index_NodeControl.is_leaf = 1;
	index_NodeControl.keynum = 0;
	index_NodeControl.parent = 0;
	memcpy(pData + sizeof(IX_FileHeader), &index_NodeControl, sizeof(IX_Node));

	MarkDirty(firstPageHandle);

	UnpinPage(firstPageHandle);

	//�ر������ļ�
	CloseFile(fileHandle);
	free(firstPageHandle);
	free(fileHandle);
	return SUCCESS;
}

RC OpenIndex(const char * fileName, IX_IndexHandle * indexHandle)
{
	//�������ļ�
	PF_FileHandle fileHandle;
	RC rc;
	if ((rc = openFile((char *)(void *)fileName, &fileHandle)) != SUCCESS) {
		return rc;
	}

	PF_PageHandle *pageHandle = NULL;
	pageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
	GetThisPage(&fileHandle, 1, pageHandle);   //��ȡ��һҳ

	char *pData;
	GetData(pageHandle, &pData);    //��ȡ��һҳ��������

	IX_FileHeader fileHeader;
	memcpy(&fileHeader, pData, sizeof(IX_FileHeader));  //���Ƶ�һҳ����������Ϣ

	indexHandle->bOpen = true;
	indexHandle->fileHandle = fileHandle;
	indexHandle->fileHeader = fileHeader;
	free(pageHandle);
	return SUCCESS;
}

RC CloseIndex(IX_IndexHandle * indexHandle)
{
	PF_FileHandle fileHandle = indexHandle->fileHandle;
	CloseFile(&fileHandle);
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


