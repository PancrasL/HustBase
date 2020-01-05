#include "stdafx.h"
#include "EditArea.h"
#include "SYS_Manager.h"
#include "QU_Manager.h"
#include "HustBase.h"
#include "HustBaseDoc.h"
#include <iostream>
#include <vector>
#include <set>
#include <string>
using std::set;
using std::string;
bool isOpened = false;//��ʶ���ݿ��Ƿ��

/*�Զ��庯��*/
//���ַ���ת��ΪСд
void toLowerString(std::string &s);
//�ڽ�����ʾһ����Ϣ
void showMsg(CEditArea *editArea, char * msg);
//���select�Ĳ�ѯ���
void showSelectResult(SelResult &res, CEditArea *editArea);


void ExecuteAndMessage(char * sql, CEditArea* editArea) {//����ִ�е���������ڽ�������ʾִ�н�����˺������޸�
	std::string s_sql = sql;
	RC rc;
	toLowerString(s_sql);
	//��ѯ���Ĵ���
	if (s_sql.find("select") == 0) {
		SelResult res;
		Init_Result(&res);
		rc = Query(sql, &res);
		if (rc == SUCCESS) {
			showSelectResult(res, editArea);
			return;
		}
	}
	//�������Ĵ���
	else {
		//ִ��
		rc = execute(sql);

		//���½���
		CHustBaseDoc *pDoc;
		pDoc = CHustBaseDoc::GetDoc();
		CHustBaseApp::pathvalue = true;
		pDoc->m_pTreeView->PopulateTree();
	}

	//ִ�н��
	switch (rc) {
	case SUCCESS:
		showMsg(editArea, "�����ɹ�");
		break;
	case SQL_SYNTAX:
		showMsg(editArea, "���﷨����");
		break;
	case TABLE_NOT_EXIST:
		showMsg(editArea, "������");
		break;
	case PF_FILEERR:
		showMsg(editArea, "�ļ�������");
		break;
	case TABLE_EXIST:
		showMsg(editArea, "���Ѵ��ڣ��޷��ظ�����");
		break;
	case TABLE_COLUMN_ERROR:
		showMsg(editArea, "����������������ͬ��");
		break;
	case DB_NOT_EXIST:
		showMsg(editArea, "���ݿⲻ����");
		break;
	default:
		showMsg(editArea, "����");
		break;
	}
}

RC execute(char * sql) {
	sqlstr *sql_str = NULL;
	RC rc;
	sql_str = get_sqlstr();
	rc = parse(sql, sql_str);//ֻ�����ַ��ؽ��SUCCESS��SQL_SYNTAX

	if (rc != SUCCESS)
		return rc;

	switch (sql_str->flag)
	{
		//case 1:
		////�ж�SQL���Ϊselect���

		//break;

	case 2:
		//�ж�SQL���Ϊinsert���
		rc = Insert(sql_str->sstr.ins.relName, sql_str->sstr.ins.nValues, sql_str->sstr.ins.values);
		break;
	case 3:
		//�ж�SQL���Ϊupdate���
		rc = Update(sql_str->sstr.upd.relName, sql_str->sstr.upd.attrName, &(sql_str->sstr.upd.value), sql_str->sstr.upd.nConditions, sql_str->sstr.upd.conditions);
		break;

	case 4:
		//�ж�SQL���Ϊdelete���
		rc = Delete(sql_str->sstr.del.relName, sql_str->sstr.del.nConditions, sql_str->sstr.del.conditions);
		break;

	case 5:
		//�ж�SQL���ΪcreateTable���
		rc = CreateTable(sql_str->sstr.cret.relName, sql_str->sstr.cret.attrCount, sql_str->sstr.cret.attributes);
		break;

	case 6:
		//�ж�SQL���ΪdropTable���
		rc = DropTable(sql_str->sstr.drt.relName);
		break;

	case 7:
		//�ж�SQL���ΪcreateIndex���
		rc = CreateIndex(sql_str->sstr.crei.indexName, sql_str->sstr.crei.relName, sql_str->sstr.crei.attrName);
		break;

	case 8:
		//�ж�SQL���ΪdropIndex���
		rc = DropIndex(sql_str->sstr.dri.indexName);
		break;

	case 9:
		//�ж�Ϊhelp��䣬���Ը���������ʾ
		break;

	case 10:
		//�ж�Ϊexit��䣬�����ɴ˽����˳�����
		break;
	}
	return rc;
}

RC CreateDB(char *dbpath, char *dbname) {
	//��dbpath�´���dbname�ļ���
	CString path = dbpath;
	path += "\\";
	path += dbname;
	CreateDirectory(path, NULL);

	//��dbname�ļ����´������ļ������ļ�
	SetCurrentDirectory(path);
	RC rc;
	rc = RM_CreateFile("SYSTABLES", sizeof(SysTable));
	if (rc != SUCCESS)
		return rc;
	rc = RM_CreateFile("SYSCOLUMNS", sizeof(SysColumn));
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

RC DropDB(char *dbname) {
	SetCurrentDirectory(dbname);
	if (_access("SYSTABLES", 0) == -1 || _access("SYSCOLUMNS", 0) == -1) {
		return DB_NOT_EXIST;
	}
	BOOL ret = TRUE;
	CFileFind finder;
	CString path;
	path.Format(_T("%s/*"), dbname);
	BOOL bWorking = finder.FindFile(path);
	while (bWorking)
	{
		bWorking = finder.FindNextFile();
		if (finder.IsDirectory() && !finder.IsDots())
		{//�����ļ���
			char subDir[200];
			std::string dir = finder.GetFilePath();
			sprintf_s(subDir, "%s", dir.c_str());
			DropDB(subDir); //�ݹ�ɾ���ļ���
			RemoveDirectory(finder.GetFilePath());
		}
		else
		{//�����ļ�
			DeleteFile(finder.GetFilePath());
		}
	}
	return SUCCESS;
}

RC OpenDB(char *dbname) {
	SetCurrentDirectory(dbname);
	if (_access("SYSTABLES", 0) == -1 || _access("SYSCOLUMNS", 0) == -1) {
		return DB_NOT_EXIST;
	}
	isOpened = true;
	return SUCCESS;
}

RC CloseDB() {
	isOpened = false;
	return SUCCESS;
}


RC CreateTable(char *relName, int attrCount, AttrInfo *attributes) {
	RC rc;
	char  *pData;
	RM_FileHandle rm_table, rm_column;
	RID rid;
	int recordsize;//���ݱ��ÿ����¼�Ĵ�С
	AttrInfo * attrtmp = attributes;

	//�����
	if (_access(relName, 0) == 0)
		return TABLE_EXIST;

	//�жϱ����Ƿ����
	if (strlen(relName) > 20)
		return TABLE_NAME_ILLEGAL;

	//�ж��������Ƿ����Ҫ��
	set<string> s;
	for (int i = 0; i < attrCount; i++) {
		if (s.find(attributes[i].attrName) != s.end())//����ͬ������
			return TABLE_COLUMN_ERROR;
		s.insert(attributes[i].attrName);
		if (strlen(attributes[i].attrName) > 10)
			return TABLE_COLUMN_ERROR;
	}
	/*��ϵͳ���ļ�������*/
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;

	pData = new char[sizeof(SysTable)];
	memcpy(pData, relName, 21);
	memcpy(pData + 21, &attrCount, sizeof(int));
	rid.bValid = false;
	rc = InsertRec(&rm_table, pData, &rid);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = RM_CloseFile(&rm_table);
	if (rc != SUCCESS) {
		return rc;
	}
	//�ͷ�������ڴ�ռ�
	delete[] pData;

	/*�����ļ�������*/
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS)
		return rc;
	for (int i = 0, offset = 0; i < attrCount; i++, attrtmp++) {
		pData = new char[sizeof(SysColumn)];
		memcpy(pData, relName, 21);
		memcpy(pData + 21, attrtmp->attrName, 21);
		memcpy(pData + 42, &(attrtmp->attrType), sizeof(AttrType));
		memcpy(pData + 42 + sizeof(AttrType), &(attrtmp->attrLength), sizeof(int));
		memcpy(pData + 42 + sizeof(int) + sizeof(AttrType), &offset, sizeof(int));
		memcpy(pData + 42 + 2 * sizeof(int) + sizeof(AttrType), "0", sizeof(char));
		rid.bValid = false;
		rc = InsertRec(&rm_column, pData, &rid);
		if (rc != SUCCESS) {
			return rc;
		}
		delete[] pData;
		offset += attrtmp->attrLength;
	}
	rc = RM_CloseFile(&rm_column);
	if (rc != SUCCESS) {
		return rc;
	}

	/*�������ݱ�*/

	//����recordsize�Ĵ�С
	recordsize = 0;
	attrtmp = attributes;
	for (int i = 0; i < attrCount; i++, attrtmp++) {
		recordsize += attrtmp->attrLength;
	}
	rc = RM_CreateFile(relName, recordsize);
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

RC DropTable(char *relName) {
	CFile tmp;
	RM_FileHandle rm_table, rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record rectab, reccol;
	int attrcount;//��ʱ �������������Գ��ȣ�����ƫ��

	//ɾ���������ڵı�
	if (_access(relName, 0) == -1)
		return TABLE_NOT_EXIST;

	/*ɾ�����ݱ��ļ�*/
	tmp.Remove((LPCTSTR)relName);

	/*��ϵͳ���ϵͳ���ж�Ӧ�����ݱ����ؼ�¼ɾ��*/
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS)
		return rc;

	//��ȡϵͳ����Ϣ
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_table, 0, NULL);
	if (rc != SUCCESS)
		return rc;

	while (GetNextRec(&FileScan, &rectab) == SUCCESS) {//ѭ��������¼
		if (strcmp(relName, rectab.pData) == 0) {
			memcpy(&attrcount, rectab.pData + 21, sizeof(int));
			DeleteRec(&rm_table, &(rectab.rid));//���ü�¼����ģ��ӿ�ɾ���ñ������Ϣ
			delete[] rectab.pData;
			break;
		}
		delete[] rectab.pData;
	}

	//��ȡϵͳ����Ϣ
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_column, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//����֮ǰ��ȡ��ϵͳ������Ϣ����ȡ������Ϣ
	for (int i = 0; i < attrcount && GetNextRec(&FileScan, &reccol) == SUCCESS;) {
		if (strcmp(relName, reccol.pData) == 0) {//ɾ�����Լ�¼
			DeleteRec(&rm_column, &(reccol.rid));
			i++;
		}
		delete[] reccol.pData;
	}

	//�ر��ļ����
	rc = RM_CloseFile(&rm_table);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_column);
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

RC CreateIndex(char *indexName, char *relName, char *attrName) {
	//�޸�������Ϣ�����������ֶ�Ϊ��������
	RC rc;

	RM_Record colRec;

	//��ϵͳ���ļ�
	RM_FileHandle colHandle;
	colHandle.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &colHandle);
	CHECK_RC(rc);

	//����ɨ������
	Con conditions[2];
	conditions[0].attrType = chars;
	conditions[0].compOp = EQual;
	conditions[0].bLhsIsAttr = 1;
	conditions[0].LattrLength = 21;
	conditions[0].LattrOffset = 0;
	conditions[0].bRhsIsAttr = 0;
	conditions[0].Rvalue = relName;

	conditions[1].attrType = chars;
	conditions[1].compOp = EQual;
	conditions[1].bLhsIsAttr = 1;
	conditions[1].LattrLength = 21;
	conditions[1].LattrOffset = 21;
	conditions[1].bRhsIsAttr = 0;
	conditions[1].Rvalue = attrName;
	RM_FileScan fileScan;
	fileScan.bOpen = false;
	OpenScan(&fileScan, &colHandle, 2, conditions);

	rc = GetNextRec(&fileScan, &colRec);
	if (rc != SUCCESS)
	{
		return rc;
	}

	if (*(colRec.pData + 42 + 3 * sizeof(int)) != '0')
	{
		return FAIL;
	}

	*(colRec.pData + 42 + 3 * sizeof(int)) = '1';   //����������ʶΪ1
	memset(colRec.pData + 42 + 3 * sizeof(int) + sizeof(char), '\0', 21);
	memcpy(colRec.pData + 42 + 3 * sizeof(int) + sizeof(char), indexName, strlen(indexName));

	//����ϵͳ���ļ�
	UpdateRec(&colHandle, &colRec);

	//�ر�ɨ�輰ϵͳ���ļ�
	CloseScan(&fileScan);
	RM_CloseFile(&colHandle);

	//�������������ļ�
	AttrType attrType;
	memcpy(&attrType, colRec.pData + 42, sizeof(int));

	//���������ļ�
	int length;
	memcpy(&length, colRec.pData + 42 + sizeof(int), sizeof(int));
	CreateIndex(indexName, attrType, length);

	//�������ļ�
	IX_IndexHandle indexHandle;
	indexHandle.bOpen = false;
	OpenIndex(indexName, &indexHandle);

	//�������ļ��м���������
	//���ȣ��򿪼�¼�ļ�,ɨ�����м�¼
	RM_FileHandle recFileHandle;
	RM_FileScan recFileScan;
	RM_Record rec;

	recFileHandle.bOpen = false;
	recFileScan.bOpen = false;
	rec.bValid = false;

	RM_OpenFile(relName, &recFileHandle);
	OpenScan(&recFileScan, &recFileHandle, 0, NULL);

	int attrOffset, attrLength;
	memcpy(&attrOffset, colRec.pData + 42 + 2 * sizeof(int), sizeof(int));
	memcpy(&attrLength, colRec.pData + 42 + sizeof(int), sizeof(int));

	char *attrValue = NULL;
	attrValue = (char *)malloc(sizeof(char)*attrLength);

	//��ɨ�赽�ļ�¼���뵽������
	while (GetNextRec(&recFileScan, &rec) == SUCCESS)
	{
		memcpy(attrValue, rec.pData + attrOffset, attrLength);
		InsertEntry(&indexHandle, attrValue, &rec.rid);
	}

	CloseScan(&recFileScan);
	RM_CloseFile(&recFileHandle);
	CloseIndex(&indexHandle);

	return SUCCESS;
}

RC DropIndex(char *indexName) {
	IX_IndexHandle indexHandle;
	RC rc;

	//�ж������Ƿ����
	indexHandle.bOpen = false;
	if ((rc = OpenIndex(indexName, &indexHandle)) != SUCCESS)
	{
		return rc;
	}
	CloseIndex(&indexHandle);

	//�ӱ����������������ʶ
	RM_FileHandle colHandle;
	RM_FileScan fileScan;
	RM_Record colRec;

	colHandle.bOpen = false;
	fileScan.bOpen = false;

	//��ϵͳ���ļ�
	rc = RM_OpenFile("SYSCOLUMNS", &colHandle);
	if (rc != SUCCESS)
	{
		return rc;
	}

	//����ɨ������
	Con *conditions = NULL;
	conditions = (Con *)malloc(sizeof(Con));
	(*conditions).attrType = chars;
	(*conditions).compOp = EQual;
	(*conditions).bLhsIsAttr = 1;
	(*conditions).LattrLength = strlen(indexName) + 1;
	(*conditions).LattrOffset = 42 + 3 * sizeof(int) + sizeof(char);
	(*conditions).bRhsIsAttr = 0;
	(*conditions).Rvalue = indexName;

	OpenScan(&fileScan, &colHandle, 1, conditions);

	rc = GetNextRec(&fileScan, &colRec);

	if (rc == SUCCESS) {
		*(colRec.pData + 42 + 3 * sizeof(int)) = '0';   //����������ʶΪ0
		memset(colRec.pData + 42 + 3 * sizeof(int) + sizeof(char), '\0', 21);
		//����ϵͳ���ļ�
		UpdateRec(&colHandle, &colRec);
	}

	//�ر�ɨ�輰ϵͳ���ļ�
	CloseScan(&fileScan);
	RM_CloseFile(&colHandle);

	DeleteFile((LPCTSTR)indexName);//ɾ�����ݱ��ļ�

	return SUCCESS;
}

RC Insert(char *relName, int nValues, Value *values) {
	RC rc;
	/*�жϱ��Ƿ����*/
	if (_access(relName, 0) == -1)
		return TABLE_NOT_EXIST;

	/*ɨ��ϵͳ���ñ�����Ը���*/
	RM_FileHandle rm_table;
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;

	Con con;
	con.attrType = chars;
	con.bLhsIsAttr = 1;
	con.LattrOffset = 0;
	con.LattrLength = strlen(relName) + 1;
	con.compOp = EQual;
	con.bRhsIsAttr = 0;
	con.Rvalue = relName;

	RM_FileScan rm_fileScan;
	RM_Record record;
	rm_fileScan.bOpen = false;
	rc = OpenScan(&rm_fileScan, &rm_table, 1, &con);
	if (rc != SUCCESS)
		return rc;
	rc = GetNextRec(&rm_fileScan, &record);
	if (rc != SUCCESS)
		return rc;

	int attrcount = 0;
	memcpy(&attrcount, record.pData + 21, sizeof(int));
	CloseScan(&rm_fileScan);
	RM_CloseFile(&rm_table);

	if (attrcount != nValues) {//��������岻һ��
		return FIELD_MISSING;
	}

	/*��ȡϵͳ����Ϣ*/
	RM_FileHandle rm_column;
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = OpenScan(&rm_fileScan, &rm_column, 0, NULL);
	if (rc != SUCCESS) {
		return rc;
	}
	SysColumn * column;
	column = new SysColumn[attrcount]();
	RM_Record reccol;
	while (GetNextRec(&rm_fileScan, &reccol) == SUCCESS) {
		if (strcmp(relName, reccol.pData) == 0) {//����ҵ�ΪrelName�ļ�¼��Ȼ���ȡ����
			for (int i = 0; i < attrcount; i++) {
				memcpy(column[i].tabName, reccol.pData, 21);
				memcpy(column[i].attrName, reccol.pData + 21, 21);
				memcpy(&(column[i].attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(column[i].attrLength), reccol.pData + 46, sizeof(int));
				memcpy(&(column[i].attrOffset), reccol.pData + 50, sizeof(int));
				rc = GetNextRec(&rm_fileScan, &reccol);
				if (rc != SUCCESS)
					break;
			}
			break;
		}
	}
	CloseScan(&rm_fileScan);
	RM_CloseFile(&rm_column);
	//�ж��������������Ƿ�һ��
	for (int i = 0; i < nValues; i++) {
		if (values[nValues - 1 - i].type != column[i].attrType)
			return FIELD_TYPE_MISMATCH;
	}

	//�Ѽ�¼���뵽���ݱ���
	RM_FileHandle rm_data;
	rm_data.bOpen = false;
	rc = RM_OpenFile(relName, &rm_data);
	if (rc != SUCCESS) {
		return rc;
	}
	char * value;
	auto ctmp = column;
	RID rid;
	value = new char[rm_data.fileSubHeader->recordSize];
	values = values + nValues - 1;
	for (int i = 0; i < nValues; i++, values--, ctmp++) {//��Ԫ���ݴ浽value��
		memcpy(value + ctmp->attrOffset, values->data, ctmp->attrLength);
	}
	rid.bValid = false;
	InsertRec(&rm_data, value, &rid);
	delete[] value;
	delete[] column;
	RM_CloseFile(&rm_data);
	return SUCCESS;
}

RC Delete(char *relName, int nConditions, Condition *conditions) {
	RM_FileHandle rm_data, rm_table, rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record recdata, rectab, reccol;
	SysColumn *column, *ctmp, *ctmpleft, *ctmpright;
	Condition *contmp;
	int i, torf;//�Ƿ�����ɾ�������ı�־
	int attrcount;//��¼��������
	int intleft, intright;//int�͵��������Ե�ֵ
	char *charleft, *charright;//char���������Ե�ֵ
	float floatleft, floatright;//float�͵��������Ե�ֵ
	AttrType attrtype;

	//�жϱ��Ƿ����
	if (_access(relName, 0) == -1)
		return TABLE_NOT_EXIST;

	//�����ݱ�,ϵͳ��ϵͳ���ļ�
	rm_data.bOpen = false;
	rc = RM_OpenFile(relName, &rm_data);
	if (rc != SUCCESS)
		return rc;
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS)
		return rc;

	//�õ�ϵͳ����Ϣ
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_table, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//�����õ�������Ը���
	while (GetNextRec(&FileScan, &rectab) == SUCCESS) {
		if (strcmp(relName, rectab.pData) == 0) {
			memcpy(&attrcount, rectab.pData + 21, sizeof(int));
			delete[] rectab.pData;
			break;
		}
		delete[] rectab.pData;
	}

	//�õ�ϵͳ����Ϣ
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_column, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	column = new SysColumn[attrcount]();
	ctmp = column;
	set<string> s;
	while (GetNextRec(&FileScan, &reccol) == SUCCESS) {
		if (strcmp(relName, reccol.pData) == 0) {//��ȡattrcount��Ԫ���¼
			for (int i = 0; i < attrcount; i++) {
				memcpy(ctmp->tabName, reccol.pData, 21);
				memcpy(ctmp->attrName, reccol.pData + 21, 21);
				s.insert(ctmp->attrName);
				memcpy(&(ctmp->attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(ctmp->attrLength), reccol.pData + 42 + sizeof(AttrType), sizeof(int));
				memcpy(&(ctmp->attrOffset), reccol.pData + 42 + sizeof(int) + sizeof(AttrType), sizeof(int));
				ctmp++;
				delete[] reccol.pData;
				rc = GetNextRec(&FileScan, &reccol);
				if (rc != SUCCESS)
					break;
			}
			break;
		}
		delete[] reccol.pData;
	}
	//�ж����Ƿ����
	for (int i = 0; i < nConditions; i++) {
		if (conditions[i].bLhsIsAttr) {
			if (s.find(conditions[i].lhsAttr.attrName) == s.end())
				return TABLE_COLUMN_ERROR;
		}
		if (conditions[i].bRhsIsAttr) {
			if (s.find(conditions[i].rhsAttr.attrName) == s.end())
				return TABLE_COLUMN_ERROR;
		}
	}

	//�õ�ϵͳ����Ϣ
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_data, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	while (GetNextRec(&FileScan, &recdata) == SUCCESS) {
		for (i = 0, torf = 1, contmp = conditions; i < nConditions; i++, contmp++) {//����conditions���������ж�
			ctmpleft = ctmpright = column;
			//��������ֵ
			if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 0) {
				//����ϵͳ���ļ�������conditions�ҵ�����������Ӧ������
				for (int j = 0; j < attrcount; j++) {
					if (contmp->lhsAttr.relName == NULL) {//���δָ������ʱ����ȱʡ����ΪrelName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//���ݱ����������ҵ���Ӧ����
						break;
					}
					ctmpleft++;
				}
				switch (ctmpleft->attrType) {//�ж��������Ͳ�����Ӧֵ����
				case ints:
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, contmp->rhsValue.data, sizeof(int));
					break;
				case chars:
					attrtype = chars;
					charleft = new char[ctmpleft->attrLength];
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpleft->attrLength);
					charright = new char[ctmpleft->attrLength];
					memcpy(charright, contmp->rhsValue.data, ctmpleft->attrLength);
					break;
				case floats:
					attrtype = floats;
					memcpy(&floatleft, recdata.pData + ctmpleft->attrOffset, sizeof(float));
					memcpy(&floatright, contmp->rhsValue.data, sizeof(float));
					break;
				}
			}
			//��������ֵ
			if (contmp->bLhsIsAttr == 0 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {
					if (contmp->rhsAttr.relName == NULL) {//���δָ������ʱ����ȱʡ����ΪrelName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//�ҵ���Ӧ����
						break;
					}
					ctmpright++;
				}
				switch (ctmpright->attrType) {//�ж��������Ͳ�����Ӧֵ����
				case ints:
					attrtype = ints;
					memcpy(&intleft, contmp->lhsValue.data, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
					break;
				case chars:
					attrtype = chars;
					charleft = new char[ctmpright->attrLength];
					memcpy(charleft, contmp->lhsValue.data, ctmpright->attrLength);
					charright = new char[ctmpright->attrLength];
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
					break;
				case floats:
					attrtype = floats;
					memcpy(&floatleft, contmp->lhsValue.data, sizeof(float));
					memcpy(&floatright, recdata.pData + ctmpright->attrOffset, sizeof(float));
					break;
				}
			}
			//���Ҿ�����
			else  if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {
					if (contmp->lhsAttr.relName == NULL) {//���δָ������ʱ����ȱʡ����ΪrelName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//�ҵ���Ӧ����
						break;
					}
					ctmpleft++;
				}
				for (int j = 0; j < attrcount; j++) {
					if (contmp->rhsAttr.relName == NULL) {//���δָ������ʱ����ȱʡ����ΪrelName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//�ҵ���Ӧ����
						break;
					}
					ctmpright++;
				}
				switch (ctmpright->attrType) {//�ж��������Ͳ�����Ӧֵ����
				case ints:
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
					break;
				case chars:
					attrtype = chars;
					charleft = new char[ctmpright->attrLength];
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpright->attrLength);
					charright = new char[ctmpright->attrLength];
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
					break;
				case floats:
					attrtype = floats;
					memcpy(&floatleft, recdata.pData + ctmpleft->attrOffset, sizeof(float));
					memcpy(&floatright, recdata.pData + ctmpright->attrOffset, sizeof(float));
					break;
				}
			}
			if (attrtype == ints) {
				if ((intleft == intright && contmp->op == EQual) ||
					(intleft > intright && contmp->op == GreatT) ||
					(intleft >= intright && contmp->op == GEqual) ||
					(intleft < intright && contmp->op == LessT) ||
					(intleft <= intright && contmp->op == LEqual) ||
					(intleft != intright && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
			}
			else if (attrtype == chars) {
				if ((strcmp(charleft, charright) == 0 && contmp->op == EQual) ||
					(strcmp(charleft, charright) > 0 && contmp->op == GreatT) ||
					((strcmp(charleft, charright) > 0 || strcmp(charleft, charright) == 0) && contmp->op == GEqual) ||
					(strcmp(charleft, charright) < 0 && contmp->op == LessT) ||
					((strcmp(charleft, charright) < 0 || strcmp(charleft, charright) == 0) && contmp->op == LEqual) ||
					(strcmp(charleft, charright) != 0 && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
				delete[] charleft;
				delete[] charright;
			}
			else if (attrtype == floats) {
				if ((floatleft == floatright && contmp->op == EQual) ||
					(floatleft > floatright && contmp->op == GreatT) ||
					(floatleft >= floatright && contmp->op == GEqual) ||
					(floatleft < floatright && contmp->op == LessT) ||
					(floatleft <= floatright && contmp->op == LEqual) ||
					(floatleft != floatright && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
			}
			else
				torf &= 0;
		}

		if (torf == 1) {
			DeleteRec(&rm_data, &(recdata.rid));//ɾ����¼
		}
		delete[] recdata.pData;
	}
	delete[] column;

	//�ر��ļ����
	rc = RM_CloseFile(&rm_table);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_column);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_data);
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

RC Update(char *relName, char *attrName, Value *Value, int nConditions, Condition *conditions) {
	RM_FileHandle rm_data, rm_table, rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record recdata, rectab, reccol;
	SysColumn *column, *ctmp, *cupdate, *ctmpleft, *ctmpright;
	Condition *contmp;
	int i, torf;//�Ƿ�����ɾ�������ı�־
	int attrcount;//��¼��������
	int intleft, intright;//int�͵��������Ե�ֵ
	char *charleft, *charright;//char�͵��������Ե�ֵ
	float floatleft, floatright;//float�͵��������Ե�ֵ
	AttrType attrtype;

	//���������
	if (_access(relName, 0) == -1)
		return TABLE_NOT_EXIST;
	//�����ݱ�,ϵͳ��ϵͳ���ļ�
	rm_data.bOpen = false;
	rc = RM_OpenFile(relName, &rm_data);
	if (rc != SUCCESS)
		return rc;
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS)
		return rc;

	//��ȡϵͳ����Ϣ
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_table, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//���ұ���ΪrelName��Ӧ��ϵͳ���еļ�¼
	while (GetNextRec(&FileScan, &rectab) == SUCCESS) {
		if (strcmp(relName, rectab.pData) == 0) {
			memcpy(&attrcount, rectab.pData + 21, sizeof(int));
			delete[] rectab.pData;
			break;
		}
		delete[] rectab.pData;
	}

	//��ȡϵͳ����Ϣ
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_column, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	column = new SysColumn[attrcount]();
	ctmp = column;
	set<string> s;
	while (GetNextRec(&FileScan, &reccol) == SUCCESS) {//ɨ���¼
		if (strcmp(relName, reccol.pData) == 0) {//��ȡattrcount����¼
			for (int i = 0; i < attrcount; i++) {
				memcpy(ctmp->tabName, reccol.pData, 21);
				memcpy(ctmp->attrName, reccol.pData + 21, 21);
				s.insert(ctmp->attrName);
				memcpy(&(ctmp->attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(ctmp->attrLength), reccol.pData + 42 + sizeof(AttrType), sizeof(int));
				memcpy(&(ctmp->attrOffset), reccol.pData + 42 + sizeof(int) + sizeof(AttrType), sizeof(int));
				if ((strcmp(relName, ctmp->tabName) == 0) && (strcmp(attrName, ctmp->attrName) == 0)) {
					cupdate = ctmp;//�ҵ�Ҫ���µ����� ��Ӧ������
				}
				delete[] reccol.pData;
				rc = GetNextRec(&FileScan, &reccol);
				if (rc != SUCCESS)
					break;
				ctmp++;
			}
			break;
		}
		delete[] reccol.pData;
	}
	//�в�����
	if (s.find(attrName) == s.end())
		return TABLE_COLUMN_ERROR;

	//��ȡϵͳ����Ϣ
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_data, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//����relName��Ӧ�����ݱ��еļ�¼
	while (GetNextRec(&FileScan, &recdata) == SUCCESS) {
		//����ϵͳ���ļ�������conditions�ҵ�����������Ӧ������
		for (i = 0, torf = 1, contmp = conditions; i < nConditions; i++, contmp++) {
			ctmpleft = ctmpright = column;
			//��������ֵ
			if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 0) {
				for (int j = 0; j < attrcount; j++) {
					if (contmp->lhsAttr.relName == NULL) {//���δָ������ʱ����ȱʡ����ΪrelName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//����������������ϣ����ҵ���Ӧ����
						break;
					}
					ctmpleft++;
				}
				//�ж����Ե�����
				if (ctmpleft->attrType == ints) {//�������������ints
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, contmp->rhsValue.data, sizeof(int));
				}
				else if (ctmpleft->attrType == chars) {//�������������chars
					attrtype = chars;
					charleft = new char[ctmpleft->attrLength];
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpleft->attrLength);
					charright = new char[ctmpleft->attrLength];
					memcpy(charright, contmp->rhsValue.data, ctmpleft->attrLength);
				}
				else if (ctmpleft->attrType == floats) {//�����������floats
					attrtype = floats;
					memcpy(&floatleft, recdata.pData + ctmpleft->attrOffset, sizeof(float));
					memcpy(&floatright, contmp->rhsValue.data, sizeof(float));
				}
				else
					torf &= 0;
			}
			//��������ֵ
			else  if (contmp->bLhsIsAttr == 0 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {
					if (contmp->rhsAttr.relName == NULL) {//���δָ������ʱ����ȱʡ����ΪrelName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//����������������ϣ����ҵ���Ӧ����
						break;
					}
					ctmpright++;
				}
				if (ctmpright->attrType == ints) {//�������������ints
					attrtype = ints;
					memcpy(&intleft, contmp->lhsValue.data, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
				}
				else if (ctmpright->attrType == chars) {//�������������chars
					attrtype = chars;
					charleft = new char[ctmpright->attrLength];
					memcpy(charleft, contmp->lhsValue.data, ctmpright->attrLength);
					charright = new char[ctmpright->attrLength];
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
				}
				else if (ctmpright->attrType == floats) {//�����������floats
					attrtype = floats;
					memcpy(&floatleft, contmp->lhsValue.data, sizeof(float));
					memcpy(&floatright, recdata.pData + ctmpright->attrOffset, sizeof(float));
				}
				else
					torf &= 0;
			}
			//������������
			else  if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {
					if (contmp->lhsAttr.relName == NULL) {//���δָ������ʱ����ȱʡ����ΪrelName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//����������������ϣ����ҵ���Ӧ����
						break;
					}
					ctmpleft++;
				}
				for (int j = 0; j < attrcount; j++) {
					if (contmp->rhsAttr.relName == NULL) {//���δָ������ʱ����ȱʡ����ΪrelName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//����������������ϣ����ҵ���Ӧ����
						break;
					}
					ctmpright++;
				}
				if (ctmpright->attrType == ints && ctmpleft->attrType == ints) {//�������������ints
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
				}
				else if (ctmpright->attrType == chars && ctmpleft->attrType == chars) {//�������������chars
					attrtype = chars;
					charleft = new char[ctmpright->attrLength];
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpright->attrLength);
					charright = new char[ctmpright->attrLength];
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
				}
				else if (ctmpright->attrType == floats && ctmpleft->attrType == floats) {//�������������floats
					attrtype = floats;
					memcpy(&floatleft, recdata.pData + ctmpleft->attrOffset, sizeof(float));
					memcpy(&floatright, recdata.pData + ctmpright->attrOffset, sizeof(float));
				}
				else
					torf &= 0;
			}
			if (attrtype == ints) {
				if ((intleft == intright && contmp->op == EQual) ||
					(intleft > intright && contmp->op == GreatT) ||
					(intleft >= intright && contmp->op == GEqual) ||
					(intleft < intright && contmp->op == LessT) ||
					(intleft <= intright && contmp->op == LEqual) ||
					(intleft != intright && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
			}
			else if (attrtype == chars) {
				if ((strcmp(charleft, charright) == 0 && contmp->op == EQual) ||
					(strcmp(charleft, charright) > 0 && contmp->op == GreatT) ||
					((strcmp(charleft, charright) > 0 || strcmp(charleft, charright) == 0) && contmp->op == GEqual) ||
					(strcmp(charleft, charright) < 0 && contmp->op == LessT) ||
					((strcmp(charleft, charright) < 0 || strcmp(charleft, charright) == 0) && contmp->op == LEqual) ||
					(strcmp(charleft, charright) != 0 && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
				delete[] charleft;
				delete[] charright;
			}
			else if (attrtype == floats) {
				if ((floatleft == floatright && contmp->op == EQual) ||
					(floatleft > floatright && contmp->op == GreatT) ||
					(floatleft >= floatright && contmp->op == GEqual) ||
					(floatleft < floatright && contmp->op == LessT) ||
					(floatleft <= floatright && contmp->op == LEqual) ||
					(floatleft != floatright && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
			}
			else
				torf &= 0;
		}
		if (torf == 1) {//��������,���и��²���
			memcpy(recdata.pData + cupdate->attrOffset, Value->data, cupdate->attrLength);
			UpdateRec(&rm_data, &recdata);
		}
		delete[] recdata.pData;
	}

	delete[] column;

	//�ر��ļ����
	rc = RM_CloseFile(&rm_table);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_column);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_data);
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

bool CanButtonClick() {//��Ҫ����ʵ��
	return isOpened;
}

void toLowerString(std::string & s)
{
	for (unsigned i = 0; i < s.length(); i++)
	{
		s[i] = tolower(s[i]);
	}
}

void showMsg(CEditArea * editArea, char * msg)
{
	int row_num = 1;
	char **messages = new char*[row_num];
	messages[0] = msg;
	editArea->ShowMessage(row_num, messages);
	delete[] messages;
}

void showSelectResult(SelResult & res, CEditArea * editArea)
{
	/*���ִ�н��*/
	int col_num = res.col_num;
	int row_num = res.row_num;
	char ** fields = new char*[col_num];
	for (int i = 0; i < col_num; i++) {
		fields[i] = new char[20];
		memcpy(fields[i], res.fields[i], 20);
	}
	char *** rows = new char**[row_num];
	for (int i = 0; i < row_num; i++) {
		rows[i] = new char*[col_num];
		for (int j = 0; j < col_num; j++) {
			AttrType type = res.type[j];
			int intNum;
			float floatNum;
			int y;
			switch (type)
			{
			case AttrType::ints:
				memcpy(&intNum, res.res[i][0] + res.offset[j], sizeof(int));
				rows[i][j] = new char[32];
				sprintf_s(rows[i][j], 32, "%d", intNum);
				break;
			case AttrType::floats:
				memcpy(&floatNum, res.res[i][0] + res.offset[j], sizeof(float));
				y = res.length[j];
				rows[i][j] = new char[32];
				sprintf_s(rows[i][j], 32, "%f", floatNum);
				break;
			case AttrType::chars:
				rows[i][j] = new char[res.length[j]];
				memcpy(rows[i][j], res.res[i][0] + res.offset[j], res.length[j]);
				break;
			default:
				break;
			}

		}
	}
	editArea->ShowSelResult(col_num, row_num, fields, rows);

	/*�ͷŶ�̬����Ŀռ�*/
	for (int i = 0; i < col_num; i++) {
		delete[] fields[i];
	}
	delete[] fields;
	for (int i = 0; i < row_num; i++) {
		for (int j = 0; j < col_num; j++) {
			delete[] rows[i][j];
		}
		delete[] rows[i];
	}
	delete[] rows;
	Destory_Result(&res);
}
