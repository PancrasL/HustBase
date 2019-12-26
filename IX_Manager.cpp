#include "stdafx.h"
#include "IX_Manager.h"
int threshold;
RC CreateIndex(const char * fileName, AttrType attrType, int attrLength)
{
	CreateFile(fileName);  //创建索引文件

	PF_FileHandle *fileHandle = NULL;
	fileHandle = (PF_FileHandle *)malloc(sizeof(PF_FileHandle));
	openFile((char *)(void *)fileName, fileHandle);	//打开索引文件

	PF_PageHandle *firstPageHandle = NULL;
	firstPageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
	AllocatePage(fileHandle, firstPageHandle);		//分配索引文件的第一个页面
	PageNum pageNum;
	GetPageNum(firstPageHandle, &pageNum);

	//生成索引控制信息
	IX_FileHeader index_FileHeader;
	index_FileHeader.attrLength = attrLength;
	index_FileHeader.keyLength = attrLength + sizeof(RID);
	index_FileHeader.attrType = attrType;
	index_FileHeader.rootPage = pageNum;
	index_FileHeader.first_leaf = pageNum;
	int order = (PF_PAGE_SIZE - (sizeof(IX_FileHeader) + sizeof(IX_Node))) / (2 * sizeof(RID) + attrLength);
	index_FileHeader.order = order;
	threshold = order >> 1;

	//获取第一页的数据区
	char *pData;
	GetData(firstPageHandle, &pData);
	memcpy(pData, &index_FileHeader, sizeof(IX_FileHeader));	//将索引控制信息复制到第一页

																//初始化节点控制信息，将根节点的置为叶子节点，关键字数为0，
	IX_Node index_NodeControl;
	index_NodeControl.brother = 0;
	index_NodeControl.is_leaf = 1;
	index_NodeControl.keynum = 0;
	index_NodeControl.parent = 0;
	memcpy(pData + sizeof(IX_FileHeader), &index_NodeControl, sizeof(IX_Node));

	MarkDirty(firstPageHandle);

	UnpinPage(firstPageHandle);

	//关闭索引文件
	CloseFile(fileHandle);
	free(firstPageHandle);
	free(fileHandle);
	return SUCCESS;
}

RC OpenIndex(const char * fileName, IX_IndexHandle * indexHandle)
{
	//打开索引文件
	PF_FileHandle fileHandle;
	RC rc;
	if ((rc = openFile((char *)(void *)fileName, &fileHandle)) != SUCCESS) {
		return rc;
	}

	PF_PageHandle *pageHandle = NULL;
	pageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
	GetThisPage(&fileHandle, 1, pageHandle);   //获取第一页

	char *pData;
	GetData(pageHandle, &pData);    //获取第一页的数据区

	IX_FileHeader fileHeader;
	memcpy(&fileHeader, pData, sizeof(IX_FileHeader));  //复制第一页索引控制信息

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


