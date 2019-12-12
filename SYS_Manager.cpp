#include "stdafx.h"
#include "EditArea.h"
#include "SYS_Manager.h"
#include "QU_Manager.h"
#include <iostream>

void ExecuteAndMessage(char * sql, CEditArea* editArea) {//����ִ�е���������ڽ�������ʾִ�н�����˺������޸�
	std::string s_sql = sql;
	if (s_sql.find("select") == 0) {
		SelResult res;
		Init_Result(&res);
		//rc = Query(sql,&res);
		//����ѯ�������һ�£����������������ʽ
		//����editArea->ShowSelResult(col_num,row_num,fields,rows);
		int col_num = 5;
		int row_num = 3;
		char ** fields = new char *[5];
		for (int i = 0; i < col_num; i++) {
			fields[i] = new char[20];
			memset(fields[i], 0, 20);
			fields[i][0] = 'f';
			fields[i][1] = i + '0';
		}
		char *** rows = new char**[row_num];
		for (int i = 0; i < row_num; i++) {
			rows[i] = new char*[col_num];
			for (int j = 0; j < col_num; j++) {
				rows[i][j] = new char[20];
				memset(rows[i][j], 0, 20);
				rows[i][j][0] = 'r';
				rows[i][j][1] = i + '0';
				rows[i][j][2] = '+';
				rows[i][j][3] = j + '0';
			}
		}
		editArea->ShowSelResult(col_num, row_num, fields, rows);
		for (int i = 0; i < 5; i++) {
			delete[] fields[i];
		}
		delete[] fields;
		Destory_Result(&res);
		return;
	}
	RC rc = execute(sql);
	int row_num = 0;
	char**messages;
	switch (rc) {
	case SUCCESS:
		row_num = 1;
		messages = new char*[row_num];
		messages[0] = "�����ɹ�";
		editArea->ShowMessage(row_num, messages);
		delete[] messages;
		break;
	case SQL_SYNTAX:
		row_num = 1;
		messages = new char*[row_num];
		messages[0] = "���﷨����";
		editArea->ShowMessage(row_num, messages);
		delete[] messages;
		break;
	default:
		row_num = 1;
		messages = new char*[row_num];
		messages[0] = "����δʵ��";
		editArea->ShowMessage(row_num, messages);
		delete[] messages;
		break;
	}
}

RC execute(char * sql) {
	sqlstr *sql_str = NULL;
	RC rc;
	sql_str = get_sqlstr();
	rc = parse(sql, sql_str);//ֻ�����ַ��ؽ��SUCCESS��SQL_SYNTAX

	if (rc == SUCCESS)
	{
		int i = 0;
		switch (sql_str->flag)
		{
			//case 1:
			////�ж�SQL���Ϊselect���

			//break;

		case 2:
			//�ж�SQL���Ϊinsert���

		case 3:
			//�ж�SQL���Ϊupdate���
			break;

		case 4:
			//�ж�SQL���Ϊdelete���
			break;

		case 5:
			//�ж�SQL���ΪcreateTable���
			CreateTable(sql_str->sstr.cret.relName, sql_str->sstr.cret.attrCount, sql_str->sstr.cret.attributes);
			break;

		case 6:
			//�ж�SQL���ΪdropTable���
			DropTable(sql_str->sstr.drt.relName);
			break;

		case 7:
			//�ж�SQL���ΪcreateIndex���
			break;

		case 8:
			//�ж�SQL���ΪdropIndex���
			break;

		case 9:
			//�ж�Ϊhelp��䣬���Ը���������ʾ
			break;

		case 10:
			//�ж�Ϊexit��䣬�����ɴ˽����˳�����
			break;
		}
	}
	else {
		AfxMessageBox(sql_str->sstr.errors);//���������sql���ʷ�����������Ϣ
		return rc;
	}
}

RC CreateDB(char *dbpath, char *dbname) {
	//���õ�ǰĿ¼Ϊdbpath������dbPath������dbName��
	SetCurrentDirectory(dbpath);
	RC rc;
	//����ϵͳ���ļ���ϵͳ���ļ�
	rc = RM_CreateFile("SYSTABLES", sizeof(SysTable));
	if (rc != SUCCESS)
		return rc;
	rc = RM_CreateFile("SYSCOLUMNS", sizeof(SysColumn));
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

RC DropDB(char *dbname) {
	CFileFind tempFind;
	char sTempFileFind[200];
	sprintf_s(sTempFileFind, "%s\\*.*", dbname);
	BOOL IsFinded = tempFind.FindFile(sTempFileFind);
	while (IsFinded)
	{
		IsFinded = tempFind.FindNextFile();
		if (!tempFind.IsDots())
		{
			char sFoundFileName[200];
			strcpy_s(sFoundFileName, tempFind.GetFileName().GetBuffer(200));
			if (tempFind.IsDirectory())
			{
				char sTempDir[200];
				sprintf_s(sTempDir, "%s\\%s", dbname, sFoundFileName);
				DropTable(sTempDir);
			}
			else
			{
				char sTempFileName[200];
				sprintf_s(sTempFileName, "%s\\%s", dbname, sFoundFileName);
				DeleteFile(sTempFileName);
			}
		}
	}
	tempFind.Close();
	if (!RemoveDirectory(dbname))
	{
		return FAIL;
	}
	return SUCCESS;
}

RC OpenDB(char *dbname) {
	return SUCCESS;
}

RC CloseDB() {
	return SUCCESS;
}

/*
���������
1.ϵͳ���ļ���ϵͳ���ļ��ĳ�ʼ��
2.�������ݱ��ļ�
*/
//done
RC CreateTable(char *relName, int attrCount, AttrInfo *attributes) {
	RC rc;
	char  *pData;
	RM_FileHandle *rm_table, *rm_column;
	RID *rid;
	int recordsize;//���ݱ��ÿ����¼�Ĵ�С
	AttrInfo * attrtmp = attributes;

	//��ϵͳ���ļ���ϵͳ���ļ�
	rm_table = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	rm_table->bOpen = false;
	rc = RM_OpenFile("SYSTABLES", rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	rm_column->bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", rm_column);
	if (rc != SUCCESS)
		return rc;
	//��ϵͳ���������Ϣ
	pData = (char *)malloc(sizeof(SysTable));
	memcpy(pData, relName, 21);//������
	memcpy(pData + 21, &attrCount, sizeof(int));//�����������
	rid = (RID *)malloc(sizeof(RID));
	rid->bValid = false;
	rc = InsertRec(rm_table, pData, rid);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = RM_CloseFile(rm_table);
	if (rc != SUCCESS) {
		return rc;
	}
	free(rm_table);
	free(pData);
	free(rid);//�ͷ�������ڴ�ռ�

	//��ϵͳ����ѭ�������Ϣ һ�����а�����������У�����Ҫѭ��
	for (int i = 0, offset = 0; i < attrCount; i++, attrtmp++) {
		pData = (char *)malloc(sizeof(SysColumn));
		memcpy(pData, relName, 21);
		memcpy(pData + 21, attrtmp->attrName, 21);
		memcpy(pData + 42, &(attrtmp->attrType), sizeof(AttrType));
		memcpy(pData + 42 + sizeof(AttrType), &(attrtmp->attrLength), sizeof(int));
		memcpy(pData + 42 + sizeof(int) + sizeof(AttrType), &offset, sizeof(int));
		memcpy(pData + 42 + 2 * sizeof(int) + sizeof(AttrType), "0", sizeof(char));
		rid = (RID *)malloc(sizeof(RID));
		rid->bValid = false;
		rc = InsertRec(rm_column, pData, rid);
		if (rc != SUCCESS) {
			return rc;
		}
		free(pData);
		free(rid);
		offset += attrtmp->attrLength;
	}
	rc = RM_CloseFile(rm_column);
	if (rc != SUCCESS) {
		return rc;
	}
	free(rm_column);
	//�������ݱ�
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

//ɾ�����
//1.�ѱ���ΪrelName�ı�ɾ�� 
//2.�����ݿ��иñ��Ӧ����Ϣ�ÿ�
RC DropTable(char *relName) {
	CFile tmp;
	RM_FileHandle *rm_table, *rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record rectab, reccol;
	int attrcount;//��ʱ �������������Գ��ȣ�����ƫ��

	//ɾ�����ݱ��ļ�
	tmp.Remove((LPCTSTR)relName);

	//��ϵͳ���ϵͳ���ж�Ӧ�����ؼ�¼ɾ��
	//��ϵͳ��ϵͳ���ļ�
	rm_table = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	rm_table->bOpen = false;
	rc = RM_OpenFile("SYSTABLES", rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	rm_column->bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", rm_column);
	if (rc != SUCCESS)
		return rc;

	//ͨ��getdata������ȡϵͳ����Ϣ,�õ�����Ϣ������rectab��
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, rm_table, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//ѭ�����ұ���ΪrelName��Ӧ��ϵͳ���еļ�¼,������¼��Ϣ������rectab��
	while (GetNextRec(&FileScan, &rectab) == SUCCESS) {
		if (strcmp(relName, rectab.pData) == 0) {
			memcpy(&attrcount, rectab.pData + 21, sizeof(int));
			DeleteRec(rm_table, &(rectab.rid));
			break;
		}
	}
	//ͨ��getdata������ȡϵͳ����Ϣ,�õ�����Ϣ������reccol��
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, rm_column, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//ѭ�����ұ���ΪrelName��Ӧ��ϵͳ���еļ�¼,������¼��Ϣ������rectab��
	//����֮ǰ��ȡ��ϵͳ������Ϣ����ȡ������Ϣ�����������ctmp��
	for (int i = 0; i < attrcount && GetNextRec(&FileScan, &reccol) == SUCCESS;) {
		if (strcmp(relName, reccol.pData) == 0) {//�ҵ�����ΪrelName�ĵ�һ����¼�����ζ�ȡattrcount����¼
			DeleteRec(rm_column, &(reccol.rid));
			i++;
		}
	}

	//�ر��ļ����
	rc = RM_CloseFile(rm_table);
	if (rc != SUCCESS)
		return rc;
	free(rm_table);
	rc = RM_CloseFile(rm_column);
	if (rc != SUCCESS)
		return rc;
	free(rm_column);
	return SUCCESS;
}

bool CanButtonClick() {//��Ҫ����ʵ��
	//�����ǰ�����ݿ��Ѿ���
	return true;
	//�����ǰû�����ݿ��
	//return false;
}
