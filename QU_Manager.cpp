#include "StdAfx.h"
#include "QU_Manager.h"
#include "RM_Manager.h"

RC singleConditionSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res);
RC multiSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res);
RC recurSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res, int curTable, int *offsets, char *curResult);
RC checkTable(int nRelations, char **relations);

void Init_Result(SelResult * res) {
	res->next_res = NULL;
}

void Destory_Result(SelResult * res) {
	for (int i = 0; i < res->row_num; i++) {
		for (int j = 0; j < res->col_num; j++) {
			delete[] res->res[i][j];
		}
		delete[] res->res[i];
	}
	if (res->next_res != NULL) {
		Destory_Result(res->next_res);
	}

}

RC Query(char * sql, SelResult * res) {
	RC rc;
	sqlstr * sqlType = NULL;
	sqlType = get_sqlstr();
	rc = parse(sql, sqlType);

	if (rc != SUCCESS)
	{
		return rc;
	}

	selects sel = sqlType->sstr.sel;
	rc = Select(sel.nSelAttrs, sel.selAttrs, sel.nRelations, sel.relations, sel.nConditions, sel.conditions, res);

	return rc;
}

/*
* ��һ�������ʾ��ѯ�漰������
* �ڶ����ʾ��ѯ�漰�ı�
* �������ʾ��ѯ����
* ���һ������res���ڷ��ز�ѯ�����
* ��ѯ�Ż����Ż���ѯ���̣�����ѯ�漰�����ʱ����Ƹ�Ч�Ĳ�ѯ����
*/

RC Select(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res)
{

	RC rc;
	SelResult *result = res;

	rc = checkTable(nRelations, relations);
	if (rc != SUCCESS)
	{
		return rc;
	}

	/*�ֵ����ѯ�Ͷ���ѯ*/
	if (nRelations == 1)
	{ //�����ѯ
		if (nConditions == 0)
		{  //��������ѯ

		   //���ĳ������������������������ѯ������ȫ�ļ�ɨ��
			if (false)
			{ //�˴��ж������������δʵ��

			}
			else { //��������ȫ��ɨ��

				/*ɨ��SYSTABLES��ñ�����Ը���*/
				RM_FileHandle *rm_table = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
				rm_table->bOpen = false;
				rc = RM_OpenFile("SYSTABLES", rm_table);
				if (rc != SUCCESS)
					return rc;

				RM_FileScan *rm_fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
				RM_Record *record = (RM_Record *)malloc(sizeof(RM_Record));
				Con con;

				con.attrType = chars;
				con.bLhsIsAttr = 1;
				con.LattrOffset = 0;
				con.LattrLength = strlen(*relations) + 1;
				con.compOp = EQual;
				con.bRhsIsAttr = 0;
				con.Rvalue = *relations;

				rm_fileScan->bOpen = false;
				rc = OpenScan(rm_fileScan, rm_table, 1, &con);
				if (rc != SUCCESS)
					return rc;

				rc = GetNextRec(rm_fileScan, record);
				if (rc != SUCCESS)
					return rc;

				//���ò�ѯ�������
				memcpy(&(result->col_num), record->pData + sizeof(SysTable::tabName), sizeof(SysTable::attrCount));
				//�ͷ���Դ
				CloseScan(rm_fileScan);
				RM_CloseFile(rm_table);
				free(rm_table);

				/*ɨ��SYSCOLUMNS������Ե�ƫ����������*/
				RM_FileHandle *rm_column = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
				rm_column->bOpen = false;
				rc = RM_OpenFile("SYSCOLUMNS", rm_column);
				if (rc != SUCCESS)
					return rc;

				rc = OpenScan(rm_fileScan, rm_column, 1, &con);
				if (rc != SUCCESS)
					return rc;

				for (int i = 0; i < result->col_num; i++)
				{
					rc = GetNextRec(rm_fileScan, record);
					if (rc != SUCCESS)
						return rc;

					char * column = record->pData;
					//��������
					memcpy(&result->type[i], column + 42, sizeof(AttrType));
					//������
					memcpy(&result->fields[i], column + 21, 21);
					//����ƫ����
					memcpy(&result->offset[i], column + 50, sizeof(int));
					//���Գ���
					memcpy(&result->length[i], column + 46, sizeof(int));

					free(record->pData);
				}
				CloseScan(rm_fileScan);
				RM_CloseFile(rm_column);
				free(rm_column);

				/*ɨ���¼���ҳ����м�¼*/
				RM_FileHandle *rm_data = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
				rm_data->bOpen = false;
				rc = RM_OpenFile(*relations, rm_data);
				if (rc != SUCCESS)
					return rc;

				rc = OpenScan(rm_fileScan, rm_data, 0, NULL);
				if (rc != SUCCESS)
					return rc;

				int i = 0;
				result->row_num = 0;
				SelResult *curRes = result;  //β�巨�������в����½��
				while (GetNextRec(rm_fileScan, record) == SUCCESS)
				{
					if (curRes->row_num >= 100) //ÿ���ڵ�����¼100����¼
					{ //��ǰ����Ѿ�����100����¼ʱ���½����
						curRes->next_res = (SelResult *)malloc(sizeof(SelResult));

						//��ʼ��
						*(curRes->next_res) = *curRes;

						curRes = curRes->next_res;
						curRes->row_num = 0;
						curRes->next_res = NULL;
						/*	curRes->next_res->col_num = curRes->col_num;
							for (int j = 0; j < curRes->col_num; j++)
							{
								strncpy(curRes->next_res->fields[i], curRes->fields[i], strlen(curRes->fields[i]));
								curRes->next_res->type[i] = curRes->type[i];
								curRes->next_res->offset[i] = curRes->offset[i];
							}

							curRes = curRes->next_res;
							curRes->next_res = NULL;
							curRes->row_num = 0;*/
					}

					auto &resultRecord = curRes->res[curRes->row_num];//������е�һ����¼

					int row_num = curRes->row_num;
					int col_num = curRes->col_num;
					resultRecord = new char*[col_num];//�ü�¼������ÿ�����Ե�ֵ����curRes->col_num������

					//�����������ֵ��record->pData����ȡ����
					for (int i = 0; i < result->col_num; i++) {
						resultRecord[i] = new char[curRes->length[i]];
						memcpy(resultRecord[i], record->pData + curRes->offset[i], curRes->length[i]);
					}
					curRes->row_num++;
					free(record->pData);
				}

				CloseScan(rm_fileScan);
				free(rm_fileScan);
				RM_CloseFile(rm_data);
				free(rm_data);
			}

		}
		else
		{ //������ѯ
			singleConditionSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res);
		}

	}
	else if (nRelations > 1)
	{ //���������ѯ
		multiSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res);
	}

	res = result;

	return SUCCESS;
}

//����������ѯ
RC singleConditionSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res)
{
	RC rc;
	SelResult *resHead = res;

	//���ĳ������������������������ѯ������ȫ�ļ�ɨ��
	if (false)
	{ //�˴��ж������������δʵ��

	}
	else {

		RM_FileHandle *rm_fileHandle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));

		RM_FileScan *rm_fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
		RM_Record *record = (RM_Record *)malloc(sizeof(RM_Record));
		Con cons[2];

		cons[0].attrType = chars;
		cons[0].bLhsIsAttr = 1;
		cons[0].LattrOffset = 0;
		cons[0].LattrLength = strlen(*relations) + 1;
		cons[0].compOp = EQual;
		cons[0].bRhsIsAttr = 0;
		cons[0].Rvalue = *relations;

		if (nSelAttrs == 1 && !strcmp((*selAttrs)->attrName, "*"))
		{  //��ѯ���Ϊ��������
			rc = RM_OpenFile("SYSTABLES", rm_fileHandle);
			if (rc != SUCCESS) return rc;

			OpenScan(rm_fileScan, rm_fileHandle, 1, &cons[0]);
			rc = GetNextRec(rm_fileScan, record);
			if (rc != SUCCESS) return rc;

			SysTable *table = (SysTable *)record->pData;
			memcpy(&(resHead->col_num), record->pData + 21, sizeof(int));  //���ò�ѯ�������

			CloseScan(rm_fileScan);
			RM_CloseFile(rm_fileHandle);

			//������Ե�ƫ����������
			rc = RM_OpenFile("SYSCOLUMNS", rm_fileHandle);
			if (rc != SUCCESS) return rc;

			OpenScan(rm_fileScan, rm_fileHandle, 1, &cons[0]);

			for (int i = 0; i < resHead->col_num; i++)
			{
				rc = GetNextRec(rm_fileScan, record);
				if (rc != SUCCESS) return rc;

				//SysColumns *column = (SysColumns *)record->pData;
				char * column = record->pData;
				//��������
				//resHead->attrType[i] = column->attrtype;
				memcpy(&resHead->type[i], column + 42, sizeof(int));
				//������
				memcpy(&resHead->fields[i], column + 21, 21);
				//strncpy(resHead->fields[i], column->attrname, column->attrlength);
				//����ƫ����
				memcpy(&resHead->offset[i], column + 50, sizeof(int));
				//���Գ���
				memcpy(&resHead->length[i], column + 46, sizeof(int));

			}
			CloseScan(rm_fileScan);
			RM_CloseFile(rm_fileHandle);
		}
		else {  //��ѯ���Ϊָ������
			resHead->col_num = nSelAttrs; //���ò�ѯ�������

			//������Ե�ƫ����������
			cons[1].attrType = chars;
			cons[1].bLhsIsAttr = 1;
			cons[1].LattrOffset = 21;
			cons[1].LattrLength = 21;
			cons[1].compOp = EQual;
			cons[1].bRhsIsAttr = 0;

			rc = RM_OpenFile("SYSCOLUMNS", rm_fileHandle);
			if (rc != SUCCESS) return rc;

			for (int i = 0; i < resHead->col_num; i++)
			{
				cons[1].Rvalue = (selAttrs[resHead->col_num - i - 1])->attrName;
				OpenScan(rm_fileScan, rm_fileHandle, 2, cons);

				rc = GetNextRec(rm_fileScan, record);
				if (rc != SUCCESS) return rc;

				//SysColumns *column = (SysColumns *)record->pData;
				char * column = record->pData;
				//��������
				//resHead->attrType[i] = column->attrtype;
				memcpy(&resHead->type[i], column + 42, sizeof(int));
				//������
				memcpy(&resHead->fields[i], column + 21, 21);
				//strncpy(resHead->fields[i], column->attrname, column->attrlength);
				//����ƫ����
				memcpy(&resHead->offset[i], column + 50, sizeof(int));
				//���Գ���
				memcpy(&resHead->length[i], column + 46, sizeof(int));

				CloseScan(rm_fileScan);
			}

			RM_CloseFile(rm_fileHandle);
		}

		rc = RM_OpenFile("SYSCOLUMNS", rm_fileHandle);
		if (rc != SUCCESS) return rc;

		cons[1].attrType = chars;
		cons[1].bLhsIsAttr = 1;
		cons[1].LattrOffset = 21;
		cons[1].LattrLength = 21;
		cons[1].compOp = EQual;
		cons[1].bRhsIsAttr = 0;

		//��������ѯ��������Ϊɨ������
		Con *selectCons = (Con *)malloc(sizeof(Con) * nConditions);
		for (int i = 0; i < nConditions; i++) {
			//ֻ������con[1]->rValue,����������������óɲ�ѯʱ��ֵ
			if (conditions[i].bLhsIsAttr == 0 && conditions[i].bRhsIsAttr == 1)
			{  //�����ֵ���ұ�������
				(cons + 1)->Rvalue = conditions[i].rhsAttr.attrName;
			}
			else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 0)
			{   //��������ԣ��ұ���ֵ
				(cons + 1)->Rvalue = conditions[i].lhsAttr.attrName;
			}
			else
			{  //���߶������Ի����߶���ֵ���ݲ�����

			}

			OpenScan(rm_fileScan, rm_fileHandle, 2, cons);
			rc = GetNextRec(rm_fileScan, record);
			if (rc != SUCCESS) return rc;

			//cons[i].attrType = conditions[i].attrType;
			selectCons[i].bLhsIsAttr = conditions[i].bLhsIsAttr;
			selectCons[i].bRhsIsAttr = conditions[i].bRhsIsAttr;
			selectCons[i].compOp = conditions[i].op;
			if (conditions[i].bLhsIsAttr == 1) //�������
			{ //�������Գ��Ⱥ�ƫ����
				memcpy(&selectCons[i].LattrLength, record->pData + 46, 4);
				memcpy(&selectCons[i].LattrOffset, record->pData + 50, 4);
			}
			else {
				selectCons[i].attrType = conditions[i].lhsValue.type;
				selectCons[i].Lvalue = conditions[i].lhsValue.data;
			}

			if (conditions[i].bRhsIsAttr == 1) {
				memcpy(&selectCons[i].RattrLength, record->pData + 46, 4);
				memcpy(&selectCons[i].RattrOffset, record->pData + 50, 4);
			}
			else {
				selectCons[i].attrType = conditions[i].rhsValue.type;
				selectCons[i].Rvalue = conditions[i].rhsValue.data;
			}

			CloseScan(rm_fileScan);

		}

		RM_CloseFile(rm_fileHandle);

		//ɨ���¼���ҳ����м�¼
		rc = RM_OpenFile(*relations, rm_fileHandle);
		if (rc != SUCCESS) return rc;

		OpenScan(rm_fileScan, rm_fileHandle, nConditions, selectCons);

		int i = 0;
		resHead->row_num = 0;
		SelResult *curRes = resHead;  //β�巨�������в����½��
		while (GetNextRec(rm_fileScan, record) == SUCCESS)
		{
			if (curRes->row_num >= 100) //ÿ���ڵ�����¼100����¼
			{ //��ǰ����Ѿ�����100����¼ʱ���½����
				curRes->next_res = (SelResult *)malloc(sizeof(SelResult));
				curRes->next_res->col_num = curRes->col_num;
				for (int j = 0; j < curRes->col_num; j++)
				{
					strncpy_s(curRes->next_res->fields[i], curRes->fields[i], strlen(curRes->fields[i]));
					curRes->next_res->type[i] = curRes->type[i];
					curRes->next_res->offset[i] = curRes->offset[i];
				}

				curRes = curRes->next_res;
				curRes->next_res = NULL;
				curRes->row_num = 0;
			}

			curRes->res[curRes->row_num] = (char **)malloc(sizeof(char *));
			*(curRes->res[curRes->row_num++]) = record->pData;

			//int x;
			//memcpy(&x, record->pData + 18, sizeof(int));
			//memcpy(&x, *(curRes->res[curRes->row_num-1]) + 22, sizeof(int));
		}

		CloseScan(rm_fileScan);
		RM_CloseFile(rm_fileHandle);

		free(rm_fileHandle);
		free(rm_fileScan);
		free(record);
	}

	res = resHead;

	return SUCCESS;
}

/*
* ��һ�������ʾ��ѯ�漰������
* �ڶ����ʾ��ѯ�漰�ı�
* �������ʾ��ѯ����
* ���һ������res���ڷ��ز�ѯ�����
*/
//����ѯ
RC multiSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res)
{
	RC rc;
	SelResult *resHead = res;

	//���ĳ������������������������ѯ������ȫ�ļ�ɨ��
	if (false)
	{ //�˴��ж������������δʵ��

	}
	else {
		RM_FileHandle *rm_fileHandle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
		RM_FileHandle **fileHandles = (RM_FileHandle **)malloc(sizeof(RM_FileHandle *) * nRelations);//��ѯ�漰��ÿ������ļ����

		RM_FileScan *rm_fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
		//RM_FileScan **fileScans = (RM_FileScan **)malloc(sizeof(RM_FileScan *) * nRelations); //��ѯ�漰��ÿ������ļ�ɨ��ָ��

		for (int i = 0; i < nRelations; i++)
		{
			fileHandles[i] = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
			//fileScans[i] = (RM_FileScan *)malloc(sizeof(RM_FileScan));

			rc = RM_OpenFile(relations[nRelations - i - 1], fileHandles[i]);  //�������漰���ı��ļ�
			if (rc != SUCCESS) return rc;
		}

		int *offsets = (int *)malloc(sizeof(int)*(nRelations + 1));  //ÿ�ű�Ĳ�ѯ������ܲ�ѯ����е���ʼƫ��
																	//���һ��ֵ�洢���ܲ�ѯ����ĳ���
																	//�˲�ѯ�Ĳ�ѯ����洢�漰�������б��������Ϣ
		offsets[0] = 0;
		for (int i = 1; i < nRelations + 1; i++)
		{
			offsets[i] = offsets[i - 1] + fileHandles[i - 1]->fileSubHeader->recordSize;
		}

		RM_Record *record = (RM_Record *)malloc(sizeof(RM_Record));
		Con cons[2];

		cons[0].attrType = chars;
		cons[0].bLhsIsAttr = 1;
		cons[0].LattrOffset = 0;
		cons[0].LattrLength = 21;
		cons[0].compOp = EQual;
		cons[0].bRhsIsAttr = 0;
		//cons[0].Rvalue = *relations;

		if (nSelAttrs == 1 && !strcmp((*selAttrs)->attrName, "*"))
		{  //��ѯ���Ϊ��������

		}
		else {  //��ѯ���Ϊָ������
			resHead->col_num = nSelAttrs; //���ò�ѯ�������
			resHead->row_num = 0;

			//������Ե�ƫ����������
			cons[1].attrType = chars;
			cons[1].bLhsIsAttr = 1;
			cons[1].LattrOffset = 21;
			cons[1].LattrLength = 21;
			cons[1].compOp = EQual;
			cons[1].bRhsIsAttr = 0;

			rc = RM_OpenFile("SYSCOLUMNS", rm_fileHandle);
			if (rc != SUCCESS) return rc;

			for (int i = 0; i < resHead->col_num; i++)
			{
				cons[0].Rvalue = (selAttrs[resHead->col_num - i - 1])->relName;  //����
				cons[1].Rvalue = (selAttrs[resHead->col_num - i - 1])->attrName; //������
				OpenScan(rm_fileScan, rm_fileHandle, 2, cons);

				rc = GetNextRec(rm_fileScan, record);
				if (rc != SUCCESS) return rc;

				int j = 0;   //��ǰ���ڽ���е�λ��
				for (; j < nRelations; j++)
				{
					if (!strcmp(relations[nRelations - j - 1], (char *)cons[0].Rvalue))
					{
						break;
					}
				}

				//SysColumns *column = (SysColumns *)record->pData;
				char * column = record->pData;
				//��������
				//resHead->attrType[i] = column->attrtype;
				memcpy(&resHead->type[i], column + 42, sizeof(int));
				//������
				memcpy(&resHead->fields[i], column + 21, 21);
				//strncpy(resHead->fields[i], column->attrname, column->attrlength);
				//����ƫ����
				memcpy(&resHead->offset[i], column + 50, sizeof(int));
				resHead->offset[i] += offsets[j];
				//���Գ���
				memcpy(&resHead->length[i], column + 46, sizeof(int));

				CloseScan(rm_fileScan);
			}

			RM_CloseFile(rm_fileHandle);
		}

		for (int i = 0; i < nRelations; i++)
		{
			rc = RM_CloseFile(fileHandles[i]);  //�ر������漰���ı��ļ�
			if (rc != SUCCESS) return rc;
		}

		//�ͷ�������ڴ�ռ�
		free(rm_fileHandle);
		free(rm_fileScan);
		for (int i = 0; i < nRelations; i++)
		{
			free(fileHandles[i]);
			//free(fileScans[i]);
		}
		free(fileHandles);
		//free(fileScans);
		free(record);


		//�ݹ�Ļ�ȡ����ѯ�Ĳ�ѯ���
		recurSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res, nRelations - 1, offsets, NULL);

		free(offsets);
	}

	res = resHead;

	return SUCCESS;
}

/*
* ��һ�������ʾ��ѯ�漰������
* �ڶ����ʾ��ѯ�漰�ı�
* �������ʾ��ѯ����
* ���һ������res���ڷ��ز�ѯ�����
*curTable: ��ǰҪ����ı��λ��
*offsets: ���ѯ������ܵĲ�ѯ����е�ƫ��
*/
//�ݹ�Ļ�ȡ����ѯ�Ĳ�ѯ���
RC recurSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res, int curTable, int *offsets, char *curResult) {
	if (curTable < 0)  //�ݹ����
	{

		SelResult *curRes = res;  //β�巨�������в����½��
		while (curRes->next_res != NULL) {
			curRes = curRes->next_res;
		}

		if (curRes->row_num >= 100) //ÿ���ڵ�����¼100����¼
		{ //��ǰ����Ѿ�����100����¼ʱ���½����
			curRes->next_res = (SelResult *)malloc(sizeof(SelResult));
			//curRes->next_res->col_num = curRes->col_num;
			//for (int j = 0; j < curRes->col_num; j++)
			//{
				//strncpy(curRes->next_res->fields[i], curRes->fields[i], strlen(curRes->fields[i]));
				//curRes->next_res->attrType[i] = curRes->attrType[i];
				//curRes->next_res->offset[i] = curRes->offset[i];
			//}

			curRes = curRes->next_res;
			curRes->next_res = NULL;
			curRes->row_num = 0;
		}

		curRes->res[curRes->row_num] = (char **)malloc(sizeof(char *));
		*(curRes->res[curRes->row_num]) = (char *)malloc(sizeof(char)*offsets[nRelations]);
		memcpy(*(curRes->res[curRes->row_num]), curResult, offsets[nRelations]);
		curRes->row_num++;
		return SUCCESS;
	}

	RC rc;
	RM_FileHandle *rm_fileHandle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));

	RM_FileScan *rm_fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
	RM_Record *record = (RM_Record *)malloc(sizeof(RM_Record));
	int nSelectCons = 0;  //��������
	Con **selectCons = (Con **)malloc(sizeof(Con *) * nConditions);  //����
	Con cons[2];     //���ڲ�ѯϵͳ�б���ȡ������Ϣ

	RelAttr another;
	int nAnother;

	/*selectCons[0] = (Con *)malloc(sizeof(Con));
	selectCons[0]->attrType = chars;
	selectCons[0]->bLhsIsAttr = 1;
	selectCons[0]->LattrOffset = 0;
	selectCons[0]->LattrLength = 21;
	selectCons[0]->compOp = EQual;
	selectCons[0]->bRhsIsAttr = 0;
	selectCons[0]->Rvalue = relations[curTable];*/


	rc = RM_OpenFile("SYSCOLUMNS", rm_fileHandle);
	if (rc != SUCCESS) return rc;

	//���ڲ�ѯϵͳ�б������
	cons[0].attrType = chars;
	cons[0].bLhsIsAttr = 1;
	cons[0].LattrOffset = 0;
	cons[0].LattrLength = 21;
	cons[0].compOp = EQual;
	cons[0].bRhsIsAttr = 0;
	cons[0].Rvalue = relations[curTable];

	cons[1].attrType = chars;
	cons[1].bLhsIsAttr = 1;
	cons[1].LattrOffset = 21;
	cons[1].LattrLength = 21;
	cons[1].compOp = EQual;
	cons[1].bRhsIsAttr = 0;

	for (int i = 0; i < nConditions; i++)
	{
		if (!((conditions[i].bLhsIsAttr == 1 && !strcmp(conditions[i].lhsAttr.relName, relations[curTable]))
			|| (conditions[i].bRhsIsAttr == 1 && !strcmp(conditions[i].rhsAttr.relName, relations[curTable]))))
			continue;   //�����뵱ǰ���޹ص�����

		selectCons[nSelectCons] = (Con *)malloc(sizeof(Con));

		//ֻ������con[1]->Rvalue,����������������óɲ�ѯʱ��ֵ
		if (conditions[i].bLhsIsAttr == 0 && conditions[i].bRhsIsAttr == 1)
		{  //�����ֵ���ұ�������
			cons[1].Rvalue = conditions[i].rhsAttr.attrName;
		}
		else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 0)
		{   //��������ԣ��ұ���ֵ
			cons[1].Rvalue = conditions[i].lhsAttr.attrName;
		}
		else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 1)
		{  //���߶�������
			if (!strcmp(conditions[i].lhsAttr.relName, relations[curTable]))
			{ //�����Ҫ��ѯ�ı�
				cons[1].Rvalue = conditions[i].lhsAttr.attrName;
				another.relName = conditions[i].rhsAttr.relName;
				another.attrName = conditions[i].rhsAttr.attrName;
			}
			else
			{  //�ұ���Ҫ��ѯ�ı�
				cons[1].Rvalue = conditions[i].rhsAttr.attrName;
				another.relName = conditions[i].lhsAttr.relName;
				another.attrName = conditions[i].lhsAttr.attrName;
			}

			int j = 0;
			for (; j < nRelations; j++) {
				if (!strcmp(relations[j], another.relName))
				{
					break;
				}
			}

			if (j >= nRelations)  //�﷨����
			{
				return SQL_SYNTAX;
			}

			if (j < curTable)  //������ͬһ�������ԱȽϵ����
			{
				continue;
			}

			nAnother = j;
		}
		else
		{  //���߶���ֵ���ݲ�����

		}

		OpenScan(rm_fileScan, rm_fileHandle, 2, cons);
		rc = GetNextRec(rm_fileScan, record);
		if (rc != SUCCESS) return rc;

		//�������Գ��Ⱥ�ƫ����
		if (conditions[i].bLhsIsAttr == 0 && conditions[i].bRhsIsAttr == 1)
		{  //�����ֵ���ұ�������
			selectCons[nSelectCons]->bLhsIsAttr = conditions[i].bLhsIsAttr;
			selectCons[nSelectCons]->bRhsIsAttr = conditions[i].bRhsIsAttr;
			selectCons[nSelectCons]->compOp = conditions[i].op;

			memcpy(&selectCons[nSelectCons]->RattrLength, record->pData + 46, 4);
			memcpy(&selectCons[nSelectCons]->RattrOffset, record->pData + 50, 4);
			selectCons[nSelectCons]->attrType = conditions[i].lhsValue.type;
			selectCons[nSelectCons]->Lvalue = conditions[i].lhsValue.data;
		}
		else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 0)
		{   //��������ԣ��ұ���ֵ
			selectCons[nSelectCons]->bLhsIsAttr = conditions[i].bLhsIsAttr;
			selectCons[nSelectCons]->bRhsIsAttr = conditions[i].bRhsIsAttr;
			selectCons[nSelectCons]->compOp = conditions[i].op;

			memcpy(&selectCons[nSelectCons]->LattrLength, record->pData + 46, 4);
			memcpy(&selectCons[nSelectCons]->LattrOffset, record->pData + 50, 4);
			selectCons[nSelectCons]->attrType = conditions[i].rhsValue.type;
			selectCons[nSelectCons]->Rvalue = conditions[i].rhsValue.data;
		}
		else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 1)
		{  //���߶�������
			selectCons[nSelectCons]->bLhsIsAttr = 1;
			selectCons[nSelectCons]->bRhsIsAttr = 0;
			selectCons[nSelectCons]->compOp = conditions[i].op;

			memcpy(&selectCons[nSelectCons]->LattrLength, record->pData + 46, 4);
			memcpy(&selectCons[nSelectCons]->LattrOffset, record->pData + 50, 4);
			memcpy(&selectCons[nSelectCons]->attrType, record->pData + 42, 4);
			//if (!strcmp(conditions[i].lhsAttr.relName, relations[curTable]))
			//{ //�����Ҫ��ѯ�ı�, �ұ������ѿ������ı�
			//	another.relName = conditions[i].rhsAttr.relName;
			//	another.attrName = conditions[i].rhsAttr.attrName;
			//}
			//else
			//{  //�ұ���Ҫ��ѯ�ı�, �ұ������ѿ������ı�
			//	another.relName = conditions[i].lhsAttr.relName;
			//	another.attrName = conditions[i].lhsAttr.attrName;
			//}

			//��ϵͳ�б���ȡ�������ѿ����������Ե���Ϣ

			RM_FileHandle *fileHandle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
			rc = RM_OpenFile("SYSCOLUMNS", fileHandle);
			if (rc != SUCCESS) return rc;

			RM_FileScan *fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));

			cons[0].Rvalue = another.relName;
			cons[1].Rvalue = another.attrName;
			OpenScan(fileScan, fileHandle, 2, cons);

			rc = GetNextRec(fileScan, record);
			if (rc != SUCCESS) return rc;

			//memcpy(&selectCons[nSelectCons]->attrType, record->pData + 42, 4);
			//���ϲ��ѯ�������ȡ�����ڵѿ�����������ֵ
			int offset;
			memcpy(&offset, record->pData + 50, 4);
			selectCons[nSelectCons]->Rvalue = curResult + offsets[nRelations - nAnother - 1] + offset;

			CloseScan(fileScan);
			RM_CloseFile(fileHandle);
			free(fileScan);
			free(fileHandle);
		}

		nSelectCons++;
		CloseScan(rm_fileScan);

	}
	RM_CloseFile(rm_fileHandle);

	//ɨ���¼���ҳ����м�¼
	rc = RM_OpenFile(relations[curTable], rm_fileHandle);
	if (rc != SUCCESS) return rc;

	OpenScan(rm_fileScan, rm_fileHandle, nSelectCons, *selectCons);

	rc = GetNextRec(rm_fileScan, record);

	while (rc == SUCCESS)
	{
		char * result = (char *)malloc(sizeof(char)*(offsets[nRelations - curTable - 1 + 1]));
		memcpy(result, curResult, offsets[nRelations - curTable - 1]);
		memcpy(result + offsets[nRelations - curTable - 1], record->pData, rm_fileHandle->fileSubHeader->recordSize);

		recurSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res, curTable - 1, offsets, result);
		free(result);

		rc = GetNextRec(rm_fileScan, record);
		//int x;
		//memcpy(&x, record->pData + 18, sizeof(int));
		//memcpy(&x, *(curRes->res[curRes->row_num-1]) + 22, sizeof(int));
	}

	CloseScan(rm_fileScan);
	RM_CloseFile(rm_fileHandle);

	free(rm_fileHandle);
	free(rm_fileScan);
	free(record);
	for (int i = 0; i < nSelectCons; i++)
	{
		free(selectCons[i]);
	}
	selectCons = NULL;
	//free(selectCons);
	return SUCCESS;
}


//�жϲ�ѯ�漰�ı��Ƿ񶼴���
//�������
//    nRelations: ��ĸ���
//    relations: ָ����������ָ��
//������һ��������ʱ������RM_EOF
//�����ڣ��򷵻�SUCCESS
RC checkTable(int nRelations, char **relations)
{
	RC rc;

	RM_FileHandle *sysTablesHandle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	sysTablesHandle->bOpen = false;
	rc = RM_OpenFile("SYSTABLES", sysTablesHandle);  //�����жϱ��Ƿ����
	if (rc != SUCCESS) 
		return rc;

	Con condition;
	condition.bLhsIsAttr = 1;
	condition.attrType = chars;
	condition.LattrOffset = 0;

	condition.bRhsIsAttr = 0;
	condition.compOp = EQual;

	RM_FileScan *sysTablesScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
	sysTablesScan->bOpen = false;
	RM_Record rec;
	for (int i = 0; i < nRelations; i++)
	{
		condition.Rvalue = *(relations + i);
		condition.LattrLength = strlen(*(relations + i)) + 1;
		rc = OpenScan(sysTablesScan, sysTablesHandle, 1, &condition);
		if (rc != SUCCESS) break;

		rc = GetNextRec(sysTablesScan, &rec);   //��������ڣ��򷵻�TABLE_NOT_EXIST
		if (rc != SUCCESS) {
			rc = TABLE_NOT_EXIST;
			break;
		}
		CloseScan(sysTablesScan);
	}
	if (sysTablesScan->bOpen)
	{
		CloseScan(sysTablesScan);
	}
	free(sysTablesScan);
	RM_CloseFile(sysTablesHandle);
	free(sysTablesHandle);
	return rc;
}

