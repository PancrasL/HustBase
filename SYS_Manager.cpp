#include "stdafx.h"
#include "EditArea.h"
#include "SYS_Manager.h"
#include "QU_Manager.h"
#include "HustBase.h"
#include "HustBaseDoc.h"
#include <iostream>
#include <vector>
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
	default:
		showMsg(editArea, "����δʵ��");
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
	return SUCCESS;

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
	ret = RemoveDirectory(dbname);
	return ret ? SUCCESS : FAIL;
}

RC OpenDB(char *dbname) {
	SetCurrentDirectory(dbname);
	int i, j;
	if ((i = _access("SYSTABLES", 0)) != 0 || (j = _access("SYSCOLUMNS", 0)) != 0) {
		return FAIL;
	}
	CHustBaseApp::pathvalue = true;
	CHustBaseDoc *pDoc;
	pDoc = CHustBaseDoc::GetDoc();
	pDoc->m_pTreeView->PopulateTree();
	isOpened = true;
	return SUCCESS;
}

RC CloseDB() {
	isOpened = false;
	return SUCCESS;
}

/*
���������
1.��ʼ��ϵͳ���ļ���ϵͳ���ļ�
2.�������ݱ��ļ�
*/
RC CreateTable(char *relName, int attrCount, AttrInfo *attributes) {
	RC rc;
	char  *pData;
	RM_FileHandle *rm_table, *rm_column;
	RID *rid;
	int recordsize;//���ݱ��ÿ����¼�Ĵ�С
	AttrInfo * attrtmp = attributes;

	/*��ϵͳ���ļ�������*/
	rm_table = new RM_FileHandle();
	rm_table->bOpen = false;
	rc = RM_OpenFile("SYSTABLES", rm_table);
	if (rc != SUCCESS)
		return rc;

	pData = new char[sizeof(SysTable)];
	memcpy(pData, relName, 21);
	memcpy(pData + 21, &attrCount, sizeof(int));
	rid = new RID();
	rid->bValid = false;
	rc = InsertRec(rm_table, pData, rid);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = RM_CloseFile(rm_table);
	if (rc != SUCCESS) {
		return rc;
	}
	//�ͷ�������ڴ�ռ�
	delete rm_table;
	delete[] pData;
	delete rid;

	/*�����ļ�������*/
	rm_column = new RM_FileHandle();
	rm_column->bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", rm_column);
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
		rid = new RID();
		rid->bValid = false;
		rc = InsertRec(rm_column, pData, rid);
		if (rc != SUCCESS) {
			return rc;
		}
		delete[] pData;
		delete rid;
		offset += attrtmp->attrLength;
	}
	rc = RM_CloseFile(rm_column);
	if (rc != SUCCESS) {
		return rc;
	}
	delete rm_column;

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

/*
ɾ�����
1.�ѱ���ΪrelName�ı�ɾ��
2.�����ݿ��иñ��Ӧ����Ϣ�ÿ�
*/
RC DropTable(char *relName) {
	CFile tmp;
	RM_FileHandle *rm_table, *rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record rectab, reccol;
	int attrcount;//��ʱ �������������Գ��ȣ�����ƫ��

	/*ɾ�����ݱ��ļ�*/
	tmp.Remove((LPCTSTR)relName);

	/*��ϵͳ���ϵͳ���ж�Ӧ�����ؼ�¼ɾ��*/
	//�򿪱��ļ������ļ�
	rm_table = new RM_FileHandle();
	rm_table->bOpen = false;
	rc = RM_OpenFile("SYSTABLES", rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column = new RM_FileHandle();
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
	delete rm_table;
	rc = RM_CloseFile(rm_column);
	if (rc != SUCCESS)
		return rc;
	delete rm_column;
	return SUCCESS;
}

RC CreateIndex(char *indexName, char *relName, char *attrName) {
	return SUCCESS;
}

RC DropIndex(char *indexName) {
	return SUCCESS;
}

RC Insert(char *relName, int nValues, Value *values) {
	RM_FileHandle *rm_data, *rm_table, *rm_column;
	char *value;//��ȡ���ݱ���Ϣ
	RID *rid;
	RC rc;
	SysColumn *column, *ctmp;
	RM_FileScan FileScan;
	RM_Record rectab, reccol;
	int attrcount;//��ʱ �������������Գ��ȣ�����ƫ��

	//�����ݱ�,ϵͳ��ϵͳ���ļ�
	rm_data = new RM_FileHandle();
	rm_data->bOpen = false;
	rc = RM_OpenFile(relName, rm_data);
	if (rc != SUCCESS) {
		AfxMessageBox("�����ݱ��ļ�ʧ��");
		return rc;
	}
	rm_table = new RM_FileHandle();
	rm_table->bOpen = false;
	rc = RM_OpenFile("SYSTABLES", rm_table);
	if (rc != SUCCESS) {
		AfxMessageBox("��ϵͳ���ļ�ʧ��");
		return rc;
	}
	rm_column = new RM_FileHandle();
	rm_column->bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", rm_column);
	if (rc != SUCCESS) {
		AfxMessageBox("��ϵͳ���ļ�ʧ��");
		return rc;
	}
	//ͨ��getdata������ȡϵͳ����Ϣ,�õ�����Ϣ������rectab��
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, rm_table, 0, NULL);
	if (rc != SUCCESS) {
		AfxMessageBox("��ʼ���ļ�ɨ��ʧ��");
		return rc;
	}
	//ѭ�����ұ���ΪrelName��Ӧ��ϵͳ���еļ�¼,������¼��Ϣ������rectab��
	while (GetNextRec(&FileScan, &rectab) == SUCCESS) {
		if (strcmp(relName, rectab.pData) == 0) {
			memcpy(&attrcount, rectab.pData + 21, sizeof(int));
			break;
		}
	}

	//�ж��Ƿ�Ϊ��ȫ���룬������ǣ�����fail
	if (attrcount != nValues) {
		AfxMessageBox("����ȫ��¼���룡");
		return FAIL;
	}
	//ͨ��getdata������ȡϵͳ����Ϣ,�õ�����Ϣ������reccol��
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, rm_column, 0, NULL);
	if (rc != SUCCESS) {
		AfxMessageBox("��ʼ���������ļ�ɨ��ʧ��");
		return rc;
	}
	//ѭ�����ұ���ΪrelName��Ӧ��ϵͳ���еļ�¼,������¼��Ϣ������rectab��
	//����֮ǰ��ȡ��ϵͳ������Ϣ����ȡ������Ϣ�����������ctmp��
	column = new SysColumn[attrcount]();
	ctmp = column;
	while (GetNextRec(&FileScan, &reccol) == SUCCESS) {
		if (strcmp(relName, reccol.pData) == 0) {//�ҵ�����ΪrelName�ĵ�һ����¼�����ζ�ȡattrcount����¼
			for (int i = 0; i < attrcount; i++, ctmp++) {
				memcpy(ctmp->tabName, reccol.pData, 21);
				memcpy(ctmp->attrName, reccol.pData + 21, 21);
				memcpy(&(ctmp->attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(ctmp->attrLength), reccol.pData + 42 + sizeof(AttrType), sizeof(int));
				memcpy(&(ctmp->attrOffset), reccol.pData + 42 + sizeof(int) + sizeof(AttrType), sizeof(int));
				rc = GetNextRec(&FileScan, &reccol);
				if (rc != SUCCESS)
					break;
			}
			break;
		}
	}
	ctmp = column;

	//�����ݱ���ѭ�������¼
	value = new char[rm_data->fileSubHeader->recordSize];
	values = values + nValues - 1;
	for (int i = 0; i < nValues; i++, values--, ctmp++) {
		memcpy(value + ctmp->attrOffset, values->data, ctmp->attrLength);
	}
	rid = new RID();
	rid->bValid = false;
	InsertRec(rm_data, value, rid);
	delete rid;
	delete[] value;

	delete[] column;

	//�ر��ļ����
	rc = RM_CloseFile(rm_data);
	if (rc != SUCCESS) {
		AfxMessageBox("�ر����ݱ�ʧ��");
		return rc;
	}
	delete rm_data;
	rc = RM_CloseFile(rm_table);
	if (rc != SUCCESS) {
		AfxMessageBox("�ر�ϵͳ��ʧ��");
		return rc;
	}
	delete rm_table;
	rc = RM_CloseFile(rm_column);
	if (rc != SUCCESS) {
		AfxMessageBox("�ر�ϵͳ��ʧ��");
		return rc;
	}
	delete rm_column;
	return SUCCESS;
}

RC Delete(char *relName, int nConditions, Condition *conditions) {
	RM_FileHandle *rm_data, *rm_table, *rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record recdata, rectab, reccol;
	SysColumn *column, *ctmp, *ctmpleft, *ctmpright;
	Condition *contmp;
	int i, torf;//�Ƿ����ɾ������
	int attrcount;//��ʱ ��������
	int intleft, intright;
	char *charleft, *charright;
	float floatleft, floatright;//��ʱ ���Ե�ֵ
	AttrType attrtype;

	//�����ݱ�,ϵͳ��ϵͳ���ļ�
	rm_data = new RM_FileHandle();
	rm_data->bOpen = false;
	rc = RM_OpenFile(relName, rm_data);
	if (rc != SUCCESS)
		return rc;
	rm_table = new RM_FileHandle();
	rm_table->bOpen = false;
	rc = RM_OpenFile("SYSTABLES", rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column = new RM_FileHandle();
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
	column = new SysColumn[attrcount]();
	ctmp = column;
	while (GetNextRec(&FileScan, &reccol) == SUCCESS) {
		if (strcmp(relName, reccol.pData) == 0) {//�ҵ�����ΪrelName�ĵ�һ����¼�����ζ�ȡattrcount����¼
			for (int i = 0; i < attrcount; i++) {
				memcpy(ctmp->tabName, reccol.pData, 21);
				memcpy(ctmp->attrName, reccol.pData + 21, 21);
				memcpy(&(ctmp->attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(ctmp->attrLength), reccol.pData + 42 + sizeof(AttrType), sizeof(int));
				memcpy(&(ctmp->attrOffset), reccol.pData + 42 + sizeof(int) + sizeof(AttrType), sizeof(int));
				ctmp++;
				rc = GetNextRec(&FileScan, &reccol);
				if (rc != SUCCESS)
					break;
			}
			break;
		}
	}

	//ͨ��getdata������ȡϵͳ����Ϣ,�õ�����Ϣ������recdata��
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, rm_data, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//ѭ�����ұ���ΪrelName��Ӧ�����ݱ��еļ�¼,������¼��Ϣ������recdata��
	while (GetNextRec(&FileScan, &recdata) == SUCCESS) {	//ȡ��¼���ж�
		for (i = 0, torf = 1, contmp = conditions; i < nConditions; i++, contmp++) {//conditions������һ�ж�
			ctmpleft = ctmpright = column;//ÿ��ѭ����Ҫ����������ϵͳ���ļ����ҵ�����������Ӧ������
			//��������ֵ
			if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 0) {
				for (int j = 0; j < attrcount; j++) {//attrcount��������һ�ж�
					if (contmp->lhsAttr.relName == NULL) {//��������δָ������ʱ��Ĭ��ΪrelName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//���ݱ����������ҵ���Ӧ����
						break;
					}
					ctmpleft++;
				}
				//��conditions��ĳһ�����������ж�
				switch (ctmpleft->attrType) {//�ж����Ե�����
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
				for (int j = 0; j < attrcount; j++) {//attrcount��������һ�ж�
					if (contmp->rhsAttr.relName == NULL) {//��������δָ������ʱ��Ĭ��ΪrelName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//���ݱ����������ҵ���Ӧ����
						break;
					}
					ctmpright++;
				}
				//��conditions��ĳһ�����������ж�
				switch (ctmpright->attrType) {
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
				for (int j = 0; j < attrcount; j++) {//attrcount��������һ�ж�
					if (contmp->lhsAttr.relName == NULL) {//��������δָ������ʱ��Ĭ��ΪrelName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//���ݱ����������ҵ���Ӧ����
						break;
					}
					ctmpleft++;
				}
				for (int j = 0; j < attrcount; j++) {//attrcount��������һ�ж�
					if (contmp->rhsAttr.relName == NULL) {//��������δָ������ʱ��Ĭ��ΪrelName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//���ݱ����������ҵ���Ӧ����
						break;
					}
					ctmpright++;
				}
				//��conditions��ĳһ�����������ж�
				switch (ctmpright->attrType) {
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
			DeleteRec(rm_data, &(recdata.rid));
		}
	}
	delete[] column;

	//�ر��ļ����
	rc = RM_CloseFile(rm_table);
	if (rc != SUCCESS)
		return rc;
	delete rm_table;
	rc = RM_CloseFile(rm_column);
	if (rc != SUCCESS)
		return rc;
	delete rm_column;
	rc = RM_CloseFile(rm_data);
	if (rc != SUCCESS)
		return rc;
	delete rm_data;
	return SUCCESS;
}

RC Update(char *relName, char *attrName, Value *Value, int nConditions, Condition *conditions) {
	//ֻ�ܽ��е�ֵ����
	RM_FileHandle *rm_data, *rm_table, *rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record recdata, rectab, reccol;
	SysColumn *column, *ctmp, *cupdate, *ctmpleft, *ctmpright;
	Condition *contmp;
	int i, torf;//�Ƿ����ɾ������
	int attrcount;//��ʱ ��������
	int intleft, intright;
	char *charleft, *charright;
	float floatleft, floatright;//��ʱ ���Ե�ֵ
	AttrType attrtype;

	//�����ݱ�,ϵͳ��ϵͳ���ļ�
	rm_data = new RM_FileHandle();
	rm_data->bOpen = false;
	rc = RM_OpenFile(relName, rm_data);
	if (rc != SUCCESS)
		return rc;
	rm_table = new RM_FileHandle();
	rm_table->bOpen = false;
	rc = RM_OpenFile("SYSTABLES", rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column = new RM_FileHandle();
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
	column = new SysColumn[attrcount]();
	ctmp = column;
	while (GetNextRec(&FileScan, &reccol) == SUCCESS) {
		if (strcmp(relName, reccol.pData) == 0) {//�ҵ�����ΪrelName�ĵ�һ����¼�����ζ�ȡattrcount����¼
			for (int i = 0; i < attrcount; i++) {
				memcpy(ctmp->tabName, reccol.pData, 21);
				memcpy(ctmp->attrName, reccol.pData + 21, 21);
				memcpy(&(ctmp->attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(ctmp->attrLength), reccol.pData + 42 + sizeof(AttrType), sizeof(int));
				memcpy(&(ctmp->attrOffset), reccol.pData + 42 + sizeof(int) + sizeof(AttrType), sizeof(int));
				if ((strcmp(relName, ctmp->tabName) == 0) && (strcmp(attrName, ctmp->attrName) == 0)) {
					cupdate = ctmp;//�ҵ�Ҫ�������� ��Ӧ������
				}
				rc = GetNextRec(&FileScan, &reccol);
				if (rc != SUCCESS)
					break;
				ctmp++;
			}
			break;
		}
	}

	//ͨ��getdata������ȡϵͳ����Ϣ,�õ�����Ϣ������recdata��
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, rm_data, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//ѭ�����ұ���ΪrelName��Ӧ�����ݱ��еļ�¼,������¼��Ϣ������recdata��
	while (GetNextRec(&FileScan, &recdata) == SUCCESS) {
		for (i = 0, torf = 1, contmp = conditions; i < nConditions; i++, contmp++) {//conditions������һ�ж�
			ctmpleft = ctmpright = column;//ÿ��ѭ����Ҫ����������ϵͳ���ļ����ҵ�����������Ӧ������
			//��������ֵ
			if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 0) {
				for (int j = 0; j < attrcount; j++) {//attrcount��������һ�ж�
					if (contmp->lhsAttr.relName == NULL) {//��������δָ������ʱ��Ĭ��ΪrelName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//���ݱ����������ҵ���Ӧ����
						break;
					}
					ctmpleft++;
				}
				//��conditions��ĳһ�����������ж�
				if (ctmpleft->attrType == ints) {//�ж����Ե�����
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, contmp->rhsValue.data, sizeof(int));
				}
				else if (ctmpleft->attrType == chars) {
					attrtype = chars;
					charleft = new char[ctmpleft->attrLength];
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpleft->attrLength);
					charright = new char[ctmpleft->attrLength];
					memcpy(charright, contmp->rhsValue.data, ctmpleft->attrLength);
				}
				else if (ctmpleft->attrType == floats) {
					attrtype = floats;
					memcpy(&floatleft, recdata.pData + ctmpleft->attrOffset, sizeof(float));
					memcpy(&floatright, contmp->rhsValue.data, sizeof(float));
				}
				else
					torf &= 0;
			}
			//��������ֵ
			else  if (contmp->bLhsIsAttr == 0 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {//attrcount��������һ�ж�
					if (contmp->rhsAttr.relName == NULL) {//��������δָ������ʱ��Ĭ��ΪrelName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//���ݱ����������ҵ���Ӧ����
						break;
					}
					ctmpright++;
				}
				//��conditions��ĳһ�����������ж�
				if (ctmpright->attrType == ints) {//�ж����Ե�����
					attrtype = ints;
					memcpy(&intleft, contmp->lhsValue.data, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
				}
				else if (ctmpright->attrType == chars) {
					attrtype = chars;
					charleft = new char[ctmpright->attrLength];
					memcpy(charleft, contmp->lhsValue.data, ctmpright->attrLength);
					charright = new char[ctmpright->attrLength];
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
				}
				else if (ctmpright->attrType == floats) {
					attrtype = floats;
					memcpy(&floatleft, contmp->lhsValue.data, sizeof(float));
					memcpy(&floatright, recdata.pData + ctmpright->attrOffset, sizeof(float));
				}
				else
					torf &= 0;
			}
			//���Ҿ�����
			else  if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {//attrcount��������һ�ж�
					if (contmp->lhsAttr.relName == NULL) {//��������δָ������ʱ��Ĭ��ΪrelName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//���ݱ����������ҵ���Ӧ����
						break;
					}
					ctmpleft++;
				}
				for (int j = 0; j < attrcount; j++) {//attrcount��������һ�ж�
					if (contmp->rhsAttr.relName == NULL) {//��������δָ������ʱ��Ĭ��ΪrelName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//���ݱ����������ҵ���Ӧ����
						break;
					}
					ctmpright++;
				}
				//��conditions��ĳһ�����������ж�
				if (ctmpright->attrType == ints && ctmpleft->attrType == ints) {//�ж����Ե�����
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
				}
				else if (ctmpright->attrType == chars && ctmpleft->attrType == chars) {
					attrtype = chars;
					charleft = new char[ctmpright->attrLength];
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpright->attrLength);
					charright = new char[ctmpright->attrLength];
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
				}
				else if (ctmpright->attrType == floats && ctmpleft->attrType == floats) {
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
		if (torf == 1) {
			memcpy(recdata.pData + cupdate->attrOffset, Value->data, cupdate->attrLength);
			UpdateRec(rm_data, &recdata);
		}
	}

	delete[] column;

	//�ر��ļ����
	rc = RM_CloseFile(rm_table);
	if (rc != SUCCESS)
		return rc;
	delete rm_table;
	rc = RM_CloseFile(rm_column);
	if (rc != SUCCESS)
		return rc;
	delete rm_column;
	rc = RM_CloseFile(rm_data);
	if (rc != SUCCESS)
		return rc;
	delete rm_data;
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
				memcpy(&intNum, res.res[i][j], sizeof(int));
				rows[i][j] = new char[32];
				sprintf_s(rows[i][j], 32, "%d", intNum);
				break;
			case AttrType::floats:
				memcpy(&floatNum, res.res[i][j], sizeof(float));
				y = res.length[j];
				rows[i][j] = new char[32];
				sprintf_s(rows[i][j], 32, "%f", floatNum);
				break;
			case AttrType::chars:
				rows[i][j] = new char[res.length[j]];
				memcpy(rows[i][j], res.res[i][j], res.length[j]);
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
